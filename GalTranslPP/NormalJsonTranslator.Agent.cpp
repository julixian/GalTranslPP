module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#include <Shlwapi.h>
#endif

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

std::string nowTimestampString() {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return std::to_string(now);
}

json loadJsonFileOr(const fs::path& path, const json& fallback = json::object()) {
    try {
        if (!fs::exists(path)) {
            return fallback;
        }
        std::ifstream ifs(path, std::ios::binary);
        return json::parse(ifs);
    }
    catch (...) {
        return fallback;
    }
}

void saveJsonFile(const fs::path& path, const json& value) {
    createParent(path);
    std::ofstream ofs(path, std::ios::binary);
    ofs << value.dump(2);
}

json sanitizeAgentFileNote(json note) {
    if (note.is_object()) {
        note.erase("rolling_context");
    }
    return note;
}

fs::path buildAgentFileNotePath(const fs::path& root, const fs::path& relInputPath) {
    fs::path notePath = root / relInputPath;
    notePath += L".json";
    return notePath;
}

std::string trimCopy(std::string value) {
    const auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
    const auto begin = std::ranges::find_if(value, isNotSpace);
    if (begin == value.end()) {
        return {};
    }
    const auto end = std::ranges::find_if(value | std::views::reverse, isNotSpace).base();
    return std::string(begin, end);
}

std::optional<json> tryParseJsonEnvelope(std::string text) {
    text = trimCopy(std::move(text));
    if (text.empty()) {
        return std::nullopt;
    }

    const size_t fencedStart = text.find("```");
    if (fencedStart != std::string::npos) {
        const size_t lineEnd = text.find('\n', fencedStart);
        const size_t fencedEnd = text.rfind("```");
        if (lineEnd != std::string::npos && fencedEnd != std::string::npos && fencedEnd > lineEnd) {
            text = trimCopy(text.substr(lineEnd + 1, fencedEnd - lineEnd - 1));
        }
    }

    try {
        return json::parse(text);
    }
    catch (...) {}

    text = lightRepairJsonText(text);
    try {
        return json::parse(text);
    }
    catch (...) { }

    const size_t jsonStart = text.find('{');
    const size_t jsonEnd = text.rfind('}');
    if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
        return std::nullopt;
    }

    try {
        return json::parse(text.substr(jsonStart, jsonEnd - jsonStart + 1));
    }
    catch (...) {
        try {
                return json::parse(lightRepairJsonText(text.substr(jsonStart, jsonEnd - jsonStart + 1)));
        }
        catch (...) {
            return std::nullopt;
        }
    }
}

AgentProtocolResponse parseAgentTextResponse(const std::string& content) {
    AgentProtocolResponse result;
    result.rawContent = content;
    const std::optional<json> payloadOpt = tryParseJsonEnvelope(content);
    if (!payloadOpt.has_value() || !payloadOpt->is_object()) {
        throw std::runtime_error("Agent 响应不是合法 JSON 对象");
    }

    const json& payload = *payloadOpt;
    result.action = payload.value("action", "");
    if (const auto it = payload.find("calls"); it != payload.end() && it->is_array()) {
        for (const auto& call : *it) {
            if (!call.is_object()) {
                continue;
            }
            AgentToolCallRequest parsed;
            parsed.id = call.value("id", std::format("call_{}", result.calls.size()));
            // 兼容历史/模型变体：
            // 首选 `name` + `arguments`，同时接受 `method` / `tool` / `function`
            // 以及 `params` 作为别名，避免模型因为字段漂移而整轮工具调用失效。
            parsed.name = call.value("name", "");
            if (parsed.name.empty()) {
                parsed.name = call.value("method", "");
            }
            if (parsed.name.empty()) {
                parsed.name = call.value("tool", "");
            }
            if (parsed.name.empty()) {
                parsed.name = call.value("function", "");
            }
            if (const auto argIt = call.find("arguments"); argIt != call.end()) {
                parsed.arguments = *argIt;
            }
            else if (const auto argIt = call.find("params"); argIt != call.end()) {
                parsed.arguments = *argIt;
            }
            result.calls.push_back(std::move(parsed));
        }
    }
    if (const auto it = payload.find("translations"); it != payload.end() && it->is_array()) {
        result.translations = *it;
    }
    if (const auto it = payload.find("term_updates"); it != payload.end() && it->is_array()) {
        result.termUpdates = json::array();
        for (const auto& update : *it) {
            if (!update.is_object()) {
                continue;
            }
            json normalized = update;
            if (!normalized.contains("source_term")) {
                if (const auto srcIt = normalized.find("src"); srcIt != normalized.end() && srcIt->is_string()) {
                    normalized["source_term"] = *srcIt;
                }
                else if (const auto termIt = normalized.find("term"); termIt != normalized.end() && termIt->is_string()) {
                    normalized["source_term"] = *termIt;
                }
            }
            if (!normalized.contains("target_term")) {
                if (const auto dstIt = normalized.find("dst"); dstIt != normalized.end() && dstIt->is_string()) {
                    normalized["target_term"] = *dstIt;
                }
                else if (const auto transIt = normalized.find("translation"); transIt != normalized.end() && transIt->is_string()) {
                    normalized["target_term"] = *transIt;
                }
            }
            result.termUpdates.push_back(std::move(normalized));
        }
    }
    if (const auto it = payload.find("rewrite_requests"); it != payload.end() && it->is_array()) {
        result.rewriteRequests = *it;
    }
    if (const auto it = payload.find("file_note_patch"); it != payload.end() && it->is_object()) {
        result.fileNotePatch = *it;
    }
    if (const auto it = payload.find("rolling_context"); it != payload.end() && it->is_string()) {
        result.rollingContext = it->get<std::string>();
    }
    return result;
}

int sanitizeToolLimit(int requested, int fallback, int maxLimit = 200) {
    if (requested <= 0) {
        return fallback;
    }
    return std::min(requested, maxLimit);
}

// Agent 调试日志只记录“真实交互链路”上的内容：
// 1. 实际发给模型的 messages
// 2. 模型原始响应
// 3. 模型请求的工具与参数
// 4. 工具执行结果
// 5. commit 中真正提交的协议内容
// 这些日志不会再回流给模型，只用于人工排错。
std::string truncateForAgentLog(std::string text, size_t maxChars = 4000) {
    if (text.size() <= maxChars) {
        return text;
    }
    return text.substr(0, maxChars) + std::format("\n...(truncated, total {} chars)", text.size());
}

std::string formatToolCallRequestsForLog(const std::vector<AgentToolCallRequest>& calls) {
    std::string result;
    for (const auto& [index, call] : calls | std::views::enumerate) {
        result += std::format(
            "[{}] {}({})\n",
            index + 1,
            call.name.empty() ? "<unknown>" : call.name,
            call.arguments.dump(2)
        );
    }
    return result;
}

std::string formatToolCallSummary(const std::vector<AgentToolCallRequest>& calls) {
    if (calls.empty()) {
        return "None";
    }
    std::string result;
    for (const auto& [index, call] : calls | std::views::enumerate) {
        if (index > 0) {
            result += ", ";
        }
        result += call.name.empty() ? "<unknown>" : call.name;
    }
    return result;
}

