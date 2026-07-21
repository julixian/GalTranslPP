module;

#include "GPPMacros.hpp"
#include <ctpl_stl.h>

module DictionaryGenerator;

import :ReviewAgent;
import AgentToolCommon;
import AgentCommonSourceView;
import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

// 保存审校 Agent 本轮运行要用到的控制器、Api、提示词和工具限制。
DictionaryGeneratorReviewAgent::DictionaryGeneratorReviewAgent(
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::unique_ptr<ApiPool>& apiPool,
    const std::function<std::string(std::string)>& onPerformApi,
    const fs::path& projectDir,
    const std::vector<fs::path>& relJsonPaths,
    const std::optional<fs::path>& agentProjectNotePath,
    const std::string& genDictReviewSystemPrompt,
    const std::string& genDictReviewUserPrompt,
    const std::string& apiStrategy,
    const std::string& targetLang,
    int inputBlockMaxLines,
    int maxRequestCount,
    int threadsNum,
    int agentMaxTurnsPerChunk,
    int agentSearchResultLimit,
    int agentContextLinesLimit,
    int apiTimeOutMs,
    bool checkQuota
) : m_controller(controller),
    m_logger(logger),
    m_apiPool(apiPool),
    m_onPerformApi(onPerformApi),
    m_projectDir(projectDir),
    m_relJsonPaths(relJsonPaths),
    m_agentProjectNotePath(agentProjectNotePath),
    m_genDictReviewSystemPrompt(genDictReviewSystemPrompt),
    m_genDictReviewUserPrompt(genDictReviewUserPrompt),
    m_apiStrategy(apiStrategy),
    m_targetLang(targetLang),
    m_inputBlockMaxLines(inputBlockMaxLines),
    m_maxRequestCount(maxRequestCount),
    m_threadsNum(threadsNum),
    m_agentMaxTurnsPerChunk(agentMaxTurnsPerChunk),
    m_agentSearchResultLimit(agentSearchResultLimit),
    m_agentContextLinesLimit(agentContextLinesLimit),
    m_apiTimeOutMs(apiTimeOutMs),
    m_checkQuota(checkQuota)
{

}

// 将术语加入稳定输出顺序，重复项会被跳过。
void DictionaryGeneratorReviewAgent::rememberLedgerTermOrderLocked(const std::string& sourceTerm) {
    if (sourceTerm.empty()) {
        return;
    }
    if (m_ledgerTermOrderSeen.insert(sourceTerm).second) {
        m_ledgerTermOrder.push_back(sourceTerm);
    }
}

// 将模型返回的 term_update 合并到账本。
void DictionaryGeneratorReviewAgent::applyTermUpdateLocked(const json& update) {
    if (!update.is_object()) {
        return;
    }
    const std::string sourceTerm = update.value("source_term", "");
    const std::string status = update.value("status", "accepted");
    if (sourceTerm.empty()) {
        return;
    }
    if (status != "accepted" && status != "merged" && status != "deprecated" && status != "conflict") {
        return;
    }

    const std::string targetTerm = update.value("target_term", "");
    const std::string note = update.value("note", "");
    const std::string mergeInto = update.value("merge_into", "");

    json& entry = m_ledgerMap[sourceTerm];
    if (!entry.is_object()) {
        entry = json::object();
    }
    const std::string existingTarget = entry.value("target_term", "");
    if ((status == "accepted" || status == "conflict") && targetTerm.empty() && existingTarget.empty()) {
        return;
    }
    if (status == "merged" && mergeInto.empty()) {
        return;
    }
    if (status == "merged" || status == "deprecated") {
        entry["target_term"] = "";
    }
    else if (update.contains("target_term") || !entry.contains("target_term")) {
        entry["target_term"] = targetTerm.empty() ? existingTarget : targetTerm;
    }
    if (update.contains("note") || !entry.contains("note")) {
        entry["note"] = note;
    }
    entry["status"] = status;
    entry["merge_into"] = status == "merged" ? mergeInto : "";
    entry["origin"] = update.value("origin", entry.value("origin", "term_update"));
    entry["updated_at"] = currentTimestampString();
    m_knownSourceTerms.insert(sourceTerm);
    rememberLedgerTermOrderLocked(sourceTerm);
}

