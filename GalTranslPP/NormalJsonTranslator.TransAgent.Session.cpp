module;

#include "GPPMacros.hpp"

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;
import AgentPythonExecutor;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

json agentObjectSchema(json properties)
{
    json required = json::array();
    for (const auto& item : properties.items()) {
        required.push_back(item.key());
    }
    return {
        {"type", "object"},
        {"properties", std::move(properties)},
        {"required", std::move(required)},
        {"additionalProperties", false}
    };
}

json agentArraySchema(const json& items)
{
    return {{"type", "array"}, {"items", items}};
}

void NormalJsonTranslatorTransAgent::configureWorkers(
    int workerCount, bool allowPython,
    const fs::path& inputDir, const fs::path& outputDir)
{
    // 调度前分配固定 worker 状态；运行中各线程只访问自己的槽，不需要全局会话锁。
    m_workers.resize(std::max(1, workerCount));
    for (auto& worker : m_workers) {
        if (auto api = m_apiPool->getApi(m_apiStrategy)) {
            worker.session = AgentApiSession{.api = *api, .systemPrompt = m_agentSystemPrompt};
        }
    }
    const json stringSchema = {{"type", "string"}};
    const json intSchema = {{"type", "integer"}};

    const json stringsSchema = agentArraySchema(stringSchema);
    const json idsSchema = agentArraySchema(intSchema);
    const json translationSchema = agentObjectSchema({{"id", intSchema}, {"dst", stringSchema}});
    const json suggestionSchema = agentObjectSchema({{"file", stringSchema}, {"id", intSchema}, {"suggestion", stringSchema}});

    const json termSchema = agentObjectSchema({
        {"src", stringSchema},
        {"dst", stringSchema},
        {"category", stringSchema},
        {"note", stringSchema},
        {"status", {{"type", "string"}, {"enum", json::array({"tentative", "confirmed"})}}},
        {"line_ids", idsSchema}
    });

    const json noteSchema = agentObjectSchema({
        {"summary", stringSchema},
        {"scene_state", stringSchema},
        {"unresolved_clues", stringSchema},
        {"relationship_updates", stringSchema},
        {"term_hints", stringSchema}
    });

    const json commitSchema = agentObjectSchema({
        {"translations", agentArraySchema(translationSchema)},
        {"term_updates", agentArraySchema(termSchema)},
        {"agent_suggest", agentArraySchema(suggestionSchema)},
        {"file_note_patch", noteSchema}
    });

    const auto addTool = [&](const std::string& name, const std::string& description, const json& schema)
        {
            m_nativeTools.push_back({
                {"name", name},
                {"description", description},
                {"parameters", schema}
            });
        };
    const json pageProperties = {{"offset", intSchema}, {"limit", intSchema}};
    addTool("list_files", "List source files. offset is the number of result records to skip; offset=0 starts from the first result."
        " limit=0 uses the configured limit.", agentObjectSchema(pageProperties));

    for (const auto& domain : {"source", "cache", "dictionary", "terms"}) {
        json properties = pageProperties;
        properties["file"] = stringSchema;
        properties["ids"] = idsSchema;
        properties["fields"] = stringsSchema;
        addTool(std::string("read_") + domain,
            std::format("Read {} records. file='' selects the current file (ignored for dictionary/terms)."
                " ids=[] selects all. fields=[] returns all available columns. offset is the number of result records to skip, not a page number or sentence id.",
                domain),
            agentObjectSchema(properties));
        properties["query"] = stringSchema;
        properties["match_fields"] = stringsSchema;
        properties["context_before"] = intSchema;
        properties["context_after"] = intSchema;
        addTool(std::string("search_") + domain,
            std::format("Search {} by case-insensitive substring. file='*' searches all files. query='' matches all."
                " match_fields=[] searches all data columns; fields selects returned columns. context_before/after apply to source/cache rows.",
                domain),
            agentObjectSchema(properties));
    }
    addTool("read_file_note", "Read durable notes for a source file; file='' selects the current file.",
        agentObjectSchema({{"file", stringSchema}}));
    addTool("read_project_note", "Read the configured project note.", agentObjectSchema(json::object()));
    if (allowPython) {
        const auto absolutePath = [](const fs::path& path)
            {
                return wide2Ascii(fs::absolute(path).lexically_normal());
            };
        json dictionaryFiles = json::array();
        for (const auto& path : m_gptDictionaryPaths) {
            dictionaryFiles.push_back(absolutePath(path));
        }
        m_pythonPaths = {
            {"project_dir", absolutePath(m_projectDir)},
            {"input_dir", absolutePath(inputDir)},
            {"cache_dir", absolutePath(m_transCacheDir)},
            {"output_dir", absolutePath(outputDir)},
            {"dictionary_files", std::move(dictionaryFiles)},
            {"name_table", absolutePath(m_projectDir / L"NameTable.toml")},
            {"project_note", m_agentProjectNotePath ? json(absolutePath(*m_agentProjectNotePath)) : json(nullptr)},
            {"term_ledger", absolutePath(m_agentTermLedgerPath)},
            {"file_notes_dir", absolutePath(m_agentFileNotesDir)}
        };
        // 路径直接取实际运行配置，EPUB/PDF 等转换流程的输入目录不能写死为 gt_input。
        addTool(
            "run_python",
            "Run Python code in a fresh process using the application's Python environment."
            " Use it only to read files, search and analyze translation data; do not modify/delete files, install packages,"
            " access the network or launch other processes. Print only useful results."
            " Variables do not persist between calls. cwd is the project directory. Execution lasts at most 120 seconds;"
            " stdout and stderr each return at most 64 KiB, with truncation flags."
            " The paths dictionary contains absolute paths: project_dir, input_dir (actual translation input),"
            " cache_dir (translation cache), output_dir, current_file, dictionary_files (active GPT dictionary TOML files),"
            " name_table, project_note (None if absent), term_ledger, file_notes_dir. Inspect directories with pathlib."
            " Some files/directories may not exist yet. Read UTF-8 files with encoding='utf-8-sig'."
            // 原文与缓存的字段不同，明确原文数组结构，避免模型拿缓存字段读取输入文件。
            " Source JSON files under input_dir contain arrays of objects with a required string message (source text),"
            " an optional string name (single speaker) or string array names (multiple speakers)."
            " A row without name/names is narration or monologue. Array positions are zero-based sentence indices."
            " Source JSON example: [{\"name\":\"Alice\",\"message\":\"Hello.\"},"
            "{\"message\":\"The door opened.\"},{\"names\":[\"Alice\",\"Bob\"],\"message\":\"Good morning.\"}]."
            " Cache JSON arrays use index, original_text, pre_processed_text, translated_raw_text;"
            " GPT dictionary TOML uses [[gptDict]] entries with org, rep and note. Use json and tomllib to read them."
            " Files contain the last saved state, which may lag in-memory preprocessing and translation;"
            " use the current batch supplied in the conversation as authoritative."
            " Source search example: import json; from pathlib import Path; "
            "rows = json.loads(Path(paths['current_file']).read_text(encoding='utf-8-sig')); "
            "print(json.dumps([dict(r, id=i) for i, r in enumerate(rows) if '兄' in r['message']], ensure_ascii=False))."
            " Cache search example: import json; from pathlib import Path; "
            "file = Path(paths['current_file']).relative_to(paths['input_dir']); "
            "rows = json.loads((Path(paths['cache_dir']) / file).read_text(encoding='utf-8-sig')); "
            "print([r.get('translated_raw_text', '') for r in rows if '兄' in r.get('original_text', '')])",
            agentObjectSchema({{"code", stringSchema}}));
    }
    addTool("commit_translations", "Submit every current sentence id, durable memory and sentence-level suggestions."
        " Empty note strings leave existing notes unchanged. Use [] for no term updates or suggestions.",
        commitSchema);
}

