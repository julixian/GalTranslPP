module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

// 保存本轮翻译 Agent 的运行依赖，并构建工具要读取的源文件视图。
NormalJsonTranslatorTransAgent::NormalJsonTranslatorTransAgent(
    TransEngine transEngine,
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::unique_ptr<ApiPool>& apiPool,
    const std::unique_ptr<GptDictionary>& gptDictionary,
    const std::function<std::string(const std::string&)>& onPerformApi,
    const fs::path& projectDir,
    const fs::path& sourceRootDir,
    const fs::path& transCacheDir,
    const fs::path& agentTermLedgerPath,
    const fs::path& agentFileNotesDir,
    const std::string& agentSystemPrompt,
    const std::string& agentUserPrompt,
    const std::string& targetLang,
    const std::string& apiStrategy,
    int maxRequestCount,
    int apiTimeOutMs,
    int agentMaxTurnsPerChunk,
    int agentCompactContextThresholdBytes,
    int agentSearchResultLimit,
    int agentContextLinesLimit,
    int inputBlockMaxLines,
    int problemMaxLines,
    int glossaryMaxLines,
    bool smartRetry,
    bool checkQuota,
    std::shared_mutex& transCacheMutex,
    const absl::flat_hash_set<fs::path>& savedTransCacheRelFilePaths,
    const std::vector<fs::path>& knownRelFiles,
    const std::vector<fs::path>& gptDictionaryPaths,
    const std::optional<fs::path>& agentProjectNotePath,
    const std::function<void(Sentence*)>& preProcessFunc
) : m_transEngine(transEngine),
    m_controller(controller),
    m_logger(logger),
    m_apiPool(apiPool),
    m_gptDictionary(gptDictionary),
    m_onPerformApi(onPerformApi),
    m_projectDir(projectDir),
    m_transCacheDir(transCacheDir),
    m_agentTermLedgerPath(agentTermLedgerPath),
    m_agentFileNotesDir(agentFileNotesDir),
    m_agentSystemPrompt(agentSystemPrompt),
    m_agentUserPrompt(agentUserPrompt),
    m_targetLang(targetLang),
    m_apiStrategy(apiStrategy),
    m_maxRequestCount(maxRequestCount),
    m_apiTimeOutMs(apiTimeOutMs),
    m_agentMaxTurnsPerChunk(agentMaxTurnsPerChunk),
    m_agentCompactContextThresholdBytes(agentCompactContextThresholdBytes),
    m_agentSearchResultLimit(agentSearchResultLimit),
    m_agentContextLinesLimit(agentContextLinesLimit),
    m_inputBlockMaxLines(inputBlockMaxLines),
    m_problemMaxLines(problemMaxLines),
    m_glossaryMaxLines(glossaryMaxLines),
    m_smartRetry(smartRetry),
    m_checkQuota(checkQuota),
    m_transCacheMutex(transCacheMutex),
    m_savedTranslCacheRelFilePaths(savedTransCacheRelFilePaths),
    m_knownRelFiles(knownRelFiles),
    m_gptDictionaryPaths(gptDictionaryPaths),
    m_agentProjectNotePath(agentProjectNotePath)
{
    fs::create_directories(m_agentFileNotesDir);
    if (!fs::exists(m_agentTermLedgerPath)) {
        atomicOutputFile(m_agentTermLedgerPath, m_termLedgerCache.dump(2));
    }
    else {
        try {
            m_termLedgerCache = parseJson(m_agentTermLedgerPath);
        }
        catch (...) { }
    }

    std::ifstream ifs;
    for (const fs::path& relFilePath : m_knownRelFiles) {
        const fs::path filePath = sourceRootDir / relFilePath;
        if (!fs::exists(filePath)) {
            continue;
        }
        try {
            const ordered_json data = parseOrderedJson(filePath, ifs);
            m_sourceFileViews.insert_or_assign(
                relFilePath,
                buildAgentCommonSourceFileViewFromJson(data, preProcessFunc)
            );
        }
        catch (...) { }
    }
}

// 读取当前术语账本快照，供提示词构造和搜索工具使用。
json NormalJsonTranslatorTransAgent::loadTermLedger() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_termLedgerCache;
}

// 读取指定文件备注；优先使用内存缓存，首次访问时再读磁盘。
json NormalJsonTranslatorTransAgent::loadFileNote(const fs::path& targetRelPath) {
    std::lock_guard<std::mutex> lock(m_fileNotesMutex);
    if (const auto it = m_fileNoteCache.find(targetRelPath); it != m_fileNoteCache.end()) {
        return it->second;
    }
    json& fileNote = m_fileNoteCache[targetRelPath];
    if (const fs::path fileNotePath = m_agentFileNotesDir / targetRelPath; fs::exists(fileNotePath)) {
        fileNote = parseJson(fileNotePath);
    }
    else {
        fileNote = json::object();
    }
    return fileNote;
}