// 校验当前术语组的提交结果，并把结果写入账本。
void DictionaryGeneratorReviewAgent::applyCommitResultLocked(const DictionaryReviewTermGroup& group,
    const DictionaryReviewAgentCommitResult& decision)
{
    if (decision.sourceTerm != group.sourceTerm) {
        throw std::runtime_error(gppTr(
            "DictionaryGeneratorReviewAgent.applyCommitResult",
            "提交结果 source_term=%1 与当前术语 %2 不匹配")
            .arg(decision.sourceTerm)
            .arg(group.sourceTerm)
            .toStdString());
    }
    if (decision.status != "accepted" && decision.status != "merged" &&
        decision.status != "deprecated" && decision.status != "conflict") {
        throw std::runtime_error(gppTr(
            "DictionaryGeneratorReviewAgent.applyCommitResult",
            "无效状态: %1")
            .arg(decision.status)
            .toStdString());
    }
    if ((decision.status == "accepted" || decision.status == "conflict") && decision.finalTarget.empty()) {
        throw std::runtime_error(gppTr(
            "DictionaryGeneratorReviewAgent.applyCommitResult",
            "status=%1 时必须提供 final_target")
            .arg(decision.status)
            .toStdString());
    }
    if (decision.status == "conflict" && decision.finalNote.empty()) {
        throw std::runtime_error(gppTr(
            "DictionaryGeneratorReviewAgent.applyCommitResult",
            "status=conflict 时必须提供 final_note")
            .toStdString());
    }
    if (decision.status == "merged" && decision.mergeInto.empty()) {
        throw std::runtime_error(gppTr(
            "DictionaryGeneratorReviewAgent.applyCommitResult",
            "status=merged 时必须提供 merge_into")
            .toStdString());
    }

    if (decision.termUpdates.is_array()) {
        for (const auto& update : decision.termUpdates) {
            if (update.is_object()) {
                const std::string sourceTerm = update.value("source_term", "");
                if (!sourceTerm.empty()) {
                    m_knownSourceTerms.insert(sourceTerm);
                }
            }
        }
    }

    json& entry = m_ledgerMap[group.sourceTerm];
    const std::string existingNote = entry.is_object() ? entry.value("note", "") : "";

    if (decision.status == "accepted" || decision.status == "conflict") {
        entry = {
            {"target_term", decision.finalTarget},
            {"note", decision.finalNote.empty() ? existingNote : decision.finalNote},
            {"status", decision.status},
            {"merge_into", ""},
            {"origin", "group"},
            {"updated_at", currentTimestampString()}
        };
        rememberLedgerTermOrderLocked(group.sourceTerm);
    }
    else if (decision.status == "deprecated") {
        entry = {
            {"target_term", ""},
            {"note", decision.finalNote.empty() ? existingNote : decision.finalNote},
            {"status", "deprecated"},
            {"merge_into", ""},
            {"origin", "group"},
            {"updated_at", currentTimestampString()}
        };
        rememberLedgerTermOrderLocked(group.sourceTerm);
    }
    else {
        if (!m_knownSourceTerms.contains(decision.mergeInto)) {
            throw std::runtime_error(gppTr(
                "DictionaryGeneratorReviewAgent.applyCommitResult",
                "merge_into 指向未知术语: %1")
                .arg(decision.mergeInto)
                .toStdString());
        }
        entry = {
            {"target_term", ""},
            {"note", decision.finalNote},
            {"status", "merged"},
            {"merge_into", decision.mergeInto},
            {"origin", "group"},
            {"updated_at", currentTimestampString()}
        };
        rememberLedgerTermOrderLocked(group.sourceTerm);
    }

    if (!decision.termUpdates.is_array()) {
        return;
    }
    for (const auto& update : decision.termUpdates) {
        applyTermUpdateLocked(update);
    }
}

// 解析审校 Agent 的 JSON 文本协议。
DictionaryReviewAgentProtocolResponse DictionaryGeneratorReviewAgent::parseProtocolResponse(const std::string& content) const {
    DictionaryReviewAgentProtocolResponse result;
    const std::optional<json> payloadOpt = tryParseAgentCommonJsonEnvelope(content);
    if (!payloadOpt.has_value() || !payloadOpt->is_object()) {
        throw std::runtime_error(gppTr("DictionaryGeneratorReviewAgent.parseProtocolResponse",
            "字典审校 Agent 响应不是合法 JSON 对象")
            .toStdString());
    }

    const json& payload = *payloadOpt;
    const std::string schema = payload.value("schema", "");
    if (!schema.empty() && schema != "gpp-gendict-review-v1") {
        throw std::runtime_error(gppTr("DictionaryGeneratorReviewAgent.parseProtocolResponse",
            "无效的字典审校 Agent schema: %1")
            .arg(schema)
            .toStdString());
    }

    result.action = payload.value("action", "");
    result.calls = parseAgentCommonToolCallRequests(payload);
    if (result.action.empty()) {
        throw std::runtime_error(gppTr("DictionaryGeneratorReviewAgent.parseProtocolResponse",
            "字典审校 Agent 响应缺少动作字段")
            .toStdString());
    }
    if (result.action == "tool_calls" && result.calls.empty()) {
        throw std::runtime_error(gppTr("DictionaryGeneratorReviewAgent.parseProtocolResponse",
            "字典审校 Agent 返回了空工具调用")
            .toStdString());
    }
    if (result.action != "tool_calls" && result.action != "skip" && result.action != "commit") {
        throw std::runtime_error(gppTr("DictionaryGeneratorReviewAgent.parseProtocolResponse",
            "字典审校 Agent 返回未知动作: %1")
            .arg(result.action)
            .toStdString());
    }
    if (const auto it = payload.find("result"); it != payload.end() && it->is_object()) {
        result.result.sourceTerm = it->value("source_term", "");
        result.result.finalTarget = it->value("final_target", "");
        result.result.finalNote = it->value("final_note", "");
        result.result.status = it->value("status", "");
        result.result.mergeInto = it->value("merge_into", "");
        if (const auto updatesIt = it->find("term_updates"); updatesIt != it->end() && updatesIt->is_array()) {
            result.result.termUpdates = *updatesIt;
        }
    }
    return result;
}

