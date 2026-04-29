module;

#include "GPPMacros.hpp"
#include <ctpl_stl.h>

module DictionaryReviewAgent;

import AgentToolCommon;
import AgentSourceView;
import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

struct ReviewToolCallRequest {
    std::string id;
    std::string name;
    json arguments = json::object();
};

struct ReviewCommitResult {
    std::string sourceTerm;
    std::string finalTarget;
    std::string finalNote;
    std::string status;
    std::string mergeInto;
    json termUpdates = json::array();
};

struct ReviewProtocolResponse {
    std::string action;
    std::vector<ReviewToolCallRequest> calls;
    ReviewCommitResult result;
};

struct ReviewToolExecutionEnv {
    fs::path projectDir;
    fs::path currentFile;
    const std::vector<fs::path>* relFiles = nullptr;
    const std::vector<AgentSourceFileView>* sourceFiles = nullptr;
    const absl::flat_hash_map<fs::path, const AgentSourceFileView*>* sourceFileLookup = nullptr;
    const std::vector<DictionaryReviewTermGroup>* termGroups = nullptr;
    const absl::flat_hash_map<std::string, const DictionaryReviewTermGroup*>* groupLookup = nullptr;
    const absl::flat_hash_map<std::string, json>* ledgerMap = nullptr;
    const std::vector<std::string>* ledgerOrder = nullptr;
    int searchResultLimit = 80;
    std::optional<fs::path> projectNotePath;
};

