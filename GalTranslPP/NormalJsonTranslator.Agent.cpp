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

namespace {
    struct AgentToolCallRequest {
        std::string id;
        std::string name;
        json arguments = json::object();
    };

    struct AgentProtocolResponse {
        std::string action;
        std::vector<AgentToolCallRequest> calls;
        json translations = json::array();
        json termUpdates = json::array();
        json rewriteRequests = json::array();
        json fileNotePatch = json::object();
        json summary = json::object();
        std::string rawContent;
    };

    std::string nowTimestampString() {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return std::to_string(now);
    }

    json loadJsonFileOrDefault(const fs::path& path, const json& fallback = json::object()) {
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

    void saveJsonFilePretty(const fs::path& path, const json& value) {
        createParent(path);
        std::ofstream ofs(path, std::ios::binary);
        ofs << value.dump(2);
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

        const size_t jsonStart = text.find('{');
        const size_t jsonEnd = text.rfind('}');
        if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
            return std::nullopt;
        }

        try {
            return json::parse(text.substr(jsonStart, jsonEnd - jsonStart + 1));
        }
        catch (...) {
            return std::nullopt;
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
                parsed.name = call.value("name", "");
                if (const auto argIt = call.find("arguments"); argIt != call.end()) {
                    parsed.arguments = *argIt;
                }
                result.calls.push_back(std::move(parsed));
            }
        }
        if (const auto it = payload.find("translations"); it != payload.end() && it->is_array()) {
            result.translations = *it;
        }
        if (const auto it = payload.find("term_updates"); it != payload.end() && it->is_array()) {
            result.termUpdates = *it;
        }
        if (const auto it = payload.find("rewrite_requests"); it != payload.end() && it->is_array()) {
            result.rewriteRequests = *it;
        }
        if (const auto it = payload.find("file_note_patch"); it != payload.end() && it->is_object()) {
            result.fileNotePatch = *it;
        }
        if (const auto it = payload.find("summary"); it != payload.end() && it->is_object()) {
            result.summary = *it;
        }
        return result;
    }

    AgentProtocolResponse parseAgentApiResponse(const ApiResponse& response) {
        if (response.hasToolCalls) {
            AgentProtocolResponse result;
            result.action = "tool_calls";
            result.rawContent = response.content;
            for (const auto& toolCall : response.toolCalls) {
                if (!toolCall.is_object()) {
                    continue;
                }
                AgentToolCallRequest parsed;
                parsed.id = toolCall.value("id", std::format("tool_{}", result.calls.size()));
                if (const auto funcIt = toolCall.find("function"); funcIt != toolCall.end() && funcIt->is_object()) {
                    parsed.name = funcIt->value("name", "");
                    if (const auto argsIt = funcIt->find("arguments"); argsIt != funcIt->end()) {
                        if (argsIt->is_string()) {
                            const std::string argsStr = argsIt->get<std::string>();
                            if (const auto parsedArgs = tryParseJsonEnvelope(argsStr)) {
                                parsed.arguments = *parsedArgs;
                            }
                        }
                        else {
                            parsed.arguments = *argsIt;
                        }
                    }
                }
                result.calls.push_back(std::move(parsed));
            }
            return result;
        }

        return parseAgentTextResponse(response.content);
    }

    bool shouldFallbackFromNativeFunctionCalling(const ApiResponse& response) {
        if (response.success) {
            return false;
        }
        std::string lower = response.content;
        str2LowerInplace(lower);
        return response.statusCode == 400 || response.statusCode == 404 || response.statusCode == 422
            || lower.contains("tool_choice")
            || lower.contains("tool_calls")
            || lower.contains("function")
            || lower.contains("tools");
    }

    json buildAgentNativeTools() {
        const json commonString = { {"type", "string"} };
        const json commonInt = { {"type", "integer"} };
        return json::array({
            {
                {"type", "function"},
                {"function", {
                    {"name", "list_files"},
                    {"description", "List available project files for cross-file lookup."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"pattern", commonString},
                            {"limit", commonInt}
                        }},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "read_lines"},
                    {"description", "Read a slice of source and cached translation lines from a file."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"file", commonString},
                            {"start", commonInt},
                            {"count", commonInt},
                            {"include_src", { {"type", "boolean"} }},
                            {"include_dst", { {"type", "boolean"} }}
                        }},
                        {"required", json::array({"file", "start", "count"})},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "search_text"},
                    {"description", "Search source text, cached translations, summaries, or term ledger."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"query", commonString},
                            {"scope", { {"type", "string"}, {"enum", json::array({"current_file", "all_files", "specified_file"})} }},
                            {"file", commonString},
                            {"limit", commonInt}
                        }},
                        {"required", json::array({"query", "scope"})},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "get_term"},
                    {"description", "Get the current term ledger record for a term."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", { {"term", commonString} }},
                        {"required", json::array({"term"})},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "get_file_note"},
                    {"description", "Read the saved summary and unresolved notes of a file."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", { {"file", commonString} }},
                        {"required", json::array({"file"})},
                        {"additionalProperties", false}
                    }}
                }}
            }
        });
    }

    int sanitizeToolLimit(int requested, int fallback, int maxLimit = 200) {
        if (requested <= 0) {
            return fallback;
        }
        return std::min(requested, maxLimit);
    }

    void replacePromptToken(std::string& text, const std::string& token, const std::string& value) {
        if (token.empty()) {
            return;
        }
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.replace(pos, token.size(), value);
            pos += value.size();
        }
    }
}