// 从源文件视图中推断当前术语最可能属于哪个文件。
fs::path DictionaryGeneratorReviewAgent::guessCurrentFileForTerm(const std::string& sourceTerm) const {
    for (size_t index = 0; index < m_relJsonPaths.size(); ++index) {
        const AgentCommonSourceFileView& file = (*m_sourceFiles)[index];
        const bool matched = std::ranges::any_of(file.lines, [&](const AgentCommonSourceLineView& line)
            {
                return line.speaker.contains(sourceTerm) || line.sourceText.contains(sourceTerm);
            });
        if (matched) {
            return m_relJsonPaths[index];
        }
    }
    if (!m_relJsonPaths.empty()) {
        return m_relJsonPaths.front();
    }
    return {};
}

// 从候选译名里取最高频译名作为工具展示值。
std::string DictionaryGeneratorReviewAgent::candidateTargetForGroup(const DictionaryReviewTermGroup& group) const {
    if (!group.candidateTargets.empty()) {
        return group.candidateTargets.front().value;
    }
    return {};
}

// 从候选备注里取最高频备注作为工具展示值。
std::string DictionaryGeneratorReviewAgent::candidateNoteForGroup(const DictionaryReviewTermGroup& group) const {
    if (!group.candidateNotes.empty()) {
        return group.candidateNotes.front().value;
    }
    return {};
}

// 构造审校 Agent 当前术语的系统/用户消息。
json DictionaryGeneratorReviewAgent::buildBaseMessages(
    const DictionaryReviewTermGroup& group,
    const fs::path& currentFile,
    const json& ledgerExcerpt
) const {
    const auto frequenciesToJson = [](const std::vector<DictionaryReviewValueFrequency>& values)
        {
            json result = json::array();
            for (const auto& value : values) {
                result.push_back({
                    {"value", value.value},
                    {"count", value.count}
                });
            }
            return result;
        };

    const json currentTerm = {
        {"sourceTerm", group.sourceTerm},
        {"candidateTargets", frequenciesToJson(group.candidateTargets)},
        {"candidateNotes", frequenciesToJson(group.candidateNotes)},
        {"occurrenceCount", group.occurrenceCount},
        {"sampleSegments", group.sampleSegments},
        {"isNameHint", group.isNameHint},
        {"isTokenizerWord", group.isTokenizerWord}
    };

    constexpr std::string_view schemaDescription =
        "{"
        "\"schema\":\"gpp-gendict-review-v1\","
        "\"action\":\"tool_calls|commit|skip\","
        "\"calls\":[],"
        "\"result\":{"
        "\"source_term\":\"\","
        "\"final_target\":\"\","
        "\"final_note\":\"\","
        "\"status\":\"accepted|merged|deprecated|conflict\","
        "\"merge_into\":\"\","
        "\"term_updates\":[]"
        "}"
        "}";

    std::string userPrompt = m_genDictReviewUserPrompt;
    replaceStrInplace(userPrompt, "[TargetLang]", m_targetLang);
    replaceStrInplace(userPrompt, "[ReviewSchemaDescription]", schemaDescription);
    replaceStrInplace(userPrompt, "[ReviewCurrentTerm]", currentTerm.dump(2));
    replaceStrInplace(userPrompt, "[ReviewRecentTermsExcerpt]", ledgerExcerpt.dump(2));
    replaceStrInplace(userPrompt, "[ReviewAnchorFile]", currentFile.empty() ? "None" : wide2Ascii(currentFile));

    return json::array({
        {{"role", "system"}, {"content", m_genDictReviewSystemPrompt}},
        {{"role", "user"}, {"content", userPrompt}}
    });
}

// 把候选术语组转换成 search_dictionary 的一行结果。
json DictionaryGeneratorReviewAgent::candidateToDictionarySearchJson(
    const DictionaryReviewTermGroup& group,
    const json* reviewedEntry
) const {
    const auto frequenciesToJson = [](const std::vector<DictionaryReviewValueFrequency>& values)
        {
            json result = json::array();
            for (const auto& value : values) {
                result.push_back({
                    {"value", value.value},
                    {"count", value.count}
                });
            }
            return result;
        };

    json result = {
        {"source_term", group.sourceTerm},
        {"reviewed", reviewedEntry != nullptr},
        {"candidate_targets", frequenciesToJson(group.candidateTargets)},
        {"candidate_notes", frequenciesToJson(group.candidateNotes)},
        {"occurrence_count", group.occurrenceCount},
        {"is_name_hint", group.isNameHint},
        {"is_tokenizer_word", group.isTokenizerWord}
    };

    if (reviewedEntry != nullptr && reviewedEntry->is_object()) {
        result["target_term"] = reviewedEntry->value("target_term", "");
        result["note"] = reviewedEntry->value("note", "");
        result["status"] = reviewedEntry->value("status", "");
        result["merge_into"] = reviewedEntry->value("merge_into", "");
        result["origin"] = reviewedEntry->value("origin", "");
    }
    else {
        result["target_term"] = candidateTargetForGroup(group);
        result["note"] = candidateNoteForGroup(group);
        result["status"] = "pending";
        result["merge_into"] = "";
        result["origin"] = "candidate";
    }
    return result;
}