std::string jsonStringOr(const json& obj, std::string_view key, const std::string& fallback = {}) {
    if (!obj.is_object()) {
        return fallback;
    }
    const auto it = obj.find(std::string(key));
    if (it != obj.end() && it->is_string()) {
        return it->get<std::string>();
    }
    return fallback;
}

size_t approximateMessagesChars(const json& messages) {
    size_t total = 0;
    for (const auto& item : messages) {
        total += item.dump().size();
    }
    return total;
}

std::vector<Sentence*> collectAgentPendingSentences(std::span<Sentence*> batch) {
    return batch
        | std::views::filter([](const Sentence* se) { return !se->complete; })
        | std::ranges::to<std::vector>();
}

std::string buildAgentTermLedgerExcerpt(
    const json& ledger,
    const std::string& currentInputBlock,
    const std::string& rollingSummary,
    const json& fileNote,
    int searchResultLimit
) {
    json relevant = json::array();
    json fallback = json::array();
    const std::string inputLower = str2Lower(currentInputBlock);
    const std::string summaryLower = str2Lower(rollingSummary);
    const std::string fileNoteLower = str2Lower(fileNote.dump());
    for (const auto& item : ledger.items()) {
        const std::string term = item.key();
        const json& entry = item.value();
        const json normalizedEntry = {
            {"source_term", term},
            {"term", term},
            {"target_term", entry.value("target_term", "")},
            {"status", entry.value("status", "tentative")},
            {"category", entry.value("category", "")},
            {"note", entry.value("note", "")}
        };

        const std::string termLower = str2Lower(term);
        const std::string targetLower = str2Lower(entry.value("target_term", ""));
        const std::string noteLower = str2Lower(entry.value("note", ""));
        const bool isRelevant =
            (!inputLower.empty() && inputLower.contains(termLower)) ||
            (!summaryLower.empty() && (summaryLower.contains(termLower) ||
            (!targetLower.empty() && summaryLower.contains(targetLower)))) ||
            (!fileNoteLower.empty() && (fileNoteLower.contains(termLower) ||
            (!targetLower.empty() && fileNoteLower.contains(targetLower)) ||
            (!noteLower.empty() && fileNoteLower.contains(noteLower))));

        if (isRelevant) {
            relevant.push_back(normalizedEntry);
            continue;
        }
        if ((int)fallback.size() < searchResultLimit) {
            fallback.push_back(normalizedEntry);
        }
    }
    return (relevant.empty() ? fallback : relevant).dump(2);
}

std::string buildAgentProblemSummary(std::span<Sentence*> batch) {
    const std::vector<Sentence*> pending = collectAgentPendingSentences(batch);
    return std::ranges::fold_left(pending
            | std::views::transform([](const Sentence* se) { return se->problems; })
            | std::views::join,
        std::string{},
        [](const std::string& acc, const std::string& value)
        {
            if (!acc.contains(value)) {
                return acc + value + "\n";
            }
            return acc;
        }
    );
}

std::string buildAgentGlossary(const GptDictionary& gptDictionary, TransEngine transEngine, std::span<Sentence*> batch) {
    std::vector<Sentence*> pending = collectAgentPendingSentences(batch);
    if (pending.empty()) {
        return {};
    }
    std::span<Sentence*> pendingSpan(pending.data(), pending.size());
    return gptDictionary.generatePrompt(pendingSpan, transEngine);
}

void mergeAgentFileNotePatch(json& note, const json& patch) {
    if (!patch.is_object()) {
        return;
    }
    for (const auto& item : patch.items()) {
        note[item.key()] = item.value();
    }
}

void appendAgentOccurrence(json& entry, const fs::path& file, int id) {
    if (!entry.contains("occurrences") || !entry["occurrences"].is_array()) {
        entry["occurrences"] = json::array();
    }
    const std::string fileStr = wide2Ascii(file);
    const bool exists = std::ranges::any_of(entry["occurrences"], [&](const json& occurrence) 
        {
            return occurrence.value("file", "") == fileStr && occurrence.value("id", -1) == id;
        });
    if (!exists) {
        entry["occurrences"].push_back({ {"file", fileStr}, {"id", id} });
    }
}

std::vector<int> inferAgentOccurrenceIdsFromChunk(const std::string& sourceTerm, const std::vector<Sentence*>& pending) {
    std::vector<int> matchedIds;
    for (const Sentence* se : pending) {
        if (se == nullptr || sourceTerm.empty()) {
            continue;
        }
        bool matched = false;
        if (se->nameType != NameType::None && se->name.contains(sourceTerm)) {
            matched = true;
        }
        if (!matched) {
            matched = se->original_text.contains(sourceTerm) || se->pre_processed_text.contains(sourceTerm);
        }
        if (!matched && !se->names.empty()) {
            matched = std::ranges::any_of(se->names, [&](const std::string& name)
                {
                    return name.contains(sourceTerm);
                });
        }
        if (matched) {
            matchedIds.push_back(se->index);
        }
    }
    return matchedIds;
}

void enqueueAgentRewriteRequest(json& queue, const json& request) {
    if (!request.is_object()) {
        return;
    }
    const std::string file = request.value("file", "");
    const int id = request.value("id", -1);
    const std::string sourceTerm = request.value("source_term", "");
    const bool exists = std::ranges::any_of(queue, [&](const json& item) 
        {
            return item.value("file", "") == file && 
                item.value("id", -1) == id && 
                item.value("source_term", "") == sourceTerm;
        });
    if (!exists) {
        queue.push_back(request);
    }
}

// 只读运行状态加载函数，主要给恢复调度和启动检查使用。
// 它通常每次运行只会调用少数几次，而不是按句子频繁调用。
json NormalJsonTranslator::loadAgentRunState() {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    return loadJsonFileOr(m_agentRunStatePath, json::object());
}

// 只读术语账本加载函数，主要供工具执行和提示词重建使用。
// 单个 chunk 在一次多轮循环里可能会反复读取几次。
json NormalJsonTranslator::loadAgentTermLedger() {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    return loadJsonFileOr(m_agentTermLedgerPath, json::object());
}

// 只读重翻队列加载函数，主要给恢复流程和最终 reconcile 使用。
json NormalJsonTranslator::loadAgentRewriteQueue() {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    return loadJsonFileOr(m_agentRewriteQueuePath, json::array());
}

// `file_note` 是按输入文件保存的持久化备注。
// 它不再默认注入每轮提示词，而是作为：
// 1. `get_file_note` 工具的读取源
// 2. 术语筛选时的相关性参考
// 3. `compact_context` / `commit` 的落盘目标
json NormalJsonTranslator::loadAgentFileNote(const fs::path& targetRelPath) {
    std::lock_guard<std::mutex> lock(m_agentFileNotesMutex);
    return sanitizeAgentFileNote(loadJsonFileOr(buildAgentFileNotePath(m_agentFileNotesDir, targetRelPath), json::object()));
}

void NormalJsonTranslator::saveAgentFileNote(const fs::path& targetRelPath, const json& note) {
    std::lock_guard<std::mutex> lock(m_agentFileNotesMutex);
    saveJsonFile(buildAgentFileNotePath(m_agentFileNotesDir, targetRelPath), sanitizeAgentFileNote(note));
}

