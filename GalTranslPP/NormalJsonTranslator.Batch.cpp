module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <ctpl_stl.h>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

bool NormalJsonTranslator::translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& rollingContext,
    int threadId, int batchIndex, int& recursionIndex, int& recursionCount)
{
    for (Sentence* se : batch) {
        if (se->preproc.empty()) {
            se->transCompleted = true;
        }
    }

    int requestCount = 0;
    std::string contextHistory = buildContextHistory(batch, m_transEngine, m_contextHistorySize, 1024);
    std::string glossary = m_gptDictionary->generatePrompt(batch, m_transEngine);
    const std::string batchIndexLog = recursionCount == 0
        ? std::format("{}", batchIndex)
        : std::format("{}-({}/{})", batchIndex, recursionIndex, recursionCount);


    while (requestCount < m_maxRequestCount) {
        if (m_controller->shouldStop()) {
            return false;
        }

        std::vector<Sentence*> batchToTransThisRound = batch
            | std::views::filter([](Sentence* se) { return !se->transCompleted; })
            | std::ranges::to<std::vector>();

        if (batchToTransThisRound.empty()) {
            return true;
        }

        if (m_smartRetry && requestCount == 2 && batchToTransThisRound.size() > 1) {
            m_logger->warn(gppTr(
                "NormalJsonTranslator.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [请求 %4] 开始对半拆分句子重新请求...")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(requestCount + 1)
                .toStdString());

            const size_t mid = batchToTransThisRound.size() / 2;
            std::span<Sentence*> batchToTransThisRoundSpan(batchToTransThisRound);
            std::span<Sentence*> firstHalf = batchToTransThisRoundSpan.subspan(0, mid);
            std::span<Sentence*> secondHalf = batchToTransThisRoundSpan.subspan(mid);

            recursionCount += 2;
            ++recursionIndex;
            bool firstOk = translateBatch(relInputPath, firstHalf, rollingContext,
                threadId, batchIndex, recursionIndex, recursionCount);
            ++recursionIndex;
            bool secondOk = translateBatch(relInputPath, secondHalf, rollingContext,
                threadId, batchIndex, recursionIndex, recursionCount);

            return firstOk && secondOk;
        }
        else if (m_smartRetry && requestCount == 3) {
            m_logger->warn(gppTr(
                "NormalJsonTranslator.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [请求 %4] 清空上下文后再次尝试...")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(requestCount + 1)
                .toStdString());
            contextHistory.clear();
            rollingContext.clear();
        }

        const std::string inputProblems = std::ranges::fold_left(
            batchToTransThisRound
                | std::views::transform([](Sentence* se) { return se->problems; })
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
        const std::string inputLogBlock = limitLogLines(inputBlock, m_inputBlockMaxLines);

        std::string logBlock;
        if (!inputProblems.empty()) {
            logBlock += "\nProblems:\n" + limitLogLines(inputProblems, m_problemMaxLines);
        }
        if (m_logger->should_log(spdlog::level::debug) && !rollingContext.empty()) {
            logBlock += "\nRollingContext:\n" + rollingContext + "\n";
        }
        if (m_logger->should_log(spdlog::level::trace) && !contextHistory.empty()) {
            logBlock += "\nContext:\n" + contextHistory + "\n";
        }
        if (!glossary.empty()) {
            logBlock += "\nDict:\n" + limitLogLines(glossary, m_glossaryMaxLines);
        }
        logBlock += "\ninputBlock:\n" + inputLogBlock;
        m_logger->info(gppTr("NormalJsonTranslator.translateBatch",
            "[线程 %1] [文件 %2] [批次 %3] [请求 %4] 开始翻译，剩余 %5 句:\n%6")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(batchIndexLog)
            .arg(requestCount + 1)
            .arg(batchToTransThisRound.size())
            .arg(logBlock)
            .toStdString());
        

        std::string promptReq = m_userPrompt;
        replaceStrInplace(promptReq, "[Problem Description]", inputProblems.empty() ? "None" : inputProblems);
        replaceStrInplace(promptReq, "[RollingContext]", rollingContext.empty() ? "None" : rollingContext);
        replaceStrInplace(promptReq, "[Input]", inputBlock);
        replaceStrInplace(promptReq, "[TargetLang]", m_targetLang);
        replaceStrInplace(promptReq, "[Glossary]", glossary.empty() ? "None" : glossary);

        json messages = json::array({ {{"role", "system"}, {"content", m_systemPrompt}} });
        if (!contextHistory.empty()) {
            messages.push_back({ {"role", "user"}, {"content", "<input>(...truncated history source texts...)</input><output>\n"} });
            messages.push_back({ {"role", "assistant"}, {"content", contextHistory} });
        }
        messages.push_back({ {"role", "user"}, {"content", promptReq} });

        const std::optional<TranslationApi> apiOpt = m_apiPool->getApi(m_apiStrategy);
        if (!apiOpt.has_value()) {
            throw std::runtime_error(gppTr("NormalJsonTranslator.translateBatch", "没有可用的 Api key 了")
                .toStdString());
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);

        const std::string relInputPathLog = wide2Ascii(relInputPath);
        const std::string checkResponseLogPrefix = gppTr(
            "NormalJsonTranslator.translateBatch",
            "[线程 %1] [文件 %2] [批次 %3] [请求 %4]")
            .arg(threadId)
            .arg(relInputPathLog)
            .arg(batchIndexLog)
            .arg(requestCount + 1)
            .toStdString();
        if (
            !checkResponse(
            response, m_apiPool, currentApi, checkResponseLogPrefix, relInputPath, m_apiStrategy, m_controller, m_logger,
            requestCount, m_checkQuota
            ))
        {
            continue;
        }
        if (m_logger->should_log(spdlog::level::trace)) {
            m_logger->trace(gppTr(
                "NormalJsonTranslator.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [请求 %4] 成功响应，响应内容:\n%5")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(requestCount + 1)
                .arg(response.content)
                .toStdString());
        }

        int parsedCount = parseContent(
            response.content,
            batchToTransThisRound,
            id2SentenceMap,
            currentApi.modelName,
            rollingContext,
            m_transEngine,
            m_logger->should_log(spdlog::level::debug),
            m_retransAllWhenFail
        );

        if (parsedCount == batchToTransThisRound.size()) {
            m_logger->info(gppTr(
                "NormalJsonTranslator.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [请求 %4] 剩余 %5 句文本均被解析完毕，解析结果:\n%6")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(requestCount + 1)
                .arg(batchToTransThisRound.size())
                .arg(limitLogLines(response.content, m_inputBlockMaxLines))
                .toStdString());
            return true;
        }
        else {
            if (!m_controller->shouldStop()) {
                m_logger->warn(gppTr(
                    "NormalJsonTranslator.translateBatch",
                    "[线程 %1] [文件 %2] [批次 %3] [请求 %4] 解析失败或不完整 (%5 / %6), 解析结果:\n%7")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .arg(batchIndexLog)
                    .arg(requestCount + 1)
                    .arg(parsedCount)
                    .arg(batchToTransThisRound.size())
                    .arg(limitLogLines(response.content, m_inputBlockMaxLines))
                    .toStdString());
                m_controller->recordRuntimeTransError(RuntimeTransErrorEvent{
                    .kind = "parse",
                    .level = "warning",
                    .message = gppTr("NormalJsonTranslator.translateBatch", "解析失败或不完整 (%1 / %2)")
                        .arg(parsedCount)
                        .arg(batchToTransThisRound.size())
                        .toStdString(),
                    .filename = wide2Ascii(relInputPath),
                    .indexRange = std::format("{}-{}", batchToTransThisRound.front()->index, batchToTransThisRound.back()->index),
                    .requestCount = requestCount + 1,
                    .model = currentApi.modelName,
                    .sleepSeconds = -1.0
                });
            }
            ++requestCount;
            continue;
        }
    }

    size_t failedCount = 0;
    for (Sentence* se : batch | std::views::filter([](Sentence* s) { return !s->transCompleted; })) {
        ++failedCount;
        se->transraw = "(Failed to translate)" + se->preproc;
        se->transCompleted = true;
    }
    m_logger->error(gppTr(
        "NormalJsonTranslator.translateBatch",
        "[线程 %1] [文件 %2] [批次 %3] 在 %4 次请求后彻底失败，共翻译 (%5 / %6) 句")
        .arg(threadId)
        .arg(wide2Ascii(relInputPath))
        .arg(batchIndexLog)
        .arg(requestCount)
        .arg(batch.size() - failedCount)
        .arg(batch.size())
        .toStdString()
    );
    return false;
}