// 把只存在于审校账本的术语转换成 search_dictionary 的一行结果。
json DictionaryGeneratorReviewAgent::ledgerOnlyToDictionarySearchJson(const std::string& sourceTerm, const json& entry) const {
    return json{
        {"source_term", sourceTerm},
        {"reviewed", true},
        {"candidate_targets", json::array()},
        {"candidate_notes", json::array()},
        {"occurrence_count", 0},
        {"is_name_hint", false},
        {"is_tokenizer_word", false},
        {"target_term", entry.value("target_term", "")},
        {"note", entry.value("note", "")},
        {"status", entry.value("status", "")},
        {"merge_into", entry.value("merge_into", "")},
        {"origin", entry.value("origin", "")}
    };
}

// 构造候选术语搜索用的小写文本。
std::string DictionaryGeneratorReviewAgent::candidateSearchHaystack(
    const DictionaryReviewTermGroup& group,
    const json* reviewedEntry
) const {
    std::string haystack = group.sourceTerm + "\n";
    if (reviewedEntry != nullptr && reviewedEntry->is_object()) {
        haystack += reviewedEntry->value("target_term", "") + "\n";
        haystack += reviewedEntry->value("note", "") + "\n";
        haystack += reviewedEntry->value("status", "") + "\n";
        haystack += reviewedEntry->value("merge_into", "") + "\n";
    }
    for (const DictionaryReviewValueFrequency& candidate : group.candidateTargets) {
        haystack += candidate.value + "\n";
    }
    for (const DictionaryReviewValueFrequency& candidate : group.candidateNotes) {
        haystack += candidate.value + "\n";
    }
    return str2Lower(haystack);
}

// 构造账本独有术语搜索用的小写文本。
std::string DictionaryGeneratorReviewAgent::ledgerOnlySearchHaystack(const std::string& sourceTerm, const json& entry) const {
    return str2Lower(
        sourceTerm + "\n" +
        entry.value("target_term", "") + "\n" +
        entry.value("note", "") + "\n" +
        entry.value("status", "") + "\n" +
        entry.value("merge_into", "")
    );
}

// 格式化已写入的账本项和这次提交附带的 term_updates。
std::string DictionaryGeneratorReviewAgent::formatAppliedEntry(
    const std::string& sourceTerm,
    const json& entry,
    const json& termUpdates
) const {
    if (!entry.is_object()) {
        return std::format(
            "源术语='{}'，状态=<缺失>，术语更新={}",
            sourceTerm,
            termUpdates.dump()
        );
    }
    return std::format(
        "源术语='{}'，状态={}，译名='{}'，合并到='{}'，备注 {} 字节，术语更新={}",
        sourceTerm,
        entry.value("status", ""),
        entry.value("target_term", ""),
        entry.value("merge_into", ""),
        entry.value("note", "").size(),
        termUpdates.dump()
    );
}

// 查找审校源文件视图。
const AgentCommonSourceFileView* DictionaryGeneratorReviewAgent::findSourceFileView(const fs::path& relPath) const {
    const auto fileIt = m_sourceFileLookup.find(relPath);
    if (fileIt == m_sourceFileLookup.end()) {
        return nullptr;
    }
    return fileIt->second;
}

// 返回审校源文件行数。
std::optional<int> DictionaryGeneratorReviewAgent::getSourceFileLineCount(const fs::path& relPath) const {
    const AgentCommonSourceFileView* sourceView = findSourceFileView(relPath);
    if (sourceView == nullptr) {
        return std::nullopt;
    }
    return (int)sourceView->lines.size();
}

// 在审校源文件中搜索上下文。
json DictionaryGeneratorReviewAgent::runSearchTextTool(const fs::path& currentFile, const json& arguments) const {
    return runAgentCommonSourceSearchTextTool(
        currentFile,
        m_relJsonPaths,
        [this](const fs::path& relPath) -> const AgentCommonSourceFileView*
        {
            return findSourceFileView(relPath);
        },
        m_agentSearchResultLimit,
        m_agentContextLinesLimit,
        true,
        {},
        arguments
    );
}