// 轻量的 `run_state` 生命周期更新函数。
// 通常在 worker 进入 `processFile`、处理中断、处理完成这几个时机调用。
void NormalJsonTranslator::updateAgentRunStateEntry(
    const fs::path& relInputPath,
    const std::string& status,
    int lastCommittedIndex,
    const std::string& leaseOwner
) {
    if (!m_agentEnabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    json state = loadJsonFileOr(m_agentRunStatePath, json::object());
    if (!state.contains("files") || !state["files"].is_array()) {
        state["files"] = json::array();
    }
    const std::string relPathStr = wide2Ascii(relInputPath);
    const auto it = std::ranges::find_if(state["files"], [&](const json& item) 
        {
            return item.value("file", "") == relPathStr;
        });
    if (it == state["files"].end()) {
        state["files"].push_back(json{
            {"file", relPathStr},
            {"status", status},
            {"lease_owner", leaseOwner},
            {"last_committed_index", lastCommittedIndex},
            {"updated_at", nowTimestampString()}
        });
    }
    else {
        (*it)["status"] = status;
        (*it)["lease_owner"] = leaseOwner;
        if (lastCommittedIndex >= 0) {
            (*it)["last_committed_index"] = lastCommittedIndex;
        }
        (*it)["updated_at"] = nowTimestampString();
    }
    state["updated_at"] = nowTimestampString();
    saveJsonFile(m_agentRunStatePath, state);
}

// 共享 Agent 状态文件的强串行修改入口。
// `commit` 路径必须走这里，避免多个 worker 在线程并发时发生 `load/save` 覆盖。
void NormalJsonTranslator::mutateAgentState(const std::function<void(json& runState, json& termLedger, json& rewriteQueue)>& mutator) {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    json runState = loadJsonFileOr(m_agentRunStatePath, json::object());
    json termLedger = loadJsonFileOr(m_agentTermLedgerPath, json::object());
    json rewriteQueue = loadJsonFileOr(m_agentRewriteQueuePath, json::array());
    mutator(runState, termLedger, rewriteQueue);
    saveJsonFile(m_agentRunStatePath, runState);
    saveJsonFile(m_agentTermLedgerPath, termLedger);
    saveJsonFile(m_agentRewriteQueuePath, rewriteQueue);
}

std::string NormalJsonTranslator::buildAgentLogBlock(const fs::path& relInputPath, std::span<Sentence*> batch, const std::string& rollingSummary) {
    std::vector<Sentence*> pending = ::collectAgentPendingSentences(batch);
    absl::btree_map<int, Sentence*> id2SentenceMap;
    std::string inputBlock;
    std::span<Sentence*> pendingSpan(pending.data(), pending.size());
    fillBlockAndMap(pendingSpan, id2SentenceMap, inputBlock, m_transEngine);

    const std::string inputProblems = ::buildAgentProblemSummary(batch);
    const std::string glossary = ::buildAgentGlossary(*m_gptDictionary, m_transEngine, batch);
    const json currentFileNote = loadAgentFileNote(relInputPath);
    const std::string knownTerms = ::buildAgentTermLedgerExcerpt(loadAgentTermLedger(), inputBlock, rollingSummary, currentFileNote, m_agentSearchResultLimit);

    std::string logBlock;
    if (!inputProblems.empty()) {
        logBlock += "\nProblems:\n" + inputProblems;
    }
    if (m_logger->should_log(spdlog::level::debug) && !rollingSummary.empty()) {
        logBlock += "\nRollingContext:\n" + rollingSummary + "\n";
    }
    if (m_logger->should_log(spdlog::level::debug) && !currentFileNote.empty()) {
        logBlock += "\nFileNote:\n" + currentFileNote.dump(2) + "\n";
    }
    if (!glossary.empty()) {
        logBlock += "\nGlossary:\n" + glossary;
    }
    if (m_logger->should_log(spdlog::level::debug) && knownTerms != "[]") {
        logBlock += "\nKnownTerms:\n" + knownTerms;
    }
    logBlock += "\ninputBlock:\n" + inputBlock;
    return logBlock;
}

json NormalJsonTranslator::buildAgentBaseMessages(const fs::path& relInputPath, std::span<Sentence*> batch, const std::string& rollingSummary) {
    absl::btree_map<int, Sentence*> id2SentenceMap;
    std::string inputBlock;
    fillBlockAndMap(batch, id2SentenceMap, inputBlock, m_transEngine);
    const std::string inputProblems = ::buildAgentProblemSummary(batch);
    const std::string glossary = ::buildAgentGlossary(*m_gptDictionary, m_transEngine, batch);
    const json currentFileNote = loadAgentFileNote(relInputPath);
    const std::string knownTerms = ::buildAgentTermLedgerExcerpt(loadAgentTermLedger(), inputBlock, rollingSummary, currentFileNote, m_agentSearchResultLimit);
    const std::string extraTools = m_agentProjectInfoPath.has_value()
        ? std::format("get_project_note: read optional user-provided script note file `{}`\n", 
            wide2Ascii(fs::relative(m_agentProjectInfoPath.value(), m_projectDir)))
        : "";

    const std::string schemaDescription =
        "{"
        "\"schema\":\"gpp-agent-v1\","
        "\"action\":\"tool_calls|commit|compact_context\","
        "\"calls\":[],"
        "\"translations\":[],"
        "\"term_updates\":[],"
        "\"rewrite_requests\":[],"
        "\"file_note_patch\":{},"
        "\"rolling_context\":\"\""
        "}";

    std::string userPrompt = m_agentUserPrompt;
    replaceStrInplace(userPrompt, "[AgentCurrentFile]", wide2Ascii(relInputPath));
    replaceStrInplace(userPrompt, "[AgentChunkIdRange]", std::format("{}-{}", batch.front()->index, batch.back()->index));
    replaceStrInplace(userPrompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(userPrompt, "[AgentTargetLang]", m_targetLang);
    replaceStrInplace(userPrompt, "[AgentProblemDescription]", inputProblems.empty() ? "None" : inputProblems);
    replaceStrInplace(userPrompt, "[AgentGlossary]", glossary.empty() ? "None" : glossary);
    replaceStrInplace(userPrompt, "[AgentFileNote]", currentFileNote.empty() ? "None" : currentFileNote.dump(2));
    replaceStrInplace(userPrompt, "[AgentRollingContext]", rollingSummary.empty() ? "None" : rollingSummary);
    replaceStrInplace(userPrompt, "[AgentKnownTerms]", knownTerms);
    std::string agentInputHeader;
    switch (m_transEngine) {
    case TransEngine::ForGalTsv:
        agentInputHeader = "NAME\tSRC\tID\n";
        break;
    case TransEngine::ForNovelTsv:
        agentInputHeader = "SRC\tID\n";
        break;
    default:
        throw std::invalid_argument("Agent 模式不支持的 TransEngine");
    }
    replaceStrInplace(userPrompt, "[AgentCurrentChunkTsv]", agentInputHeader + inputBlock);
    replaceStrInplace(userPrompt, "[AgentSchemaDescription]", schemaDescription);
    replaceStrInplace(userPrompt, "[AgentExtraTools]", extraTools);

    return json::array({
        {{"role", "system"}, {"content", m_agentSystemPrompt}},
        {{"role", "user"}, {"content", userPrompt}}
    });
}

void NormalJsonTranslator::applyAgentCommit(
    const fs::path& relInputPath,
    std::span<Sentence*> batch,
    std::string& backgroundText,
    int threadId,
    const AgentProtocolResponse& protocol,
    const std::string& modelName,
    int& committedCount
) {
    absl::flat_hash_map<int, json> translationMap;
    for (const auto& item : protocol.translations) {
        if (!item.is_object()) {
            continue;
        }
        const int id = item.value("id", -1);
        const std::string dst = item.value("dst", "");
        if (id >= 0 && !dst.empty()) {
            translationMap.insert_or_assign(id, item);
        }
    }

    const std::vector<Sentence*> pending = ::collectAgentPendingSentences(batch);
    if (translationMap.size() != pending.size()) {
        throw std::runtime_error("commit 未覆盖当前 chunk 的全部句子");
    }

    struct PendingSentencePatch {
        Sentence* sentence = nullptr;
        std::string dst;
    };
    std::vector<PendingSentencePatch> sentencePatches;
    sentencePatches.reserve(pending.size());
    for (Sentence* se : pending) {
        const auto it = translationMap.find(se->index);
        if (it == translationMap.end()) {
            throw std::runtime_error(std::format("commit 缺少句子 {}", se->index));
        }
        const std::string dst = it->second.value("dst", "");
        if (dst.empty()) {
            throw std::runtime_error(std::format("commit 句子 {} 的 dst 为空", se->index));
        }
        sentencePatches.push_back({
            .sentence = se,
            .dst = dst
        });
    }

    json fileNote = loadAgentFileNote(relInputPath);
    json nextFileNote = fileNote;
    std::string nextBackgroundText = backgroundText;
    ::mergeAgentFileNotePatch(nextFileNote, protocol.fileNotePatch);
    if (!protocol.rollingContext.empty()) {
        nextBackgroundText = protocol.rollingContext;
    }
    nextFileNote["updated_at"] = nowTimestampString();

    const std::vector<int> currentChunkIds = pending
        | std::views::transform([](const Sentence* se) { return se->index; })
        | std::ranges::to<std::vector>();

    int appliedTermUpdateCount = 0;
    mutateAgentState([&](json& runState, json& termLedger, json& rewriteQueue) 
        {
            if (!runState.contains("files") || !runState["files"].is_array()) {
                runState["files"] = json::array();
            }
            const std::string relPathStr = wide2Ascii(relInputPath);
            auto runStateIt = std::ranges::find_if(runState["files"], [&](const json& item) 
                {
                    return item.value("file", "") == relPathStr;
                });
            if (runStateIt == runState["files"].end()) {
                runState["files"].push_back(json{
                        {"file", relPathStr},
                        {"status", "in_progress"},
                        {"lease_owner", std::format("thread-{}", threadId)},
                        {"last_committed_index", pending.back()->index},
                        {"updated_at", nowTimestampString()}
                    });
            }
            else {
                (*runStateIt)["status"] = "in_progress";
                (*runStateIt)["lease_owner"] = std::format("thread-{}", threadId);
                (*runStateIt)["last_committed_index"] = pending.back()->index;
                (*runStateIt)["updated_at"] = nowTimestampString();
            }
            runState["updated_at"] = nowTimestampString();

            for (const auto& update : protocol.termUpdates) {
                if (!update.is_object()) {
                    continue;
                }
                const std::string sourceTerm = update.value("source_term", update.value("term", ""));
                const std::string targetTerm = update.value("target_term", update.value("translation", ""));
                if (sourceTerm.empty() || targetTerm.empty()) {
                    continue;
                }
                ++appliedTermUpdateCount;
                json& entry = termLedger[sourceTerm];
                if (!entry.is_object()) {
                    entry = json::object();
                }
                const std::string oldTarget = jsonStringOr(entry, "target_term");
                entry["target_term"] = targetTerm;
                entry["status"] = jsonStringOr(update, "status", jsonStringOr(entry, "status", "tentative"));
                entry["category"] = jsonStringOr(update, "category", jsonStringOr(entry, "category"));
                entry["note"] = jsonStringOr(update, "note", jsonStringOr(entry, "note"));
                if (update.contains("line_ids") && update["line_ids"].is_array()) {
                    for (const auto& idVal : update["line_ids"]) {
                        const int id = idVal.get<int>();
                        if (std::ranges::contains(currentChunkIds, id)) {
                            ::appendAgentOccurrence(entry, relInputPath, id);
                        }
                    }
                }
                else {
                    const std::vector<int> inferredIds = ::inferAgentOccurrenceIdsFromChunk(sourceTerm, pending);
                    for (const int id : inferredIds) {
                        ::appendAgentOccurrence(entry, relInputPath, id);
                    }
                    if (inferredIds.empty()) {
                        m_logger->debug(
                            "[线程 {}] [文件 {}] Agent 术语 {} 未提供 line_ids，且本地未在当前 chunk 匹配到出现位置，本轮不记录 occurrence。",
                            threadId,
                            wide2Ascii(relInputPath),
                            sourceTerm
                        );
                    }
                }

                if (!m_agentReconciling && !oldTarget.empty() && oldTarget != targetTerm && m_agentRewriteMode == "queue_retranslate") {
                    for (const auto& occurrence : entry["occurrences"]) {
                        ::enqueueAgentRewriteRequest(rewriteQueue, json{
                                {"file", occurrence.value("file", "")},
                                {"id", occurrence.value("id", -1)},
                                {"source_term", sourceTerm},
                                {"old_target", oldTarget},
                                {"new_target", targetTerm}
                            });
                    }
                }
            }

            if (!m_agentReconciling) {
                for (const auto& request : protocol.rewriteRequests) {
                    ::enqueueAgentRewriteRequest(rewriteQueue, request);
                }
            }
        });

    saveAgentFileNote(relInputPath, nextFileNote);
    backgroundText = nextBackgroundText;

    committedCount = 0;
    for (const auto& patch : sentencePatches) {
        patch.sentence->pre_translated_text = patch.dst;
        patch.sentence->translated_by = modelName;
        patch.sentence->complete = true;
        ++committedCount;
    }
    if (committedCount > 0) {
        m_completedSentences += committedCount;
        m_controller->updateBar(committedCount);
    }

    if (!protocol.termUpdates.empty()) {
        m_logger->debug(
            "[线程 {}] [文件 {}] Agent 术语账本本轮实际写入 {} / {} 条。",
            threadId,
            wide2Ascii(relInputPath),
            appliedTermUpdateCount,
            protocol.termUpdates.size()
        );
    }
}

struct AgentToolExecutionEnv {
    fs::path relInputPath;
    fs::path projectDir;
    fs::path inputDir;
    fs::path inputCacheDir;
    fs::path transCacheDir;
    bool needsCombining = false;
    int lookaheadLines = 0;
    int searchResultLimit = 0;
    bool allowCrossFileSearch = true;
    std::shared_mutex* transCacheMutex = nullptr;
    const std::vector<fs::path>* knownRelFiles = nullptr;
    const std::vector<fs::path>* dictionaryPaths = nullptr;
    std::optional<fs::path> projectInfoPath;
    std::function<json()> loadTermLedger;
    std::function<json(const fs::path&)> loadFileNote;
};

fs::path resolveAgentInputPath(const AgentToolExecutionEnv& env, const fs::path& relPath) {
    if (env.needsCombining && fs::exists(env.inputCacheDir / relPath)) {
        return env.inputCacheDir / relPath;
    }
    return env.inputDir / relPath;
}

absl::flat_hash_map<int, json> loadAgentCacheDstMap(const AgentToolExecutionEnv& env, const fs::path& targetRelPath) {
    absl::flat_hash_map<int, json> cacheMap;
    const fs::path cachePath = env.transCacheDir / targetRelPath;
    if (!fs::exists(cachePath)) {
        return cacheMap;
    }
    try {
        std::shared_lock<std::shared_mutex> lock(*env.transCacheMutex);
        std::ifstream ifs(cachePath, std::ios::binary);
        json cacheJson = json::parse(ifs);
        for (const auto& item : cacheJson) {
            const int index = item.value("index", -1);
            if (index >= 0) {
                cacheMap[index] = item;
            }
        }
    }
    catch (...) { }
    return cacheMap;
}

json collectLoadedDictionaryEntries(const AgentToolExecutionEnv& env) {
    json entries = json::array();
    if (env.dictionaryPaths == nullptr) {
        return entries;
    }
    for (const fs::path& dictPath : *env.dictionaryPaths) {
        try {
            const auto dictData = toml::uparse(dictPath);
            const fs::path relPath = fs::relative(dictPath, env.projectDir);
            const std::string relPathStr = wide2Ascii(relPath);
            if (!dictData.contains("gptDict")) {
                continue;
            }
            const auto& dictTbls = dictData.at("gptDict").as_array();
            for (const auto& el : dictTbls) {
                const std::string sourceTerm = el.contains("org") ? el.at("org").as_string() :
                    (el.contains("searchStr") ? el.at("searchStr").as_string() : "");
                const std::string targetTerm = el.contains("rep") ? el.at("rep").as_string() :
                    (el.contains("replaceStr") ? el.at("replaceStr").as_string() : "");
                const std::string note = el.contains("note") ? el.at("note").as_string() : "";
                if (sourceTerm.empty() && targetTerm.empty() && note.empty()) {
                    continue;
                }
                entries.push_back(json{
                        {"type", "gpt_dict"},
                        {"file", relPathStr},
                        {"source_term", sourceTerm},
                        {"target_term", targetTerm},
                        {"note", note}
                    });
            }
        }
        catch (...) { }
    }
    return entries;
}

std::vector<std::string> collectAgentDictionaryQueries(const json& arguments) {
    std::vector<std::string> queries;
    if (const auto it = arguments.find("queries"); it != arguments.end() && it->is_array()) {
        for (const auto& query : *it) {
            if (query.is_string()) {
                const std::string value = trimCopy(query.get<std::string>());
                if (!value.empty()) {
                    queries.push_back(value);
                }
            }
        }
    }
    if (queries.empty()) {
        const std::string query = trimCopy(arguments.value("query", ""));
        if (!query.empty()) {
            std::istringstream iss(query);
            for (std::string token; iss >> token;) {
                queries.push_back(token);
            }
            if (queries.empty()) {
                queries.push_back(query);
            }
        }
    }
    return queries;
}

json runAgentReadLinesTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(env.relInputPath)));
    const int start = std::max(0, arguments.value("start", 0));
    const int count = std::max(0, arguments.value("count", env.lookaheadLines));
    const bool includeSrc = arguments.value("include_src", true);
    const bool includeDst = arguments.value("include_dst", true);
    json result = { {"file", wide2Ascii(targetRelPath)}, {"lines", json::array()} };
    try {
        std::ifstream ifs(resolveAgentInputPath(env, targetRelPath), std::ios::binary);
        ordered_json inputJson = ordered_json::parse(ifs);
        const auto cacheMap = loadAgentCacheDstMap(env, targetRelPath);
        for (int i = start; i < (int)inputJson.size() && i < start + count; ++i) {
            json line = { {"id", i} };
            const auto cacheIt = cacheMap.find(i);
            if (inputJson[i].contains("name")) {
                line["name"] = inputJson[i]["name"];
            }
            if (inputJson[i].contains("names")) {
                line["names"] = inputJson[i]["names"];
            }
            if (includeSrc) {
                std::string src = inputJson[i].value("message", "");
                if (cacheIt != cacheMap.end()) {
                    src = cacheIt->second.value("pre_processed_text", src);
                }
                line["src"] = src;
            }
            if (includeDst && cacheIt != cacheMap.end()) {
                line["dst"] = cacheIt->second.value("pre_translated_text", cacheIt->second.value("translated_preview", ""));
            }
            result["lines"].push_back(std::move(line));
        }
    }
    catch (const std::exception& e) {
        result["error"] = e.what();
    }
    return result;
}

json runAgentListFilesTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const std::string pattern = str2Lower(arguments.value("pattern", ""));
    const int limit = sanitizeToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    json files = json::array();
    if (env.knownRelFiles == nullptr) {
        return json{ {"files", files} };
    }
    for (const auto& relFile : *env.knownRelFiles) {
        const std::string relFileStr = wide2Ascii(relFile);
        if (!pattern.empty() && !str2Lower(relFileStr).contains(pattern)) {
            continue;
        }
        files.push_back(relFileStr);
        if ((int)files.size() >= limit) {
            break;
        }
    }
    return json{ {"files", files} };
}

json runAgentGetDictionaryEntriesTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const int limit = sanitizeToolLimit(arguments.value("limit", 200), 200, 2000);
    std::vector<std::string> terms;
    if (const auto it = arguments.find("terms"); it != arguments.end() && it->is_array()) {
        for (const auto& term : *it) {
            if (term.is_string()) {
                const std::string value = trimCopy(term.get<std::string>());
                if (!value.empty()) {
                    terms.push_back(value);
                }
            }
        }
    }
    else if (const auto it = arguments.find("term"); it != arguments.end() && it->is_string()) {
        const std::string value = trimCopy(it->get<std::string>());
        if (!value.empty()) {
            terms.push_back(value);
        }
    }

    const json allEntries = collectLoadedDictionaryEntries(env);
    json entries = json::array();
    int matchedTotal = 0;
    for (const auto& entry : allEntries) {
        bool matched = terms.empty();
        if (!matched) {
            const std::string sourceTerm = entry.value("source_term", "");
            const std::string targetTerm = entry.value("target_term", "");
            matched = std::ranges::any_of(terms, [&](const std::string& term) {
                return sourceTerm == term || targetTerm == term;
            });
        }
        if (!matched) {
            continue;
        }
        ++matchedTotal;
        if ((int)entries.size() < limit) {
            entries.push_back(entry);
        }
    }

    return json{
        {"entries", entries},
        {"total_entries", (int)allEntries.size()},
        {"returned_entries", (int)entries.size()},
        {"matched_entries", matchedTotal},
        {"truncated", (int)entries.size() < matchedTotal}
    };
}

json runAgentSearchDictionaryTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const std::vector<std::string> queries = collectAgentDictionaryQueries(arguments);
    const std::string mode = str2Lower(arguments.value("mode", "fuzzy"));
    const int limit = sanitizeToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit, 200);
    json matches = json::array();
    const json allEntries = collectLoadedDictionaryEntries(env);
    for (const auto& entry : allEntries) {
        if ((int)matches.size() >= limit) {
            break;
        }
        const std::string sourceTerm = entry.value("source_term", "");
        const std::string targetTerm = entry.value("target_term", "");
        const std::string note = entry.value("note", "");
        const std::string haystack = str2Lower(sourceTerm + "\n" + targetTerm + "\n" + note + "\n" + entry.value("file", ""));
        const bool matched = std::ranges::any_of(queries, [&](const std::string& query) {
            if (query.empty()) {
                return false;
            }
            if (mode == "exact") {
                return sourceTerm == query || targetTerm == query;
            }
            return haystack.contains(str2Lower(query));
        });
        if (matched) {
            matches.push_back(entry);
        }
    }
    return json{ {"queries", queries}, {"mode", mode}, {"matches", matches} };
}

json runAgentGetProjectNoteTool(const AgentToolExecutionEnv& env, const json& arguments) {
    if (!env.projectInfoPath.has_value()) {
        return json{ {"available", false}, {"file", nullptr}, {"content", ""} };
    }
    const int maxChars = sanitizeToolLimit(arguments.value("max_chars", 20000), 20000, 120000);
    std::ifstream ifs(env.projectInfoPath.value(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const bool truncated = (int)content.size() > maxChars;
    return json{
        {"available", true},
        {"file", wide2Ascii(fs::relative(env.projectInfoPath.value(), env.projectDir))},
        {"content", truncated ? content.substr(0, maxChars) : content},
        {"truncated", truncated},
        {"total_chars", (int)content.size()}
    };
}

json runAgentSearchTextTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const std::string query = arguments.value("query", "");
    const std::string queryLower = str2Lower(query);
    const std::string scope = arguments.value("scope", "current_file");
    const int limit = sanitizeToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    std::vector<fs::path> targetFiles;

    if (scope != "current_file" && scope != "all_files" && scope != "specified_file") {
        return json{
            {"error", std::format("search_text.scope 非法: {}。允许值仅有 current_file|all_files|specified_file", scope)},
            {"allowed_scope", json::array({"current_file", "all_files", "specified_file"})}
        };
    }

    if (scope == "specified_file") {
        targetFiles.push_back(ascii2Wide(arguments.value("file", wide2Ascii(env.relInputPath))));
    }
    else if (scope == "all_files" && env.allowCrossFileSearch && env.knownRelFiles != nullptr) {
        targetFiles = *env.knownRelFiles;
    }
    else {
        targetFiles.push_back(env.relInputPath);
    }

    json matches = json::array();
    const json termLedger = env.loadTermLedger ? env.loadTermLedger() : json::object();
    for (const auto& item : termLedger.items()) {
        if ((int)matches.size() >= limit) {
            break;
        }
        const std::string term = item.key();
        const json& entry = item.value();
        const std::string targetTerm = entry.value("target_term", "");
        if (str2Lower(term).contains(queryLower) || str2Lower(targetTerm).contains(queryLower)) {
            matches.push_back(json{
                {"type", "term"},
                {"term", term},
                {"target_term", targetTerm},
                {"status", entry.value("status", "tentative")},
                {"note", entry.value("note", "")}
            });
        }
    }

    for (const auto& targetRelPath : targetFiles) {
        if ((int)matches.size() >= limit) {
            break;
        }
        try {
            std::ifstream ifs(resolveAgentInputPath(env, targetRelPath), std::ios::binary);
            ordered_json inputJson = ordered_json::parse(ifs);
            const auto cacheMap = loadAgentCacheDstMap(env, targetRelPath);
            for (const auto& [index, item] : inputJson | std::views::enumerate) {
                if ((int)matches.size() >= limit) {
                    break;
                }
                const std::string src = item.value("message", "");
                std::string dst;
                if (const auto it = cacheMap.find((int)index); it != cacheMap.end()) {
                    dst = it->second.value("translated_preview", it->second.value("pre_translated_text", ""));
                }
                if (!str2Lower(src).contains(queryLower) && !str2Lower(dst).contains(queryLower)) {
                    continue;
                }
                matches.push_back(json{
                    {"type", "line"},
                    {"file", wide2Ascii(targetRelPath)},
                    {"id", (int)index},
                    {"src", src},
                    {"dst", dst}
                });
            }

            if ((int)matches.size() >= limit || !env.loadFileNote) {
                continue;
            }
            const json fileNote = env.loadFileNote(targetRelPath);
            if (!fileNote.empty() && str2Lower(fileNote.dump()).contains(queryLower)) {
                matches.push_back(json{
                    {"type", "file_note"},
                    {"file", wide2Ascii(targetRelPath)},
                    {"note", fileNote}
                });
            }
        }
        catch (...) { }
    }

    return json{ {"matches", matches} };
}

json runAgentGetTermTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const std::string term = arguments.value("term", "");
    const json ledger = env.loadTermLedger ? env.loadTermLedger() : json::object();
    return json{ {"term", term}, {"entry", ledger.contains(term) ? ledger.at(term) : json(nullptr)} };
}

