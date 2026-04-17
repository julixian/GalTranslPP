module;

#include "GPPMacros.hpp"

module DictionaryReviewAgent;

import AgentToolCommon;
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
    json addTerms = json::array();
};

struct ReviewProtocolResponse {
    std::string action;
    std::vector<ReviewToolCallRequest> calls;
    ReviewCommitResult result;
    std::string rawContent;
};

struct ReviewSourceLine {
    int index = -1;
    std::string speaker;
    std::string message;
    std::string joinedText;
    std::string joinedTextLower;
};

struct ReviewSourceFile {
    fs::path relPath;
    std::vector<ReviewSourceLine> lines;
};

struct ReviewToolExecutionEnv {
    fs::path projectDir;
    fs::path currentFile;
    const std::vector<fs::path>* relFiles = nullptr;
    const std::vector<ReviewSourceFile>* sourceFiles = nullptr;
    const absl::btree_map<std::string, json>* ledger = nullptr;
    int searchResultLimit = 80;
    bool allowCrossFileSearch = true;
    std::optional<fs::path> projectNotePath;
};

std::string trimReviewCopy(const std::string& value) {
    const auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
    const auto begin = std::ranges::find_if(value, isNotSpace);
    if (begin == value.end()) {
        return {};
    }
    const auto end = std::ranges::find_if(value | std::views::reverse, isNotSpace).base();
    return std::string(begin, end);
}

std::optional<json> tryParseReviewJsonEnvelope(const std::string& text) {
    std::string newText = trimReviewCopy(text);
    if (newText.empty()) {
        return std::nullopt;
    }

    const size_t fencedStart = newText.find("```");
    if (fencedStart != std::string::npos) {
        const size_t lineEnd = newText.find('\n', fencedStart);
        const size_t fencedEnd = newText.rfind("```");
        if (lineEnd != std::string::npos && fencedEnd != std::string::npos && fencedEnd > lineEnd) {
            newText = trimReviewCopy(newText.substr(lineEnd + 1, fencedEnd - lineEnd - 1));
        }
    }

    try {
        return json::parse(newText);
    }
    catch (...) { }

    newText = lightRepairJsonText(newText);
    try {
        return json::parse(newText);
    }
    catch (...) { }

    const size_t jsonStart = newText.find('{');
    const size_t jsonEnd = newText.rfind('}');
    if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
        return std::nullopt;
    }

    try {
        return json::parse(newText.substr(jsonStart, jsonEnd - jsonStart + 1));
    }
    catch (...) {
        try {
            return json::parse(lightRepairJsonText(newText.substr(jsonStart, jsonEnd - jsonStart + 1)));
        }
        catch (...) {
            return std::nullopt;
        }
    }
}

ReviewProtocolResponse parseReviewProtocolResponse(const std::string& content) {
    ReviewProtocolResponse result;
    result.rawContent = content;
    const std::optional<json> payloadOpt = tryParseReviewJsonEnvelope(content);
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
            else if (const auto paramIt = call.find("params"); paramIt != call.end()) {
                parsed.arguments = *paramIt;
            }
            result.calls.push_back(std::move(parsed));
        }
    }

    if (const auto it = payload.find("result"); it != payload.end() && it->is_object()) {
        result.result.sourceTerm = trimReviewCopy(it->value("source_term", ""));
        result.result.finalTarget = trimReviewCopy(it->value("final_target", ""));
        result.result.finalNote = trimReviewCopy(it->value("final_note", ""));
        result.result.status = trimReviewCopy(it->value("status", ""));
        result.result.mergeInto = trimReviewCopy(it->value("merge_into", ""));
        if (const auto addIt = it->find("add_terms"); addIt != it->end() && addIt->is_array()) {
            result.result.addTerms = *addIt;
        }
    }

    return result;
}

int sanitizeReviewToolLimit(int requested, int fallback, int maxLimit = 200) {
    if (requested <= 0) {
        return fallback;
    }
    return std::min(requested, maxLimit);
}

std::string truncateReviewLog(std::string text, size_t maxChars = 4000) {
    if (text.size() <= maxChars) {
        return text;
    }
    return text.substr(0, maxChars) + std::format("\n...(truncated, total {} chars)", text.size());
}