// 在粗候选和已审校账本快照中搜索术语。
json DictionaryGeneratorReviewAgent::runSearchDictionaryTool(
    const std::vector<std::string>& ledgerTermOrder,
    const absl::flat_hash_map<std::string, json>& ledgerMap,
    const json& arguments
) const {
    const std::vector<std::string> queries = collectAgentCommonToolQueries(arguments);
    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
    json matches = json::array();

    int matchCount = 0;
    int reviewedTotal = 0;
    int pendingTotal = 0;
    absl::flat_hash_set<std::string> processedSourceTerms;
    processedSourceTerms.reserve(m_groups.size());
    for (const DictionaryReviewTermGroup& group : m_groups) {
        if (ledgerMap.contains(group.sourceTerm)) {
            ++reviewedTotal;
        }
        else {
            ++pendingTotal;
        }
    }
    for (const auto& sourceTerm : ledgerMap | std::views::keys) {
        if (!m_groupLookup.contains(sourceTerm)) {
            ++reviewedTotal;
        }
    }

    const auto appendMatchedEntry = [&](const DictionaryReviewTermGroup& group)
        {
            const auto ledgerIt = ledgerMap.find(group.sourceTerm);
            const json* reviewedEntry = ledgerIt != ledgerMap.end() ? &ledgerIt->second : nullptr;
            const std::string haystack = candidateSearchHaystack(group, reviewedEntry);
            const bool matched = queryLowers.empty() || std::ranges::any_of(queryLowers, [&](const std::string& queryLower)
                {
                    return !queryLower.empty() && haystack.contains(queryLower);
                });
            if (!matched) {
                return;
            }
            ++matchCount;
            if (matchCount <= start || (int)matches.size() >= limit) {
                return;
            }
            matches.push_back(candidateToDictionarySearchJson(group, reviewedEntry));
        };

    const auto appendMatchedLedgerOnlyEntry = [&](const std::string& sourceTerm, const json& entry)
        {
            const std::string haystack = ledgerOnlySearchHaystack(sourceTerm, entry);
            const bool matched = queryLowers.empty() || std::ranges::any_of(queryLowers, [&](const std::string& queryLower)
                {
                    return !queryLower.empty() && haystack.contains(queryLower);
                });
            if (!matched) {
                return;
            }
            ++matchCount;
            if (matchCount <= start || (int)matches.size() >= limit) {
                return;
            }
            matches.push_back(ledgerOnlyToDictionarySearchJson(sourceTerm, entry));
        };

    for (const std::string& sourceTerm : ledgerTermOrder) {
        if (!processedSourceTerms.insert(sourceTerm).second) {
            continue;
        }
        const auto ledgerIt = ledgerMap.find(sourceTerm);
        const DictionaryReviewTermGroup* group = nullptr;
        const auto groupIt = m_groupLookup.find(sourceTerm);
        if (groupIt != m_groupLookup.end()) {
            group = groupIt->second;
        }
        if (group != nullptr) {
            appendMatchedEntry(*group);
        }
        else if (ledgerIt != ledgerMap.end()) {
            appendMatchedLedgerOnlyEntry(sourceTerm, ledgerIt->second);
        }
    }
    for (const DictionaryReviewTermGroup& group : m_groups) {
        if (processedSourceTerms.contains(group.sourceTerm)) {
            continue;
        }
        appendMatchedEntry(group);
    }

    return json{
        {"queries", queries},
        {"start", start},
        {"limit", limit},
        {"total", matchCount},
        {"reviewed_total", reviewedTotal},
        {"pending_total", pendingTotal},
        {"matches", matches}
    };
}

// 执行审校 Agent 当前轮次的工具调用。
DictionaryGeneratorReviewAgent::DictionaryReviewToolCallResult DictionaryGeneratorReviewAgent::executeToolCalls(
    const fs::path& currentFile,
    const std::vector<std::string>& ledgerTermOrder,
    const absl::flat_hash_map<std::string, json>& ledgerMap,
    const std::vector<AgentCommonToolCallRequest>& calls,
    bool collectDetail
) const {
    DictionaryReviewToolCallResult executionResult;
    executionResult.summary = formatAgentCommonToolCallDetails(calls);
    for (const auto& call : calls) {
        json result = {
            {"id", call.id},
            {"name", call.name}
        };
        if (call.name == "list_files") {
            result["result"] = runAgentCommonListFilesTool(
                m_relJsonPaths,
                [this](const fs::path& relPath)
                {
                    return getSourceFileLineCount(relPath);
                },
                m_agentSearchResultLimit,
                call.arguments
            );
        }
        else if (call.name == "search_text") {
            result["result"] = runSearchTextTool(currentFile, call.arguments);
        }
        else if (call.name == "search_dictionary") {
            result["result"] = runSearchDictionaryTool(ledgerTermOrder, ledgerMap, call.arguments);
        }
        else if (call.name == "get_project_note") {
            result["result"] = runAgentCommonGetProjectNoteTool(m_projectDir, m_agentProjectNotePath, call.arguments);
        }
        else {
            result["error"] = gppTr("DictionaryGeneratorReviewAgent.executeToolCalls", "未知工具: %1")
                .arg(call.name)
                .toStdString();
        }
        executionResult.results.push_back(std::move(result));
    }
    if (collectDetail) {
        executionResult.detail = gppTr(
            "DictionaryGeneratorReviewAgent.executeToolCalls",
            "工具返回结果:\n%1")
            .arg(executionResult.results.dump(2))
            .toStdString();
    }
    return executionResult;
}

