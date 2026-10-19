module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Shlwapi.h>
#endif

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

// 保存本轮翻译 Agent 的运行依赖，并构建工具要读取的源文件视图。
NormalJsonTranslatorTransAgent::NormalJsonTranslatorTransAgent(
    TransEngine transEngine,
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::unique_ptr<ApiPool>& apiPool,
    const std::unique_ptr<GptDictionary>& gptDictionary,
    const std::function<std::string(std::string_view)>& onPerformApi,
    const fs::path& projectDir,
    const absl::flat_hash_map<fs::path, ordered_json>& inputJsonMap,
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
    int enhanceJailbreak,
    std::shared_mutex& transCacheMutex,
    absl::flat_hash_map<fs::path, json>& savedTranslCacheMap,
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
    m_enhanceJailbreak(enhanceJailbreak),
    m_transCacheMutex(transCacheMutex),
    m_savedTranslCacheMap(savedTranslCacheMap),
    m_knownRelFiles(knownRelFiles),
    m_gptDictionaryPaths(gptDictionaryPaths),
    m_agentProjectNotePath(agentProjectNotePath)
{
    std::ranges::sort(m_knownRelFiles, [](const fs::path& a, const fs::path& b)
        {
#ifdef _WIN32
            return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
#else
            return a < b;
#endif
        });

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

    for (const fs::path& relFilePath : m_knownRelFiles) {
        const ordered_json& inputJson = inputJsonMap.at(relFilePath);
        try {
            m_sourceFileViews.insert_or_assign(
                relFilePath,
                buildAgentCommonSourceFileViewFromJson(inputJson, preProcessFunc)
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

// 解析 Agent 建议目标；file 为空时默认使用当前文件。
std::optional<std::pair<fs::path, int>> NormalJsonTranslatorTransAgent::parseAgentSuggestTarget(
    const json& suggestion, const fs::path& defaultRelPath) const
{
    if (!suggestion.is_object()) {
        return std::nullopt;
    }
    const std::string file = suggestion.value("file", "");
    const auto idOpt = suggestion.contains("id") ? parseAgentCommonJsonInt(suggestion["id"]) : std::nullopt;
    if (!idOpt.has_value() || idOpt.value() < 0) {
        return std::nullopt;
    }
    const fs::path targetPath = file.empty() ? defaultRelPath : fs::path(ascii2Wide(file));
    if (targetPath.empty()) {
        return std::nullopt;
    }
    return std::make_pair(targetPath, idOpt.value());
}

// 读取翻译 Agent 可搜索的 GPT 字典项。
std::vector<NormalJsonTranslatorTransAgent::LoadedDictionaryEntry> NormalJsonTranslatorTransAgent::loadDictionaryEntries() const {
    std::vector<LoadedDictionaryEntry> entries;
    for (const fs::path& dictPath : m_gptDictionaryPaths) {
        const auto dictData = gpp::uparseToml(dictPath);
        if (!dictData.contains("gptDict")) {
            continue;
        }
        const auto& dictTbls = dictData.at("gptDict").as_array();
        for (const auto& el : dictTbls) {
            if (!el.contains("org") || !el.contains("rep")) {
                continue;
            }
            const std::string& sourceTerm = el.at("org").as_string();
            const std::string& targetTerm = el.at("rep").as_string();
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
    struct TermExcerptRow {
        std::string src;
        std::string dst;
        std::string status;
        std::string category;
        std::string note;
    };
    std::vector<TermExcerptRow> relevant;
    std::vector<TermExcerptRow> fallback;
    const std::string inputLower = str2Lower(currentInputBlock);
    const std::string rollingContextLower = str2Lower(rollingContext);
    const std::string fileNoteLower = str2Lower(fileNote.dump());
    for (const auto& item : ledger.items()) {
        const std::string& term = item.key();
        const json& entry = item.value();
        TermExcerptRow row{
            .src = term,
            .dst = entry.value("dst", ""),
            .status = entry.value("status", "tentative"),
            .category = entry.value("category", ""),
            .note = entry.value("note", "")
        };

        const std::string termLower = str2Lower(row.src);
        const std::string targetLower = str2Lower(row.dst);
        const std::string noteLower = str2Lower(row.note);
        const bool isRelevant =
            (!inputLower.empty() && inputLower.contains(termLower)) ||
            (!rollingContextLower.empty() && (rollingContextLower.contains(termLower) ||
            (!targetLower.empty() && rollingContextLower.contains(targetLower)))) ||
            (!fileNoteLower.empty() && (fileNoteLower.contains(termLower) ||
            (!targetLower.empty() && fileNoteLower.contains(targetLower)) ||
            (!noteLower.empty() && fileNoteLower.contains(noteLower))));

        if (isRelevant) {
            relevant.push_back(std::move(row));
            continue;
        }
        if ((int)fallback.size() < m_agentSearchResultLimit) {
            fallback.push_back(std::move(row));
        }
    }

    const auto& chosen = relevant.empty() ? fallback : relevant;
    if (chosen.empty()) {
        return {};
    }

    std::string content;
    switch (m_transEngine) {
    case TransEngine::ForGalJson:
    case TransEngine::ForNovelJson:
        content = "| src | dst | status | category | note |\n| --- | --- | --- | --- | --- |\n";
        for (const auto& r : chosen) {
            content += std::format("| {} | {} | {} | {} | {} |\n", r.src, r.dst, r.status, r.category, r.note);
        }
        return content;

    case TransEngine::ForGalTsv:
    case TransEngine::ForNovelTsv:
    case TransEngine::NameTrans:
        content = "SRC\tDST\tSTATUS\tCATEGORY\tNOTE\n";
        for (const auto& r : chosen) {
            content += std::format("{}\t{}\t{}\t{}\t{}\n", r.src, r.dst, r.status, r.category, r.note);
        }
        return content;

    default:
        throw std::runtime_error(gppTr("GptDictionary.getPrompt", "内部错误: 无效的提示词类型").toStdString());
    }
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
    std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
    json cacheJson;
    if (const auto it = m_savedTranslCacheMap.find(targetRelPath); it != m_savedTranslCacheMap.end()) {
        cacheJson = it->second;
    }
    else if (fs::exists(cachePath)) {
        cacheJson = parseJson(cachePath);
    }
    else return cacheMap;
    for (const auto& item : cacheJson) {
        const int index = item.value("index", -1);
        if (index >= 0) {
            cacheMap[index] = item;
        }
    }
    return cacheMap;
}

// Agent 按翻译格式构造输入；小说格式不包含姓名。
std::string NormalJsonTranslatorTransAgent::buildInputBlock(std::span<Sentence*> pending) const {
    std::string inputBlock;
    fillBlockAndMap(pending, inputBlock, m_transEngine);
    return inputBlock;
}

// 构造 Agent 开始请求前写入日志的可读诊断块。
std::string NormalJsonTranslatorTransAgent::buildLogBlock(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    const std::string& rollingContext
) {
    const std::string inputBlock = buildInputBlock(pending);

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
    if (m_logger->should_log(spdlog::level::debug) && !knownTerms.empty()) {
        logBlock += "\nKnownTerms:\n" + knownTerms;
    }
    logBlock += "\ninputBlock:\n" + limitLogLines(inputBlock, m_inputBlockMaxLines);
    return logBlock;
}

// 构造当前批次的用户提示词；摘要和文件备注仅用于筛选相关术语，不直接注入。
std::string NormalJsonTranslatorTransAgent::buildUserPrompt(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    const std::string& rollingContext
) {
    const std::string inputBlock = buildInputBlock(pending);
    const std::string inputProblems = buildProblemSummary(pending);
    const std::string glossary = buildGlossary(pending);
    const json currentFileNote = loadFileNote(relInputPath);
    const std::string knownTerms = buildTermLedgerExcerpt(loadTermLedger(), inputBlock, rollingContext, currentFileNote);
    std::string prompt = m_agentUserPrompt;
    replaceStrInplace(prompt, "[AgentCurrentFile]", wide2Ascii(relInputPath));
    replaceStrInplace(prompt, "[AgentChunkIdRange]", std::format("{}-{}", pending.front()->index, pending.back()->index));
    replaceStrInplace(prompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(prompt, "[AgentTargetLang]", m_targetLang);
    replaceStrInplace(prompt, "[AgentProblemDescription]", inputProblems.empty() ? "None" : inputProblems);
    replaceStrInplace(prompt, "[AgentGlossary]", glossary.empty() ? "None" : glossary);
    replaceStrInplace(prompt, "[AgentKnownTerms]", knownTerms.empty() ? "None" : knownTerms);
    replaceStrInplace(prompt,
        m_transEngine == TransEngine::ForGalTsv || m_transEngine == TransEngine::ForNovelTsv
            ? "[AgentCurrentChunkTsv]" : "[AgentCurrentChunkJson]", inputBlock);
    return prompt;
}

// 校验 Agent 提交结果，并应用句子译文、术语账本、文件备注和 Agent 建议副作用。
int NormalJsonTranslatorTransAgent::applyCommit(
    const fs::path& relInputPath,
    std::span<Sentence*> pending,
    int threadId,
    const json& arguments,
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
    for (const auto& item : arguments.at("translations")) {
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
        commitResultLog += std::format("{}\t{}\t{}\n",
            patch.sentence->nameType != NameType::None ? getNameString(*patch.sentence) : "null",
            patch.dst, patch.sentence->index);
    }

    int committedCount = 0;
    for (const auto& patch : sentencePatches) {
        patch.sentence->transby = modelName;
        patch.sentence->transraw = patch.dst;
        patch.sentence->transCompleted = true;
        ++committedCount;
    }

    const absl::flat_hash_set<int> currentChunkIds = pending
        | std::views::transform([](Sentence* se) { return se->index; })
        | std::ranges::to<absl::flat_hash_set<int>>();
    auto addSuggestionFunc = [&](const fs::path& file, int id, const std::string& suggestion)
        {
            std::vector<std::string>& suggestions = m_agentSuggestions[file][id];
            if (!std::ranges::contains(suggestions, suggestion)) {
                suggestions.push_back(suggestion);
                ++recordedSuggestionCount;
            }
        };

    try {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!arguments.at("term_updates").empty()) {
            for (const auto& updateItem : arguments.at("term_updates")) {
                if (!updateItem.is_object()) {
                    continue;
                }
                const std::string sourceTerm = updateItem.value("src", "");
                const std::string targetTerm = updateItem.value("dst", "");
                if (sourceTerm.empty() || targetTerm.empty()) {
                    continue;
                }
                ++recordedTermUpdateCount;
                json& termItem = m_termLedgerCache[sourceTerm];
                if (!termItem.is_object()) {
                    termItem = json::object();
                }
                const std::string oldTarget = termItem.value("dst", "");
                termItem["dst"] = targetTerm;
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

        for (const auto& suggestionItem : arguments.at("agent_suggest")) {
            const auto target = parseAgentSuggestTarget(suggestionItem, relInputPath);
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
        mergeFileNotePatch(fileNote, arguments.at("file_note_patch"));
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


// 将 commit 中收集到的 Agent 建议写入翻译缓存。
void NormalJsonTranslatorTransAgent::applyAgentSuggestions() {
    if (m_agentSuggestions.empty()) {
        return;
    }

    int markedCount = 0;
    for (const auto& [relFilePath, suggestionsByIndex] : m_agentSuggestions) {
        const auto cacheIt = m_savedTranslCacheMap.find(relFilePath);
        if (cacheIt == m_savedTranslCacheMap.end()) {
            continue;
        }
        const fs::path cachePath = m_transCacheDir / relFilePath;
        json& cacheJson = cacheIt->second;

        bool cacheChanged = false;
        for (json& item : cacheJson) {
            const int index = item.value("index", -1);
            const auto suggestionIt = suggestionsByIndex.find(index);
            if (suggestionIt == suggestionsByIndex.end()) {
                continue;
            }
            auto otherinfo = item.value("other_info", absl::btree_map<std::string, std::string>{});
            otherinfo.insert_or_assign("gpp.agent.suggestion", suggestionIt->second
                | std::views::join_with('\n') | std::ranges::to<std::string>());
            item["other_info"] = std::move(otherinfo);
            markedCount += (int)suggestionIt->second.size();
            cacheChanged = true;
        }

        if (!cacheChanged) {
            continue;
        }

        std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
        atomicOutputFile(cachePath, cacheJson.dump(2));
    }

    if (markedCount > 0) {
        m_logger->info(gppTr(
            "NormalJsonTranslatorTransAgent.applyAgentSuggestions",
            "Agent 已将 %1 条建议写入缓存附加信息")
            .arg(markedCount)
            .toStdString());
    }
}

NAMESPACE_END(gpp)