// 只读运行状态加载函数，主要给恢复调度和启动检查使用。
// 它通常每次运行只会调用少数几次，而不是按句子频繁调用。
json NormalJsonTranslator::loadAgentRunState() {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    return loadJsonFileOrDefault(m_agentRunStatePath, json::object());
}

// 只读术语账本加载函数，主要供工具执行和提示词重建使用。
// 单个 chunk 在一次多轮循环里可能会反复读取几次。
json NormalJsonTranslator::loadAgentTermLedger() {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    return loadJsonFileOrDefault(m_agentTermLedgerPath, json::object());
}

// 只读重翻队列加载函数，主要给恢复流程和最终 reconcile 使用。
json NormalJsonTranslator::loadAgentRewriteQueue() {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    return loadJsonFileOrDefault(m_agentRewriteQueuePath, json::array());
}

// `file_note` 是按输入文件保存的摘要。
// 多数 agent 轮次在进入前都会读一次，在 `compact_context` 或 `commit` 后可能再写回一次。
json NormalJsonTranslator::loadAgentFileNote(const fs::path& targetRelPath) {
    std::lock_guard<std::mutex> lock(m_agentFileNotesMutex);
    return loadJsonFileOrDefault(buildAgentFileNotePath(m_agentFileNotesDir, targetRelPath), json::object());
}

