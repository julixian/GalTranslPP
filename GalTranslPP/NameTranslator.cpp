module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <ctpl_stl.h>
#include <toml.hpp>

module NameTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

NameTranslator::NameTranslator(
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::unique_ptr<ApiPool>& apiPool,
    const std::unique_ptr<GptDictionary>& gptDictionary,
    const std::function<std::string(std::string)>& onPerformApi,
    const std::string& systemPrompt,
    const std::string& userPrompt,
    const std::string& apiStrategy,
    const std::string& targetLang,
    int threadsNum,
    int batchSize,
    int inputBlockMaxLines,
    int maxRequestCount,
    int apiTimeoutMs,
    bool checkQuota
)
    : m_controller(controller), m_logger(logger), m_apiPool(apiPool), m_gptDictionary(gptDictionary),
    m_onPerformApi(onPerformApi), m_systemPrompt(systemPrompt), m_userPrompt(userPrompt),
    m_apiStrategy(apiStrategy), m_targetLang(targetLang), m_threadsNum(std::max(1, threadsNum)),
    m_batchSize(std::max(1, batchSize)), m_inputBlockMaxLines(inputBlockMaxLines),
    m_maxRequestCount(maxRequestCount), m_apiTimeoutMs(apiTimeoutMs),
    m_checkQuota(checkQuota)
{

}

void NameTranslator::translateBatch(std::span<const std::string> batchNames, int threadId, size_t batchIndex,
    absl::flat_hash_map<std::string, std::string>& translationResults, std::mutex& translationResultsMutex)
{
    std::vector<std::pair<std::string_view, std::string>> pendingNames;
    pendingNames.reserve(batchNames.size());
    for (const auto& name : batchNames) {
        std::string protocolName(name);
        std::ranges::replace_if(protocolName, [](char ch) { return ch == '\r' || ch == '\n' || ch == '\t'; }, ' ');
        pendingNames.emplace_back(name, std::move(protocolName));
    }

    int requestCount = 0;
    while (requestCount < m_maxRequestCount) {
        if (m_controller->shouldStop()) {
            return;
        }

        if (pendingNames.empty()) {
            return;
        }
        const size_t pendingNameCount = pendingNames.size();

        std::vector<Sentence> dummySentences;
        std::vector<Sentence*> dummyPtrs;
        dummySentences.reserve(pendingNames.size());
        dummyPtrs.reserve(pendingNames.size());

        std::string inputBlock;
        for (const auto& [name, protocolName] : pendingNames) {
            Sentence se;
            se.preproc = name;
            dummySentences.push_back(std::move(se));
            inputBlock += protocolName;
            inputBlock += '\n';
        }
        for (Sentence& se : dummySentences) {
            dummyPtrs.push_back(&se);
        }

        const std::string glossary = m_gptDictionary->generatePrompt(dummyPtrs, TransEngine::NameTrans);

        std::string prompt = m_userPrompt;
        replaceStrInplace(prompt, "[TargetLang]", m_targetLang);
        replaceStrInplace(prompt, "[Glossary]", glossary.empty() ? "None" : glossary);
        replaceStrInplace(prompt, "[Input]", inputBlock);

        json messages = json::array({
            {{"role", "system"}, {"content", m_systemPrompt}},
            {{"role", "user"}, {"content", prompt}}
            });

        const std::optional<TranslationApi> apiOpt = m_apiPool->getApi(m_apiStrategy);
        if (!apiOpt) {
            throw std::runtime_error(gppTr("NameTranslator.translateBatch", "没有可用的 Api key 了")
                .toStdString());
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        std::string logBlock;
        if (!glossary.empty()) {
            logBlock += "\nDict:\n" + glossary;
        }
        logBlock += "\ninputBlock:\n" + limitLogLines(inputBlock, m_inputBlockMaxLines);
        m_logger->info(gppTr(
            "NameTranslator.translateBatch",
            "[线程 %1] [批次 %2] [请求 %3] 开始翻译人名，剩余 %4 个:\n%5")
            .arg(threadId)
            .arg(batchIndex)
            .arg(requestCount + 1)
            .arg(pendingNames.size())
            .arg(logBlock)
            .toStdString());

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger,
            threadId, m_apiTimeoutMs);

        const std::string checkResponseLogPrefix = gppTr(
            "NameTranslator.translateBatch",
            "[线程 %1] [批次 %2] [请求 %3]")
            .arg(threadId)
            .arg(batchIndex)
            .arg(requestCount + 1)
            .toStdString();
        if (!checkResponse(
            response, m_apiPool, currentApi, checkResponseLogPrefix, fs::path{},
            m_apiStrategy, m_controller, m_logger, requestCount, m_checkQuota
            ))
        {
            continue;
        }

        if (m_logger->should_log(spdlog::level::trace)) {
            m_logger->trace(gppTr(
                "NameTranslator.translateBatch",
                "[线程 %1] [批次 %2] [请求 %3] 人名翻译成功响应，响应内容:\n%4")
                .arg(threadId)
                .arg(batchIndex)
                .arg(requestCount + 1)
                .arg(limitLogLines(response.content, m_inputBlockMaxLines))
                .toStdString());
        }

        int parsedCount = 0;
        for (std::string_view line : splitStringView(response.content, '\n')) {
            const std::vector<std::string_view> parts = splitStringView(line, '\t');
            if (parts.size() < 2 || parts[0] == "Source") {
                continue;
            }
            const auto pendingIt = std::ranges::find_if(pendingNames, [&](const auto& pendingName)
	            {
                    return pendingName.second == parts[0];
	            });
            if (pendingIt == pendingNames.end() || parts[1].empty()) {
                continue;
            }
            {
                std::lock_guard lock(translationResultsMutex);
                translationResults.insert_or_assign(pendingIt->first, parts[1]);
            }
            pendingNames.erase(pendingIt);
            ++parsedCount;
        }

        if (parsedCount == (int)pendingNameCount) {
            m_logger->info(gppTr(
                "NameTranslator.translateBatch",
                "[线程 %1] [批次 %2] [请求 %3] 剩余 %4 个人名均被解析完毕，解析结果:\n%5")
                .arg(threadId)
                .arg(batchIndex)
                .arg(requestCount + 1)
                .arg(pendingNameCount)
                .arg(limitLogLines(response.content, m_inputBlockMaxLines))
                .toStdString());
            return;
        }
        else {
            m_logger->warn(gppTr(
                "NameTranslator.translateBatch",
                "[线程 %1] [批次 %2] [请求 %3] 人名翻译响应解析不完整 (%4 / %5)，解析结果:\n%6")
                .arg(threadId)
                .arg(batchIndex)
                .arg(requestCount + 1)
                .arg(parsedCount)
                .arg(pendingNameCount)
                .arg(limitLogLines(response.content, m_inputBlockMaxLines))
                .toStdString());
            ++requestCount;
            continue;
        }
    }
    m_logger->error(gppTr(
        "NameTranslator.translateBatch",
        "[线程 %1] [批次 %2] 人名翻译在 %3 次请求后彻底失败，共翻译 (%4 / %5) 个")
        .arg(threadId)
        .arg(batchIndex)
        .arg(m_maxRequestCount)
        .arg(batchNames.size() - pendingNames.size())
        .arg(batchNames.size())
        .toStdString());
}