bool NormalJsonTranslatorTransAgent::translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch,
    int& recursionIndex, int& recursionCount, int threadId, int batchIndex)
{
    if (m_controller->shouldStop()) {
        return false;
    }
    for (Sentence* se : batch) {
        if (se->preproc.empty()) {
            se->transCompleted = true;
        }
    }

    auto pending = collectPendingSentences(batch);
    if (pending.empty()) {
        return true;
    }

    auto& worker = m_workers.at(threadId - 1);
    auto& rollingContext = worker.rollingContext;

    const std::string batchIndexLog = recursionCount == 0
        ? std::format("{}", batchIndex)
        : std::format("{}-({}/{})", batchIndex, recursionIndex, recursionCount);
    const std::string logPrefix = gppTr("NormalJsonTranslatorTransAgent.translateBatch", "[线程 %1] [文件 %2] [批次 %3] Agent")
        .arg(threadId).arg(wide2Ascii(relInputPath)).arg(batchIndexLog).toStdString();
    m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 开始翻译，最多 %2 轮，共 %3 句:\n%4")
        .arg(logPrefix).arg(m_agentMaxTurnsPerChunk).arg(pending.size())
        .arg(buildLogBlock(relInputPath, pending, rollingContext)).toStdString());

    const auto rebuildSession = [&](const TranslationApi& api)
        {
            worker.session = AgentApiSession{.api = api, .systemPrompt = m_agentSystemPrompt};
            if (!worker.rollingContext.empty()) {
                appendAgentUserMessage(*worker.session, "Conversation summary:\n" + worker.rollingContext);
            }
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 创建会话: %2 / %3")
                .arg(logPrefix).arg(apiProtocolToString(api.protocol)).arg(api.modelName).toStdString());
        };
    const auto selectApi = [&](bool fallbackCheck)
        {
            if (!worker.session || !m_apiPool->containsApi(worker.session->api) || (fallbackCheck && m_apiStrategy == "fallback")) {
                auto api = m_apiPool->getApi(m_apiStrategy);
                if (!api) {
                    throw std::runtime_error(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "没有可用的 Api key 了").toStdString());
                }
                if (!worker.session || !isSameApi(worker.session->api, *api)) {
                    rebuildSession(*api);
                }
            }
        };
    selectApi(true);

    AgentApiSession sessionBeforeBatch;
    const auto appendBatch = [&]()
        {
            if (m_smartRetry && pending.size() > 1) {
                sessionBeforeBatch = *worker.session;
            }
            appendAgentUserMessage(*worker.session, buildUserPrompt(relInputPath, pending, rollingContext));
        };
    appendBatch();

    const json compactSchema = agentObjectSchema({{"rolling_context", {{"type", "string"}}}});
    const json compactTools = json::array({
        {
            {"name", "compact_context"},
            {"description",
                "Summarize the conversation for the next session. Preserve translation decisions, character relationships, unresolved clues and current progress."
                " Do not translate or commit."},
            {"parameters", compactSchema}
        }
    });

    bool compacting = false;
    size_t compactedMessageCount = 0;
    int noToolCallCount = 0;
    bool retryExhausted = false;
    int turn = 0;
    int requestCount = 0;
    std::string requestLogPrefix;
    const auto restartAfterCompaction = [&](std::string memory)
        {
            const auto api = worker.session->api;
            // 有效摘要仅在重建时注入；压缩失败传入空摘要，旧记忆、历史和续接位置全部清除。
            rollingContext = std::move(memory);
            rebuildSession(api);
            appendBatch();
            compactedMessageCount = worker.session->history.size();
            compacting = false;
            noToolCallCount = 0;
        };

    for (; turn < m_agentMaxTurnsPerChunk; ++turn) {
        if (m_controller->shouldStop()) {
            return false;
        }
        const auto& currentSession = *worker.session;
        const bool nativeAutoCompaction = currentSession.api.agentNativeAutoCompaction &&
            (currentSession.api.protocol == ApiProtocol::OpenAIRes || currentSession.api.protocol == ApiProtocol::Claude);
        // 没有启用原生压缩时，先让模型总结完整历史，再用摘要重建会话；不直接截断旧消息。
        // 刚重建的批次本身可能已超过阈值，等历史增加后再判断，避免连续只做压缩。
        if (!compacting && !nativeAutoCompaction && currentSession.history.size() > compactedMessageCount &&
            currentSession.history.dump().size() > (size_t)m_agentCompactContextThresholdBytes)
        {
            compacting = true;
            appendAgentUserMessage(*worker.session,
                "Context exceeded the compact context threshold. Summarize the full conversation into rolling_context using compact_context, "
                "preserving translation decisions, character relationships, "
                "unresolved clues and current progress. Do not call other tools or commit in this turn.");
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 [轮次 %2] 上下文超过字节阈值，要求模型先压缩上下文")
                .arg(logPrefix).arg(turn + 1).toStdString());
        }
        requestCount = 0;
        AgentApiResponse response;
        while (requestCount < m_maxRequestCount) {
            if (m_controller->shouldStop()) {
                return false;
            }
            pending = collectPendingSentences(batch);
            if (pending.empty()) {
                return true;
            }
            if (!m_apiPool->containsApi(worker.session->api)) {
                selectApi(false);
                appendBatch();
                compacting = false;
                compactedMessageCount = 0;
            }
            if (m_smartRetry && !compacting && requestCount == 2 && pending.size() > 1) {
                m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 [请求 %2] 开始对半拆分句子重新请求...")
                    .arg(logPrefix).arg(requestCount + 1).toStdString());

                // 撤回失败批次及其工具轮次，服务端也从批次前的 id 续接，避免仍携带完整批次。
                worker.session = std::move(sessionBeforeBatch);
                const std::span<Sentence*> pendingSpan(pending);
                const size_t mid = pending.size() / 2;

                recursionCount += 2;
                ++recursionIndex;
                const bool firstOk = translateBatch(relInputPath, pendingSpan.first(mid),
                    recursionIndex, recursionCount, threadId, batchIndex);
                ++recursionIndex;
                const bool secondOk = translateBatch(relInputPath, pendingSpan.subspan(mid),
                    recursionIndex, recursionCount, threadId, batchIndex);
                return firstOk && secondOk;
            }
            requestLogPrefix = gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 [轮次 %2] [请求 %3]")
                .arg(logPrefix).arg(turn + 1).arg(requestCount + 1).toStdString();
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 开始请求，剩余 %2 句，上下文预估 %3 字节")
                .arg(requestLogPrefix).arg(pending.size()).arg(worker.session->history.dump().size()).toStdString());
            response = performAgentApiRequest(*worker.session, compacting ? compactTools : m_nativeTools,
                m_enhanceJailbreak, m_onPerformApi, m_controller, m_logger, m_apiTimeOutMs);
            if (response.content) {
                break;
            }
            if (m_controller->shouldStop()) {
                return false;
            }
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
            m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 压缩请求重试耗尽，清空上下文后继续当前批次: %2")
                .arg(requestLogPrefix).arg(formatApiError(response.content.error())).toStdString());
            restartAfterCompaction({});
            continue;
        }

        auto& session = *worker.session;
        auto& reply = *response.content;
        if (m_logger->should_log(spdlog::level::trace)) {
            // 原生工具参数也属于响应内容，不能只记录可能为空的正文。
            json calls = json::array();
            for (const auto& call : reply.calls) {
                calls.push_back({ {"id", call.id}, {"name", call.name}, {"arguments", call.arguments} });
            }
            m_logger->trace(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 成功响应，回复正文:\n%2\n工具调用:\n%3")
                .arg(requestLogPrefix)
                .arg(reply.text.empty() ? gppTr("NormalJsonTranslatorTransAgent.translateBatch", "[GPP.正文为空]").toStdString() : reply.text)
                .arg(calls.dump(2)).toStdString());
        }
        if (!reply.calls.empty() && m_logger->should_log(spdlog::level::info)) {
            std::string overview;
            for (const auto& [index, call] : reply.calls | std::views::enumerate) {
                const auto callOverview = std::format(
                    "[{}] {}({})\n",
                    index + 1,
                    call.name.empty() ? "<unknown>" : call.name,
                    call.arguments.dump(2)
                );
                overview += limitLogLines(callOverview, m_inputBlockMaxLines);
            }
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 工具调用 %2 个，调用概览:\n%3")
                .arg(requestLogPrefix).arg(reply.calls.size())
                .arg(overview).toStdString());
        }

        // 连续没有工具调用时沿用请求重试次数作为上限，任意工具调用都会清零。
        noToolCallCount = reply.calls.empty() ? noToolCallCount + 1 : 0;
        if (noToolCallCount > 0) {
            m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 未调用工具，连续 %2 / %3 轮未调用工具，回复正文:\n%4")
                .arg(requestLogPrefix).arg(noToolCallCount).arg(m_maxRequestCount)
                .arg(reply.text.empty() ? gppTr("NormalJsonTranslatorTransAgent.translateBatch", "[GPP.正文为空]").toStdString()
                    : limitLogLines(reply.text, m_inputBlockMaxLines)).toStdString());
        }
        if (compacting) {
            std::string memory;
            try {
                if (reply.calls.size() != 1 || reply.calls.front().name != "compact_context") {
                    throw std::runtime_error("Call compact_context only in this turn.");
                }
                const auto& arguments = reply.calls.front().arguments;
                const json summary = arguments.is_string()
                    ? json::parse(arguments.get_ref<const std::string&>())
                    : arguments;
                memory = summary.at("rolling_context").get<std::string>();
                if (memory.empty()) {
                    throw std::runtime_error("rolling_context must contain the conversation summary.");
                }
            }
            catch (const std::exception& e) {
                m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 摘要无效，清空上下文后继续当前批次: %2")
                    .arg(requestLogPrefix).arg(e.what()).toStdString());
                restartAfterCompaction({});
                continue;
            }
            if (m_logger->should_log(spdlog::level::debug)) {
                m_logger->debug(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 压缩后的滚动上下文:\n%2")
                    .arg(requestLogPrefix).arg(memory).toStdString());
            }
            restartAfterCompaction(std::move(memory));
            m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 已完成上下文压缩，继续当前批次")
                .arg(requestLogPrefix).toStdString());
            continue;
        }
        if (noToolCallCount >= m_maxRequestCount) {
            break;
        }

        bool committed = false;
        json results = json::array();
        const auto commit = [&](json arguments)
            {
                if (committed) {
                    throw std::runtime_error("This batch has already been committed.");
                }
                // 严格 schema 的空备注字段代表不修改，不能覆盖已有记忆。
                if (arguments.contains("file_note_patch")) {
                    auto& patch = arguments["file_note_patch"];
                    for (auto it = patch.begin(); it != patch.end();) {
                        if (it->is_string() && it->get_ref<const std::string&>().empty()) {
                            it = patch.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                }
                if (auto updates = arguments.find("term_updates"); updates != arguments.end()) {
                    for (auto& update : *updates) {
                        if (update.contains("line_ids") && update["line_ids"].empty()) {
                            update.erase("line_ids");
                        }
                    }
                }
                std::string commitLog;
                int termCount = 0, suggestionCount = 0;
                const int count = applyCommit(relInputPath, pending, threadId, arguments,
                    makeTransby(session.api.apikey, session.api.modelName),
                    batchIndexLog, turn, requestCount, commitLog, termCount, suggestionCount);
                committed = true;
                m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 响应处理成功，处理结果:\n%2")
                    .arg(requestLogPrefix)
                    .arg(gppTr("NormalJsonTranslatorTransAgent.translateBatch",
                        "该批次 %1 句译文均已提交，记录术语 %2 条，记录建议 %3 条，翻译结果:\n%4")
                        .arg(count).arg(termCount).arg(suggestionCount).arg(limitLogLines(commitLog, m_inputBlockMaxLines)))
                    .toStdString());
                return json{{"committed", count}};
            };
        for (const auto& call : reply.calls) {
            if (session.api.fastPause && m_controller->shouldStop()) {
                return false;
            }
            json result;
            try {
                const json arguments = call.arguments.is_string()
                    ? json::parse(call.arguments.get_ref<const std::string&>())
                    : call.arguments;
                if (call.name == "commit_translations") {
                    result = commit(arguments);
                }
                else if (call.name == "run_python" && !m_pythonPaths.is_null()) {
                    json paths = m_pythonPaths;
                    paths["current_file"] = wide2Ascii(
                        fs::path(ascii2Wide(paths.at("input_dir").get<std::string>())) / relInputPath);
                    result = runAgentPython(arguments.at("code").get_ref<const std::string&>(), paths, m_controller, session.api.fastPause);
                }
                else {
                    result = runReadTool(relInputPath, call.name, arguments);
                }
            }
            catch (const std::exception& e) {
                result = {{"error", e.what()}};
                m_logger->warn(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 工具 %2 执行失败: %3")
                    .arg(requestLogPrefix).arg(call.name).arg(e.what()).toStdString());
                m_controller->recordRuntimeTransError(RuntimeTransErrorEvent{
                    .kind = "agent",
                    .level = "warning",
                    .message = gppTr("NormalJsonTranslatorTransAgent.translateBatch", "工具 %1 执行失败: %2")
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
                m_logger->debug(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 工具调用明细:\n%2")
                    .arg(requestLogPrefix)
                    .arg(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "工具返回结果:\n%1").arg(results.dump(2)))
                    .toStdString());
            }
            appendAgentToolResults(session, results);
            if (m_logger->should_log(spdlog::level::info)) {
                std::string overview;
                for (const auto& [index, item] : results | std::views::enumerate) {
                    if (item.at("name") == "commit_translations") {
                        continue;
                    }
                    std::string resultOverview;
                    if (item.at("name") == "run_python") {
                        // 输出正文保留换行，避免 JSON 转义让多行运行结果挤在一行。
                        json result = item.at("result");
                        const auto stdoutText = result.value("stdout", std::string{});
                        const auto stderrText = result.value("stderr", std::string{});
                        result.erase("stdout");
                        result.erase("stderr");
                        resultOverview += result.dump() + "\n";
                        if (!stdoutText.empty()) {
                            resultOverview += "stdout:\n" + stdoutText + "\n";
                        }
                        if (!stderrText.empty()) {
                            resultOverview += "stderr:\n" + stderrText + "\n";
                        }
                    }
                    else {
                        resultOverview += item.at("result").dump(2) + "\n";
                    }
                    overview += std::format("[{}] {}:\n", index + 1, item.at("name").get<std::string>())
                        + limitLogLines(resultOverview, m_inputBlockMaxLines);
                }
                if (!overview.empty()) {
                    m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 已回填 %2 个工具结果，回填概览:\n%3")
                        .arg(requestLogPrefix).arg(results.size())
                        .arg(overview).toStdString());
                }
            }
            if (!committed) {
                m_logger->info(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 已回填 %2 个工具结果，进入下一轮")
                    .arg(requestLogPrefix).arg(results.size()).toStdString());
            }
        }
        else {
            appendAgentUserMessage(session, "Text responses do not submit translations. Call a read/search tool if needed, "
                "or call commit_translations to submit every sentence in the current batch.");
        }
        if (committed) {
            return true;
        }
    }

    appendAgentUserMessage(*worker.session, "The current batch was abandoned after reaching the request/turn limit. Do not commit it later.");
    for (Sentence* se : pending) {
        se->transraw = "(Failed to translate)" + se->preproc;
        se->transCompleted = true;
    }
    // 分别记录轮次耗尽、请求重试耗尽和连续无工具调用。
    const size_t translatedCount = batch.size() - pending.size();
    if (retryExhausted) {
        m_logger->error(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 [轮次 %2] 在 %3 次请求后彻底失败，共翻译 (%4 / %5) 句")
            .arg(logPrefix).arg(turn + 1).arg(requestCount).arg(translatedCount).arg(batch.size()).toStdString());
    }
    else if (noToolCallCount >= m_maxRequestCount) {
        m_logger->error(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 [轮次 %2] 因连续 %3 轮未调用工具而失败，共翻译 (%4 / %5) 句")
            .arg(logPrefix).arg(turn + 1).arg(noToolCallCount).arg(translatedCount).arg(batch.size()).toStdString());
    }
    else {
        m_logger->error(gppTr("NormalJsonTranslatorTransAgent.translateBatch", "%1 因超过最大轮数 (%2 轮) 而失败，共翻译 (%3 / %4) 句")
            .arg(logPrefix).arg(m_agentMaxTurnsPerChunk).arg(translatedCount).arg(batch.size()).toStdString());
    }
    return false;
}

NAMESPACE_END(gpp)
