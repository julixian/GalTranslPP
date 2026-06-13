module;

#include "GPPMacros.hpp"
#include <toml.hpp>
#include <ctpl_stl.h>

module NameTranslator;

import Tool;

namespace fs = std::filesystem;

NameTranslator::NameTranslator(
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::unique_ptr<APIPool>& apiPool,
    const std::unique_ptr<GptDictionary>& gptDictionary,
    const std::function<std::string(std::string)>& onPerformApi,
    const std::string& systemPrompt,
    const std::string& userPrompt,
    const std::string& apiStrategy,
    const std::string& targetLang,
    int maxRetries,
    int apiTimeoutMs,
    int threadsNum,
    int batchSize,
    bool checkQuota
)
    : m_controller(controller), m_logger(logger), m_apiPool(apiPool), m_gptDictionary(gptDictionary),
    m_onPerformApi(onPerformApi), m_systemPrompt(systemPrompt), m_userPrompt(userPrompt),
    m_apiStrategy(apiStrategy), m_targetLang(targetLang), m_maxRetries(maxRetries),
    m_apiTimeoutMs(apiTimeoutMs), m_threadsNum(std::max(1, threadsNum)), m_batchSize(std::max(1, batchSize)),
    m_checkQuota(checkQuota)
{

}

absl::flat_hash_map<std::string, std::string> NameTranslator::translateBatch(std::span<const std::string> batchNames, int threadId) {
    absl::flat_hash_map<std::string, std::string> resultMap;

    // 1. 准备 Glossary
    // 为了利用 GptDictionary，我们需要构造假的 Sentence 对象
    std::vector<Sentence> dummySentences;
    std::vector<Sentence*> dummyPtrs;
    dummySentences.reserve(batchNames.size());

    std::string inputBlock;
    for (const auto& name : batchNames) {
        Sentence se;
        se.pre_processed_text = name; // 将名字作为原文，以便字典匹配
        dummySentences.push_back(std::move(se));
        inputBlock += name + "\n";
    }
    for (auto& se : dummySentences) {
        dummyPtrs.push_back(&se);
    }

    const std::string glossary = m_gptDictionary->generatePrompt(dummyPtrs, TransEngine::NameTrans);

    // 2. 准备 Prompt
    std::string prompt = m_userPrompt;
    replaceStrInplace(prompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(prompt, "[Glossary]", glossary.empty() ? "None" : glossary);
    replaceStrInplace(prompt, "[Input]", inputBlock);

    json messages = json::array({
        {{"role", "system"}, {"content", m_systemPrompt}},
        {{"role", "user"}, {"content", prompt}}
        });

    // 3. 调用 API
    int retryCount = 0;
    while (retryCount == 0 || retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return resultMap;
        }

        const std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
        if (!apiOpt) {
            throw std::runtime_error("NameTrans: 没有可用的 API key 了");
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        std::string logBlock;
        if (!glossary.empty()) {
            logBlock += "\nDict:\n" + glossary;
        }
        logBlock += "\ninputBlock:\n" + inputBlock;
        m_logger->info("[线程 {}] 正在翻译人名表:\n{}", threadId, logBlock);

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeoutMs);

        if (!checkResponse(
            response, m_apiPool, currentApi, L"人名表翻译", m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
        )) {
            continue;
        }

        // 4. 解析结果
        m_logger->info("[线程 {}] AI 翻译人名成功:\n{}", threadId, response.content);
        const auto lines = splitStringView(response.content, '\n');
        for (const auto& line : lines) {
            const auto parts = splitStringView(line, '\t');
            // 跳过表头和无效行
            if (parts.size() < 2 || parts[0].starts_with("Original_Names")) {
                continue;
            }
            const std::string_view& original = parts[0];
            const std::string_view& translation = parts[1];

            if (!original.empty() && !translation.empty()) {
                resultMap[original] = translation;
            }
        }
        return resultMap; // 成功则退出重试循环
    }
    m_logger->error("[线程 {}] NameTrans: 批次翻译失败，已达到最大重试次数。", threadId);
    return resultMap;
}