json runAgentGetFileNoteTool(const AgentToolExecutionEnv& env, const json& arguments) {
    const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(env.relInputPath)));
    return json{
        {"file", wide2Ascii(targetRelPath)},
        {"note", env.loadFileNote ? env.loadFileNote(targetRelPath) : json::object()}
    };
}

json executeAgentToolCalls(const AgentToolExecutionEnv& env, const std::vector<AgentToolCallRequest>& calls) {
    json toolResults = json::array();
    for (const auto& call : calls) {
        json result = { {"id", call.id}, {"name", call.name} };
        try {
            if (call.name == "list_files") {
                result["result"] = runAgentListFilesTool(env, call.arguments);
            }
            else if (call.name == "read_lines") {
                result["result"] = runAgentReadLinesTool(env, call.arguments);
            }
            else if (call.name == "search_text") {
                result["result"] = runAgentSearchTextTool(env, call.arguments);
            }
            else if (call.name == "search_dictionary") {
                result["result"] = runAgentSearchDictionaryTool(env, call.arguments);
            }
            else if (call.name == "get_dictionary_entries") {
                result["result"] = runAgentGetDictionaryEntriesTool(env, call.arguments);
            }
            else if (call.name == "get_term") {
                result["result"] = runAgentGetTermTool(env, call.arguments);
            }
            else if (call.name == "get_file_note") {
                result["result"] = runAgentGetFileNoteTool(env, call.arguments);
            }
            else if (call.name == "get_project_note") {
                result["result"] = runAgentGetProjectNoteTool(env, call.arguments);
            }
            else {
                result["error"] = std::format("未知工具: {}", call.name);
            }
        }
        catch (const std::exception& e) {
            result["error"] = e.what();
        }
        toolResults.push_back(std::move(result));
    }
    return toolResults;
}

