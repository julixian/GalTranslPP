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
        if (const auto it = payload.find("summary"); it != payload.end()) {
            if (it->is_object()) {
                result.summary = *it;
            }
            else if (it->is_string()) {
                result.summary = json::object({
                    {"rolling_context", it->get<std::string>()}
                });
            }
        }
        return result;
    }

    int sanitizeToolLimit(int requested, int fallback, int maxLimit = 200) {
        if (requested <= 0) {
            return fallback;
        }
        return std::min(requested, maxLimit);
    }

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

    bool jsonLooksMeaningfulArrayText(const std::string& text) {
        const std::string trimmed = trimCopy(text);
        return !trimmed.empty() && trimmed != "[]" && trimmed != "null";
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

    // `search_dictionary`：在当前运行实际加载过的 GPT 字典文件里搜索词条。
    auto searchDictionaryTool = [&](const json& arguments) {
        const std::string query = arguments.value("query", "");
        const std::string queryLower = str2Lower(query);
        const int limit = sanitizeToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit, 200);
        json matches = json::array();
        auto pushMatch = [&](json&& item) {
            if ((int)matches.size() < limit) {
                matches.push_back(std::move(item));
            }
        };

        for (const fs::path& dictPath : m_agentDictionaryPaths) {
            if ((int)matches.size() >= limit) {
                break;
            }
            try {
                const auto dictData = toml::uparse(dictPath);
                const fs::path relPath = fs::relative(dictPath, m_projectDir);
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
                    const std::string haystack = str2Lower(sourceTerm + "\n" + targetTerm + "\n" + note + "\n" + relPathStr);
                    if (queryLower.empty() || haystack.contains(queryLower)) {
                        pushMatch(json{
                            {"type", "gpt_dict"},
                            {"file", relPathStr},
                            {"source_term", sourceTerm},
                            {"target_term", targetTerm},
                            {"note", note}
                        });
                        if ((int)matches.size() >= limit) {
                            break;
                        }
                    }
                }
            }
            catch (...) {}
        }
        return json{ {"matches", matches} };
    };

    // `get_project_note`：读取项目根目录中的用户脚本说明文件（如果存在）。
    auto getProjectNoteTool = [&](const json& arguments) {
        if (!m_agentProjectInfoPath.has_value()) {
            return json{
                {"available", false},
                {"file", nullptr},
                {"content", ""}
            };
        }
        const int maxChars = sanitizeToolLimit(arguments.value("max_chars", 20000), 20000, 120000);
        std::ifstream ifs(m_agentProjectInfoPath.value(), std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        const bool truncated = (int)content.size() > maxChars;
        return json{
            {"available", true},
            {"file", wide2Ascii(fs::relative(m_agentProjectInfoPath.value(), m_projectDir))},
            {"content", truncated ? content.substr(0, maxChars) : content},
            {"truncated", truncated},
            {"total_chars", (int)content.size()}
        };
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
                else if (call.name == "search_dictionary") {
                    result["result"] = searchDictionaryTool(call.arguments);
                }
                else if (call.name == "get_term") {
                    result["result"] = getTermTool(call.arguments);
                }
                else if (call.name == "get_file_note") {
                    result["result"] = getFileNoteTool(call.arguments);
                }
                else if (call.name == "get_project_note") {
                    result["result"] = getProjectNoteTool(call.arguments);
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
    auto termLedgerExcerpt = [&](const std::string& currentInputBlock, const std::string& rollingSummary, const json& fileNote) {
        const json ledger = loadAgentTermLedger();
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
                (!inputLower.empty() && inputLower.contains(termLower))
                || (!summaryLower.empty() && (summaryLower.contains(termLower) || (!targetLower.empty() && summaryLower.contains(targetLower))))
                || (!fileNoteLower.empty() && (fileNoteLower.contains(termLower) || (!targetLower.empty() && fileNoteLower.contains(targetLower)) || (!noteLower.empty() && fileNoteLower.contains(noteLower))));

            if (isRelevant) {
                relevant.push_back(normalizedEntry);
                continue;
            }
            if ((int)fallback.size() < m_agentSearchResultLimit) {
                fallback.push_back(normalizedEntry);
            }
        }
        return (relevant.empty() ? fallback : relevant).dump(2);
    };

    auto currentProblemSummary = [&]() {
        const std::vector<Sentence*> pending = currentChunk();
        return std::ranges::fold_left(
            pending
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
    };

    auto currentGlossary = [&]() {
        std::vector<Sentence*> pending = currentChunk();
        if (pending.empty()) {
            return std::string{};
        }
        std::span<Sentence*> pendingSpan(pending);
        return m_gptDictionary->generatePrompt(pendingSpan, m_transEngine);
    };

    auto currentInputBlock = [&]() {
        std::vector<Sentence*> pending = currentChunk();
        absl::btree_map<int, Sentence*> id2SentenceMap;
        std::string inputBlock;
        std::span<Sentence*> pendingSpan(pending);
        fillBlockAndMap(pendingSpan, id2SentenceMap, inputBlock, m_transEngine);
        return inputBlock;
    };

    // 组装“一轮模型请求”的基础消息。
    // 这里本身不发请求，只负责把 chunk、rolling context、problem/background/glossary、
    // file note、术语摘要等拼好。
    auto buildBaseMessages = [&](const std::string& rollingSummary, const json& fileNote) {
        absl::btree_map<int, Sentence*> id2SentenceMap;
        std::string inputBlock;
        fillBlockAndMap(batch, id2SentenceMap, inputBlock, m_transEngine);
        const std::string inputProblems = currentProblemSummary();
        const std::string glossary = currentGlossary();
        const std::string knownTerms = termLedgerExcerpt(inputBlock, rollingSummary, fileNote);
        const std::string extraTools = m_agentProjectInfoPath.has_value()
            ? std::format("get_project_note: read optional user-provided script note file `{}`\n", wide2Ascii(fs::relative(m_agentProjectInfoPath.value(), m_projectDir)))
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
            "\"summary\":{}"
            "}";

        std::string userPrompt = m_agentUserPrompt;
        replaceStrInplace(userPrompt, "[AgentCurrentFile]", wide2Ascii(relInputPath));
        replaceStrInplace(userPrompt, "[AgentChunkIdRange]", std::format("{}-{}", batch.front()->index, batch.back()->index));
        replaceStrInplace(userPrompt, "[TargetLang]", m_targetLang);
        replaceStrInplace(userPrompt, "[AgentTargetLang]", m_targetLang);
        replaceStrInplace(userPrompt, "[Problem Description]", inputProblems.empty() ? "None" : inputProblems);
        replaceStrInplace(userPrompt, "[AgentProblemDescription]", inputProblems.empty() ? "None" : inputProblems);
        replaceStrInplace(userPrompt, "[Glossary]", glossary.empty() ? "None" : glossary);
        replaceStrInplace(userPrompt, "[AgentGlossary]", glossary.empty() ? "None" : glossary);
        replaceStrInplace(userPrompt, "[AgentRollingContext]", rollingSummary.empty() ? "None" : rollingSummary);
        replaceStrInplace(userPrompt, "[AgentFileNote]", fileNote.empty() ? "None" : fileNote.dump(2));
        replaceStrInplace(userPrompt, "[AgentKnownTerms]", knownTerms);
        replaceStrInplace(userPrompt, "[AgentCurrentChunkTsv]", std::string("NAME\tSRC\tID\n") + inputBlock);
        replaceStrInplace(userPrompt, "[AgentSchemaDescription]", schemaDescription);
        replaceStrInplace(userPrompt, "[AgentExtraTools]", extraTools);
        replaceStrInplace(userPrompt, "[AgentTurnGuidance]",
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
        mergeFileNotePatch(nextFileNote, protocol.fileNotePatch);
        if (protocol.summary.contains("rolling_context") && protocol.summary["rolling_context"].is_string()) {
            nextBackgroundText = protocol.summary["rolling_context"].get<std::string>();
            nextFileNote["rolling_context"] = nextBackgroundText;
        }
        else if (protocol.summary.contains("context") && protocol.summary["context"].is_string()) {
            nextBackgroundText = protocol.summary["context"].get<std::string>();
            nextFileNote["rolling_context"] = nextBackgroundText;
        }
        nextFileNote["updated_at"] = nowTimestampString();

        const std::vector<int> currentChunkIds = pending
            | std::views::transform([](const Sentence* se) { return se->index; })
            | std::ranges::to<std::vector>();

        // `commit` 是 Agent 模式唯一允许修改共享持久化状态的入口。
        // 这里会在同一把锁下同时更新 `run_state`、`term_ledger` 和 `rewrite_queue`。
        int appliedTermUpdateCount = 0;
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

        const std::string inputProblems = currentProblemSummary();
        const std::string glossary = currentGlossary();
        const std::string inputBlock = currentInputBlock();
        json fileNote = loadAgentFileNote(relInputPath);
        const std::string knownTerms = termLedgerExcerpt(inputBlock, backgroundText, fileNote);
        json messages = buildBaseMessages(backgroundText, fileNote);
        bool compactRequested = false;

        std::string logBlock;
        if (!inputProblems.empty()) {
            logBlock += "\nProblems:\n" + inputProblems;
        }
        if (!backgroundText.empty()) {
            logBlock += "\nBackground:\n" + truncateForAgentLog(backgroundText);
        }
        if (m_logger->should_log(spdlog::level::debug) && !fileNote.empty()) {
            logBlock += "\nFileNote:\n" + truncateForAgentLog(fileNote.dump(2));
        }
        if (!glossary.empty()) {
            logBlock += "\nGlossary:\n" + truncateForAgentLog(glossary);
        }
        if (jsonLooksMeaningfulArrayText(knownTerms) || m_logger->should_log(spdlog::level::trace)) {
            logBlock += "\nKnownTerms:\n" + truncateForAgentLog(knownTerms, 12000);
        }
        logBlock += "\ninputBlock:\n" + inputBlock;
        m_logger->info("[线程 {}] [文件 {}] Agent 开始翻译:{}", threadId, wide2Ascii(relInputPath), logBlock);
        if (m_logger->should_log(spdlog::level::trace)) {
            m_logger->trace(
                "[线程 {}] [文件 {}] Agent 初始请求消息:\n{}",
                threadId,
                wide2Ascii(relInputPath),
                truncateForAgentLog(messages.dump(2), 20000)
            );
        }

        // 这里开始才是“单个 chunk 的模型多轮循环”。
        // 最典型的链路是：准备 messages -> 调模型 -> 执行工具/压缩上下文/commit -> 进入下一轮或结束。
        for (int turn = 0; turn < m_agentMaxTurnsPerChunk; ++turn) {
            const size_t messageChars = approximateMessagesChars(messages);
            m_logger->debug("[线程 {}] [文件 {}] Agent 第 {}/{} 轮，请求上下文约 {} 字符。",
                threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, messageChars);
            if (m_logger->should_log(spdlog::level::trace)) {
                m_logger->trace(
                    "[线程 {}] [文件 {}] Agent 第 {}/{} 轮请求消息:\n{}",
                    threadId,
                    wide2Ascii(relInputPath),
                    turn + 1,
                    m_agentMaxTurnsPerChunk,
                    truncateForAgentLog(messages.dump(2), 20000)
                );
            }

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

            m_logger->debug("[线程 {}] [文件 {}] Agent 第 {}/{} 轮返回 action='{}'。", threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, protocol.action);
            if (m_logger->should_log(spdlog::level::trace) && !response.content.empty()) {
                m_logger->trace("[线程 {}] [文件 {}] Agent 第 {}/{} 轮原始响应:\n{}", threadId, wide2Ascii(relInputPath), turn + 1, m_agentMaxTurnsPerChunk, response.content);
            }

            if (protocol.action == "tool_calls" && !protocol.calls.empty()) {
                m_logger->info(
                    "[线程 {}] [文件 {}] Agent 请求 {} 个工具调用:\n{}",
                    threadId,
                    wide2Ascii(relInputPath),
                    protocol.calls.size(),
                    formatToolCallRequestsForLog(protocol.calls)
                );
                const json toolResults = executeToolCalls(protocol.calls);
                compactRequested = false;
                if (m_logger->should_log(spdlog::level::debug)) {
                    m_logger->debug(
                        "[线程 {}] [文件 {}] Agent 工具返回:\n{}",
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
                if (m_logger->should_log(spdlog::level::debug) && !backgroundText.empty()) {
                    m_logger->debug("[线程 {}] [文件 {}] Agent 压缩后摘要:\n{}", threadId, wide2Ascii(relInputPath), truncateForAgentLog(backgroundText, 12000));
                }
                messages = buildBaseMessages(backgroundText, loadAgentFileNote(relInputPath));
                compactRequested = false;
                continue;
            }

            if (protocol.action == "commit") {
                // commit 成功后，这个 chunk 的多轮循环立即结束，控制权返回外层批处理调度。
                if (m_logger->should_log(spdlog::level::debug)) {
                    m_logger->debug(
                        "[线程 {}] [文件 {}] Agent commit 内容:\ntranslations={}\nterm_updates={}\nrewrite_requests={}\nfile_note_patch={}\nsummary={}",
                        threadId,
                        wide2Ascii(relInputPath),
                        truncateForAgentLog(protocol.translations.dump(2), 12000),
                        truncateForAgentLog(protocol.termUpdates.dump(2), 12000),
                        truncateForAgentLog(protocol.rewriteRequests.dump(2), 12000),
                        truncateForAgentLog(protocol.fileNotePatch.dump(2), 12000),
                        truncateForAgentLog(protocol.summary.dump(2), 12000)
                    );
                }
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
