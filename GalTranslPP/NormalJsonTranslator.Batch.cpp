module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#include <Shlwapi.h>
#endif
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NormalJsonTranslatorHelperTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

bool NormalJsonTranslator::translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText, int threadId)
{
    for (Sentence* se : batch) {
        if (se->pre_processed_text.empty()) {
            se->complete = true;
            ++m_completedSentences;
            m_controller->updateBar(); // 空句直接跳过
        }
    }

    int retryCount = 0;
    std::string contextHistory = buildContextHistory(batch, m_transEngine, m_contextHistorySize, 1024);
    std::string glossary = m_gptDictionary->generatePrompt(batch, m_transEngine);

    while (retryCount == 0 || retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return false;
        }

        std::vector<Sentence*> batchToTransThisRound = batch
            | std::views::filter([](const Sentence* se) { return !se->complete; })
            | std::ranges::to<std::vector>();

        if (batchToTransThisRound.empty()) {
            return true;
        }

        if (m_smartRetry && retryCount == 2 && batchToTransThisRound.size() > 1) {
            m_logger->warn("[线程 {}] [文件 {}] 开始拆分批次进行重试...", threadId, wide2Ascii(relInputPath));

            const size_t mid = batchToTransThisRound.size() / 2;
            std::span<Sentence*> batchToTransThisRoundSpan(batchToTransThisRound);
            std::span<Sentence*> firstHalf = batchToTransThisRoundSpan.subspan(0, mid);
            std::span<Sentence*> secondHalf = batchToTransThisRoundSpan.subspan(mid);

            bool firstOk = translateBatch(relInputPath, firstHalf, backgroundText, threadId);
            bool secondOk = translateBatch(relInputPath, secondHalf, backgroundText, threadId);

            return firstOk && secondOk;
        }
        else if (m_smartRetry && retryCount == 3) {
            m_logger->warn("[线程 {}] [文件 {}] 清空上下文后再次尝试...", threadId, wide2Ascii(relInputPath));
            contextHistory.clear();
            backgroundText.clear();
        }

        const std::string inputProblems = std::ranges::fold_left(
            batchToTransThisRound
                | std::views::transform([](const Sentence* se) { return se->problems; })
                | std::views::join,
            std::string{},
            [](const auto& acc, const auto& value)
            {
                if (!acc.contains(value)) {
                    return acc + value + "\n";
                }
                return acc;
            }
        );

        std::string inputBlock;
        absl::flat_hash_map<int, Sentence*> id2SentenceMap; // TSV/JSON 解析需要的索引表
        fillBlockAndMap(batchToTransThisRound, inputBlock, m_transEngine, &id2SentenceMap);

        std::string logBlock;
        if (!inputProblems.empty()) {
            logBlock += "\nProblems:\n" + inputProblems;
        }
        if (m_logger->should_log(spdlog::level::debug) && !backgroundText.empty()) {
            logBlock += "\nBackground:\n" + backgroundText + "\n";
        }
        if (m_logger->should_log(spdlog::level::trace) && !contextHistory.empty()) {
            logBlock += "\nContext:\n" + contextHistory + "\n";
        }
        if (!glossary.empty()) {
            logBlock += "\nDict:\n" + glossary;
        }
        logBlock += "\ninputBlock:\n" + inputBlock;
        m_logger->info("[线程 {}] [文件 {}] 开始翻译:\n{}", threadId, wide2Ascii(relInputPath), logBlock);

        std::string promptReq = m_userPrompt;
        replaceStrInplace(promptReq, "[Problem Description]", inputProblems.empty() ? "None" : inputProblems);
        replaceStrInplace(promptReq, "[Background Description]", backgroundText.empty() ? "None" : backgroundText);
        replaceStrInplace(promptReq, "[Input]", inputBlock);
        replaceStrInplace(promptReq, "[TargetLang]", m_targetLang);
        replaceStrInplace(promptReq, "[Glossary]", glossary.empty() ? "None" : glossary);

        json messages = json::array({ {{"role", "system"}, {"content", m_systemPrompt}} });
        if (!contextHistory.empty()) {
            messages.push_back({ {"role", "user"}, {"content", "<input>(...truncated history source texts...)</input><output>\n"} });
            messages.push_back({ {"role", "assistant"}, {"content", contextHistory} });
        }
        messages.push_back({ {"role", "user"}, {"content", promptReq} });

        const std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
        if (!apiOpt.has_value()) {
            throw std::runtime_error("没有可用的 API key 了");
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);

        if (!checkResponse(
            response, m_apiPool, currentApi, relInputPath, m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
        )) {
            continue;
        }
        m_logger->trace("[线程 {}] [文件 {}] 成功响应，响应内容:\n{}", threadId, wide2Ascii(relInputPath), response.content);

        int parsedCount = parseContent(
            response.content,
            batchToTransThisRound,
            id2SentenceMap,
            currentApi.modelName,
            m_controller,
            backgroundText,
            m_completedSentences,
            m_transEngine,
            m_logger->should_log(spdlog::level::debug),
            m_retransAllWhenFail
        );

        if (parsedCount != batchToTransThisRound.size()) {
            ++retryCount;
            if (!m_controller->shouldStop()) {
                m_logger->warn(
                    "[线程 {}] [文件 {}] 解析失败或不完整 ({} / {}), 进行第 {} 次重试..., 解析结果: \n{}",
                    threadId, wide2Ascii(relInputPath), parsedCount, batchToTransThisRound.size(), retryCount, response.content
                );
            }
            continue;
        }

        m_logger->info("[线程 {}] [文件 {}] 成功解析 {} 句，解析结果: \n{}", threadId, wide2Ascii(relInputPath), parsedCount, response.content);
        return true;
    }

    size_t failedCount = 0;
    for (Sentence* se : batch | std::views::filter([](const Sentence* s) { return !s->complete; })) {
        ++failedCount;
        se->pre_translated_text = "(Failed to translate)" + se->pre_processed_text;
        se->complete = true;
        ++m_completedSentences;
        m_controller->updateBar(); // 标记失败句
    }
    m_logger->error(
        "[线程 {}] [文件 {}] 批次翻译在 {} 次重试后彻底失败，共翻译 {} / {} 句。",
        threadId, wide2Ascii(relInputPath), retryCount, batch.size() - failedCount, batch.size()
    );
    return false;
}