bool NormalJsonTranslator::translateBatchAgent(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText, int threadId) {
    // Agent 模式复用外层 batch 调度，但单个 chunk 内可能进行多轮交互：
    // 先发起工具调用，再按需压缩上下文，最后提交通过校验的 `commit`。
    for (Sentence* se : batch) {
        if (se->pre_processed_text.empty()) {
            se->complete = true;
            ++m_completedSentences;
            m_controller->updateBar();
        }
    }

    // 工具实现已经搬到文件级自由函数，translateBatchAgent 这里只保留状态机和调度。
    AgentToolExecutionEnv toolEnv{
        .relInputPath = relInputPath,
        .projectDir = m_projectDir,
        .inputDir = m_inputDir,
        .inputCacheDir = m_inputCacheDir,
        .transCacheDir = m_transCacheDir,
        .needsCombining = m_needsCombining,
        .lookaheadLines = m_agentLookaheadLines,
        .searchResultLimit = m_agentSearchResultLimit,
        .allowCrossFileSearch = m_agentAllowCrossFileSearch,
        .transCacheMutex = &m_transCacheMutex,
        .knownRelFiles = &m_agentKnownRelFiles,
        .dictionaryPaths = &m_agentDictionaryPaths,
        .projectInfoPath = m_agentProjectInfoPath,
        .loadTermLedger = [&]() { return loadAgentTermLedger(); },
        .loadFileNote = [&](const fs::path& targetRelPath) { return loadAgentFileNote(targetRelPath); }
    };

    int retryCount = 0;
    while (retryCount == 0 || retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return false;
        }

        std::vector<Sentence*> pending = ::collectAgentPendingSentences(batch);
        if (pending.empty()) {
            return true;
        }

        if (m_smartRetry && retryCount == 2 && pending.size() > 1) {
            m_logger->warn("[线程 {}] [文件 {}] Agent 开始拆分批次进行重试...", threadId, wide2Ascii(relInputPath));

            const size_t mid = pending.size() / 2;
            std::span<Sentence*> pendingSpan(pending);
            std::span<Sentence*> firstHalf = pendingSpan.subspan(0, mid);
            std::span<Sentence*> secondHalf = pendingSpan.subspan(mid);

            bool firstOk = translateBatchAgent(relInputPath, firstHalf, backgroundText, threadId);
            bool secondOk = translateBatchAgent(relInputPath, secondHalf, backgroundText, threadId);
            return firstOk && secondOk;
        }
        else if (m_smartRetry && retryCount == 3) {
            m_logger->warn("[线程 {}] [文件 {}] Agent 清空上下文后再次尝试...", threadId, wide2Ascii(relInputPath));
            backgroundText.clear();
        }

        m_logger->debug("[线程 {}] [文件 {}] Agent chunk 开始，当前待提交 {} 句，允许最多 {} 轮。", threadId, wide2Ascii(relInputPath), pending.size(), m_agentMaxTurnsPerChunk);
        json messages = buildAgentBaseMessages(relInputPath, batch, backgroundText);
        bool compactRequested = false;
        const std::string logBlock = buildAgentLogBlock(relInputPath, batch, backgroundText);
        m_logger->info("[线程 {}] [文件 {}] Agent 开始翻译，当前 chunk {}-{}，待提交 {} 句，最多 {} 轮:\n{}",
            threadId, wide2Ascii(relInputPath), pending.front()->index, pending.back()->index, pending.size(), m_agentMaxTurnsPerChunk, logBlock);
        if (m_logger->should_log(spdlog::level::trace)) {
            m_logger->trace(
                "[线程 {}] [文件 {}] Agent 初始请求消息（实际发送给模型）:\n{}",
                threadId,
                wide2Ascii(relInputPath),
                truncateForAgentLog(messages.dump(2), 20000)
            );
        }

        // 这里开始才是“单个 chunk 的模型多轮循环”。
        // 最典型的链路是：准备 messages -> 调模型 -> 执行工具/压缩上下文/commit -> 进入下一轮或结束。
        for (int turn = 0; turn < m_agentMaxTurnsPerChunk; ++turn) {
            const size_t messageChars = ::approximateMessagesChars(messages);
            m_logger->debug("[线程 {}] [文件 {}] Agent 第 {}/{} 轮，请求上下文约 {} 字符。",
                threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, messageChars);
            if (m_logger->should_log(spdlog::level::trace)) {
                m_logger->trace(
                    "[线程 {}] [文件 {}] Agent 第 {}/{} 轮请求消息（实际发送给模型）:\n{}",
                    threadId,
                    wide2Ascii(relInputPath),
                    turn + 1,
                    m_agentMaxTurnsPerChunk,
                    truncateForAgentLog(messages.dump(2), 20000)
                );
            }

            if (messageChars > (size_t)m_agentHardContextChars) {
                m_logger->warn("[线程 {}] [文件 {}] Agent 上下文超过 hardContextChars，回退到最近摘要重建消息。", threadId, wide2Ascii(relInputPath));
                messages = buildAgentBaseMessages(relInputPath, batch, backgroundText);
                compactRequested = false;
            }
            else if (!compactRequested && messageChars > (size_t)m_agentSoftContextChars) {
                m_logger->info("[线程 {}] [文件 {}] Agent 上下文接近上限，要求模型先压缩上下文。", threadId, wide2Ascii(relInputPath));
                messages.push_back({
                    {"role", "user"},
                    {"content", "Context is close to the limit. Return a compact_context action only. Do not call tools or commit in this turn."}
                });
                compactRequested = true;
            }

            const std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
            if (!apiOpt.has_value()) {
                throw std::runtime_error("没有可用的API Key了");
            }
            const TranslationApi& currentApi = apiOpt.value();

            json payload = { {"messages", messages} };

            // 整个 translateBatchAgent 里，真正向模型发请求的地方只有这里。
            ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);

            if (!checkResponse(
                response, m_apiPool, currentApi, relInputPath, m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
            )) {
                break;
            }

            AgentProtocolResponse protocol;
            try {
                protocol = parseAgentTextResponse(response.content);
            }
            catch (const std::exception& e) {
                ++retryCount;
                m_logger->warn("[线程 {}] [文件 {}] Agent 响应解析失败，第 {} 次重试。原始响应: {}\n错误: {}",
                    threadId, wide2Ascii(relInputPath), retryCount, response.content, e.what());
                break;
            }

            m_logger->info("[线程 {}] [文件 {}] Agent 第 {}/{} 轮返回 action='{}'。",
                threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, protocol.action);
            if (m_logger->should_log(spdlog::level::trace) && !response.content.empty()) {
                m_logger->trace(
                    "[线程 {}] [文件 {}] Agent 第 {}/{} 轮原始响应:\n{}",
                    threadId,
                    wide2Ascii(relInputPath),
                    turn + 1,
                    m_agentMaxTurnsPerChunk,
                    truncateForAgentLog(response.content, 20000)
                );
            }

            if (protocol.action == "tool_calls" && !protocol.calls.empty()) {
                m_logger->info(
                    "[线程 {}] [文件 {}] Agent 请求 {} 个工具调用: {}。",
                    threadId,
                    wide2Ascii(relInputPath),
                    protocol.calls.size(),
                    formatToolCallSummary(protocol.calls)
                );
                if (m_logger->should_log(spdlog::level::debug)) {
                    m_logger->debug(
                        "[线程 {}] [文件 {}] Agent 工具调用明细:\n{}",
                        threadId,
                        wide2Ascii(relInputPath),
                        formatToolCallRequestsForLog(protocol.calls)
                    );
                }
                const json toolResults = ::executeAgentToolCalls(toolEnv, protocol.calls);
                compactRequested = false;
                m_logger->info(
                    "[线程 {}] [文件 {}] Agent 工具执行完成，返回 {} 项结果，继续下一轮。",
                    threadId,
                    wide2Ascii(relInputPath),
                    toolResults.size()
                );
                if (m_logger->should_log(spdlog::level::debug)) {
                    m_logger->debug(
                        "[线程 {}] [文件 {}] Agent 工具返回结果:\n{}",
                        threadId,
                        wide2Ascii(relInputPath),
                        truncateForAgentLog(toolResults.dump(2), 12000)
                    );
                }
                // 工具调用分支不会直接完成 chunk，而是把工具结果回填给下一轮模型继续推理。
                messages.push_back({ {"role", "assistant"}, {"content", response.content} });
                messages.push_back({
                    {"role", "user"},
                    {"content", std::string("Tool results:\n```json\n") + toolResults.dump(2) + "\n```"}
                });
                continue;
            }

            if (protocol.action == "compact_context") {
                // 压缩分支只更新 rolling context / file note，不提交任何句子。
                if (!protocol.rollingContext.empty()) {
                    backgroundText = protocol.rollingContext;
                }
                else if (!response.content.empty()) {
                    backgroundText = response.content;
                }
                m_logger->info("[线程 {}] [文件 {}] Agent 已压缩上下文，摘要长度 {} 字符。", threadId, wide2Ascii(relInputPath), backgroundText.size());
                messages = buildAgentBaseMessages(relInputPath, batch, backgroundText);
                compactRequested = false;
                continue;
            }

            if (protocol.action == "commit") {
                // commit 成功后，这个 chunk 的多轮循环立即结束，控制权返回外层批处理调度。
                if (m_logger->should_log(spdlog::level::debug)) {
                    m_logger->debug(
                        "[线程 {}] [文件 {}] Agent commit 内容:\ntranslations={}\nterm_updates={}\nrewrite_requests={}\nfile_note_patch={}\nrolling_context={}",
                        threadId,
                        wide2Ascii(relInputPath),
                        truncateForAgentLog(protocol.translations.dump(2), 12000),
                        truncateForAgentLog(protocol.termUpdates.dump(2), 12000),
                        truncateForAgentLog(protocol.rewriteRequests.dump(2), 12000),
                        truncateForAgentLog(protocol.fileNotePatch.dump(2), 12000),
                        truncateForAgentLog(protocol.rollingContext, 12000)
                    );
                }
                int committedCount = 0;
                try {
                    applyAgentCommit(relInputPath, batch, backgroundText, threadId, protocol, currentApi.modelName, committedCount);
                }
                catch (const std::exception& e) {
                    ++retryCount;
                    m_logger->warn("[线程 {}] [文件 {}] Agent commit 校验失败，第 {} 次重试。错误: {}",
                        threadId, wide2Ascii(relInputPath), retryCount, e.what());
                    break;
                }
                m_logger->info("[线程 {}] [文件 {}] Agent commit 成功，提交 {} 句，术语更新 {} 条，重翻请求 {} 条，新的 rolling_context 长度 {} 字符。",
                    threadId, wide2Ascii(relInputPath), committedCount, protocol.termUpdates.size(), protocol.rewriteRequests.size(), protocol.rollingContext.size());
                return true;
            }

            ++retryCount;
            m_logger->warn("[线程 {}] [文件 {}] Agent 返回未知 action '{}'，第 {} 次重试。", threadId, wide2Ascii(relInputPath), protocol.action, retryCount);
            break;
        }
    }

    size_t failedCount = 0;
    for (Sentence* se : batch | std::views::filter([](const Sentence* s) { return !s->complete; })) {
        ++failedCount;
        se->pre_translated_text = "(Failed to translate)" + se->pre_processed_text;
        se->complete = true;
        ++m_completedSentences;
        m_controller->updateBar();
    }
    m_logger->error("[线程 {}] [文件 {}] Agent 批次在 {} 次重试后彻底失败，共翻译 {} / {} 句。",
        threadId, wide2Ascii(relInputPath), retryCount, batch.size() - failedCount, batch.size());
    return false;
}

