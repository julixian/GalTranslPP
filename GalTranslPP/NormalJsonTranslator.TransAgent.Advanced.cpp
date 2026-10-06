module;

#include "GPPMacros.hpp"

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

json advancedAgentObjectSchema(json properties)
{
    json required = json::array();
    for (const auto& item : properties.items()) required.push_back(item.key());
    return {{"type", "object"}, {"properties", std::move(properties)}, {"required", std::move(required)}, {"additionalProperties", false}};
}

json advancedAgentArraySchema(const json& items)
{
    return {{"type", "array"}, {"items", items}};
}

void NormalJsonTranslatorTransAgent::configureAdvanced(bool enabled, int workerCount)
{
    m_advancedEnabled = enabled;
    if (!enabled) return;
    // 调度前分配固定 worker 状态；运行中各线程只访问自己的槽，不需要全局会话锁。
    m_workers.resize(std::max(1, workerCount));
    for (auto& worker : m_workers) {
        if (auto api = m_apiPool->getApi(m_apiStrategy))
            worker.session = AdvancedAgentApiSession{.api = *api, .systemPrompt = m_agentSystemPrompt};
    }
    const json stringSchema = {{"type", "string"}};
    const json intSchema = {{"type", "integer"}};
    const json stringsSchema = advancedAgentArraySchema(stringSchema);
    const json idsSchema = advancedAgentArraySchema(intSchema);
    const json translationSchema = advancedAgentObjectSchema({{"id", intSchema}, {"dst", stringSchema}});
    const json suggestionSchema = advancedAgentObjectSchema({{"file", stringSchema}, {"id", intSchema}, {"suggestion", stringSchema}});
    const json termSchema = advancedAgentObjectSchema({{"src", stringSchema}, {"dst", stringSchema}, {"category", stringSchema},
        {"note", stringSchema}, {"status", {{"type", "string"}, {"enum", json::array({"tentative", "confirmed"})}}},
        {"line_ids", idsSchema}});
    const json noteSchema = advancedAgentObjectSchema({{"summary", stringSchema}, {"scene_state", stringSchema},
        {"unresolved_clues", stringSchema}, {"relationship_updates", stringSchema}, {"term_hints", stringSchema}});
    const json commitSchema = advancedAgentObjectSchema({{"translations", advancedAgentArraySchema(translationSchema)},
        {"term_updates", advancedAgentArraySchema(termSchema)}, {"agent_suggest", advancedAgentArraySchema(suggestionSchema)},
        {"file_note_patch", noteSchema}, {"rolling_context", stringSchema}});
    const auto addTool = [&](const std::string& name, const std::string& description, const json& schema)
        { m_nativeTools.push_back({{"name", name}, {"description", description}, {"parameters", schema}}); };
    const json pageProperties = {{"offset", intSchema}, {"limit", intSchema}};
    addTool("list_files", "List source files. offset is the number of result records to skip; offset=0 starts from the first result."
        " limit=0 uses the configured limit.", advancedAgentObjectSchema(pageProperties));
    for (const auto& domain : {"source", "cache", "dictionary", "terms"}) {
        json properties = pageProperties;
        properties["file"] = stringSchema;
        properties["ids"] = idsSchema;
        properties["fields"] = stringsSchema;
        addTool(std::string("read_") + domain,
            std::format("Read {} records. file='' selects the current file (ignored for dictionary/terms)."
                " ids=[] selects all. fields=[] returns all available columns. offset is the number of result records to skip, not a page number or sentence id.",
                domain),
            advancedAgentObjectSchema(properties));
        properties["query"] = stringSchema;
        properties["match_fields"] = stringsSchema;
        properties["context_before"] = intSchema;
        properties["context_after"] = intSchema;
        addTool(std::string("search_") + domain,
            std::format("Search {} by case-insensitive substring. file='*' searches all files. query='' matches all."
                " match_fields=[] searches all data columns; fields selects returned columns. context_before/after apply to source/cache rows.",
                domain),
            advancedAgentObjectSchema(properties));
    }
    addTool("read_file_note", "Read durable notes for a source file; file='' selects the current file.",
        advancedAgentObjectSchema({{"file", stringSchema}}));
    addTool("read_project_note", "Read the configured project note.", advancedAgentObjectSchema(json::object()));
    addTool("commit_translations", "Submit every current sentence id, durable memory and sentence-level suggestions."
        " Empty note strings leave existing notes unchanged. Use [] for no term updates or suggestions.",
        commitSchema);
}