void NameTranslator::run(const fs::path& nameTablePath) {
    if (!fs::exists(nameTablePath)) {
        m_logger->error(gppTr("NameTranslator.run", "NameTrans: 未找到人名表文件 %1")
            .arg(wide2Ascii(nameTablePath))
            .toStdString());
        return;
    }

    m_logger->info(gppTr("NameTranslator.run", "NameTrans: 开始处理人名表...").toStdString());

    // 1. 读取 TOML
    toml::ordered_value nameTableData;
    try {
        nameTableData = toml::uoparse(nameTablePath);
    }
    catch (const toml::exception& e) {
        m_logger->error(gppTr("NameTranslator.run", "NameTrans: 解析人名表失败: %1")
            .arg(e.what())
            .toStdString());
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
        m_logger->info(gppTr("NameTranslator.run", "NameTrans: 没有发现需要翻译的名字（所有条目均已有译名）")
            .toStdString());
        return;
    }

    m_logger->info(gppTr("NameTranslator.run", "NameTrans: 共发现 %1 个待翻译的名字")
        .arg(namesToTranslate.size())
        .toStdString());

    // 3. 分批处理
    absl::flat_hash_map<std::string, std::string> translationResults;
    const size_t batchCount = (namesToTranslate.size() + (size_t)m_batchSize - 1) / (size_t)m_batchSize;
    const int workerCount = std::max(1, std::min(m_threadsNum, (int)batchCount));
    m_logger->info(gppTr("NameTranslator.run", "NameTrans: 启动 %1 个线程，每批处理 %2 个名字")
        .arg(workerCount)
        .arg(m_batchSize)
        .toStdString());
    m_controller->makeBar((int)namesToTranslate.size(), workerCount);

    ctpl::thread_pool pool(workerCount);
    std::vector<std::future<void>> results;
    std::mutex translationResultsMutex;
    for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
        const size_t start = batchIndex * (size_t)m_batchSize;
        const size_t count = std::min((size_t)m_batchSize, namesToTranslate.size() - start);
        results.emplace_back(pool.push([&](const int threadId)
            {
                ActiveWorkerGuard workerGuard(m_controller);
                if (!m_controller->shouldStop()) {
                    translateBatch(std::span<const std::string>(namesToTranslate.data() + start, count), threadId,
                        batchIndex + 1, translationResults, translationResultsMutex);
                    m_controller->updateBar((int)count);
                }
            }));
    }
    waitForThreads(pool, results);

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
    atomicOutputFile(nameTablePath, toml::format(nameTableData));

    m_logger->info(gppTr("NameTranslator.run", "NameTrans 处理完成，已更新 %1 个译名，保存至 [%2]")
        .arg(updatedCount)
        .arg(wide2Ascii(nameTablePath))
        .toStdString());
}