void NormalJsonTranslator::runAgentFinalReconcile() {
    if (!m_agentEnabled || !m_agentFinalReconcileSingleThread) {
        return;
    }

    json rewriteQueue = loadAgentRewriteQueue();
    if (!rewriteQueue.is_array() || rewriteQueue.empty()) {
        return;
    }

    m_logger->info("Agent 模式开始最终单线程 reconcile，共 {} 条重翻请求。", rewriteQueue.size());
    m_agentReconciling = true;

    absl::btree_map<fs::path, std::vector<int>> fileToIds;
    for (const auto& request : rewriteQueue) {
        if (!request.is_object()) {
            continue;
        }
        const std::string file = request.value("file", "");
        const int id = request.value("id", -1);
        if (!file.empty() && id >= 0) {
            fileToIds[ascii2Wide(file)].push_back(id);
        }
    }

    for (auto& [filePath, ids] : fileToIds) {
        std::ranges::sort(ids);
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        const fs::path cachePath = m_transCacheDir / filePath;
        if (fs::exists(cachePath)) {
            try {
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                std::ifstream ifs(cachePath, std::ios::binary);
                json cacheJson = json::parse(ifs);
                ifs.close();
                json filtered = json::array();
                if (cacheJson.is_array()) {
                    for (const auto& item : cacheJson) {
                        if (!std::ranges::contains(ids, item.value("index", -1))) {
                            filtered.push_back(item);
                        }
                    }
                }
                else {
                    filtered = cacheJson;
                }
                std::ofstream ofs(cachePath, std::ios::binary);
                ofs << filtered.dump(2);
            }
            catch (const std::exception& e) {
                m_logger->error("reconcile 清理缓存 {} 失败: {}", wide2Ascii(filePath), e.what());
                continue;
            }
        }

        try {
            m_logger->info("Agent reconcile 重翻文件 {}，目标句数 {}。", wide2Ascii(filePath), ids.size());
            processFile(filePath, 0);
        }
        catch (const std::exception& e) {
            m_logger->error("reconcile 重翻文件 {} 失败: {}", wide2Ascii(filePath), e.what());
        }
    }

    mutateAgentState([&](json&, json&, json& queue) {
        queue = json::array();
    });
    m_agentReconciling = false;
    m_logger->info("Agent 模式最终单线程 reconcile 完成。");
}