// 解析一轮响应并执行对应动作；失败时由外层重新请求当前轮次。
std::expected<DictionaryGeneratorReviewAgent::DictionaryReviewAgentTurnResult, std::string>
DictionaryGeneratorReviewAgent::parseAndApplyTurnResponse(
    const fs::path& currentFile,
    const DictionaryReviewTermGroup& group,
    json& messages,
    const std::string& content,
    const std::string& reviewIndexLog,
    int turn,
    int requestCount,
    int threadId
)
{
    try {
        const DictionaryReviewAgentProtocolResponse protocol = parseProtocolResponse(content);
        if (protocol.action == "tool_calls") {
            std::vector<std::string> toolLedgerTermOrder;
            absl::flat_hash_map<std::string, json> toolLedgerMap;
            {
                std::lock_guard<std::mutex> lock(m_ledgerMutex);
                toolLedgerTermOrder = m_ledgerTermOrder;
                toolLedgerMap = m_ledgerMap;
            }

            const DictionaryReviewToolCallResult toolCallResult = executeToolCalls(
                currentFile,
                toolLedgerTermOrder,
                toolLedgerMap,
                protocol.calls,
                m_logger->should_log(spdlog::level::debug)
            );
            if (!toolCallResult.detail.empty()) {
                m_logger->debug(gppTr(
                    "DictionaryGeneratorReviewAgent.parseAndApplyTurnResponse",
                    "[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 工具调用明细:\n%5")
                    .arg(threadId)
                    .arg(reviewIndexLog)
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
            return DictionaryReviewAgentTurnResult{
                .action = DictionaryReviewAgentTurnResult::Action::ContinueTurn,
                .summary = gppTr(
                    "DictionaryGeneratorReviewAgent.parseAndApplyTurnResponse",
                    "执行工具调用 %1 个，进入下一轮。调用参数:\n%2")
                    .arg(protocol.calls.size())
                    .arg(toolCallResult.summary)
                    .toStdString()
            };
        }

        if (protocol.action == "skip") {
            return DictionaryReviewAgentTurnResult{
                .action = DictionaryReviewAgentTurnResult::Action::CompleteTerm,
                .summary = gppTr(
                    "DictionaryGeneratorReviewAgent.parseAndApplyTurnResponse",
                    "选择跳过，该术语不会输出到最终字典")
                    .toStdString()
            };
        }

        json appliedEntry = json::object();
        {
            std::lock_guard<std::mutex> lock(m_ledgerMutex);
            applyCommitResultLocked(group, protocol.result);
            const auto appliedEntryIt = m_ledgerMap.find(group.sourceTerm);
            appliedEntry = appliedEntryIt != m_ledgerMap.end()
                ? appliedEntryIt->second
                : json::object();
        }
        return DictionaryReviewAgentTurnResult{
            .action = DictionaryReviewAgentTurnResult::Action::CompleteTerm,
            .summary = gppTr(
                "DictionaryGeneratorReviewAgent.parseAndApplyTurnResponse",
                "提交审校结果:\n%1")
                .arg(formatAppliedEntry(
                    group.sourceTerm,
                    appliedEntry,
                    protocol.result.termUpdates
                ))
                .toStdString()
        };
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string(e.what()));
    }
}

// 审校一个术语组，直到提交、跳过、请求耗尽或达到轮数上限。
void DictionaryGeneratorReviewAgent::reviewTermGroup(const DictionaryReviewTermGroup& group, int threadId) {
    const auto preserveCandidateAsAccepted = [&]()
        {
            std::lock_guard<std::mutex> lock(m_ledgerMutex);
            if (m_ledgerMap.contains(group.sourceTerm)) {
                return;
            }
            m_ledgerMap.emplace(group.sourceTerm, json{
                {"target_term", candidateTargetForGroup(group)},
                {"note", candidateNoteForGroup(group)},
                {"status", "accepted"},
                {"merge_into", ""},
                {"origin", "review_fallback"},
                {"updated_at", currentTimestampString()}
            });
            rememberLedgerTermOrderLocked(group.sourceTerm);
        };
    if (m_controller->shouldStop()) {
        preserveCandidateAsAccepted();
        return;
    }

    const fs::path currentFile = guessCurrentFileForTerm(group.sourceTerm);
    const std::string sourceTermLog = std::format("`{}`", group.sourceTerm);
    m_logger->info(gppTr(
        "DictionaryGeneratorReviewAgent.reviewTermGroup",
        "[线程 %1] [术语 %2] 字典审校 Agent 开始处理，最多 %3 轮，候选译名 %4 个，候选备注 %5 个，粗候选累计出现 %6 次")
        .arg(threadId)
        .arg(sourceTermLog)
        .arg(m_agentMaxTurnsPerChunk)
        .arg(group.candidateTargets.size())
        .arg(group.candidateNotes.size())
        .arg(group.occurrenceCount)
        .toStdString());

    json ledgerExcerpt = json::array();
    {
        std::lock_guard<std::mutex> lock(m_ledgerMutex);
        const int start = std::max(0, (int)m_ledgerTermOrder.size() - 12);
        for (int index = start; index < (int)m_ledgerTermOrder.size(); ++index) {
            const std::string& sourceTerm = m_ledgerTermOrder[index];
            if (const auto it = m_ledgerMap.find(sourceTerm); it != m_ledgerMap.end()) {
                const json& entry = it->second;
                ledgerExcerpt.push_back({
                    {"source_term", sourceTerm},
                    {"target_term", entry.value("target_term", "")},
                    {"category", entry.value("category", "")},
                    {"note", entry.value("note", "")},
                    {"status", entry.value("status", "")},
                    {"merge_into", entry.value("merge_into", "")},
                    {"origin", entry.value("origin", "")},
                    {"occurrences", entry.value("occurrences", json::array())}
                });
            }
        }
    }
    json messages = buildBaseMessages(group, currentFile, ledgerExcerpt);
    bool retryExhausted = false;

    int turn = 0;
    int requestCount = 0;
    for (; turn < m_agentMaxTurnsPerChunk; ++turn) {
        requestCount = 0;
        bool turnCompleted = false;
        const size_t messageBytes = approximateAgentCommonMessagesBytes(messages);
        while (requestCount < m_maxRequestCount) {
            if (m_controller->shouldStop()) {
                preserveCandidateAsAccepted();
                return;
            }

            const std::optional<TranslationApi> apiOpt = m_apiPool->getApi(m_apiStrategy);
            if (!apiOpt.has_value()) {
                throw std::runtime_error(gppTr(
                    "DictionaryGeneratorReviewAgent.reviewTermGroup",
                    "没有可用的 Api key 了")
                    .toStdString());
            }
            const TranslationApi& currentApi = apiOpt.value();

            json payload = { {"messages", messages} };
            m_logger->info(gppTr(
                "DictionaryGeneratorReviewAgent.reviewTermGroup",
                "[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 开始请求，上下文 %5 字节")
                .arg(threadId)
                .arg(sourceTermLog)
                .arg(turn + 1)
                .arg(requestCount + 1)
                .arg(messageBytes)
                .toStdString());
            ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId,
                m_apiTimeOutMs);
            const std::string checkResponseLogPrefix = gppTr(
                "DictionaryGeneratorReviewAgent.reviewTermGroup",
                "[线程 %1] [术语 %2] [轮次 %3] [请求 %4]")
                .arg(threadId)
                .arg(sourceTermLog)
                .arg(turn + 1)
                .arg(requestCount + 1)
                .toStdString();
            if (!checkResponse(
                response, m_apiPool, currentApi, checkResponseLogPrefix, fs::path{}, m_apiStrategy,
                m_controller, m_logger, requestCount, m_checkQuota
                ))
            {
                continue;
            }
            if (m_logger->should_log(spdlog::level::trace)) {
                m_logger->trace(gppTr(
                    "DictionaryGeneratorReviewAgent.reviewTermGroup",
                    "[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 成功响应，响应内容:\n%5")
                    .arg(threadId)
                    .arg(sourceTermLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(response.content)
                    .toStdString());
            }

            std::expected<DictionaryReviewAgentTurnResult, std::string> turnResult = parseAndApplyTurnResponse(
                currentFile,
                group,
                messages,
                response.content,
                sourceTermLog,
                turn,
                requestCount,
                threadId
            );
            if (turnResult.has_value()) {
                m_logger->info(gppTr(
                    "DictionaryGeneratorReviewAgent.reviewTermGroup",
                    "[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 响应处理成功，处理结果:\n%5")
                    .arg(threadId)
                    .arg(sourceTermLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(turnResult->summary)
                    .toStdString());

                if (turnResult->action == DictionaryReviewAgentTurnResult::Action::CompleteTerm) {
                    return;
                }
            }
            else {
                m_logger->warn(gppTr(
                    "DictionaryGeneratorReviewAgent.reviewTermGroup",
                    "[线程 %1] [术语 %2] [轮次 %3] [请求 %4] 字典审校 Agent 响应处理失败，错误: %5，响应内容:\n%6")
                    .arg(threadId)
                    .arg(sourceTermLog)
                    .arg(turn + 1)
                    .arg(requestCount + 1)
                    .arg(turnResult.error())
                    .arg(limitLogLines(response.content, m_inputBlockMaxLines))
                    .toStdString());
                m_controller->recordRuntimeTransError(RuntimeTransErrorEvent{
                    .kind = "agent",
                    .level = "warning",
                    .message = gppTr("DictionaryGeneratorReviewAgent.reviewTermGroup", "字典审校 Agent 响应处理失败: %1")
                        .arg(turnResult.error())
                        .toStdString(),
                    .indexRange = group.sourceTerm,
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

    if (!retryExhausted) {
        m_logger->warn(gppTr(
            "DictionaryGeneratorReviewAgent.reviewTermGroup",
            "[线程 %1] [术语 %2] 字典审校 Agent 因超过最大轮数 (%3 轮) 而失败，将输出原始字典结果")
            .arg(threadId)
            .arg(sourceTermLog)
            .arg(m_agentMaxTurnsPerChunk)
            .toStdString());
    }
    else {
        m_logger->warn(gppTr(
            "DictionaryGeneratorReviewAgent.reviewTermGroup",
            "[线程 %1] [术语 %2] [轮次 %3] 字典审校 Agent 在 %4 次请求后彻底失败，将输出原始字典结果")
            .arg(threadId)
            .arg(sourceTermLog)
            .arg(turn + 1)
            .arg(m_maxRequestCount)
            .toStdString());
    }

    preserveCandidateAsAccepted();
}

// 启动审校 worker 处理当前所有术语组。
void DictionaryGeneratorReviewAgent::runReviewWorkers() {
    const int reviewThreads = std::max(1, std::min(m_threadsNum, (int)m_groups.size()));
    m_controller->makeBar((int)m_groups.size(), reviewThreads);
    m_logger->info(gppTr(
        "DictionaryGeneratorReviewAgent.runReviewWorkers",
        "字典审校 Agent 启动 %1 个审校线程处理 %2 个术语")
        .arg(reviewThreads)
        .arg(m_groups.size())
        .toStdString());
    ctpl::thread_pool pool(reviewThreads);
    std::vector<std::future<void>> results;
    results.reserve(m_groups.size());
    for (const auto& group : m_groups) {
        results.emplace_back(pool.push([this, &group](int threadId)
            {
                ActiveWorkerGuard workerGuard(m_controller);
                reviewTermGroup(group, threadId + 1);
                m_controller->updateBar();
            }));
    }
    waitForThreads(m_controller, pool, results);
}

// 将审校账本转换为最终保留的字典列表；未审校的术语不补默认候选。
DictList DictionaryGeneratorReviewAgent::buildFinalDictionary() const {
    DictList finalList;
    finalList.reserve(m_ledgerTermOrder.size());
    for (const DictionaryReviewTermGroup& group : m_groups) {
        const auto it = m_ledgerMap.find(group.sourceTerm);
        if (it == m_ledgerMap.end()) {
            continue;
        }
        const std::string status = it->second.value("status", "");
        if (status != "accepted" && status != "conflict") {
            continue;
        }
        finalList.emplace_back(
            group.sourceTerm,
            it->second.value("target_term", ""),
            it->second.value("note", "")
        );
    }

    for (const std::string& sourceTerm : m_ledgerTermOrder) {
        if (m_groupLookup.contains(sourceTerm)) {
            continue;
        }
        const auto it = m_ledgerMap.find(sourceTerm);
        if (it == m_ledgerMap.end()) {
            continue;
        }
        const std::string status = it->second.value("status", "");
        if (status != "accepted" && status != "conflict") {
            continue;
        }
        finalList.emplace_back(
            sourceTerm,
            it->second.value("target_term", ""),
            it->second.value("note", "")
        );
    }
    return finalList;
}

// 审校所有粗候选术语组，并返回最终保留词条。
DictList DictionaryGeneratorReviewAgent::review(
    const DictList& coarseCandidates,
    const absl::flat_hash_map<std::string, int>& finalCounter,
    const std::vector<std::string>& segments,
    const std::vector<int>& selectedIndices,
    const absl::flat_hash_set<std::string>& nameSet,
    const absl::flat_hash_map<std::string, int>& wordCounter,
    const std::vector<AgentCommonSourceFileView>& sourceFiles
) {
    // 字典审校 Agent 流程导览（示例）：
    // 1. review() 先把粗提取结果聚合成 DictionaryReviewTermGroup，例如：
    //    音夢 -> candidateTargets=[{value:"音梦",count:55}], sampleSegments=[...]。
    // 2. 建立账本、输出顺序、源文件索引和术语组索引。
    // 3. runReviewWorkers() 为每个术语调用 reviewTermGroup()。模型可以返回工具调用，
    //    例如 search_text/search_dictionary；工具结果会回填 messages 进入下一轮。
    // 4. 模型最终返回提交或跳过。提交结果由 applyCommitResultLocked() 校验并写 ledger。
    // 5. buildFinalDictionary() 输出账本里保留的审校结果。
    m_groups = buildDictionaryReviewTermGroups(coarseCandidates, finalCounter, segments, selectedIndices, nameSet, wordCounter);
    if (m_groups.empty()) {
        return {};
    }

    m_sourceFiles = &sourceFiles;
    if (m_relJsonPaths.size() != sourceFiles.size()) {
        throw std::runtime_error(gppTr(
            "DictionaryGeneratorReviewAgent.review",
            "字典审校源文件路径数量(%1)与源文件视图数量(%2)不一致")
            .arg(m_relJsonPaths.size())
            .arg(sourceFiles.size())
            .toStdString());
    }

    m_sourceFileLookup.reserve(m_relJsonPaths.size());
    for (size_t index = 0; index < m_relJsonPaths.size(); ++index) {
        m_sourceFileLookup.insert_or_assign(m_relJsonPaths[index], &sourceFiles[index]);
    }

    m_groupLookup.reserve(m_groups.size());
    m_knownSourceTerms.reserve(m_groups.size() * 2);
    m_ledgerTermOrderSeen.reserve(m_groups.size() * 2);
    m_ledgerMap.reserve(m_groups.size() * 2);
    m_ledgerTermOrder.reserve(m_groups.size());
    for (const DictionaryReviewTermGroup& group : m_groups) {
        m_groupLookup.insert_or_assign(group.sourceTerm, &group);
        m_knownSourceTerms.insert(group.sourceTerm);
    }

    runReviewWorkers();
    const bool reviewStopped = m_controller->shouldStop();
    DictList finalList = buildFinalDictionary();
    if (reviewStopped) {
        m_logger->info(gppTr("DictionaryGeneratorReviewAgent.review",
            "字典审校 Agent 已停止。最终保留术语数: %1")
            .arg(finalList.size())
            .toStdString());
    }
    else {
        m_logger->info(gppTr("DictionaryGeneratorReviewAgent.review",
            "字典审校 Agent 完成。最终保留术语数: %1")
            .arg(finalList.size())
            .toStdString());
    }
    return finalList;
}