bool NormalJsonTranslatorTransAgent::translateAdvancedBatch(const fs::path& relInputPath, std::span<Sentence*> batch,
    std::string& rollingContext, int threadId, int batchIndex)
{
    for (Sentence* se : batch) {
        if (se->preproc.empty()) se->transCompleted = true;
    }
    auto pending = collectPendingSentences(batch);
    if (pending.empty()) return true;
    auto& worker = m_workers.at(threadId - 1);
    if (!worker.rollingContext.empty()) rollingContext = worker.rollingContext;
    const std::string logPrefix = gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "[线程 %1] [文件 %2] [批次 %3] 高级 Agent")
        .arg(threadId).arg(wide2Ascii(relInputPath)).arg(batchIndex).toStdString();
    m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 开始翻译，最多 %2 轮，共 %3 句:\n%4")
        .arg(logPrefix).arg(m_agentMaxTurnsPerChunk).arg(pending.size())
        .arg(buildLogBlock(relInputPath, pending, rollingContext)).toStdString());
    const auto rebuildSession = [&](const TranslationApi& api)
        {
            worker.session = AdvancedAgentApiSession{.api = api, .systemPrompt = m_agentSystemPrompt};
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 创建会话: %2 / %3")
                .arg(logPrefix).arg(apiProtocolToString(api.protocol)).arg(api.modelName).toStdString());
        };
    const auto selectApi = [&](bool fallbackCheck)
        {
            if (!worker.session || !m_apiPool->containsApi(worker.session->api) || (fallbackCheck && m_apiStrategy == "fallback")) {
                auto api = m_apiPool->getApi(m_apiStrategy);
                if (!api) throw std::runtime_error(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "没有可用的 Api key 了").toStdString());
                if (!worker.session || !isSameApi(worker.session->api, *api)) rebuildSession(*api);
            }
        };
    selectApi(true);
    const auto appendBatch = [&]()
        {
            const json messages = buildBaseMessages(relInputPath, pending, rollingContext);
            appendAdvancedAgentUserMessage(*worker.session, messages.at(1).at("content"));
        };
    appendBatch();
    const json compactSchema = advancedAgentObjectSchema({{"rolling_context", {{"type", "string"}}}});
    const json compactTools = json::array({{{"name", "compact_context"},
        {"description",
            "Summarize the conversation for the next session. Preserve translation decisions, character relationships, unresolved clues and current progress."
            " Do not translate or commit."},
        {"parameters", compactSchema}}});
    bool compacting = false;
    size_t compactedMessageCount = 0;
    int noToolCallCount = 0;
    bool retryExhausted = false;
    int turn = 0;
    int requestCount = 0;
    std::string requestLogPrefix;
    const auto restartAfterCompaction = [&]()
        {
            const auto api = worker.session->api;
            // 新摘要无效时沿用上次提交的滚动记忆；旧消息、服务端续接 id 和推理签名一并清除。
            rebuildSession(api);
            appendBatch();
            compactedMessageCount = worker.session->history.size();
            compacting = false;
        };
    for (; turn < m_agentMaxTurnsPerChunk; ++turn) {
        const auto& currentSession = *worker.session;
        const bool nativeAutoCompaction = currentSession.api.agentNativeAutoCompaction &&
            (currentSession.api.protocol == ApiProtocol::OpenAIRes || currentSession.api.protocol == ApiProtocol::Claude);
        // 没有启用原生压缩时，先让模型总结完整历史，再用摘要重建会话；不直接截断旧消息。
        // 刚重建的批次本身可能已超过阈值，等历史增加后再判断，避免连续只做压缩。
        if (!compacting && !nativeAutoCompaction && currentSession.history.size() > compactedMessageCount &&
            currentSession.history.dump().size() > (size_t)m_agentCompactContextThresholdBytes) {
            compacting = true;
            appendAdvancedAgentUserMessage(*worker.session,
                "Context exceeded the compact context threshold. Summarize the full conversation into rolling_context using compact_context, "
                "preserving translation decisions, character relationships, "
                "unresolved clues and current progress. Do not call other tools or commit in this turn.");
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 [轮次 %2] 上下文超过字节阈值，要求模型先压缩上下文")
                .arg(logPrefix).arg(turn + 1).toStdString());
        }
        requestCount = 0;
        AdvancedAgentApiResponse response;
        while (requestCount < m_maxRequestCount) {
            if (m_controller->shouldStop()) return false;
            if (!m_apiPool->containsApi(worker.session->api)) {
                selectApi(false);
                appendBatch();
                compacting = false;
                compactedMessageCount = 0;
            }
            requestLogPrefix = gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 [轮次 %2] [请求 %3]")
                .arg(logPrefix).arg(turn + 1).arg(requestCount + 1).toStdString();
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 开始请求，剩余 %2 句，本地上下文 %3 字节")
                .arg(requestLogPrefix).arg(pending.size()).arg(worker.session->history.dump().size()).toStdString());
            response = performAdvancedAgentApiRequest(*worker.session, compacting ? compactTools : m_nativeTools,
                m_onPerformApi, m_controller, m_logger, m_apiTimeOutMs);
            if (response.content) break;
            if (m_controller->shouldStop()) return false;
            handleApiError(response.content.error(), m_apiPool, worker.session->api,
                requestLogPrefix, relInputPath, m_apiStrategy, m_controller, m_logger, requestCount, m_checkQuota);
            // 报错允许 fallback 轮转；正常工具轮一直使用同一 API。
            const auto previousApi = worker.session->api;
            selectApi(true);
            if (!isSameApi(previousApi, worker.session->api)) {
                appendBatch();
                compacting = false;
                compactedMessageCount = 0;
            }
        }
        if (!response.content) {
            if (!compacting) {
                retryExhausted = true;
                break;
            }
            m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 压缩请求重试耗尽，清空会话历史后继续当前批次: %2")
                .arg(requestLogPrefix).arg(formatApiError(response.content.error())).toStdString());
            restartAfterCompaction();
            continue;
        }
        auto& session = *worker.session;
        auto& reply = *response.content;
        if (m_logger->should_log(spdlog::level::trace)) {
            // 原生工具参数也属于响应内容，不能只记录可能为空的正文。
            json calls = json::array();
            for (const auto& call : reply.calls)
                calls.push_back({{"id", call.id}, {"name", call.name}, {"arguments", call.arguments}});
            m_logger->trace(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 成功响应，回复正文:\n%2\n工具调用:\n%3")
                .arg(requestLogPrefix)
                .arg(reply.text.empty() ? gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "[GPP.正文为空]").toStdString() : reply.text)
                .arg(calls.dump(2)).toStdString());
        }
        if (!reply.calls.empty() && m_logger->should_log(spdlog::level::info)) {
            std::vector<AgentCommonToolCallRequest> calls;
            for (const auto& call : reply.calls) calls.push_back({call.id, call.name, call.arguments});
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 工具调用 %2 个，调用概览:\n%3")
                .arg(requestLogPrefix).arg(calls.size())
                .arg(limitLogLines(formatAgentCommonToolCallDetails(calls), m_inputBlockMaxLines)).toStdString());
        }
        // 连续没有工具调用时沿用请求重试次数作为上限，任意工具调用都会清零。
        noToolCallCount = reply.calls.empty() ? noToolCallCount + 1 : 0;
        if (noToolCallCount > 0) {
            m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 未调用工具，连续 %2 / %3 轮未调用工具，回复正文:\n%4")
                .arg(requestLogPrefix).arg(noToolCallCount).arg(m_maxRequestCount)
                .arg(reply.text.empty() ? gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "[GPP.正文为空]").toStdString()
                    : limitLogLines(reply.text, m_inputBlockMaxLines)).toStdString());
        }
        if (noToolCallCount >= m_maxRequestCount) break;
        if (compacting) {
            std::string memory;
            try {
                if (reply.calls.size() != 1 || reply.calls.front().name != "compact_context")
                    throw std::runtime_error("Call compact_context only in this turn.");
                const auto& arguments = reply.calls.front().arguments;
                const json summary = arguments.is_string() ? json::parse(arguments.get<std::string>()) : arguments;
                memory = summary.at("rolling_context").get<std::string>();
                if (memory.empty()) throw std::runtime_error("rolling_context must contain the conversation summary.");
            }
            catch (const std::exception& e) {
                m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 摘要无效，清空会话历史后继续当前批次: %2")
                    .arg(requestLogPrefix).arg(e.what()).toStdString());
                restartAfterCompaction();
                continue;
            }
            rollingContext = memory;
            worker.rollingContext = memory;
            if (m_logger->should_log(spdlog::level::debug)) {
                m_logger->debug(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 压缩后的滚动上下文:\n%2")
                    .arg(requestLogPrefix).arg(memory).toStdString());
            }
            restartAfterCompaction();
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 已完成上下文压缩，继续当前批次")
                .arg(requestLogPrefix).toStdString());
            continue;
        }
        bool committed = false;
        json results = json::array();
        const auto commit = [&](json arguments)
            {
                if (committed) throw std::runtime_error("This batch has already been committed.");
                arguments["action"] = "commit";
                // 严格 schema 的空备注字段代表不修改，不能覆盖已有记忆。
                if (arguments.contains("file_note_patch")) {
                    auto& patch = arguments["file_note_patch"];
                    for (auto it = patch.begin(); it != patch.end();) {
                        if (it->is_string() && it->get_ref<const std::string&>().empty()) it = patch.erase(it);
                        else ++it;
                    }
                }
                if (auto updates = arguments.find("term_updates"); updates != arguments.end()) {
                    for (auto& update : *updates) {
                        if (update.contains("line_ids") && update["line_ids"].empty()) update.erase("line_ids");
                    }
                }
                const auto protocol = parseProtocolResponse(arguments.dump());
                std::string commitLog;
                int termCount = 0, suggestionCount = 0;
                const int count = applyCommit(relInputPath, pending, rollingContext, threadId, protocol,
                    makeTransby(session.api.apikey, session.api.modelName),
                    std::to_string(batchIndex), turn, requestCount, commitLog, termCount, suggestionCount);
                worker.rollingContext = rollingContext;
                committed = true;
                m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 响应处理成功，处理结果:\n%2")
                    .arg(requestLogPrefix)
                    .arg(gppTr("NormalJsonTranslatorTransAgent.parseAndApplyTurnResponse",
                        "该批次 %1 句译文均已提交，记录术语 %2 条，记录建议 %3 条，翻译结果:\n%4")
                        .arg(count).arg(termCount).arg(suggestionCount).arg(limitLogLines(commitLog, m_inputBlockMaxLines)))
                    .toStdString());
                return json{{"committed", count}};
            };
        for (const auto& call : reply.calls) {
            json result;
            try {
                const json arguments = call.arguments.is_string() ? json::parse(call.arguments.get<std::string>()) : call.arguments;
                result = call.name == "commit_translations" ? commit(arguments) : runReadTool(relInputPath, call.name, arguments);
            }
            catch (const std::exception& e) {
                result = {{"error", e.what()}};
                m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 工具 %2 执行失败: %3")
                    .arg(requestLogPrefix).arg(call.name).arg(e.what()).toStdString());
                m_controller->recordRuntimeTransError(RuntimeTransErrorEvent{
                    .kind = "agent",
                    .level = "warning",
                    .message = gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "工具 %1 执行失败: %2")
                        .arg(call.name).arg(e.what()).toStdString(),
                    .filename = wide2Ascii(relInputPath),
                    .indexRange = std::format("{}-{}", pending.front()->index, pending.back()->index),
                    .requestCount = requestCount + 1,
                    .model = makeTransby(session.api.apikey, session.api.modelName),
                    .sleepSeconds = -1.0
                });
            }
            results.push_back({{"id", call.id}, {"name", call.name}, {"result", std::move(result)}});
        }
        // 即使其中一个 commit 成功，也给全部并行调用回填结果，再结束批次。
        if (!results.empty()) {
            if (m_logger->should_log(spdlog::level::debug)) {
                m_logger->debug(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 工具调用明细:\n%2")
                    .arg(requestLogPrefix)
                    .arg(gppTr("NormalJsonTranslatorTransAgent.executeToolCalls", "工具返回结果:\n%1").arg(results.dump(2)))
                    .toStdString());
            }
            appendAdvancedAgentToolResults(session, results);
            if (!committed) {
                m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 已回填 %2 个工具结果，进入下一轮")
                    .arg(requestLogPrefix).arg(results.size()).toStdString());
            }
        }
        else {
            appendAdvancedAgentUserMessage(session, "Text responses do not submit translations. Call a read/search tool if needed, "
                "or call commit_translations to submit every sentence in the current batch.");
        }
        if (committed) return true;
    }
    appendAdvancedAgentUserMessage(*worker.session, "The current batch was abandoned after reaching the request/turn limit. Do not commit it later.");
    for (Sentence* se : pending) {
        se->transraw = "(Failed to translate)" + se->preproc;
        se->transCompleted = true;
    }
    // 与普通 Agent 一样分别记录轮次耗尽和请求重试耗尽，额外区分连续无工具调用。
    const size_t translatedCount = batch.size() - pending.size();
    if (retryExhausted) {
        m_logger->error(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 [轮次 %2] 在 %3 次请求后彻底失败，共翻译 (%4 / %5) 句")
            .arg(logPrefix).arg(turn + 1).arg(requestCount).arg(translatedCount).arg(batch.size()).toStdString());
    }
    else if (noToolCallCount >= m_maxRequestCount) {
        m_logger->error(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 [轮次 %2] 因连续 %3 轮未调用工具而失败，共翻译 (%4 / %5) 句")
            .arg(logPrefix).arg(turn + 1).arg(noToolCallCount).arg(translatedCount).arg(batch.size()).toStdString());
    }
    else {
        m_logger->error(gppTr("NormalJsonTranslatorTransAgent.translateAdvancedBatch", "%1 因超过最大轮数 (%2 轮) 而失败，共翻译 (%3 / %4) 句")
            .arg(logPrefix).arg(m_agentMaxTurnsPerChunk).arg(translatedCount).arg(batch.size()).toStdString());
    }
    return false;
}

NAMESPACE_END(gpp)