void NameTranslator::run(const fs::path& nameTablePath) {
    if (!fs::exists(nameTablePath)) {
        m_logger->error("NameTrans: 未找到人名表文件 {}", wide2Ascii(nameTablePath));
        return;
    }

    m_logger->info("NameTrans: 开始处理人名表...");

    // 1. 读取 TOML
    toml::ordered_value nameTableData;
    try {
        nameTableData = toml::uoparse(nameTablePath);
    }
    catch (const toml::exception& e) {
        m_logger->error("NameTrans: 解析人名表失败: {}", e.what());
        return;
    }

    // 2. 收集需要翻译的名字
    std::vector<std::string> namesToTranslate;

    for (const auto& [key, value] : nameTableData.as_table()) {
        if (value.is_array() && value.size() > 0) {
            const std::string& currentTrans = value[0].as_string();
            // 如果译名为空，加入待翻译列表
            if (currentTrans.empty()) {
                namesToTranslate.push_back(key);
            }
        }
    }

    if (namesToTranslate.empty()) {
        m_logger->info("NameTrans: 没有发现需要翻译的名字（所有条目均已有译名）。");
        return;
    }

    m_logger->info("NameTrans: 共发现 {} 个待翻译的名字。", namesToTranslate.size());

    // 3. 分批处理
    absl::flat_hash_map<std::string, std::string> translationResults;
    const size_t batchCount = (namesToTranslate.size() + (size_t)m_batchSize - 1) / (size_t)m_batchSize;
    const int workerCount = std::max(1, std::min(m_threadsNum, (int)batchCount));
    m_logger->info("NameTrans: 启动 {} 个线程，每批处理 {} 个名字。", workerCount, m_batchSize);
    m_controller->makeBar((int)namesToTranslate.size(), workerCount);

    ctpl::thread_pool pool(workerCount);
    std::vector<std::future<absl::flat_hash_map<std::string, std::string>>> results;
    std::atomic<size_t> nextNameIndex = 0;
    for (int i = 0; i < workerCount; ++i) {
        results.emplace_back(pool.push([this, &namesToTranslate, &nextNameIndex](const int threadId)
            {
                m_controller->addThreadNum();
                try {
                    absl::flat_hash_map<std::string, std::string> workerResults;
                    while (!m_controller->shouldStop()) {
                        const size_t start = nextNameIndex.fetch_add((size_t)m_batchSize);
                        if (start >= namesToTranslate.size()) {
                            break;
                        }
                        const size_t count = std::min((size_t)m_batchSize, namesToTranslate.size() - start);
                        auto batchResults = translateBatch(std::span<const std::string>(namesToTranslate.data() + start, count), threadId);
                        for (auto&& [name, trans] : batchResults) {
                            workerResults.insert_or_assign(std::move(name), std::move(trans));
                        }
                        m_controller->updateBar((int)count);
                    }
                    m_controller->reduceThreadNum();
                    return workerResults;
                }
                catch (...) {
                    m_controller->reduceThreadNum();
                    throw;
                }
            }));
    }
    std::exception_ptr firstException = nullptr;
    for (auto& result : results) {
        try {
            for (auto&& [name, trans] : result.get()) {
                translationResults.insert_or_assign(std::move(name), std::move(trans));
            }
        }
        catch (...) {
            if (!firstException) {
                pool.stop();
                firstException = std::current_exception();
            }
        }
    }
    if (firstException) {
        std::rethrow_exception(firstException);
    }

    // 4. 回写结果
    int updatedCount = 0;
    for (const auto& [key, trans] : translationResults) {
        if (nameTableData.contains(key)) {
            auto& val = nameTableData.at(key);
            if (val.is_array() && val.size() > 0) {
                // 更新译名 (index 0)
                val.as_array()[0] = trans;
                ++updatedCount;
            }
        }
    }

    // 5. 保存文件
    std::ofstream ofs(nameTablePath, std::ios::binary);
    ofs << nameTableData;
    ofs.close();

    m_logger->info("NameTrans: 处理完成，已更新 {} 个译名，保存至 {}", updatedCount, wide2Ascii(nameTablePath));
}