// 解析翻译 Agent 的 JSON 文本协议。
TransAgentProtocolResponse NormalJsonTranslatorTransAgent::parseProtocolResponse(const std::string& content) const {
    TransAgentProtocolResponse result;
    const std::optional<json> payloadOpt = tryParseAgentCommonJsonEnvelope(content);
    if (!payloadOpt.has_value() || !payloadOpt->is_object()) {
        throw std::runtime_error(gppTr("NormalJsonTranslatorTransAgent.parseProtocolResponse",
            "翻译 Agent 响应不是合法 JSON 对象")
            .toStdString());
    }

    const json& payload = *payloadOpt;
    result.action = payload.value("action", "");
    result.calls = parseAgentCommonToolCallRequests(payload);
    if (result.action.empty()) {
        throw std::runtime_error(gppTr("NormalJsonTranslatorTransAgent.parseProtocolResponse",
            "翻译 Agent 响应缺少动作字段")
            .toStdString());
    }
    if (result.action == "tool_calls" && result.calls.empty()) {
        throw std::runtime_error(gppTr("NormalJsonTranslatorTransAgent.parseProtocolResponse",
            "翻译 Agent 返回了空工具调用")
            .toStdString());
    }
    if (result.action != "tool_calls" && result.action != "compact_context" && result.action != "commit") {
        throw std::runtime_error(gppTr("NormalJsonTranslatorTransAgent.parseProtocolResponse",
            "翻译 Agent 返回未知动作: %1")
            .arg(result.action)
            .toStdString());
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
    if (const auto it = payload.find("agent_suggest"); it != payload.end() && it->is_array()) {
        result.agentSuggestions = *it;
    }
    if (const auto it = payload.find("file_note_patch"); it != payload.end() && it->is_object()) {
        result.fileNotePatch = *it;
    }
    if (const auto it = payload.find("rolling_context"); it != payload.end() && it->is_string()) {
        it->get_to(result.rollingContext);
    }
    return result;
}

// 解析 Agent 建议目标。
std::optional<std::pair<fs::path, int>> NormalJsonTranslatorTransAgent::parseAgentSuggestTarget(const json& suggestion) const {
    if (!suggestion.is_object()) {
        return std::nullopt;
    }
    const std::string file = suggestion.value("file", "");
    const auto idOpt = suggestion.contains("id") ? parseAgentCommonJsonInt(suggestion["id"]) : std::nullopt;
    if (file.empty() || !idOpt.has_value() || idOpt.value() < 0) {
        return std::nullopt;
    }
    return std::make_pair(ascii2Wide(file), idOpt.value());
}

// 读取翻译 Agent 可搜索的 GPT 字典项。
std::vector<NormalJsonTranslatorTransAgent::LoadedDictionaryEntry> NormalJsonTranslatorTransAgent::loadDictionaryEntries() const {
    std::vector<LoadedDictionaryEntry> entries;
    for (const fs::path& dictPath : m_gptDictionaryPaths) {
        const auto dictData = toml::uparse(dictPath);
        if (!dictData.contains("gptDict")) {
            continue;
        }
        const auto& dictTbls = dictData.at("gptDict").as_array();
        for (const auto& el : dictTbls) {
            if (!el.contains("org") || !el.contains("rep")) {
                continue;
            }
            const std::string sourceTerm = el.at("org").as_string();
            const std::string targetTerm = el.at("rep").as_string();
            const std::string note = toml::find_or(el, "note", "");
            if (sourceTerm.empty() && targetTerm.empty() && note.empty()) {
                continue;
            }
            entries.push_back({
                .sourceTerm = sourceTerm,
                .targetTerm = targetTerm,
                .note = note,
                .haystackLower = str2Lower(sourceTerm + "\n" + targetTerm + "\n" + note)
            });
        }
    }
    return entries;
}

// 收集当前批次里还没有完成翻译的句子。
std::vector<Sentence*> NormalJsonTranslatorTransAgent::collectPendingSentences(std::span<Sentence*> batch) const {
    return batch
        | std::views::filter([](Sentence* se) { return !se->transCompleted; })
        | std::ranges::to<std::vector>();
}

// 构造翻译 Agent 当前请求可直接放进提示词的术语账本摘要。
std::string NormalJsonTranslatorTransAgent::buildTermLedgerExcerpt(
    const json& ledger,
    const std::string& currentInputBlock,
    const std::string& rollingContext,
    const json& fileNote
) const {
    json relevant = json::array();
    json fallback = json::array();
    const std::string inputLower = str2Lower(currentInputBlock);
    const std::string rollingContextLower = str2Lower(rollingContext);
    const std::string fileNoteLower = str2Lower(fileNote.dump());
    for (const auto& item : ledger.items()) {
        const std::string term = item.key();
        const json& entry = item.value();
        const json normalizedEntry = {
            {"source_term", term},
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
            (!rollingContextLower.empty() && (rollingContextLower.contains(termLower) ||
            (!targetLower.empty() && rollingContextLower.contains(targetLower)))) ||
            (!fileNoteLower.empty() && (fileNoteLower.contains(termLower) ||
            (!targetLower.empty() && fileNoteLower.contains(targetLower)) ||
            (!noteLower.empty() && fileNoteLower.contains(noteLower))));

        if (isRelevant) {
            relevant.push_back(normalizedEntry);
            continue;
        }
        if ((int)fallback.size() < m_agentSearchResultLimit) {
            fallback.push_back(normalizedEntry);
        }
    }
    return (relevant.empty() ? fallback : relevant).dump(2);
}

// 汇总当前待翻译句子已有的问题文本。
std::string NormalJsonTranslatorTransAgent::buildProblemSummary(std::span<Sentence*> pending) const {
    return std::ranges::fold_left(pending
            | std::views::transform([](Sentence* se) { return se->problems; })
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

// 生成当前待翻译句子的 GPT 字典提示片段。
std::string NormalJsonTranslatorTransAgent::buildGlossary(std::span<Sentence*> pending) const {
    return m_gptDictionary->generatePrompt(pending, m_transEngine);
}

// 合并模型提交的文件备注补丁。
void NormalJsonTranslatorTransAgent::mergeFileNotePatch(json& note, const json& patch) const {
    if (!patch.is_object()) {
        return;
    }
    for (const auto& item : patch.items()) {
        note[item.key()] = item.value();
    }
}

// 记录术语在当前文件和句子 id 上的出现位置。
void NormalJsonTranslatorTransAgent::addToTermOccurrence(json& entry, const fs::path& file, int id) const {
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

// 在当前 chunk 内推断术语出现的句子 id。
std::vector<int> NormalJsonTranslatorTransAgent::inferOccurrenceIdsFromChunk(
    const std::string& sourceTerm,
    std::span<Sentence*> pending
) const {
    std::vector<int> matchedIds;
    for (Sentence* se : pending) {
        if (se == nullptr || sourceTerm.empty()) {
            continue;
        }
        bool matched = false;
        if (se->nameType != NameType::None && se->name.contains(sourceTerm)) {
            matched = true;
        }
        if (!matched) {
            matched = se->orig.contains(sourceTerm) || se->preproc.contains(sourceTerm);
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

// 格式化术语变化引发的 Agent 建议文本。
std::string NormalJsonTranslatorTransAgent::formatTermUpdateSuggestion(
    const std::string& sourceTerm,
    const std::string& oldTarget,
    const std::string& newTarget) const
{
    return gppTr("NormalJsonTranslatorTransAgent.formatTermUpdateSuggestion",
        "注意！从此处生成/更新的术语 `%1` 的译名已由 `%2` 更新为 `%3`。请自行搜索全文以确认是否符合预期")
        .arg(sourceTerm)
        .arg(oldTarget)
        .arg(newTarget)
        .toStdString();
}

// 查找指定相对路径的源文件视图。
const AgentCommonSourceFileView* NormalJsonTranslatorTransAgent::findSourceFileView(const fs::path& relPath) const {
    const auto it = m_sourceFileViews.find(relPath);
    if (it == m_sourceFileViews.end()) {
        return nullptr;
    }
    return &it->second;
}

// 返回源文件视图里的行数，供 list_files 工具显示。
std::optional<int> NormalJsonTranslatorTransAgent::getSourceFileLineCount(const fs::path& relPath) const {
    const AgentCommonSourceFileView* sourceView = findSourceFileView(relPath);
    if (sourceView == nullptr) {
        return std::nullopt;
    }
    return (int)sourceView->lines.size();
}

// 读取翻译缓存并按句子 id 建索引。
absl::flat_hash_map<int, json> NormalJsonTranslatorTransAgent::loadCacheDstMap(const fs::path& targetRelPath) const {
    absl::flat_hash_map<int, json> cacheMap;
    const fs::path cachePath = m_transCacheDir / targetRelPath;
    if (!fs::exists(cachePath)) {
        return cacheMap;
    }
    std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
    json cacheJson = parseJson(cachePath);
    for (const auto& item : cacheJson) {
        const int index = item.value("index", -1);
        if (index >= 0) {
            cacheMap[index] = item;
        }
    }
    return cacheMap;
}

// 读取源文件连续行，并按需附带已有缓存译文。
json NormalJsonTranslatorTransAgent::runReadLinesTool(const fs::path& relInputPath, const json& arguments) {
    const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(relInputPath)));
    const int start = std::max(0, arguments.value("start", 0));
    const int count = std::max(0, arguments.value("count", m_agentSearchResultLimit));
    const bool includeSrc = arguments.value("include_src", true);
    const bool includeDst = arguments.value("include_dst", true);
    json result = { {"file", wide2Ascii(targetRelPath)}, {"lines", json::array()} };
    const AgentCommonSourceFileView* sourceView = findSourceFileView(targetRelPath);
    const auto cacheMap = loadCacheDstMap(targetRelPath);
    if (sourceView == nullptr) {
        result["error"] = std::format("Source view not found for {}", wide2Ascii(targetRelPath));
        return result;
    }
    for (int i = start; i < (int)sourceView->lines.size() && i < start + count; ++i) {
        const AgentCommonSourceLineView& sourceLine = sourceView->lines[i];
        json line = { {"id", sourceLine.id} };
        const auto cacheIt = cacheMap.find(sourceLine.id);
        if (!sourceLine.speaker.empty()) {
            line["name"] = sourceLine.speaker;
        }
        if (includeSrc) {
            line["src"] = sourceLine.sourceText;
        }
        if (includeDst && cacheIt != cacheMap.end()) {
            line["dst"] = cacheIt->second.value("translated_raw_text", "");
        }
        result["lines"].push_back(std::move(line));
    }
    return result;
}

// 在源文中搜索文本。
json NormalJsonTranslatorTransAgent::runSearchTextTool(const fs::path& relInputPath, const json& arguments) const {
    return runAgentCommonSourceSearchTextTool(
        relInputPath,
        m_knownRelFiles,
        [this](const fs::path& relPath)
        {
            return findSourceFileView(relPath);
        },
        m_agentSearchResultLimit,
        m_agentContextLinesLimit,
        false,
        gppTr(
            "NormalJsonTranslatorTransAgent.runSearchTextTool",
            "search_text.scope 非法: %1。允许值仅有 current_file|all_files|specified_file")
            .arg(arguments.value("scope", ""))
            .toStdString(),
        arguments
    );
}

// 在术语账本里搜索术语。
json NormalJsonTranslatorTransAgent::runSearchTermTool(const json& arguments) {
    const std::vector<std::string> queries = collectAgentCommonToolQueries(arguments);
    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
    const json termLedger = loadTermLedger();
    json matches = json::array();

    if (!termLedger.is_object()) {
        return json{
            {"queries", queries},
            {"start", start},
            {"limit", limit},
            {"total", 0},
            {"matches", matches}
        };
    }

    int matchCount = 0;
    for (const auto& item : termLedger.items()) {
        const json& entry = item.value();
        const std::string sourceTerm = item.key();
        const std::string targetTerm = entry.value("target_term", "");
        const std::string category = entry.value("category", "");
        const std::string note = entry.value("note", "");
        const std::string haystack = queryLowers.empty()
            ? std::string{}
            : str2Lower(sourceTerm + "\n" + targetTerm + "\n" + category + "\n" + note);
        const bool matched = queryLowers.empty() || std::ranges::any_of(queryLowers, [&](const std::string& queryLower)
            {
                return !queryLower.empty() && haystack.contains(queryLower);
            });
        if (!matched) {
            continue;
        }

        ++matchCount;
        if (matchCount <= start || (int)matches.size() >= limit) {
            continue;
        }
        matches.push_back(json{
            {"source_term", sourceTerm},
            {"target_term", targetTerm},
            {"status", entry.value("status", "tentative")},
            {"category", category},
            {"note", note},
            {"occurrences", entry.value("occurrences", json::array())}
        });
    }

    return json{
        {"queries", queries},
        {"start", start},
        {"limit", limit},
        {"total", matchCount},
        {"matches", matches}
    };
}

// 在配置的 GPT 字典里搜索术语。
json NormalJsonTranslatorTransAgent::runSearchDictionaryTool(const json& arguments) {
    const std::vector<std::string> queries = collectAgentCommonToolQueries(arguments);
    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
    json matches = json::array();
    int matchCount = 0;

    const auto matchedByPrecomputedHaystack = [&](const std::string& haystackLower)
        {
            if (queryLowers.empty()) {
                return true;
            }
            return std::ranges::any_of(queryLowers, [&](const std::string& queryLower)
                {
                    return !queryLower.empty() && haystackLower.contains(queryLower);
                });
        };

    const auto loadDictionaryEntriesCache = [&]() -> std::shared_ptr<const json>
        {
            {
                std::shared_lock<std::shared_mutex> lock(m_loadedDictionaryEntriesCacheMutex);
                if (m_loadedDictionaryEntriesCache) {
                    return m_loadedDictionaryEntriesCache;
                }
            }

            std::unique_lock<std::shared_mutex> lock(m_loadedDictionaryEntriesCacheMutex);
            if (m_loadedDictionaryEntriesCache) {
                return m_loadedDictionaryEntriesCache;
            }
            json loadedEntries = json::array();
            for (const LoadedDictionaryEntry& entry : loadDictionaryEntries()) {
                loadedEntries.push_back({
                    {"source_term", entry.sourceTerm},
                    {"target_term", entry.targetTerm},
                    {"note", entry.note},
                    {"haystack_lower", entry.haystackLower}
                });
            }
            m_loadedDictionaryEntriesCache = std::make_shared<const json>(std::move(loadedEntries));
            return m_loadedDictionaryEntriesCache;
        };

    for (const json& entry : *loadDictionaryEntriesCache()) {
        if (!matchedByPrecomputedHaystack(entry.value("haystack_lower", ""))) {
            continue;
        }
        ++matchCount;
        if (matchCount <= start || (int)matches.size() >= limit) {
            continue;
        }
        matches.push_back({
            {"source_term", entry.value("source_term", "")},
            {"target_term", entry.value("target_term", "")},
            {"note", entry.value("note", "")}
        });
    }

    return json{
        {"queries", queries},
        {"start", start},
        {"limit", limit},
        {"total", matchCount},
        {"matches", matches}
    };
}

// 读取某个文件的 Agent 文件备注。
json NormalJsonTranslatorTransAgent::runGetFileNoteTool(const fs::path& relInputPath, const json& arguments) {
    const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(relInputPath)));
    return json{
        {"file", wide2Ascii(targetRelPath)},
        {"note", loadFileNote(targetRelPath)}
    };
}

// 执行模型请求的工具调用序列。
NormalJsonTranslatorTransAgent::TransAgentToolCallResult NormalJsonTranslatorTransAgent::executeToolCalls(
    const fs::path& relInputPath,
    const std::vector<AgentCommonToolCallRequest>& calls,
    bool collectDetail
) {
    TransAgentToolCallResult executionResult;
    executionResult.summary = formatAgentCommonToolCallDetails(calls);
    for (const auto& call : calls) {
        json result = { {"id", call.id}, {"name", call.name} };
        if (call.name == "list_files") {
            result["result"] = runAgentCommonListFilesTool(
                m_knownRelFiles,
                [this](const fs::path& relPath)
                {
                    return getSourceFileLineCount(relPath);
                },
                m_agentSearchResultLimit,
                call.arguments
            );
        }
        else if (call.name == "read_lines") {
            result["result"] = runReadLinesTool(relInputPath, call.arguments);
        }
        else if (call.name == "search_text") {
            result["result"] = runSearchTextTool(relInputPath, call.arguments);
        }
        else if (call.name == "search_dictionary") {
            result["result"] = runSearchDictionaryTool(call.arguments);
        }
        else if (call.name == "search_term") {
            result["result"] = runSearchTermTool(call.arguments);
        }
        else if (call.name == "get_file_note") {
            result["result"] = runGetFileNoteTool(relInputPath, call.arguments);
        }
        else if (call.name == "get_project_note") {
            result["result"] = runAgentCommonGetProjectNoteTool(m_projectDir, m_agentProjectNotePath, call.arguments);
        }
        else {
            result["error"] = gppTr("NormalJsonTranslatorTransAgent.executeToolCalls", "未知工具: %1")
                .arg(call.name)
                .toStdString();
        }
        executionResult.results.push_back(std::move(result));
    }
    if (collectDetail) {
        executionResult.detail = gppTr(
            "NormalJsonTranslatorTransAgent.executeToolCalls",
            "工具返回结果:\n%1")
            .arg(executionResult.results.dump(2))
            .toStdString();
    }
    return executionResult;
}

// 解析一轮响应并执行对应动作；失败时把错误交给外层重新请求当前轮次。
std::expected<NormalJsonTranslatorTransAgent::TransAgentTurnResult, std::string> NormalJsonTranslatorTransAgent::parseAndApplyTurnResponse(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    std::string& rollingContext,
    json& messages,
    const std::string& content,
    const std::string& modelName,
    const std::string& batchIndexLog,
    int turn,
    int requestCount,
    int threadId
) {
    try {
        const TransAgentProtocolResponse protocol = parseProtocolResponse(content);
        if (protocol.action == "tool_calls") {
            const TransAgentToolCallResult toolCallResult = executeToolCalls(
                relInputPath,
                protocol.calls,
                m_logger->should_log(spdlog::level::debug)
            );
            if (!toolCallResult.detail.empty()) {
                m_logger->debug(gppTr(
                    "NormalJsonTranslatorTransAgent.parseAndApplyTurnResponse",
                    "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 工具调用明细:\n%6")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .arg(batchIndexLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(toolCallResult.detail)
                    .toStdString());
            }
            messages.push_back({ {"role", "assistant"}, {"content", content} });
            messages.push_back({
                {"role", "user"},
                {"content", "Tool results:\n```json\n" + toolCallResult.results.dump() + "\n```"}
            });
            return TransAgentTurnResult{
                .action = TransAgentTurnResult::Action::ContinueTurn,
                .summary = gppTr(
                    "NormalJsonTranslatorTransAgent.parseAndApplyTurnResponse",
                    "执行工具调用 %1 个，进入下一轮。调用参数:\n%2")
                    .arg(protocol.calls.size())
                    .arg(toolCallResult.summary)
                    .toStdString()
            };
        }

        if (protocol.action == "compact_context") {
            rollingContext = protocol.rollingContext.empty() ? content : protocol.rollingContext;
            messages = buildBaseMessages(relInputPath, pending, rollingContext);
            return TransAgentTurnResult{
                .action = TransAgentTurnResult::Action::ContinueTurn,
                .summary = gppTr(
                    "NormalJsonTranslatorTransAgent.parseAndApplyTurnResponse",
                    "完成上下文压缩，进入下一轮")
                    .toStdString()
            };
        }

        std::string commitResultLog;
        int recordedTermUpdateCount = 0;
        int recordedSuggestionCount = 0;
        const int committedCount = applyCommit(relInputPath, pending, rollingContext, threadId, protocol,
            modelName, batchIndexLog, turn, requestCount, commitResultLog, recordedTermUpdateCount, recordedSuggestionCount);
        return TransAgentTurnResult{
            .action = TransAgentTurnResult::Action::CompleteBatch,
            .summary = gppTr(
                "NormalJsonTranslatorTransAgent.parseAndApplyTurnResponse",
                "该批次 %1 句译文均已提交，记录术语 %2 条，记录建议 %3 条，翻译结果:\n%4")
                .arg(committedCount)
                .arg(recordedTermUpdateCount)
                .arg(recordedSuggestionCount)
                .arg(limitLogLines(commitResultLog, m_inputBlockMaxLines))
                .toStdString()
        };
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string(e.what()));
    }
}

// 构造 Agent 开始请求前写入日志的可读诊断块。
std::string NormalJsonTranslatorTransAgent::buildLogBlock(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    const std::string& rollingContext
) {
    std::string inputBlock;
    fillBlockAndMap(pending, inputBlock, m_transEngine);

    const std::string inputProblems = buildProblemSummary(pending);
    const std::string glossary = buildGlossary(pending);
    const json currentFileNote = loadFileNote(relInputPath);
    const std::string knownTerms = buildTermLedgerExcerpt(loadTermLedger(), inputBlock, rollingContext, currentFileNote);

    std::string logBlock;
    if (!inputProblems.empty()) {
        logBlock += "\nProblems:\n" + limitLogLines(inputProblems, m_problemMaxLines);
    }
    if (m_logger->should_log(spdlog::level::debug) && !rollingContext.empty()) {
        logBlock += "\nRollingContext:\n" + rollingContext + "\n";
    }
    if (m_logger->should_log(spdlog::level::debug) && !currentFileNote.empty()) {
        logBlock += "\nFileNote:\n" + currentFileNote.dump(2) + "\n";
    }
    if (!glossary.empty()) {
        logBlock += "\nGlossary:\n" + limitLogLines(glossary, m_glossaryMaxLines);
    }
    if (m_logger->should_log(spdlog::level::debug) && knownTerms != "[]") {
        logBlock += "\nKnownTerms:\n" + knownTerms;
    }
    logBlock += "\ninputBlock:\n" + limitLogLines(inputBlock, m_inputBlockMaxLines);
    return logBlock;
}

// 构造当前 chunk 和滚动上下文对应的模型 messages。
json NormalJsonTranslatorTransAgent::buildBaseMessages(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    const std::string& rollingContext
) {
    std::string inputBlock;
    fillBlockAndMap(pending, inputBlock, m_transEngine);
    const std::string inputProblems = buildProblemSummary(pending);
    const std::string glossary = buildGlossary(pending);
    const json currentFileNote = loadFileNote(relInputPath);
    const std::string knownTerms = buildTermLedgerExcerpt(loadTermLedger(), inputBlock, rollingContext, currentFileNote);
    constexpr std::string_view schemaDescription =
        "{"
        "\"schema\":\"gpp-agent-v1\","
        "\"action\":\"tool_calls|commit|compact_context\","
        "\"calls\":[],"
        "\"translations\":[],"
        "\"term_updates\":[],"
        "\"agent_suggest\":[],"
        "\"file_note_patch\":{},"
        "\"rolling_context\":\"\""
        "}";

    std::string userPrompt = m_agentUserPrompt;
    replaceStrInplace(userPrompt, "[AgentCurrentFile]", wide2Ascii(relInputPath));
    replaceStrInplace(userPrompt, "[AgentChunkIdRange]", std::format("{}-{}", pending.front()->index, pending.back()->index));
    replaceStrInplace(userPrompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(userPrompt, "[AgentTargetLang]", m_targetLang);
    replaceStrInplace(userPrompt, "[AgentProblemDescription]", inputProblems.empty() ? "None" : inputProblems);
    replaceStrInplace(userPrompt, "[AgentGlossary]", glossary.empty() ? "None" : glossary);
    replaceStrInplace(userPrompt, "[AgentFileNote]", currentFileNote.empty() ? "None" : currentFileNote.dump());
    replaceStrInplace(userPrompt, "[AgentRollingContext]", rollingContext.empty() ? "None" : rollingContext);
    replaceStrInplace(userPrompt, "[AgentKnownTerms]", knownTerms);
    replaceStrInplace(userPrompt, "[AgentCurrentChunkTsv]", inputBlock);
    replaceStrInplace(userPrompt, "[AgentSchemaDescription]", schemaDescription);

    return json::array({
        {{"role", "system"}, {"content", m_agentSystemPrompt}},
        {{"role", "user"}, {"content", userPrompt}}
    });
}

// 校验 Agent 提交结果，并应用句子译文、术语账本、文件备注和 Agent 建议副作用。
int NormalJsonTranslatorTransAgent::applyCommit(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    std::string& rollingContext,
    int threadId,
    const TransAgentProtocolResponse& protocol,
    const std::string& modelName,
    const std::string& batchIndexLog,
    int turn,
    int requestCount,
    std::string& commitResultLog,
    int& recordedTermUpdateCount,
    int& recordedSuggestionCount
) {
    recordedTermUpdateCount = 0;
    recordedSuggestionCount = 0;

    absl::flat_hash_map<int, json> translationMap;
    for (const auto& item : protocol.translations) {
        if (!item.is_object()) {
            continue;
        }
        const auto idOpt = item.contains("id") ? parseAgentCommonJsonInt(item["id"]) : std::nullopt;
        const int id = idOpt.value_or(-1);
        const std::string dst = item.value("dst", "");
        if (id >= 0 && !dst.empty()) {
            translationMap.insert_or_assign(id, item);
        }
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
            throw std::runtime_error(gppTr(
                "NormalJsonTranslatorTransAgent.applyCommit",
                "提交结果缺少句子 %1")
                .arg(se->index)
                .toStdString());
        }
        const std::string dst = it->second.value("dst", "");
        if (dst.empty()) {
            throw std::runtime_error(gppTr(
                "NormalJsonTranslatorTransAgent.applyCommit",
                "提交结果中句子 %1 的 dst 为空")
                .arg(se->index)
                .toStdString());
        }
        sentencePatches.push_back({
            .sentence = se,
            .dst = dst
        });
    }
    
    for (const auto& patch : sentencePatches) {
        commitResultLog += std::format("id: {}, dst: {}\n", patch.sentence->index, patch.dst);
    }

    int committedCount = 0;
    for (const auto& patch : sentencePatches) {
        patch.sentence->transby = modelName;
        patch.sentence->transraw = patch.dst;
        patch.sentence->transCompleted = true;
        ++committedCount;
    }

    if (!protocol.rollingContext.empty()) {
        rollingContext = protocol.rollingContext;
    }

    const absl::flat_hash_set<int> currentChunkIds = pending
        | std::views::transform([](Sentence* se) { return se->index; })
        | std::ranges::to<absl::flat_hash_set<int>>();
    auto addSuggestionFunc = [&](const fs::path& file, int id, const std::string& problem)
        {
            std::vector<std::string>& problems = m_agentSuggestions[file][id];
            if (!std::ranges::contains(problems, problem)) {
                problems.push_back(problem);
                ++recordedSuggestionCount;
            }
        };

    try {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!protocol.termUpdates.empty()) {
            for (const auto& updateItem : protocol.termUpdates) {
                if (!updateItem.is_object()) {
                    continue;
                }
                const std::string sourceTerm = updateItem.value("source_term", "");
                const std::string targetTerm = updateItem.value("target_term", "");
                if (sourceTerm.empty() || targetTerm.empty()) {
                    continue;
                }
                ++recordedTermUpdateCount;
                json& termItem = m_termLedgerCache[sourceTerm];
                if (!termItem.is_object()) {
                    termItem = json::object();
                }
                const std::string oldTarget = termItem.value("target_term", "");
                termItem["target_term"] = targetTerm;
                termItem["status"] = updateItem.value("status", termItem.value("status", "tentative"));
                termItem["category"] = updateItem.value("category", termItem.value("category", ""));
                termItem["note"] = updateItem.value("note", termItem.value("note", ""));

                if (!oldTarget.empty() && oldTarget != targetTerm) {
                    if (const auto it = termItem.find("occurrences"); it != termItem.end() && it->is_array()) {
                        const std::string suggestion = formatTermUpdateSuggestion(sourceTerm, oldTarget, targetTerm);
                        for (const auto& occurrence : *it) {
                            const std::string occurrenceFile = occurrence.value("file", "");
                            const int occurrenceId = occurrence.value("id", -1);
                            if (occurrenceFile.empty() || occurrenceId < 0) {
                                continue;
                            }
                            addSuggestionFunc(ascii2Wide(occurrenceFile), occurrenceId, suggestion);
                        }
                    }
                }

                // 出现记录只保存“这次处理当前分块时，术语在哪些句子里被提交命中”。
                // 如果模型显式给了 line_ids，就只接受当前 chunk 内的那些 id；
                // 否则退回到本地按 source_term 在当前 pending 句子中做一次轻量推断。
                // 这里不会去跨文件全文扫描，也不会把出现记录当成严格完整的全局出现表。
                if (const auto it = updateItem.find("line_ids"); it != updateItem.end() && it->is_array()) {
                    for (const auto& idVal : *it) {
                        const auto idOpt = parseAgentCommonJsonInt(idVal);
                        if (idOpt.has_value() && currentChunkIds.contains(idOpt.value())) {
                            addToTermOccurrence(termItem, relInputPath, idOpt.value());
                        }
                    }
                }
                else {
                    const std::vector<int> inferredIds = inferOccurrenceIdsFromChunk(sourceTerm, pending);
                    for (const int id : inferredIds) {
                        addToTermOccurrence(termItem, relInputPath, id);
                    }
                }
            }
            atomicOutputFile(m_agentTermLedgerPath, m_termLedgerCache.dump(2));
        }

        for (const auto& suggestionItem : protocol.agentSuggestions) {
            const auto target = parseAgentSuggestTarget(suggestionItem);
            if (!target.has_value()) {
                continue;
            }
            const std::string suggestion = suggestionItem.value("suggestion", "");
            if (!suggestion.empty()) {
                addSuggestionFunc(target->first, target->second, suggestion);
            }
        }
    }
    catch (const std::exception& e) {
        m_logger->warn(gppTr(
            "NormalJsonTranslatorTransAgent.applyCommit",
            "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 译文已提交，但术语账本/建议写入中途出现异常，本次不重新请求: %6")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(batchIndexLog)
            .arg(turn + 1)
            .arg(requestCount + 1)
            .arg(e.what())
            .toStdString());
    }

    try {
        json fileNote = loadFileNote(relInputPath);
        mergeFileNotePatch(fileNote, protocol.fileNotePatch);
        fileNote["updated_at"] = currentTimestampString();
        std::lock_guard<std::mutex> lock(m_fileNotesMutex);
        atomicOutputFile(m_agentFileNotesDir / relInputPath, fileNote.dump(2));
        m_fileNoteCache.insert_or_assign(relInputPath, fileNote);
    }
    catch (const std::exception& e) {
        m_logger->warn(gppTr(
            "NormalJsonTranslatorTransAgent.applyCommit",
            "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 译文已提交，但 file note 写入中途出现异常，本次不重新请求: %6")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(batchIndexLog)
            .arg(turn + 1)
            .arg(requestCount + 1)
            .arg(e.what())
            .toStdString());
    }

    return committedCount;
}


// 执行一个翻译 chunk 的完整 Agent 工作流。
bool NormalJsonTranslatorTransAgent::translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& rollingContext,
    int threadId, int batchIndex)
{
    // 智能体模式复用外层批次调度，但单个分块内可能进行多轮交互：
    // 先发起工具调用，再按需压缩上下文，最后提交通过校验的 `commit`。
    //
    // 流程导览（示例）：
    // 1. 构造时已经把 gt_input/chapter01.json 解析成 AgentCommonSourceFileView：
    //    lines[20] == { id:20, speaker:"アリス", sourceText:"おはよう" }。
    //    list_files 的 lines 就来自 sourceView.lines.size()，search/read 工具都读这份只读视图。
    // 2. processFile() 按 m_batchSize 把待翻译句子切成 chunk；例如当前 chunk 是 id 20-39。
    //    本函数只处理这个 chunk，外层文件内 batch 仍然顺序执行，rollingContext 负责传入/带出滚动上下文。
    // 3. buildAgentBaseMessages() 把当前 chunk TSV、file_note、term_ledger 摘要、rollingContext 和工具说明拼成 messages。
    // 4. 模型可以先返回 tool_calls：
    //    { "action":"tool_calls", "calls":[
    //      { "name":"list_files", "arguments":{"start":0,"limit":20} },
    //      { "name":"read_lines", "arguments":{"file":"chapter01.json","start":40,"count":5} },
    //      { "name":"search_term", "arguments":{"query":"アリス","limit":5} }
    //    ] }
    //    executeToolCalls() 会同步执行这些只读工具，把结果作为新的 user message 回填给下一轮模型。
    // 5. 模型最终必须返回 commit：
    //    { "action":"commit", "translations":[{"id":20,"translation":"早上好"}],
    //      "term_updates":[{"source_term":"アリス","target_term":"爱丽丝","line_ids":[20]}],
    //      "file_note_patch":{"summary":"..."},"rolling_context":"..." }
    //    translations 的 id 可以是数字或数字字符串；它和 sourceView.lines 的 id、list_files 的 lines 范围、
    //    read_lines.start / search result id 都在同一个翻译索引空间内。
    //    term_updates 写入共享 term_ledger；file_note_patch 合并到 file_notes/chapter01.json；
    //    rolling_context 更新 rollingContext。
    // 6. 若某个 term 的 target_term 变化，applyCommit() 会基于 term_ledger 里已有 occurrences
    //    给受影响句子的缓存追加 Agent 建议，供后续人工检查或流程处理。
    for (Sentence* se : batch) {
        if (se->preproc.empty()) {
            se->transCompleted = true;
        }
    }

    bool retryExhausted = false;
    const std::string batchIndexLog = std::format("{}", batchIndex);

    std::vector<Sentence*> pending = collectPendingSentences(batch);
    if (pending.empty()) {
        return true;
    }
    std::span<Sentence*> pendingSpan(pending);
    json messages = buildBaseMessages(relInputPath, pendingSpan, rollingContext);

    const std::string logBlock = buildLogBlock(relInputPath, pendingSpan, rollingContext);
    m_logger->info(gppTr(
        "NormalJsonTranslatorTransAgent.translateBatch",
        "[线程 %1] [文件 %2] [批次 %3] Agent 开始翻译，最多 %4 轮，共 %5 句:\n%6")
        .arg(threadId)
        .arg(wide2Ascii(relInputPath))
        .arg(batchIndexLog)
        .arg(m_agentMaxTurnsPerChunk)
        .arg(pending.size())
        .arg(logBlock)
        .toStdString());

    int turn = 0;
    int requestCount = 0;
    for (; turn < m_agentMaxTurnsPerChunk; ++turn) {
        requestCount = 0;
        bool turnCompleted = false;
        size_t messageBytes = approximateAgentCommonMessagesBytes(messages);
        if (messageBytes > (size_t)m_agentCompactContextThresholdBytes) {
            m_logger->info(gppTr(
                "NormalJsonTranslatorTransAgent.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] Agent 上下文接近上限，要求模型先压缩上下文")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(turn + 1)
                .toStdString());
            messages.push_back({
                {"role", "user"},
                {"content", "Context exceeded the compact context threshold. Return a compact_context action only. Do not call tools or commit in this turn."}
            });
            messageBytes = approximateAgentCommonMessagesBytes(messages);
        }

        while (requestCount < m_maxRequestCount) {
            if (m_controller->shouldStop()) {
                return false;
            }

            pending = collectPendingSentences(batch);
            if (pending.empty()) {
                return true;
            }
            pendingSpan = std::span<Sentence*>(pending);

            const std::optional<TranslationApi> apiOpt = m_apiPool->getApi(m_apiStrategy);
            if (!apiOpt.has_value()) {
                throw std::runtime_error(gppTr(
                    "NormalJsonTranslatorTransAgent.translateBatch",
                    "没有可用的 Api key 了")
                    .toStdString());
            }
            const TranslationApi& currentApi = apiOpt.value();

            json payload = { {"messages", messages} };

            m_logger->info(gppTr(
                "NormalJsonTranslatorTransAgent.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 开始请求，剩余 %6 句，上下文 %7 字节")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(turn + 1)
                .arg(requestCount + 1)
                .arg(pending.size())
                .arg(messageBytes)
                .toStdString());

            ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger,
                threadId, m_apiTimeOutMs);

            const std::string checkResponseLogPrefix = gppTr(
                "NormalJsonTranslatorTransAgent.translateBatch",
                "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5]")
                .arg(threadId)
                .arg(wide2Ascii(relInputPath))
                .arg(batchIndexLog)
                .arg(turn + 1)
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
                    "NormalJsonTranslatorTransAgent.translateBatch",
                    "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 成功响应，响应内容:\n%6")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .arg(batchIndexLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(response.content)
                    .toStdString());
            }

            std::expected<TransAgentTurnResult, std::string> turnResult = parseAndApplyTurnResponse(
                relInputPath,
                pendingSpan,
                rollingContext,
                messages,
                response.content,
                makeTransby(currentApi.apikey, currentApi.modelName),
                batchIndexLog,
                turn,
                requestCount,
                threadId
            );
            if (turnResult.has_value()) {
                m_logger->info(gppTr(
                    "NormalJsonTranslatorTransAgent.translateBatch",
                    "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 响应处理成功，处理结果:\n%6")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .arg(batchIndexLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(turnResult->summary)
                    .toStdString());

                if (turnResult->action == TransAgentTurnResult::Action::CompleteBatch) {
                    return true;
                }
            }
            else {
                m_logger->warn(gppTr(
                    "NormalJsonTranslatorTransAgent.translateBatch",
                    "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] [请求 %5] Agent 响应处理失败，错误: %6，响应内容:\n%7")
                    .arg(threadId)
                    .arg(wide2Ascii(relInputPath))
                    .arg(batchIndexLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(turnResult.error())
                    .arg(response.content.empty()
                        ? gppTr("NormalJsonTranslatorTransAgent.translateBatch", "内容为空").toStdString()
                        : limitLogLines(response.content, m_inputBlockMaxLines))
                    .toStdString());
                m_controller->recordRuntimeTransError(RuntimeTransErrorEvent{
                    .kind = "agent",
                    .level = "warning",
                    .message = gppTr("NormalJsonTranslatorTransAgent.translateBatch", "Agent 响应处理失败: %1")
                        .arg(turnResult.error())
                        .toStdString(),
                    .filename = wide2Ascii(relInputPath),
                    .indexRange = std::format("{}-{}", pending.front()->index, pending.back()->index),
                    .requestCount = requestCount + 1,
                    .model = makeTransby(currentApi.apikey, currentApi.modelName),
                    .sleepSeconds = -1.0
                });
                ++requestCount;
                continue;
            }

            turnCompleted = true;
            break;
        }

        if (!turnCompleted) {
            retryExhausted = true;
            break;
        }
    }

    size_t failedCount = 0;
    for (Sentence* se : batch | std::views::filter([](Sentence* se_) { return !se_->transCompleted; })) {
        ++failedCount;
        se->transraw = "(Failed to translate)" + se->preproc;
        se->transCompleted = true;
    }
    if (!retryExhausted) {
        m_logger->error(gppTr(
            "NormalJsonTranslatorTransAgent.translateBatch",
            "[线程 %1] [文件 %2] [批次 %3] Agent 因超过最大轮数 (%4 轮) 而失败，共翻译 (%5 / %6) 句")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(batchIndexLog)
            .arg(m_agentMaxTurnsPerChunk)
            .arg(batch.size() - failedCount)
            .arg(batch.size())
            .toStdString());
    }
    else {
        m_logger->error(gppTr(
            "NormalJsonTranslatorTransAgent.translateBatch",
            "[线程 %1] [文件 %2] [批次 %3] [轮次 %4] Agent 在 %5 次请求后彻底失败，共翻译 (%6 / %7) 句")
            .arg(threadId)
            .arg(wide2Ascii(relInputPath))
            .arg(batchIndexLog)
            .arg(turn + 1)
            .arg(m_maxRequestCount)
            .arg(batch.size() - failedCount)
            .arg(batch.size())
            .toStdString());
    }
    return false;
}

// 将 commit 中收集到的 Agent 建议写入翻译缓存。
void NormalJsonTranslatorTransAgent::applyAgentSuggestions() {
    if (m_agentSuggestions.empty()) {
        return;
    }

    int markedCount = 0;
    std::ifstream ifs;
    for (const auto& [relFilePath, suggestionsByIndex] : m_agentSuggestions) {
        if (!m_savedTranslCacheRelFilePaths.contains(relFilePath)) {
            continue;
        }
        const fs::path cachePath = m_transCacheDir / relFilePath;
        if (!fs::exists(cachePath)) {
            m_logger->warn(gppTr(
                "NormalJsonTranslatorTransAgent.applyAgentSuggestions",
                "Agent 建议目标 [%1] 没有缓存文件，已跳过")
                .arg(wide2Ascii(relFilePath))
                .toStdString());
            continue;
        }

        json cacheJson;
        try {
            cacheJson = parseJson(cachePath, ifs);
        }
        catch (const std::exception& e) {
            m_logger->warn(gppTr(
                "NormalJsonTranslatorTransAgent.applyAgentSuggestions",
                "Agent 建议目标缓存 [%1] 读取失败: %2")
                .arg(wide2Ascii(relFilePath))
                .arg(e.what())
                .toStdString());
            continue;
        }

        bool cacheChanged = false;
        for (json& item : cacheJson) {
            const int index = item.value("index", -1);
            const auto suggestionIt = suggestionsByIndex.find(index);
            if (suggestionIt == suggestionsByIndex.end()) {
                continue;
            }
            if (!item.contains("problems") || !item["problems"].is_array()) {
                item["problems"] = json::array();
            }
            for (const std::string& problem : suggestionIt->second) {
                item["problems"].push_back(problem);
                ++markedCount;
                cacheChanged = true;
            }
        }

        if (!cacheChanged) {
            continue;
        }

        atomicOutputFile(cachePath, cacheJson.dump(2));
    }

    if (markedCount > 0) {
        m_logger->info(gppTr(
            "NormalJsonTranslatorTransAgent.applyAgentSuggestions",
            "Agent 已将 %1 条建议写入缓存问题")
            .arg(markedCount)
            .toStdString());
    }
}