std::string formatToolCallSummary(const std::vector<ReviewToolCallRequest>& calls) {
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

json sampleSegmentsToJson(const std::vector<DictionaryReviewSampleSegment>& sampleSegments) {
    json result = json::array();
    for (const auto& sample : sampleSegments) {
        result.push_back({
            {"segment_id", sample.segmentId},
            {"text", sample.text}
        });
    }
    return result;
}

json groupToJson(const DictionaryReviewTermGroup& group) {
    return {
        {"sourceTerm", group.sourceTerm},
        {"candidateTargets", valueFrequenciesToJson(group.candidateTargets)},
        {"candidateNotes", valueFrequenciesToJson(group.candidateNotes)},
        {"occurrenceCount", group.occurrenceCount},
        {"segmentIds", group.segmentIds},
        {"sampleSegments", sampleSegmentsToJson(group.sampleSegments)},
        {"isNameHint", group.isNameHint},
        {"isTokenizerWord", group.isTokenizerWord}
    };
}

int sanitizeReviewSearchContextLines(int requested, int maxLimit = 20) {
    if (requested < 0) {
        return 0;
    }
    return std::min(requested, maxLimit);
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
    "\"add_terms\":[]"
    "}"
    "}";
}

std::vector<ReviewSourceFile> loadReviewSourceFiles(const DictionaryReviewAgentConfig& config) {
    std::vector<ReviewSourceFile> files;
    files.reserve(config.relInputFiles.size());
    for (const fs::path& relPath : config.relInputFiles) {
        std::ifstream ifs(config.inputDir / relPath, std::ios::binary);
        json inputJson = json::parse(ifs);

        ReviewSourceFile file;
        file.relPath = relPath;
        file.lines.reserve(inputJson.size());
        for (const auto& [index, item] : inputJson | std::views::enumerate) {
            std::string speaker;
            if (item.contains("name")) {
                speaker = item.value("name", "");
            }
            else if (item.contains("names")) {
                const std::vector<std::string> names = item["names"].get<std::vector<std::string>>();
                for (size_t nameIndex = 0; nameIndex < names.size(); ++nameIndex) {
                    if (nameIndex > 0) {
                        speaker += '/';
                    }
                    speaker += names[nameIndex];
                }
            }
            const std::string message = item.value("message", "");
            const std::string joinedText = speaker.empty()
                ? message
                : std::format("{}: {}", speaker, message);
            file.lines.push_back({
                .index = (int)index,
                .speaker = speaker,
                .message = message,
                .joinedText = joinedText,
                .joinedTextLower = str2Lower(joinedText)
            });
        }
        files.push_back(std::move(file));
    }
    return files;
}

fs::path guessCurrentFileForTerm(
    const std::string& sourceTerm,
    const std::vector<ReviewSourceFile>& sourceFiles,
    const std::vector<fs::path>& fallbackRelFiles
) {
    for (const ReviewSourceFile& file : sourceFiles) {
        const bool matched = std::ranges::any_of(file.lines, [&](const ReviewSourceLine& line)
            {
                return line.joinedText.contains(sourceTerm);
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

json buildReviewSearchNearbyLines(const ReviewSourceFile& file, int matchIndex, int contextLines) {
    json nearbyLines = json::array();
    const int start = std::max(0, matchIndex - contextLines);
    const int end = std::min((int)file.lines.size() - 1, matchIndex + contextLines);
    for (int i = start; i <= end; ++i) {
        const ReviewSourceLine& line = file.lines[i];
        nearbyLines.push_back({
            {"id", line.index},
            {"speaker", line.speaker},
            {"message", line.message},
            {"joined_text", line.joinedText},
            {"is_match", line.index == matchIndex}
        });
    }
    return nearbyLines;
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
    else if (scope == "all_files" && env.allowCrossFileSearch) {
        if (env.sourceFiles != nullptr) {
            targetFiles = *env.sourceFiles
                | std::views::transform([](const ReviewSourceFile& file) { return file.relPath; })
                | std::ranges::to<std::vector>();
        }
    }
    else {
        targetFiles.push_back(env.currentFile);
    }

    json matches = json::array();
    const int limit = sanitizeReviewToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    const int contextLines = arguments.contains("context_lines")
        ? sanitizeReviewSearchContextLines(arguments.value("context_lines", 0))
        : 2;
    if (env.sourceFiles == nullptr) {
        return { {"queries", queries}, {"context_lines", contextLines}, {"matches", matches} };
    }

    for (const fs::path& targetFile : targetFiles) {
        const auto fileIt = std::ranges::find_if(*env.sourceFiles, [&](const ReviewSourceFile& file)
            {
                return file.relPath == targetFile;
            });
        if (fileIt == env.sourceFiles->end()) {
            continue;
        }

        for (const ReviewSourceLine& line : fileIt->lines) {
            if ((int)matches.size() >= limit) {
                break;
            }

            std::string matchedQuery;
            for (const auto& [index, queryLower] : queryLowers | std::views::enumerate) {
                if (!queryLower.empty() && line.joinedTextLower.contains(queryLower)) {
                    matchedQuery = queries[index];
                    break;
                }
            }
            if (matchedQuery.empty()) {
                continue;
            }

            matches.push_back({
                {"file", wide2Ascii(fileIt->relPath)},
                {"id", line.index},
                {"speaker", line.speaker},
                {"message", line.message},
                {"joined_text", line.joinedText},
                {"matched_query", matchedQuery},
                {"nearby_lines", buildReviewSearchNearbyLines(*fileIt, line.index, contextLines)}
            });
        }

        if ((int)matches.size() >= limit) {
            break;
        }
    }

    return {
        {"queries", queries},
        {"scope", scope},
        {"current_file", wide2Ascii(env.currentFile)},
        {"context_lines", contextLines},
        {"matches", matches}
    };
}

json buildReviewLedgerJson(const absl::btree_map<std::string, json>& ledger) {
    json result = json::object();
    for (const auto& [sourceTerm, entry] : ledger) {
        result[sourceTerm] = entry;
    }
    return result;
}

AgentSharedToolEnv buildReviewSharedToolEnv(const ReviewToolExecutionEnv& env) {
    return {
        .projectDir = env.projectDir,
        .relFiles = env.relFiles,
        .dictionaryPaths = nullptr,
        .projectNotePath = env.projectNotePath,
        .loadTermLedger = [ledger = env.ledger]()
        {
            return ledger == nullptr ? json::object() : buildReviewLedgerJson(*ledger);
        },
        .searchResultLimit = env.searchResultLimit,
        .includeLoadedDictionaryEntriesInSearch = false,
        .includeLoadedDictionaryEntriesInGetEntries = false,
        .includeTermLedgerInSearchDictionary = true,
        .includeTermLedgerInGetDictionaryEntries = true,
        .ledgerEntryType = "review_ledger"
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
                result["result"] = runAgentCommonSearchDictionaryTool(sharedEnv, call.arguments);
            }
            else if (call.name == "get_dictionary_entries") {
                result["result"] = runAgentCommonGetDictionaryEntriesTool(sharedEnv, call.arguments);
            }
            else if (call.name == "get_term") {
                result["result"] = runAgentCommonGetTermTool(sharedEnv, call.arguments);
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

json buildLedgerExcerpt(const std::vector<std::string>& entryOrder, const absl::btree_map<std::string, json>& ledger, int limit = 12) {
    json excerpt = json::array();
    if (entryOrder.empty()) {
        return excerpt;
    }
    const int start = std::max(0, (int)entryOrder.size() - limit);
    for (int index = start; index < (int)entryOrder.size(); ++index) {
        const std::string& sourceTerm = entryOrder[index];
        if (const auto it = ledger.find(sourceTerm); it != ledger.end()) {
            excerpt.push_back(agentToolLedgerEntryToJson(sourceTerm, it->second, "review_ledger"));
        }
    }
    return excerpt;
}

std::string buildReviewProjectNoteStatus(const DictionaryReviewAgentConfig& config) {
    if (!config.projectNotePath.has_value()) {
        return "No optional project note file was found.";
    }
    return std::format(
        "Optional project note is available. Use `get_project_note()` to read the user-provided script note file `{}` when it may help.",
        safeRelativePath(config.projectNotePath.value(), config.projectDir)
    );
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
    const std::vector<std::string>& entryOrder,
    const absl::btree_map<std::string, json>& ledger
) {
    std::string prompt = config.userPrompt;
    replaceStrInplace(prompt, "[TargetLang]", config.targetLang);
    replaceStrInplace(prompt, "[ReviewSchemaDescription]", buildReviewSchemaDescription());
    replaceStrInplace(prompt, "[ReviewCurrentTerm]", groupToJson(group).dump(2));
    replaceStrInplace(prompt, "[ReviewRecentTermsExcerpt]", buildLedgerExcerpt(entryOrder, ledger).dump(2));
    replaceStrInplace(prompt, "[ReviewProjectNoteStatus]", buildReviewProjectNoteStatus(config));
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

DictList DictionaryReviewAgent::review(const std::vector<DictionaryReviewTermGroup>& groups) {
    std::vector<ReviewSourceFile> sourceFiles = loadReviewSourceFiles(m_config);

    absl::btree_map<std::string, json> ledger;
    std::vector<std::string> entryOrder;
    entryOrder.reserve(groups.size());
    absl::flat_hash_map<std::string, const DictionaryReviewTermGroup*> groupLookup;
    absl::flat_hash_set<std::string> groupSourceTerms;
    groupSourceTerms.reserve(groups.size());
    for (const DictionaryReviewTermGroup& group : groups) {
        groupLookup.insert_or_assign(group.sourceTerm, &group);
        groupSourceTerms.insert(group.sourceTerm);
    }
    absl::flat_hash_set<std::string> allKnownTerms = groupSourceTerms;

    const auto ensureEntryOrder = [&](const std::string& sourceTerm)
        {
            if (sourceTerm.empty()) {
                return;
            }
            if (std::ranges::find(entryOrder, sourceTerm) == entryOrder.end()) {
                entryOrder.push_back(sourceTerm);
            }
        };

    const auto applyFallbackForGroup = [&](const DictionaryReviewTermGroup& group, std::string_view reason)
        {
            const std::string fallbackTarget = fallbackTargetForGroup(group);
            const std::string fallbackNote = fallbackNoteForGroup(group);
            json& entry = ledger[group.sourceTerm];
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
            ensureEntryOrder(group.sourceTerm);
        };

    const auto applyDecisionEntry = [&](const DictionaryReviewTermGroup& group, ReviewCommitResult decision)
        {
            const std::string fallbackTarget = fallbackTargetForGroup(group);
            const std::string fallbackNote = fallbackNoteForGroup(group);
            if (decision.sourceTerm != group.sourceTerm) {
                throw std::runtime_error(std::format("commit.source_term={} does not match current term {}", decision.sourceTerm, group.sourceTerm));
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

            json& entry = ledger[group.sourceTerm];
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
                ensureEntryOrder(group.sourceTerm);
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
                ensureEntryOrder(group.sourceTerm);
            }
            else {
                if (!allKnownTerms.contains(decision.mergeInto)) {
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
                ensureEntryOrder(group.sourceTerm);
            }

            if (!decision.addTerms.is_array()) {
                return;
            }

            int acceptedAddTerms = 0;
            for (const auto& addTerm : decision.addTerms) {
                if (acceptedAddTerms >= 3) {
                    break;
                }
                if (!addTerm.is_object()) {
                    continue;
                }
                const std::string sourceTerm = trimReviewCopy(addTerm.value("source_term", ""));
                const std::string targetTerm = trimReviewCopy(addTerm.value("target_term", ""));
                const std::string note = trimReviewCopy(addTerm.value("note", ""));
                if (sourceTerm.empty() || targetTerm.empty()) {
                    continue;
                }
                ++acceptedAddTerms;
                allKnownTerms.insert(sourceTerm);
                json& addEntry = ledger[sourceTerm];
                if (addEntry.is_object()) {
                    if (addEntry.value("note", "").empty() && !note.empty()) {
                        addEntry["note"] = note;
                    }
                    if (addEntry.value("origin", "").empty()) {
                        addEntry["origin"] = "add_term";
                    }
                    continue;
                }

                addEntry = {
                    {"target_term", targetTerm},
                    {"note", note},
                    {"status", "accepted"},
                    {"merge_into", ""},
                    {"origin", "add_term"},
                    {"updated_at", nowTimestampString()}
                };
                ensureEntryOrder(sourceTerm);
            }
        };

    const auto isRetainedStatus = [](std::string_view status)
        {
            return status == "accepted" || status == "conflict";
        };

    for (const auto& [groupIndex, group] : groups | std::views::enumerate) {
        if (m_controller->shouldStop()) {
            m_logger->info("GenDict Review Agent received stop signal; remaining terms will use local fallback.");
            applyFallbackForGroup(group, "stopped");
            continue;
        }

        const fs::path currentFile = guessCurrentFileForTerm(group.sourceTerm, sourceFiles, m_config.relInputFiles);
        m_logger->info(
            "GenDict Review Agent reviewing term {}/{}: {}, {} target candidates, {} note candidates, {} occurrences.",
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
                m_logger->info("GenDict Review Agent received stop signal while reviewing {}; using local fallback.", group.sourceTerm);
                applyFallbackForGroup(group, "stopped");
                completed = true;
                break;
            }

            json messages = buildReviewBaseMessages(m_config, group, currentFile, entryOrder, ledger);
            bool turnLoopExitedByRetry = false;
            for (int turn = 0; turn < m_config.maxTurnsPerTerm; ++turn) {
                const std::optional<TranslationApi> apiOpt = m_config.apiStrategy == "random"
                    ? m_apiPool->getApi()
                    : m_apiPool->getFirstApi();
                if (!apiOpt.has_value()) {
                    throw std::runtime_error("没有可用的API Key了");
                }
                const TranslationApi& currentApi = apiOpt.value();

                json payload = { {"messages", messages} };
                ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, 0, m_config.apiTimeoutMs);
                if (!checkResponse(
                    response, m_apiPool, currentApi, L"GenDict review agent", m_config.apiStrategy,
                    m_controller, m_logger, retryCount, 0, m_config.checkQuota
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
                        truncateReviewLog(response.content, 6000)
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
                        .ledger = &ledger,
                        .searchResultLimit = m_config.searchResultLimit,
                        .allowCrossFileSearch = m_config.allowCrossFileSearch,
                        .projectNotePath = m_config.projectNotePath
                    };

                    const json toolResults = executeReviewToolCalls(env, protocol.calls);
                    m_logger->info(
                        "GenDict Review Agent term {} executed {} tool calls: {}.",
                        group.sourceTerm,
                        protocol.calls.size(),
                        formatToolCallSummary(protocol.calls)
                    );
                    if (m_logger->should_log(spdlog::level::debug)) {
                        m_logger->debug(
                            "GenDict Review Agent term {} tool results:\n{}",
                            group.sourceTerm,
                            truncateReviewLog(toolResults.dump(2), 12000)
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
                    m_logger->info("GenDict Review Agent term {} chose skip, using local fallback.", group.sourceTerm);
                    applyFallbackForGroup(group, "skip");
                    completed = true;
                    break;
                }

                if (protocol.action == "commit") {
                    try {
                        applyDecisionEntry(group, protocol.result);
                        completed = true;
                        if (m_logger->should_log(spdlog::level::debug)) {
                            m_logger->debug(
                                "GenDict Review Agent term {} commit succeeded:\n{}",
                                group.sourceTerm,
                                truncateReviewLog(response.content, 12000)
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
                            truncateReviewLog(response.content, 6000)
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
            applyFallbackForGroup(group, exceededTurnLimit ? "max_turns" : "retry_exhausted");
        }
    }

    for (auto& [sourceTerm, entry] : ledger) {
        if (entry.value("status", "") != "merged") {
            continue;
        }
        const std::string mergeInto = entry.value("merge_into", "");
        const auto mergeIt = ledger.find(mergeInto);
        const bool mergeTargetKept = mergeIt != ledger.end() && 
            isRetainedStatus(mergeIt->second.value("status", ""));
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
    finalList.reserve(entryOrder.size());
    for (const DictionaryReviewTermGroup& group : groups) {
        const auto it = ledger.find(group.sourceTerm);
        if (it == ledger.end()) {
            continue;
        }
        const std::string status = it->second.value("status", "");
        if (!isRetainedStatus(status)) {
            continue;
        }
        finalList.emplace_back(
            group.sourceTerm,
            it->second.value("target_term", ""),
            it->second.value("note", "")
        );
    }

    for (const std::string& sourceTerm : entryOrder) {
        if (groupSourceTerms.contains(sourceTerm)) {
            continue;
        }
        const auto it = ledger.find(sourceTerm);
        if (it == ledger.end()) {
            continue;
        }
        const std::string status = it->second.value("status", "");
        if (!isRetainedStatus(status)) {
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