void NormalJsonTranslator::saveAgentFileNote(const fs::path& targetRelPath, const json& note) {
    std::lock_guard<std::mutex> lock(m_agentFileNotesMutex);
    saveJsonFilePretty(buildAgentFileNotePath(m_agentFileNotesDir, targetRelPath), note);
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
    json state = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
    if (!state.contains("files") || !state["files"].is_array()) {
        state["files"] = json::array();
    }
    const std::string relPathStr = wide2Ascii(relInputPath);
    auto it = std::ranges::find_if(state["files"], [&](const json& item) {
        return item.value("file", "") == relPathStr;
    });
    if (it == state["files"].end()) {
        state["files"].push_back({
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
    saveJsonFilePretty(m_agentRunStatePath, state);
}

// 共享 Agent 状态文件的强串行修改入口。
// `commit` 路径必须走这里，避免多个 worker 在线程并发时发生 `load/save` 覆盖。
void NormalJsonTranslator::mutateAgentState(const std::function<void(json& runState, json& termLedger, json& rewriteQueue)>& mutator) {
    std::lock_guard<std::mutex> lock(m_agentStateMutex);
    json runState = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
    json termLedger = loadJsonFileOrDefault(m_agentTermLedgerPath, json::object());
    json rewriteQueue = loadJsonFileOrDefault(m_agentRewriteQueuePath, json::array());
    mutator(runState, termLedger, rewriteQueue);
    saveJsonFilePretty(m_agentRunStatePath, runState);
    saveJsonFilePretty(m_agentTermLedgerPath, termLedger);
    saveJsonFilePretty(m_agentRewriteQueuePath, rewriteQueue);
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

    // 下面这些 lambda 都只是当前函数内部复用的小工具。
    // 它们本身不会向模型发请求，职责大致分成三类：
    // 1. 输入/缓存读取辅助
    // 2. 模型可调用的只读工具实现
    // 3. commit 校验与落盘辅助
    // 真正会触发模型请求的地方只有后面 turn 循环里的 performApiRequest。

    // 返回当前 chunk 里还未完成的句子。
    // 构造消息、校验 commit、失败重试时都会重复调用它。
    auto currentChunk = [&]() {
        return batch
            | std::views::filter([](const Sentence* se) { return !se->complete; })
            | std::ranges::to<std::vector>();
    };

    // 把相对路径解析成实际应读取的输入文件。
    // splitFile 场景下优先读输入缓存里的 part 文件，否则读原始输入目录。
    auto resolveInputPath = [&](const fs::path& relPath) {
        if (m_needsCombining && fs::exists(m_inputCacheDir / relPath)) {
            return m_inputCacheDir / relPath;
        }
        return m_inputDir / relPath;
    };

    // 把某个文件当前已有的缓存译文读成内存 map，供只读工具和搜索逻辑复用。
    // 这里只读 trans_cache，不会写回任何状态。
    auto loadCacheDstMap = [&](const fs::path& targetRelPath) {
        absl::flat_hash_map<int, json> cacheMap;
        const fs::path cachePath = m_transCacheDir / targetRelPath;
        if (!fs::exists(cachePath)) {
            return cacheMap;
        }
        try {
            std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
            std::ifstream ifs(cachePath, std::ios::binary);
            json cacheJson = json::parse(ifs);
            for (const auto& item : cacheJson) {
                const int index = item.value("index", -1);
                if (index >= 0) {
                    cacheMap[index] = item;
                }
            }
        }
        catch (...) {}
        return cacheMap;
    };

    // 下面开始是“模型工具调用”在本地侧的只读实现。
    // 模型可以请求这些工具，但它们都不能直接写文件；真正落盘必须走 commit。
    auto readLinesTool = [&](const json& arguments) {
        const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(relInputPath)));
        const int start = std::max(0, arguments.value("start", 0));
        const int count = std::max(0, arguments.value("count", m_agentLookaheadLines));
        const bool includeSrc = arguments.value("include_src", true);
        const bool includeDst = arguments.value("include_dst", true);

        json result = {
            {"file", wide2Ascii(targetRelPath)},
            {"lines", json::array()}
        };

        try {
            std::ifstream ifs(resolveInputPath(targetRelPath), std::ios::binary);
            ordered_json inputJson = ordered_json::parse(ifs);
            const auto cacheMap = loadCacheDstMap(targetRelPath);
            for (int i = start; i < (int)inputJson.size() && i < start + count; ++i) {
                json line = { {"id", i} };
                if (inputJson[i].contains("name")) {
                    line["name"] = inputJson[i]["name"];
                }
                if (inputJson[i].contains("names")) {
                    line["names"] = inputJson[i]["names"];
                }
                if (includeSrc) {
                    line["src"] = inputJson[i].value("message", "");
                }
                if (includeDst) {
                    if (const auto it = cacheMap.find(i); it != cacheMap.end()) {
                        line["dst"] = it->second.value("translated_preview", it->second.value("pre_translated_text", ""));
                    }
                }
                result["lines"].push_back(std::move(line));
            }
        }
        catch (const std::exception& e) {
            result["error"] = e.what();
        }
        return result;
    };

    // `list_files`：给模型一个可用文件列表，方便后续跨文件定位。
    auto listFilesTool = [&](const json& arguments) {
        const std::string pattern = str2Lower(arguments.value("pattern", ""));
        const int limit = sanitizeToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
        json files = json::array();
        for (const auto& relFile : m_agentKnownRelFiles) {
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
    };

    // `search_text`：统一搜索术语账本、原文、缓存译文以及 file note。
    // 这是跨 chunk / 跨文件补上下文时最常用的工具之一。
    auto searchTextTool = [&](const json& arguments) {
        const std::string query = arguments.value("query", "");
        const std::string queryLower = str2Lower(query);
        const std::string scope = arguments.value("scope", "current_file");
        const int limit = sanitizeToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
        std::vector<fs::path> targetFiles;

        if (scope == "specified_file") {
            targetFiles.push_back(ascii2Wide(arguments.value("file", wide2Ascii(relInputPath))));
        }
        else if (scope == "all_files" && m_agentAllowCrossFileSearch) {
            targetFiles = m_agentKnownRelFiles;
        }
        else {
            targetFiles.push_back(relInputPath);
        }

        json matches = json::array();
        auto pushMatch = [&](json&& item) {
            if ((int)matches.size() < limit) {
                matches.push_back(std::move(item));
            }
        };

        const json termLedger = loadAgentTermLedger();
        for (const auto& item : termLedger.items()) {
            if ((int)matches.size() >= limit) {
                break;
            }
            const std::string term = item.key();
            const json& entry = item.value();
            const std::string targetTerm = entry.value("target_term", "");
            if (str2Lower(term).contains(queryLower) || str2Lower(targetTerm).contains(queryLower)) {
                pushMatch(json{
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
                std::ifstream ifs(resolveInputPath(targetRelPath), std::ios::binary);
                ordered_json inputJson = ordered_json::parse(ifs);
                const auto cacheMap = loadCacheDstMap(targetRelPath);
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
                    pushMatch(json{
                        {"type", "line"},
                        {"file", wide2Ascii(targetRelPath)},
                        {"id", (int)index},
                        {"src", src},
                        {"dst", dst}
                    });
                }

                if ((int)matches.size() >= limit) {
                    continue;
                }
                const json fileNote = loadAgentFileNote(targetRelPath);
                if (!fileNote.empty() && str2Lower(fileNote.dump()).contains(queryLower)) {
                    pushMatch(json{
                        {"type", "file_note"},
                        {"file", wide2Ascii(targetRelPath)},
                        {"note", fileNote}
                    });
                }
            }
            catch (...) {}
        }

        return json{ {"matches", matches} };
    };

    // `get_term`：读取某个术语在全局账本中的当前记录。
    auto getTermTool = [&](const json& arguments) {
        const std::string term = arguments.value("term", "");
        const json ledger = loadAgentTermLedger();
        return json{
            {"term", term},
            {"entry", ledger.contains(term) ? ledger.at(term) : json(nullptr)}
        };
    };

    // `get_file_note`：读取文件级摘要，帮助模型拿到长程剧情/场景信息。
    auto getFileNoteTool = [&](const json& arguments) {
        const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(relInputPath)));
        return json{
            {"file", wide2Ascii(targetRelPath)},
            {"note", loadAgentFileNote(targetRelPath)}
        };
    };

    // 按工具名把模型请求分发到上面的只读工具实现，并收集执行结果。
    // 这里只执行工具，不会再次请求模型。
    auto executeToolCalls = [&](const std::vector<AgentToolCallRequest>& calls) {
        json toolResults = json::array();
        for (const auto& call : calls) {
            json result = {
                {"id", call.id},
                {"name", call.name}
            };
            try {
                if (call.name == "list_files") {
                    result["result"] = listFilesTool(call.arguments);
                }
                else if (call.name == "read_lines") {
                    result["result"] = readLinesTool(call.arguments);
                }
                else if (call.name == "search_text") {
                    result["result"] = searchTextTool(call.arguments);
                }
                else if (call.name == "get_term") {
                    result["result"] = getTermTool(call.arguments);
                }
                else if (call.name == "get_file_note") {
                    result["result"] = getFileNoteTool(call.arguments);
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
    };

    // 从全局术语账本里截一小段摘要塞进提示词，减少模型每轮都去查工具的次数。
    auto termLedgerExcerpt = [&]() {
        const json ledger = loadAgentTermLedger();
        json excerpt = json::array();
        for (const auto& item : ledger.items()) {
            const std::string term = item.key();
            const json& entry = item.value();
            excerpt.push_back({
                {"term", term},
                {"target_term", entry.value("target_term", "")},
                {"status", entry.value("status", "tentative")},
                {"category", entry.value("category", "")}
            });
            if ((int)excerpt.size() >= m_agentSearchResultLimit) {
                break;
            }
        }
        return excerpt.dump(2);
    };

    // 组装“一轮模型请求”的基础消息。
    // 这里本身不发请求，只负责把 chunk、rolling context、file note、术语摘要等拼好。
    auto buildBaseMessages = [&](const std::string& rollingSummary, const json& fileNote) {
        absl::btree_map<int, Sentence*> id2SentenceMap;
        std::string inputBlock;
        fillBlockAndMap(batch, id2SentenceMap, inputBlock, m_transEngine);

        const std::string schemaDescription =
            "{"
            "\"schema\":\"gpp-agent-v1\","
            "\"action\":\"tool_calls|commit|compact_context\","
            "\"calls\":[],"
            "\"translations\":[],"
            "\"term_updates\":[],"
            "\"rewrite_requests\":[],"
            "\"file_note_patch\":{},"
            "\"summary\":{}"
            "}";

        std::string userPrompt = m_agentUserPrompt;
        replacePromptToken(userPrompt, "[AgentCurrentFile]", wide2Ascii(relInputPath));
        replacePromptToken(userPrompt, "[AgentChunkIdRange]", std::format("{}-{}", batch.front()->index, batch.back()->index));
        replacePromptToken(userPrompt, "[AgentRollingContext]", rollingSummary.empty() ? "None" : rollingSummary);
        replacePromptToken(userPrompt, "[AgentFileNote]", fileNote.empty() ? "None" : fileNote.dump(2));
        replacePromptToken(userPrompt, "[AgentKnownTerms]", termLedgerExcerpt());
        replacePromptToken(userPrompt, "[AgentCurrentChunkTsv]", std::string("NAME\tSRC\tID\n") + inputBlock);
        replacePromptToken(userPrompt, "[AgentSchemaDescription]", schemaDescription);
        replacePromptToken(userPrompt, "[AgentTurnGuidance]",
            "If context is near limit, return compact_context only. "
            "Otherwise either call read-only tools or return a commit that covers every current chunk id exactly once.");

        const std::string& agentSystemPrompt = m_agentSystemPrompt.empty() ? m_systemPrompt : m_agentSystemPrompt;
        return json::array({
            {{"role", "system"}, {"content", agentSystemPrompt}},
            {{"role", "user"}, {"content", userPrompt}}
        });
    };

    // 粗略估算消息大小，用于决定是否先要求模型压缩上下文。
    auto approximateMessagesChars = [](const json& messages) {
        size_t total = 0;
        for (const auto& item : messages) {
            total += item.dump().size();
        }
        return total;
    };

    // 合并模型给出的 file_note_patch。
    auto mergeFileNotePatch = [&](json& note, const json& patch) {
        if (!patch.is_object()) {
            return;
        }
        for (const auto& item : patch.items()) {
            note[item.key()] = item.value();
        }
    };

    // 给术语账本追加出现位置，便于后续术语改判时回溯受影响句子。
    auto appendOccurrence = [](json& entry, const fs::path& file, int id) {
        if (!entry.contains("occurrences") || !entry["occurrences"].is_array()) {
            entry["occurrences"] = json::array();
        }
        const std::string fileStr = wide2Ascii(file);
        const bool exists = std::ranges::any_of(entry["occurrences"], [&](const json& occurrence) {
            return occurrence.value("file", "") == fileStr && occurrence.value("id", -1) == id;
        });
        if (!exists) {
            entry["occurrences"].push_back({ {"file", fileStr}, {"id", id} });
        }
    };

    // 向 rewrite_queue 去重追加重翻请求。
    // 请求来源可能是模型显式提交，也可能是术语译法变化后程序自动生成。
    auto enqueueRewriteRequest = [&](json& queue, const json& request) {
        if (!request.is_object()) {
            return;
        }
        const std::string file = request.value("file", "");
        const int id = request.value("id", -1);
        const std::string sourceTerm = request.value("source_term", "");
        const bool exists = std::ranges::any_of(queue, [&](const json& item) {
            return item.value("file", "") == file && item.value("id", -1) == id && item.value("source_term", "") == sourceTerm;
        });
        if (!exists) {
            queue.push_back(request);
        }
    };

    // `commit` 的本地落盘入口：
    // 1. 校验模型是否完整覆盖当前 chunk
    // 2. 更新句子对象、file note 和共享状态文件
    // 3. 推进进度条与 run_state
    // 它本身不会再调用模型；校验失败后由外层 turn/retry 逻辑决定是否重试。
    auto applyCommit = [&](const AgentProtocolResponse& protocol, const std::string& modelName, int& committedCount) {
        std::unordered_map<int, json> translationMap;
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

        const std::vector<Sentence*> pending = currentChunk();
        if (translationMap.size() != pending.size()) {
            throw std::runtime_error("commit 未覆盖当前 chunk 的全部句子");
        }

        committedCount = 0;
        for (Sentence* se : pending) {
            const auto it = translationMap.find(se->index);
            if (it == translationMap.end()) {
                throw std::runtime_error(std::format("commit 缺少句子 {}", se->index));
            }
            se->pre_translated_text = it->second.value("dst", "");
            if (se->pre_translated_text.empty()) {
                throw std::runtime_error(std::format("commit 句子 {} 的 dst 为空", se->index));
            }
            se->translated_by = modelName;
            se->complete = true;
            ++committedCount;
        }

        if (committedCount > 0) {
            m_completedSentences += committedCount;
            m_controller->updateBar(committedCount);
        }

        json fileNote = loadAgentFileNote(relInputPath);
        mergeFileNotePatch(fileNote, protocol.fileNotePatch);
        if (protocol.summary.contains("rolling_context") && protocol.summary["rolling_context"].is_string()) {
            backgroundText = protocol.summary["rolling_context"].get<std::string>();
            fileNote["rolling_context"] = backgroundText;
        }
        else if (protocol.summary.contains("context") && protocol.summary["context"].is_string()) {
            backgroundText = protocol.summary["context"].get<std::string>();
            fileNote["rolling_context"] = backgroundText;
        }
        fileNote["updated_at"] = nowTimestampString();
        saveAgentFileNote(relInputPath, fileNote);

        const std::vector<int> currentChunkIds = pending
            | std::views::transform([](const Sentence* se) { return se->index; })
            | std::ranges::to<std::vector>();

        // `commit` 是 Agent 模式唯一允许修改共享持久化状态的入口。
        // 这里会在同一把锁下同时更新 `run_state`、`term_ledger` 和 `rewrite_queue`。
        mutateAgentState([&](json& runState, json& termLedger, json& rewriteQueue) {
            if (!runState.contains("files") || !runState["files"].is_array()) {
                runState["files"] = json::array();
            }
            const std::string relPathStr = wide2Ascii(relInputPath);
            auto runStateIt = std::ranges::find_if(runState["files"], [&](const json& item) {
                return item.value("file", "") == relPathStr;
            });
            if (runStateIt == runState["files"].end()) {
                runState["files"].push_back({
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
                json& entry = termLedger[sourceTerm];
                const std::string oldTarget = entry.value("target_term", "");
                entry["target_term"] = targetTerm;
                entry["status"] = update.value("status", entry.value("status", "tentative"));
                entry["category"] = update.value("category", entry.value("category", ""));
                entry["note"] = update.value("note", entry.value("note", ""));
                if (update.contains("line_ids") && update["line_ids"].is_array()) {
                    for (const auto& idVal : update["line_ids"]) {
                        appendOccurrence(entry, relInputPath, idVal.get<int>());
                    }
                }
                else {
                    for (const int id : currentChunkIds) {
                        appendOccurrence(entry, relInputPath, id);
                    }
                }

                if (!m_agentReconciling && !oldTarget.empty() && oldTarget != targetTerm && m_agentRewriteMode == "queue_retranslate") {
                    for (const auto& occurrence : entry["occurrences"]) {
                        enqueueRewriteRequest(rewriteQueue, {
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
                    enqueueRewriteRequest(rewriteQueue, request);
                }
            }
        });
    };

    int retryCount = 0;
    while (retryCount == 0 || retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return false;
        }

        const std::vector<Sentence*> pending = currentChunk();
        if (pending.empty()) {
            return true;
        }

        m_logger->debug("[线程 {}] [文件 {}] Agent chunk 开始，当前待提交 {} 句，允许最多 {} 轮。", threadId, wide2Ascii(relInputPath), pending.size(), m_agentMaxTurnsPerChunk);

        bool useNativeFunctionCalling = m_agentNativeFunctionCalling != "off";
        json fileNote = loadAgentFileNote(relInputPath);
        json messages = buildBaseMessages(backgroundText, fileNote);
        bool compactRequested = false;

        // 这里开始才是“单个 chunk 的模型多轮循环”。
        // 最典型的链路是：准备 messages -> 调模型 -> 执行工具/压缩上下文/commit -> 进入下一轮或结束。
        for (int turn = 0; turn < m_agentMaxTurnsPerChunk; ++turn) {
            const size_t messageChars = approximateMessagesChars(messages);
            m_logger->debug("[线程 {}] [文件 {}] Agent 第 {}/{} 轮，请求上下文约 {} 字符。",
                threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, messageChars);

            if (messageChars > (size_t)m_agentHardContextChars) {
                m_logger->warn("[线程 {}] [文件 {}] Agent 上下文超过 hardContextChars，回退到最近摘要重建消息。", threadId, wide2Ascii(relInputPath));
                messages = buildBaseMessages(backgroundText, loadAgentFileNote(relInputPath));
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
            if (useNativeFunctionCalling && !currentApi.stream) {
                payload["tools"] = buildAgentNativeTools();
                payload["tool_choice"] = "auto";
            }

            // 整个 translateBatchAgent 里，真正向模型发请求的地方只有这里。
            ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);
            if (useNativeFunctionCalling && m_agentNativeFunctionCalling == "auto" && shouldFallbackFromNativeFunctionCalling(response)) {
                m_logger->warn("[线程 {}] [文件 {}] 原生函数调用不可用，自动退回文本协议。", threadId, wide2Ascii(relInputPath));
                useNativeFunctionCalling = false;
                continue;
            }

            if (!checkResponse(
                response, m_apiPool, currentApi, relInputPath, m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
            )) {
                break;
            }

            AgentProtocolResponse protocol;
            try {
                protocol = parseAgentApiResponse(response);
            }
            catch (const std::exception& e) {
                ++retryCount;
                m_logger->warn("[线程 {}] [文件 {}] Agent 响应解析失败，第 {} 次重试。原始响应: {}\n错误: {}",
                    threadId, wide2Ascii(relInputPath), retryCount, response.content, e.what());
                break;
            }

            m_logger->debug("[线程 {}] [文件 {}] Agent 第 {}/{} 轮返回 action='{}'。", threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, protocol.action);

            if (protocol.action == "tool_calls" && !protocol.calls.empty()) {
                m_logger->info("[线程 {}] [文件 {}] Agent 请求 {} 个工具调用。", threadId, wide2Ascii(relInputPath), protocol.calls.size());
                const json toolResults = executeToolCalls(protocol.calls);
                compactRequested = false;
                // 工具调用分支不会直接完成 chunk，而是把工具结果回填给下一轮模型继续推理。
                if (response.hasToolCalls && useNativeFunctionCalling && response.message.is_object() && !response.message.empty()) {
                    messages.push_back(response.message);
                    for (const auto& result : toolResults) {
                        messages.push_back({
                            {"role", "tool"},
                            {"tool_call_id", result.value("id", "")},
                            {"content", result.dump(2)}
                        });
                    }
                }
                else {
                    messages.push_back({ {"role", "assistant"}, {"content", response.content} });
                    messages.push_back({
                        {"role", "user"},
                        {"content", std::string("Tool results:\n```json\n") + toolResults.dump(2) + "\n```"}
                    });
                }
                continue;
            }

            if (protocol.action == "compact_context") {
                // 压缩分支只更新 rolling context / file note，不提交任何句子。
                if (protocol.summary.contains("rolling_context") && protocol.summary["rolling_context"].is_string()) {
                    backgroundText = protocol.summary["rolling_context"].get<std::string>();
                }
                else if (protocol.summary.contains("context") && protocol.summary["context"].is_string()) {
                    backgroundText = protocol.summary["context"].get<std::string>();
                }
                else if (!response.content.empty()) {
                    backgroundText = response.content;
                }
                json fileNotePatch = loadAgentFileNote(relInputPath);
                fileNotePatch["rolling_context"] = backgroundText;
                fileNotePatch["updated_at"] = nowTimestampString();
                saveAgentFileNote(relInputPath, fileNotePatch);
                m_logger->info("[线程 {}] [文件 {}] Agent 已压缩上下文，摘要长度 {} 字符。", threadId, wide2Ascii(relInputPath), backgroundText.size());
                messages = buildBaseMessages(backgroundText, loadAgentFileNote(relInputPath));
                compactRequested = false;
                continue;
            }

            if (protocol.action == "commit") {
                // commit 成功后，这个 chunk 的多轮循环立即结束，控制权返回外层批处理调度。
                int committedCount = 0;
                try {
                    applyCommit(protocol, currentApi.modelName, committedCount);
                }
                catch (const std::exception& e) {
                    ++retryCount;
                    m_logger->warn("[线程 {}] [文件 {}] Agent commit 校验失败，第 {} 次重试。错误: {}",
                        threadId, wide2Ascii(relInputPath), retryCount, e.what());
                    break;
                }
                m_logger->info("[线程 {}] [文件 {}] Agent commit 成功，提交 {} 句，术语更新 {} 条，重翻请求 {} 条。",
                    threadId, wide2Ascii(relInputPath), committedCount, protocol.termUpdates.size(), protocol.rewriteRequests.size());
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