ReviewProtocolResponse parseReviewProtocolResponse(const std::string& content) {
    ReviewProtocolResponse result;
    const std::optional<json> payloadOpt = tryParseAgentJsonEnvelope(content);
    if (!payloadOpt.has_value() || !payloadOpt->is_object()) {
        throw std::runtime_error("Review agent response is not a valid JSON object");
    }

    const json& payload = *payloadOpt;
    const std::string schema = payload.value("schema", "");
    if (!schema.empty() && schema != "gpp-gendict-review-v1") {
        throw std::runtime_error(std::format("Invalid review agent schema: {}", schema));
    }

    result.action = payload.value("action", "");
    if (const auto it = payload.find("calls"); it != payload.end() && it->is_array()) {
        for (const auto& call : *it) {
            if (!call.is_object()) {
                continue;
            }
            ReviewToolCallRequest parsed;
            parsed.id = call.value("id", std::format("call_{}", result.calls.size()));
            parsed.name = call.value("name", "");
            if (const auto argIt = call.find("arguments"); argIt != call.end()) {
                parsed.arguments = *argIt;
            }
            result.calls.push_back(std::move(parsed));
        }
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

std::string formatReviewToolCallDetailsForInfo(const std::vector<ReviewToolCallRequest>& calls) {
    if (calls.empty()) {
        return "None";
    }
    std::string result;
    for (const auto& [index, call] : calls | std::views::enumerate) {
        if (index > 0) {
            result += "; ";
        }
        result += std::format(
            "{}({})",
            call.name.empty() ? "<unknown>" : call.name,
            truncateUtf8Prefix(call.arguments.dump(), 360)
        );
    }
    return result;
}

std::string jsonStringForInfo(const json& object, std::string_view key) {
    if (!object.is_object()) {
        return {};
    }
    const auto it = object.find(std::string(key));
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

json jsonValueForInfo(const json& object, std::string_view key, json fallback = json::object()) {
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(std::string(key));
    return it != object.end() ? *it : fallback;
}

std::string summarizeReviewToolResultsForInfo(const json& toolResults) {
    if (!toolResults.is_array() || toolResults.empty()) {
        return "None";
    }

    std::string result;
    for (const auto& [index, item] : toolResults | std::views::enumerate) {
        if (index > 0) {
            result += "; ";
        }
        std::string name = jsonStringForInfo(item, "name");
        if (name.empty()) {
            name = "<unknown>";
        }
        if (item.contains("error")) {
            result += std::format("{} error={}", name, truncateUtf8Prefix(jsonStringForInfo(item, "error"), 240));
            continue;
        }

        const json payload = jsonValueForInfo(item, "result");
        if (!payload.is_object()) {
            result += std::format("{} result={}", name, truncateUtf8Prefix(payload.dump(), 360));
            continue;
        }
        if (name == "search_text" || name == "search_dictionary") {
            const int returned = payload.contains("matches") && payload["matches"].is_array()
                ? (int)payload["matches"].size()
                : 0;
            result += std::format(
                "{} queries={} start={} limit={} total={} returned={}",
                name,
                truncateUtf8Prefix(jsonValueForInfo(payload, "queries", json::array()).dump(), 240),
                payload.value("start", 0),
                payload.value("limit", 0),
                payload.value("total", 0),
                returned
            );
        }
        else if (name == "list_files") {
            const int returned = payload.contains("files") && payload["files"].is_array()
                ? (int)payload["files"].size()
                : 0;
            result += std::format(
                "list_files start={} limit={} total={} returned={}",
                payload.value("start", 0),
                payload.value("limit", 0),
                payload.value("total", 0),
                returned
            );
        }
        else if (name == "get_project_note") {
            const std::string content = jsonStringForInfo(payload, "content");
            result += std::format(
                "get_project_note file={} content_chars={}",
                jsonStringForInfo(payload, "file"),
                content.size()
            );
        }
        else {
            result += std::format("{} result={}", name, truncateUtf8Prefix(payload.dump(), 360));
        }
    }
    return result;
}

std::string summarizeReviewTermUpdatesForInfo(const json& termUpdates) {
    if (!termUpdates.is_array() || termUpdates.empty()) {
        return "[]";
    }

    json compact = json::array();
    int emitted = 0;
    for (const auto& update : termUpdates) {
        if (emitted >= 6) {
            break;
        }
        if (!update.is_object()) {
            continue;
        }
        compact.push_back({
            {"source_term", update.value("source_term", "")},
            {"status", update.value("status", "")},
            {"target_term", update.value("target_term", "")},
            {"merge_into", update.value("merge_into", "")}
        });
        ++emitted;
    }
    if (termUpdates.size() > compact.size()) {
        compact.push_back({ {"more", termUpdates.size() - compact.size()} });
    }
    return truncateUtf8Prefix(compact.dump(), 720);
}

std::string formatReviewAppliedEntryForInfo(
    const std::string& sourceTerm,
    const json& entry,
    const json& termUpdates
) {
    if (!entry.is_object()) {
        return std::format(
            "source_term='{}', status=<missing>, term_updates={}",
            truncateUtf8Prefix(sourceTerm, 180),
            summarizeReviewTermUpdatesForInfo(termUpdates)
        );
    }
    return std::format(
        "source_term='{}', status={}, target='{}', merge_into='{}', note_chars={}, term_updates={}",
        truncateUtf8Prefix(sourceTerm, 180),
        jsonStringForInfo(entry, "status"),
        truncateUtf8Prefix(jsonStringForInfo(entry, "target_term"), 180),
        truncateUtf8Prefix(jsonStringForInfo(entry, "merge_into"), 180),
        jsonStringForInfo(entry, "note").size(),
        summarizeReviewTermUpdatesForInfo(termUpdates)
    );
}

json valueFrequenciesToJson(const std::vector<DictionaryReviewValueFrequency>& values) {
    json result = json::array();
    for (const auto& value : values) {
        result.push_back({
            {"value", value.value},
            {"count", value.count}
        });
    }
    return result;
}

json groupToJson(const DictionaryReviewTermGroup& group) {
    return json{
        {"sourceTerm", group.sourceTerm},
        {"candidateTargets", valueFrequenciesToJson(group.candidateTargets)},
        {"candidateNotes", valueFrequenciesToJson(group.candidateNotes)},
        {"occurrenceCount", group.occurrenceCount},
        {"sampleSegments", group.sampleSegments},
        {"isNameHint", group.isNameHint},
        {"isTokenizerWord", group.isTokenizerWord}
    };
}

std::string fallbackTargetForGroup(const DictionaryReviewTermGroup& group);
std::string fallbackNoteForGroup(const DictionaryReviewTermGroup& group);

json reviewCandidateToDictionarySearchJson(
    const DictionaryReviewTermGroup& group,
    const json* reviewedEntry
) {
    json result = {
        {"source_term", group.sourceTerm},
        {"reviewed", reviewedEntry != nullptr},
        {"candidate_targets", valueFrequenciesToJson(group.candidateTargets)},
        {"candidate_notes", valueFrequenciesToJson(group.candidateNotes)},
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
        result["target_term"] = fallbackTargetForGroup(group);
        result["note"] = fallbackNoteForGroup(group);
        result["status"] = "pending";
        result["merge_into"] = "";
        result["origin"] = "candidate";
    }

    return result;
}

json reviewLedgerOnlyToDictionarySearchJson(const std::string& sourceTerm, const json& entry) {
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

std::string reviewCandidateSearchHaystack(const DictionaryReviewTermGroup& group, const json* reviewedEntry) {
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

std::string reviewLedgerOnlySearchHaystack(const std::string& sourceTerm, const json& entry) {
    return str2Lower(
        sourceTerm + "\n" +
        entry.value("target_term", "") + "\n" +
        entry.value("note", "") + "\n" +
        entry.value("status", "") + "\n" +
        entry.value("merge_into", "")
    );
}

std::string buildReviewSchemaDescription() {
    return
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
}

fs::path guessCurrentFileForTerm(
    const std::string& sourceTerm,
    const std::vector<AgentSourceFileView>& sourceFiles,
    const std::vector<fs::path>& fallbackRelFiles
) {
    for (const AgentSourceFileView& file : sourceFiles) {
        const bool matched = std::ranges::any_of(file.lines, [&](const AgentSourceLineView& line)
            {
                return line.speaker.contains(sourceTerm) || line.toolText.contains(sourceTerm);
            });
        if (matched) {
            return file.relPath;
        }
    }
    if (!fallbackRelFiles.empty()) {
        return fallbackRelFiles.front();
    }
    return {};
}

std::optional<int> getReviewSourceFileLineCount(const ReviewToolExecutionEnv& env, const fs::path& relPath) {
    if (env.sourceFileLookup == nullptr) {
        return std::nullopt;
    }
    const auto fileIt = env.sourceFileLookup->find(relPath);
    if (fileIt == env.sourceFileLookup->end() || fileIt->second == nullptr) {
        return std::nullopt;
    }
    return (int)fileIt->second->lines.size();
}

std::string fallbackTargetForGroup(const DictionaryReviewTermGroup& group) {
    if (!group.candidateTargets.empty()) {
        return group.candidateTargets.front().value;
    }
    return {};
}

std::string fallbackNoteForGroup(const DictionaryReviewTermGroup& group) {
    std::string bestNote;
    for (const auto& note : group.candidateNotes) {
        if (note.value.size() > bestNote.size()) {
            bestNote = note.value;
        }
    }
    return bestNote;
}

json runReviewSearchTextTool(const ReviewToolExecutionEnv& env, const json& arguments) {
    const std::vector<std::string> queries = collectAgentToolQueries(arguments);
    if (queries.empty()) {
        return { {"error", "search_text requires query or queries"} };
    }

    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();

    const std::string scope = arguments.value("scope", "current_file");
    if (scope != "current_file" && scope != "all_files" && scope != "specified_file") {
        return {
            {"error", std::format("Invalid search_text.scope: {}", scope)},
            {"allowed_scope", json::array({"current_file", "all_files", "specified_file"})}
        };
    }

    std::vector<fs::path> targetFiles;
    if (scope == "specified_file") {
        targetFiles.push_back(ascii2Wide(arguments.value("file", wide2Ascii(env.currentFile))));
    }
    else if (scope == "all_files") {
        if (env.sourceFiles != nullptr) {
            targetFiles = *env.sourceFiles
                | std::views::transform([](const AgentSourceFileView& file) { return file.relPath; })
                | std::ranges::to<std::vector>();
        }
    }
    else {
        targetFiles.push_back(env.currentFile);
    }

    json matches = json::array();
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    const int contextLines = sanitizeAgentContextLines(arguments.value("context_lines", 2));
    if (env.sourceFileLookup == nullptr) {
        return {
            {"queries", queries},
            {"start", start},
            {"limit", limit},
            {"total", 0},
            {"context_lines", contextLines},
            {"matches", matches}
        };
    }

    int matchCount = 0;
    for (const fs::path& targetFile : targetFiles) {
        const auto fileIt = env.sourceFileLookup->find(targetFile);
        if (fileIt == env.sourceFileLookup->end() || fileIt->second == nullptr) {
            continue;
        }
        const AgentSourceFileView& sourceFile = *fileIt->second;

        for (const auto& [lineIndex, line] : sourceFile.lines | std::views::enumerate) {
            std::string matchedQuery;
            for (const auto& [index, queryLower] : queryLowers | std::views::enumerate) {
                if (!queryLower.empty() && line.speakerToolTextLower.contains(queryLower)) {
                    matchedQuery = queries[index];
                    break;
                }
            }
            if (matchedQuery.empty()) {
                continue;
            }

            ++matchCount;
            if (matchCount <= start || (int)matches.size() >= limit) {
                continue;
            }
            matches.push_back({
                {"file", wide2Ascii(sourceFile.relPath)},
                {"id", line.id},
                {"speaker", line.speaker},
                {"message", line.toolText},
                {"matched_query", matchedQuery},
                {"nearby_lines", buildAgentSourceNearbyLines(sourceFile.lines, (int)lineIndex, contextLines)}
            });
        }
    }

    return json{
        {"queries", queries},
        {"scope", scope},
        {"current_file", wide2Ascii(env.currentFile)},
        {"start", start},
        {"limit", limit},
        {"total", matchCount},
        {"context_lines", contextLines},
        {"matches", matches}
    };
}

json runReviewSearchDictionaryTool(const ReviewToolExecutionEnv& env, const json& arguments) {
    const std::vector<std::string> queries = collectAgentToolQueries(arguments);
    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    json matches = json::array();

    if (env.termGroups == nullptr || env.ledgerMap == nullptr) {
        return json{
            {"queries", queries},
            {"start", start},
            {"limit", limit},
            {"total", 0},
            {"reviewed_total", 0},
            {"pending_total", 0},
            {"matches", matches}
        };
    }

    int matchCount = 0;
    int reviewedTotal = 0;
    int pendingTotal = 0;
    absl::flat_hash_set<std::string> processedSourceTerms;
    processedSourceTerms.reserve(env.termGroups->size());
    for (const DictionaryReviewTermGroup& group : *env.termGroups) {
        if (env.ledgerMap->contains(group.sourceTerm)) {
            ++reviewedTotal;
        }
        else {
            ++pendingTotal;
        }
    }
    for (const auto& sourceTerm : *env.ledgerMap | std::views::keys) {
        if (env.groupLookup == nullptr || !env.groupLookup->contains(sourceTerm)) {
            ++reviewedTotal;
        }
    }

    const auto appendMatchedEntry = [&](const DictionaryReviewTermGroup& group)
        {
            const auto ledgerIt = env.ledgerMap->find(group.sourceTerm);
            const json* reviewedEntry = ledgerIt != env.ledgerMap->end() ? &ledgerIt->second : nullptr;
            const std::string haystack = reviewCandidateSearchHaystack(group, reviewedEntry);
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
            matches.push_back(reviewCandidateToDictionarySearchJson(group, reviewedEntry));
        };

    const auto appendMatchedLedgerOnlyEntry = [&](const std::string& sourceTerm, const json& entry)
        {
            const std::string haystack = reviewLedgerOnlySearchHaystack(sourceTerm, entry);
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
            matches.push_back(reviewLedgerOnlyToDictionarySearchJson(sourceTerm, entry));
        };

    if (env.ledgerOrder != nullptr) {
        for (const std::string& sourceTerm : *env.ledgerOrder) {
            if (!processedSourceTerms.insert(sourceTerm).second) {
                continue;
            }
            const auto ledgerIt = env.ledgerMap->find(sourceTerm);
            const DictionaryReviewTermGroup* group = nullptr;
            if (env.groupLookup != nullptr) {
                const auto groupIt = env.groupLookup->find(sourceTerm);
                if (groupIt != env.groupLookup->end()) {
                    group = groupIt->second;
                }
            }
            if (group != nullptr) {
                appendMatchedEntry(*group);
            }
            else if (ledgerIt != env.ledgerMap->end()) {
                appendMatchedLedgerOnlyEntry(sourceTerm, ledgerIt->second);
            }
        }
    }
    for (const DictionaryReviewTermGroup& group : *env.termGroups) {
        if (processedSourceTerms.contains(group.sourceTerm)) {
            continue;
        }
        appendMatchedEntry(group);
    }
    if (env.ledgerOrder == nullptr) {
        for (const auto& [sourceTerm, entry] : *env.ledgerMap) {
            if (env.groupLookup != nullptr && env.groupLookup->contains(sourceTerm)) {
                continue;
            }
            appendMatchedLedgerOnlyEntry(sourceTerm, entry);
        }
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

AgentSharedToolEnv buildReviewSharedToolEnv(const ReviewToolExecutionEnv& env) {
    return {
        .projectDir = env.projectDir,
        .relFiles = env.relFiles,
        .projectNotePath = env.projectNotePath,
        .getFileLineCount = [&](const fs::path& relPath)
            {
                return getReviewSourceFileLineCount(env, relPath);
            },
        .searchResultLimit = env.searchResultLimit,
    };
}

json executeReviewToolCalls(const ReviewToolExecutionEnv& env, const std::vector<ReviewToolCallRequest>& calls) {
    json toolResults = json::array();
    const AgentSharedToolEnv sharedEnv = buildReviewSharedToolEnv(env);
    for (const auto& call : calls) {
        json result = {
            {"id", call.id},
            {"name", call.name}
        };
        try {
            if (call.name == "list_files") {
                result["result"] = runAgentCommonListFilesTool(sharedEnv, call.arguments);
            }
            else if (call.name == "search_text") {
                result["result"] = runReviewSearchTextTool(env, call.arguments);
            }
            else if (call.name == "search_dictionary") {
                result["result"] = runReviewSearchDictionaryTool(env, call.arguments);
            }
            else if (call.name == "get_project_note") {
                result["result"] = runAgentCommonGetProjectNoteTool(sharedEnv, call.arguments);
            }
            else {
                result["error"] = std::format("Unknown tool: {}", call.name);
            }
        }
        catch (const std::exception& e) {
            result["error"] = e.what();
        }
        toolResults.push_back(std::move(result));
    }
    return toolResults;
}

json buildLedgerExcerpt(const std::vector<std::string>& entriesVec, const absl::flat_hash_map<std::string, json>& ledgerMap, int limit = 12) {
    json excerpt = json::array();
    if (entriesVec.empty()) {
        return excerpt;
    }
    const int start = std::max(0, (int)entriesVec.size() - limit);
    for (int index = start; index < (int)entriesVec.size(); ++index) {
        const std::string& sourceTerm = entriesVec[index];
        if (const auto it = ledgerMap.find(sourceTerm); it != ledgerMap.end()) {
            excerpt.push_back(agentToolLedgerEntryToJson(sourceTerm, it->second));
        }
    }
    return excerpt;
}

json buildLedgerExcerptWithLock(
    const std::vector<std::string>& entriesVec,
    const absl::flat_hash_map<std::string, json>& ledgerMap,
    std::mutex& mutex,
    int limit = 12
) {
    std::lock_guard<std::mutex> lock(mutex);
    return buildLedgerExcerpt(entriesVec, ledgerMap, limit);
}

std::string buildReviewExtraTools(const DictionaryReviewAgentConfig& config) {
    if (!config.projectNotePath.has_value()) {
        return {};
    }
    return std::format(
        "- `get_project_note()`: read the optional user-provided script note file `{}`.\n",
        safeRelativePath(config.projectNotePath.value(), config.projectDir)
    );
}

json buildReviewBaseMessages(
    const DictionaryReviewAgentConfig& config,
    const DictionaryReviewTermGroup& group,
    const fs::path& currentFile,
    const std::vector<std::string>& entriesVec,
    const absl::flat_hash_map<std::string, json>& ledgerMap
) {
    std::string prompt = config.userPrompt;
    replaceStrInplace(prompt, "[TargetLang]", config.targetLang);
    replaceStrInplace(prompt, "[ReviewSchemaDescription]", buildReviewSchemaDescription());
    replaceStrInplace(prompt, "[ReviewCurrentTerm]", groupToJson(group).dump(2));
    replaceStrInplace(prompt, "[ReviewRecentTermsExcerpt]", buildLedgerExcerpt(entriesVec, ledgerMap).dump(2));
    replaceStrInplace(prompt, "[ReviewExtraTools]", buildReviewExtraTools(config));
    replaceStrInplace(prompt, "[ReviewAnchorFile]", currentFile.empty() ? "None" : wide2Ascii(currentFile));

    return json::array({
        {{"role", "system"}, {"content", config.systemPrompt}},
        {{"role", "user"}, {"content", prompt}}
    });
}

json buildReviewBaseMessagesWithLedgerExcerpt(
    const DictionaryReviewAgentConfig& config,
    const DictionaryReviewTermGroup& group,
    const fs::path& currentFile,
    const json& ledgerExcerpt
) {
    std::string prompt = config.userPrompt;
    replaceStrInplace(prompt, "[TargetLang]", config.targetLang);
    replaceStrInplace(prompt, "[ReviewSchemaDescription]", buildReviewSchemaDescription());
    replaceStrInplace(prompt, "[ReviewCurrentTerm]", groupToJson(group).dump(2));
    replaceStrInplace(prompt, "[ReviewRecentTermsExcerpt]", ledgerExcerpt.dump(2));
    replaceStrInplace(prompt, "[ReviewExtraTools]", buildReviewExtraTools(config));
    replaceStrInplace(prompt, "[ReviewAnchorFile]", currentFile.empty() ? "None" : wide2Ascii(currentFile));

    return json::array({
        {{"role", "system"}, {"content", config.systemPrompt}},
        {{"role", "user"}, {"content", prompt}}
    });
}

DictionaryReviewAgent::DictionaryReviewAgent(
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::unique_ptr<APIPool>& apiPool,
    const std::function<std::string(std::string)>& onPerformApi,
    const DictionaryReviewAgentConfig& config
) : m_apiPool(apiPool), m_controller(controller), m_logger(logger), m_onPerformApi(onPerformApi), m_config(config) 
{
	
}

DictList DictionaryReviewAgent::review(const std::vector<DictionaryReviewTermGroup>& groups, const std::vector<AgentSourceFileView>& sourceFiles) {
    //ledgerMap: sourceTerm->review decision json
    //entriesVec: ledger 输出顺序
    //groupLookup: sourceTerm->group
    //knownSourceTerms: 当前 ledger 可引用的 term，初始为原始 group，随后随 term_updates 扩展
    //sourceFileLookup : relPath->AgentSourceFileView*
    //
    // GenDict Review Agent 流程导览（示例）：
    // 1. DictionaryGenerator 先把粗提取结果聚合成 DictionaryReviewTermGroup，例如：
    //    音夢 -> candidateTargets=[{value:"音梦",count:55}], sampleSegments=[...]。
    // 2. review() 逐个 term group 运行一个小型工具循环。ledgerMap 保存已经确认的 review 决策；
    //    entriesVec 只负责稳定输出/展示顺序；knownSourceTerms 表示当前允许 merge_into 引用的 source term。
    // 3. 每个 term 开始时用 guessCurrentFileForTerm() 找 anchor file；它会同时查 speaker 和 message，
    //    所以角色名只出现在说话人字段时也能定位到文件。
    // 4. buildReviewBaseMessages() 把当前 term group、最近已确认 ledger 摘要、anchor file 和工具说明发给模型。
    // 5. 模型如果不确定，可以返回 tool_calls，例如：
    //    { "action":"tool_calls", "calls":[
    //      { "name":"search_text", "arguments":{"query":"音夢","scope":"all_files","limit":10} },
    //      { "name":"search_dictionary", "arguments":{"query":"音夢","start":0,"limit":10} }
    //    ] }
    //    search_text 查 sourceFiles；search_dictionary 查完整候选词表并标记 reviewed/pending。工具结果回填给模型进入下一轮。
    // 6. 模型最终返回 commit。result 决定当前 term 的 accepted/merged/deprecated/conflict；
    //    result.term_updates 是通用 upsert：可新增 term，也可修改已有 ledger entry 的 target/note/status/merge_into，
    //    例如把别名标成 merged，或者补充一个当前 group 没出现但从上下文确认了的昵称。
    // 7. 全部 term 处理完后，merged 项会做一次 merge target 保留性检查；最后只输出 accepted/conflict，
    //    原始 group 按原顺序输出，term_updates 新增的非 group 项再按 entriesVec 顺序追加。

    absl::flat_hash_map<std::string, json> ledgerMap;
    absl::flat_hash_map<fs::path, const AgentSourceFileView*> sourceFileLookup;
    std::vector<std::string> entriesVec;
    entriesVec.reserve(groups.size());
    absl::flat_hash_map<std::string, const DictionaryReviewTermGroup*> groupLookup;
    absl::flat_hash_set<std::string> knownSourceTerms;
    absl::flat_hash_set<std::string> entryOrderSeen;
    std::mutex ledgerMutex;
    sourceFileLookup.reserve(sourceFiles.size());
    groupLookup.reserve(groups.size());
    knownSourceTerms.reserve(groups.size() * 2);
    entryOrderSeen.reserve(groups.size() * 2);
    ledgerMap.reserve(groups.size() * 2);
    for (const AgentSourceFileView& sourceFile : sourceFiles) {
        sourceFileLookup.insert_or_assign(sourceFile.relPath, &sourceFile);
    }
    for (const DictionaryReviewTermGroup& group : groups) {
        groupLookup.insert_or_assign(group.sourceTerm, &group);
        knownSourceTerms.insert(group.sourceTerm);
    }

    auto pushBackEntryWithOrderUnlockedFunc = [&](const std::string& sourceTerm)
        {
            if (sourceTerm.empty()) {
                return;
            }
            if (entryOrderSeen.insert(sourceTerm).second) {
                entriesVec.push_back(sourceTerm);
            }
        };

    auto applyReviewTermUpdateUnlockedFunc = [&](const json& update)
        {
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

            json& entry = ledgerMap[sourceTerm];
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
            entry["updated_at"] = nowTimestampString();
            knownSourceTerms.insert(sourceTerm);
            pushBackEntryWithOrderUnlockedFunc(sourceTerm);
        };

    auto applyFallbackForGroupUnlockedFunc = [&](const DictionaryReviewTermGroup& group, std::string_view reason)
        {
            const std::string fallbackTarget = fallbackTargetForGroup(group);
            const std::string fallbackNote = fallbackNoteForGroup(group);
            json& entry = ledgerMap[group.sourceTerm];
            if (fallbackTarget.empty()) {
                entry = {
                    {"target_term", ""},
                    {"note", fallbackNote},
                    {"status", "deprecated"},
                    {"merge_into", ""},
                    {"origin", "fallback"},
                    {"reason", std::string(reason)},
                    {"updated_at", nowTimestampString()}
                };
            }
            else {
                entry = {
                    {"target_term", fallbackTarget},
                    {"note", fallbackNote},
                    {"status", "accepted"},
                    {"merge_into", ""},
                    {"origin", "fallback"},
                    {"reason", std::string(reason)},
                    {"updated_at", nowTimestampString()}
                };
            }
            pushBackEntryWithOrderUnlockedFunc(group.sourceTerm);
            knownSourceTerms.insert(group.sourceTerm);
        };

    const auto applyDecisionEntryUnlocked = [&](const DictionaryReviewTermGroup& group, ReviewCommitResult decision)
        {
            const std::string fallbackTarget = fallbackTargetForGroup(group);
            const std::string fallbackNote = fallbackNoteForGroup(group);
            if (decision.sourceTerm != group.sourceTerm) {
                throw std::runtime_error(std::format("commit.source_term={} does not match current term {}", decision.sourceTerm, group.sourceTerm));
            }
            if (decision.termUpdates.is_array()) {
                for (const auto& update : decision.termUpdates) {
                    if (update.is_object()) {
                        const std::string sourceTerm = update.value("source_term", "");
                        if (!sourceTerm.empty()) {
                            knownSourceTerms.insert(sourceTerm);
                        }
                    }
                }
            }
            if (decision.status != "accepted" && decision.status != "merged" &&
                decision.status != "deprecated" && decision.status != "conflict") {
                throw std::runtime_error(std::format("Invalid status: {}", decision.status));
            }
            if ((decision.status == "accepted" || decision.status == "conflict") && decision.finalTarget.empty()) {
                throw std::runtime_error(std::format("final_target is required when status={}", decision.status));
            }
            if (decision.status == "conflict" && decision.finalNote.empty()) {
                throw std::runtime_error("final_note is required when status=conflict");
            }
            if (decision.status == "merged" && decision.mergeInto.empty()) {
                throw std::runtime_error("merge_into is required when status=merged");
            }

            json& entry = ledgerMap[group.sourceTerm];
            const std::string existingNote = entry.is_object() ? entry.value("note", "") : "";

            if (decision.status == "accepted" || decision.status == "conflict") {
                entry = {
                    {"target_term", decision.finalTarget},
                    {"note", decision.finalNote.empty() ? existingNote : decision.finalNote},
                    {"status", decision.status},
                    {"merge_into", ""},
                    {"origin", "group"},
                    {"updated_at", nowTimestampString()}
                };
                pushBackEntryWithOrderUnlockedFunc(group.sourceTerm);
            }
            else if (decision.status == "deprecated") {
                entry = {
                    {"target_term", ""},
                    {"note", decision.finalNote.empty() ? existingNote : decision.finalNote},
                    {"status", "deprecated"},
                    {"merge_into", ""},
                    {"origin", "group"},
                    {"updated_at", nowTimestampString()}
                };
                pushBackEntryWithOrderUnlockedFunc(group.sourceTerm);
            }
            else {
                if (!knownSourceTerms.contains(decision.mergeInto)) {
                    entry = {
                        {"target_term", decision.finalTarget.empty() ? fallbackTarget : decision.finalTarget},
                        {"note", decision.finalNote.empty()
                            ? std::format("Suggested merge target {} does not exist; kept as conflict for manual review", decision.mergeInto)
                            : decision.finalNote},
                        {"status", "conflict"},
                        {"merge_into", ""},
                        {"origin", "group"},
                        {"updated_at", nowTimestampString()}
                    };
                }
                else {
                    entry = {
                        {"target_term", ""},
                        {"note", decision.finalNote.empty() ? fallbackNote : decision.finalNote},
                        {"status", "merged"},
                        {"merge_into", decision.mergeInto},
                        {"origin", "group"},
                        {"updated_at", nowTimestampString()}
                    };
                }
                pushBackEntryWithOrderUnlockedFunc(group.sourceTerm);
            }

            if (!decision.termUpdates.is_array()) {
                return;
            }

            for (const auto& update : decision.termUpdates) {
                applyReviewTermUpdateUnlockedFunc(update);
            }
        };

    auto isRetainedStatusFunc = [](std::string_view status)
        {
            return status == "accepted" || status == "conflict";
        };

    auto snapshotLedgerFunc = [&]()
        {
            std::lock_guard<std::mutex> lock(ledgerMutex);
            return std::make_pair(entriesVec, ledgerMap);
        };

    auto reviewGroupFunc = [&](int groupIndex, const DictionaryReviewTermGroup& group, int threadId)
        {
        if (m_controller->shouldStop()) {
            m_logger->debug("GenDict Review Agent received stop signal; remaining terms will use local fallback.");
            {
                std::lock_guard<std::mutex> lock(ledgerMutex);
                applyFallbackForGroupUnlockedFunc(group, "stopped");
            }
            return;
        }

        const fs::path currentFile = guessCurrentFileForTerm(group.sourceTerm, sourceFiles, m_config.relInputFiles);
        m_logger->info(
            "[线程 {}] GenDict Review Agent reviewing term {}/{}: {}, {} target candidates, {} note candidates, {} occurrences.",
            threadId,
            groupIndex + 1,
            groups.size(),
            group.sourceTerm,
            group.candidateTargets.size(),
            group.candidateNotes.size(),
            group.occurrenceCount
        );

        int retryCount = 0;
        bool completed = false;
        bool exceededTurnLimit = false;
        while (!completed && (retryCount == 0 || retryCount < m_config.maxRetries)) {
            if (m_controller->shouldStop()) {
                std::lock_guard<std::mutex> lock(ledgerMutex);
                applyFallbackForGroupUnlockedFunc(group, "stopped");
                completed = true;
                break;
            }

            json messages = buildReviewBaseMessagesWithLedgerExcerpt(
                m_config,
                group,
                currentFile,
                buildLedgerExcerptWithLock(entriesVec, ledgerMap, ledgerMutex)
            );
            bool turnLoopExitedByRetry = false;
            for (int turn = 0; turn < m_config.maxTurnsPerTerm; ++turn) {
                const std::optional<TranslationApi> apiOpt = m_config.apiStrategy == "random"
                    ? m_apiPool->getApi()
                    : m_apiPool->getFirstApi();
                if (!apiOpt.has_value()) {
                    throw std::runtime_error("没有可用的 API key 了");
                }
                const TranslationApi& currentApi = apiOpt.value();

                json payload = { {"messages", messages} };
                ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_config.apiTimeoutMs);
                if (!checkResponse(
                    response, m_apiPool, currentApi, L"GenDict review agent", m_config.apiStrategy,
                    m_controller, m_logger, retryCount, threadId, m_config.checkQuota
                )) {
                    turnLoopExitedByRetry = true;
                    break;
                }

                ReviewProtocolResponse protocol;
                try {
                    protocol = parseReviewProtocolResponse(response.content);
                }
                catch (const std::exception& e) {
                    ++retryCount;
                    m_logger->warn(
                        "GenDict Review Agent term {} turn {}/{} returned an invalid response, retry {} / {}. Error: {}. Raw response: {}",
                        group.sourceTerm,
                        turn + 1,
                        m_config.maxTurnsPerTerm,
                        retryCount,
                        m_config.maxRetries,
                        e.what(),
                        truncateUtf8Prefix(response.content, 6000)
                    );
                    turnLoopExitedByRetry = true;
                    break;
                }

                m_logger->info(
                    "GenDict Review Agent term {} turn {}/{} returned action='{}'.",
                    group.sourceTerm,
                    turn + 1,
                    m_config.maxTurnsPerTerm,
                    protocol.action
                );

                if (protocol.action == "tool_calls") {
                    if (protocol.calls.empty()) {
                        ++retryCount;
                        m_logger->warn(
                            "GenDict Review Agent term {} turn {}/{} returned empty tool_calls, retry {} / {}.",
                            group.sourceTerm,
                            turn + 1,
                            m_config.maxTurnsPerTerm,
                            retryCount,
                            m_config.maxRetries
                        );
                        turnLoopExitedByRetry = true;
                        break;
                    }

                    ReviewToolExecutionEnv env{
                        .projectDir = m_config.projectDir,
                        .currentFile = currentFile,
                        .relFiles = &m_config.relInputFiles,
                        .sourceFiles = &sourceFiles,
                        .sourceFileLookup = &sourceFileLookup,
                        .termGroups = &groups,
                        .groupLookup = &groupLookup,
                        .ledgerMap = nullptr,
                        .ledgerOrder = nullptr,
                        .searchResultLimit = m_config.searchResultLimit,
                        .projectNotePath = m_config.projectNotePath
                    };
                    auto [toolEntriesSnapshot, toolLedgerSnapshot] = snapshotLedgerFunc();
                    env.ledgerMap = &toolLedgerSnapshot;
                    env.ledgerOrder = &toolEntriesSnapshot;

                    m_logger->info(
                        "GenDict Review Agent term {} requested {} tool calls: {}.",
                        group.sourceTerm,
                        protocol.calls.size(),
                        formatReviewToolCallDetailsForInfo(protocol.calls)
                    );
                    const json toolResults = ::executeReviewToolCalls(env, protocol.calls);
                    m_logger->info(
                        "GenDict Review Agent term {} tool results: {}.",
                        group.sourceTerm,
                        summarizeReviewToolResultsForInfo(toolResults)
                    );
                    if (m_logger->should_log(spdlog::level::debug)) {
                        m_logger->debug(
                            "GenDict Review Agent term {} tool results:\n{}",
                            group.sourceTerm,
                            truncateUtf8Prefix(toolResults.dump(2), 12000)
                        );
                    }

                    messages.push_back({ {"role", "assistant"}, {"content", response.content} });
                    messages.push_back({
                        {"role", "user"},
                        {"content", std::string("Tool results:\n```json\n") + toolResults.dump(2) + "\n```"}
                    });
                    continue;
                }

                if (protocol.action == "skip") {
                    m_logger->info(
                        "GenDict Review Agent term {} chose skip, using local fallback target='{}', note_chars={}.",
                        group.sourceTerm,
                        truncateUtf8Prefix(fallbackTargetForGroup(group), 180),
                        fallbackNoteForGroup(group).size()
                    );
                    {
                        std::lock_guard<std::mutex> lock(ledgerMutex);
                        applyFallbackForGroupUnlockedFunc(group, "skip");
                    }
                    completed = true;
                    break;
                }

                if (protocol.action == "commit") {
                    try {
                        json appliedEntry = json::object();
                        {
                            std::lock_guard<std::mutex> lock(ledgerMutex);
                            applyDecisionEntryUnlocked(group, protocol.result);
                            const auto appliedEntryIt = ledgerMap.find(group.sourceTerm);
                            appliedEntry = appliedEntryIt != ledgerMap.end()
                                ? appliedEntryIt->second
                                : json::object();
                        }
                        completed = true;
                        m_logger->info(
                            "GenDict Review Agent term {} commit accepted: {}.",
                            group.sourceTerm,
                            formatReviewAppliedEntryForInfo(
                                group.sourceTerm,
                                appliedEntry,
                                protocol.result.termUpdates
                            )
                        );
                        if (m_logger->should_log(spdlog::level::debug)) {
                            m_logger->debug(
                                "GenDict Review Agent term {} commit succeeded:\n{}",
                                group.sourceTerm,
                                truncateUtf8Prefix(response.content, 12000)
                            );
                        }
                    }
                    catch (const std::exception& e) {
                        ++retryCount;
                        m_logger->warn(
                            "GenDict Review Agent term {} commit validation failed, retry {} / {}. Error: {}. Raw response: {}",
                            group.sourceTerm,
                            retryCount,
                            m_config.maxRetries,
                            e.what(),
                            truncateUtf8Prefix(response.content, 6000)
                        );
                        turnLoopExitedByRetry = true;
                    }
                    break;
                }

                ++retryCount;
                m_logger->warn(
                    "GenDict Review Agent term {} returned unknown action '{}', retry {} / {}.",
                    group.sourceTerm,
                    protocol.action,
                    retryCount,
                    m_config.maxRetries
                );
                turnLoopExitedByRetry = true;
                break;
            }

            if (completed) {
                break;
            }
            if (!turnLoopExitedByRetry) {
                exceededTurnLimit = true;
                m_logger->warn(
                    "GenDict Review Agent term {} reached the max turn limit ({}), using local fallback.",
                    group.sourceTerm,
                    m_config.maxTurnsPerTerm
                );
                break;
            }
        }

        if (!completed) {
            std::lock_guard<std::mutex> lock(ledgerMutex);
            applyFallbackForGroupUnlockedFunc(group, exceededTurnLimit ? "max_turns" : "retry_exhausted");
        }
        if (!m_controller->shouldStop()) {
            m_controller->updateBar();
        }
        };

    const int reviewThreads = std::max(1, std::min(m_config.threadsNum, (int)groups.size()));
    m_logger->info("GenDict Review Agent starting {} review workers for {} terms.", reviewThreads, groups.size());
    ctpl::thread_pool pool(reviewThreads);
    std::vector<std::future<void>> results;
    results.reserve(groups.size());
    for (int groupIndex = 0; groupIndex < (int)groups.size(); ++groupIndex) {
        results.emplace_back(pool.push([&, groupIndex](int threadId)
            {
                struct ActiveWorkerGuard {
                    std::shared_ptr<IController> controller;
                    explicit ActiveWorkerGuard(std::shared_ptr<IController> controller) : controller(std::move(controller))
                    {
                        this->controller->addThreadNum();
                    }
                    ~ActiveWorkerGuard()
                    {
                        controller->reduceThreadNum();
                    }
                } workerGuard(m_controller);
                reviewGroupFunc(groupIndex, groups[groupIndex], threadId);
            }));
    }
    waitForThreads(pool, results);

    for (auto& [sourceTerm, entry] : ledgerMap) {
        if (entry.value("status", "") != "merged") {
            continue;
        }
        const std::string mergeInto = entry.value("merge_into", "");
        const auto mergeIt = ledgerMap.find(mergeInto);
        const bool mergeTargetKept = mergeIt != ledgerMap.end() &&
            isRetainedStatusFunc(mergeIt->second.value("status", ""));
        if (mergeTargetKept) {
            continue;
        }

        const auto groupIt = groupLookup.find(sourceTerm);
        const std::string fallbackTarget = groupIt != groupLookup.end()
            ? fallbackTargetForGroup(*groupIt->second)
            : entry.value("target_term", "");
        const std::string fallbackNote = groupIt != groupLookup.end()
            ? fallbackNoteForGroup(*groupIt->second)
            : entry.value("note", "");
        entry["status"] = fallbackTarget.empty() ? "deprecated" : "conflict";
        entry["target_term"] = fallbackTarget;
        entry["merge_into"] = "";
        if (entry.value("note", "").empty()) {
            entry["note"] = fallbackNote.empty()
                ? std::format("Original merge target {} was not kept; restored for manual review", mergeInto)
                : fallbackNote;
        }
        entry["origin"] = "merge_recovered";
        entry["updated_at"] = nowTimestampString();
    }

    DictList finalList;
    finalList.reserve(entriesVec.size());
    for (const DictionaryReviewTermGroup& group : groups) {
        const auto it = ledgerMap.find(group.sourceTerm);
        if (it == ledgerMap.end()) {
            continue;
        }
        const std::string status = it->second.value("status", "");
        if (!isRetainedStatusFunc(status)) {
            continue;
        }
        finalList.emplace_back(
            group.sourceTerm,
            it->second.value("target_term", ""),
            it->second.value("note", "")
        );
    }

    for (const std::string& sourceTerm : entriesVec) {
        if (groupLookup.contains(sourceTerm)) {
            continue;
        }
        const auto it = ledgerMap.find(sourceTerm);
        if (it == ledgerMap.end()) {
            continue;
        }
        const std::string status = it->second.value("status", "");
        if (!isRetainedStatusFunc(status)) {
            continue;
        }
        finalList.emplace_back(
            sourceTerm,
            it->second.value("target_term", ""),
            it->second.value("note", "")
        );
    }

    m_logger->info("GenDict Review Agent finished. Final retained term count: {}.", finalList.size());
    return finalList;
}
