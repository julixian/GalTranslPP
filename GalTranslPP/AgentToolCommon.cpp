module;

#include "GPPMacros.hpp"

#ifdef _WIN32
#include <Shlwapi.h>
#endif

module AgentToolCommon;

import Tool;

namespace fs = std::filesystem;

json makeAgentCommonSearchTextResult(
    const std::vector<std::string>& queries,
    std::string_view scope,
    const fs::path& currentFile,
    int start,
    int limit,
    bool includeSpeaker,
    int total,
    int contextLines,
    json matches
) {
    return json{
        {"queries", queries},
        {"scope", scope},
        {"current_file", wide2Ascii(currentFile)},
        {"start", start},
        {"limit", limit},
        {"include_speaker", includeSpeaker},
        {"total", total},
        {"context_lines", contextLines},
        {"matches", std::move(matches)}
    };
}

json runAgentCommonSourceSearchTextTool(
    const fs::path& currentFile,
    const std::vector<fs::path>& relFiles,
    const std::function<const AgentCommonSourceFileView*(const fs::path&)>& findSourceFile,
    int searchResultLimit,
    int contextLinesLimit,
    bool requireQuery,
    const std::string& invalidScopeErrorMessage,
    const json& arguments
) {
    const std::vector<std::string> queries = collectAgentCommonToolQueries(arguments);
    if (requireQuery && queries.empty()) {
        return { {"error", "search_text requires query or queries"} };
    }

    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();
    const std::string scope = arguments.value("scope", "current_file");
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", searchResultLimit), searchResultLimit);
    const int contextLines = sanitizeAgentCommonContextLines(arguments.value("context_lines", 1), contextLinesLimit);
    const bool includeSpeaker = arguments.value("include_speaker", true);

    if (scope != "current_file" && scope != "all_files" && scope != "specified_file") {
        return json{
            {"error", invalidScopeErrorMessage.empty()
                ? std::format("Invalid search_text.scope: {}", scope)
                : invalidScopeErrorMessage},
            {"allowed_scope", json::array({"current_file", "all_files", "specified_file"})}
        };
    }

    std::vector<fs::path> targetFiles;
    if (scope == "specified_file") {
        targetFiles.push_back(ascii2Wide(arguments.value("file", wide2Ascii(currentFile))));
    }
    else if (scope == "all_files") {
        targetFiles = relFiles;
    }
    else {
        targetFiles.push_back(currentFile);
    }

    json matches = json::array();
    if (!findSourceFile) {
        return makeAgentCommonSearchTextResult(
            queries,
            scope,
            currentFile,
            start,
            limit,
            includeSpeaker,
            0,
            contextLines,
            std::move(matches)
        );
    }

    int matchCount = 0;
    for (const fs::path& targetFile : targetFiles) {
        const AgentCommonSourceFileView* sourceFile = findSourceFile(targetFile);
        if (sourceFile == nullptr) {
            continue;
        }

        for (const auto& [lineIndex, line] : sourceFile->lines | std::views::enumerate) {
            const std::string& sourceHaystackLower = includeSpeaker ? line.sourceTextWithSpeakerLower : line.sourceTextLower;
            const bool matched = queryLowers.empty() || std::ranges::any_of(queryLowers, [&](const std::string& queryLower)
                {
                    return !queryLower.empty() && sourceHaystackLower.contains(queryLower);
                });
            if (!matched) {
                continue;
            }

            ++matchCount;
            if (matchCount <= start || (int)matches.size() >= limit) {
                continue;
            }

            matches.push_back({
                {"file", wide2Ascii(targetFile)},
                {"id", line.id},
                {"speaker", line.speaker},
                {"message", line.sourceText},
                {"nearby_lines", buildAgentCommonSourceNearbyLines(sourceFile->lines, (int)lineIndex, contextLines)}
            });
        }
    }

    return makeAgentCommonSearchTextResult(
        queries,
        scope,
        currentFile,
        start,
        limit,
        includeSpeaker,
        matchCount,
        contextLines,
        std::move(matches)
    );
}

std::optional<int> parseAgentCommonJsonInt(const json& object) {
    if (object.is_number_integer()) {
        const int64_t value = object.get<int64_t>();
        if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
            return (int)value;
        }
        return std::nullopt;
    }
    if (object.is_string()) {
        const std::string value = object.get<std::string>();
        int parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec == std::errc{} && ptr == value.data() + value.size()) {
            return parsed;
        }
    }
    return std::nullopt;
}

size_t approximateAgentCommonMessagesBytes(const json& messages) {
    return std::ranges::fold_left(messages, 0uz, [](size_t acc, const auto& item)
        {
            return acc + item.dump().size();
        });
}

std::optional<json> tryParseAgentCommonJsonEnvelope(const std::string& text) {
    std::string newText = text;
    if (newText.empty()) {
        return std::nullopt;
    }

    const size_t fencedStart = newText.find("```");
    if (fencedStart != std::string::npos) {
        const size_t lineEnd = newText.find('\n', fencedStart);
        const size_t fencedEnd = newText.rfind("```");
        if (lineEnd != std::string::npos && fencedEnd != std::string::npos && fencedEnd > lineEnd) {
            newText = newText.substr(lineEnd + 1, fencedEnd - lineEnd - 1);
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

    const std::string jsonSlice = newText.substr(jsonStart, jsonEnd - jsonStart + 1);
    try {
        return json::parse(jsonSlice);
    }
    catch (...) { }

    try {
        return json::parse(lightRepairJsonText(jsonSlice));
    }
    catch (...) { }

    return std::nullopt;
}

std::vector<AgentCommonToolCallRequest> parseAgentCommonToolCallRequests(const json& payload) {
    std::vector<AgentCommonToolCallRequest> calls;
    if (const auto it = payload.find("calls"); it != payload.end() && it->is_array()) {
        for (const auto& call : *it) {
            if (!call.is_object()) {
                continue;
            }
            AgentCommonToolCallRequest parsed;
            parsed.id = call.value("id", std::format("call_{}", calls.size()));
            parsed.name = call.value("name", "");
            if (const auto argIt = call.find("arguments"); argIt != call.end()) {
                parsed.arguments = *argIt;
            }
            calls.push_back(std::move(parsed));
        }
    }
    return calls;
}

std::string formatAgentCommonToolCallDetails(const std::vector<AgentCommonToolCallRequest>& calls) {
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

std::string formatAgentCommonToolCallNames(const std::vector<AgentCommonToolCallRequest>& calls) {
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

int sanitizeAgentCommonToolLimit(int requested, int maxLimit) {
    if (requested <= 0) {
        return maxLimit;
    }
    return std::min(requested, maxLimit);
}

int sanitizeAgentCommonContextLines(int requested, int maxLimit) {
    if (requested < 0) {
        return maxLimit;
    }
    return std::min(requested, maxLimit);
}

std::string safeRelativePath(const fs::path& path, const fs::path& root) {
    std::error_code ec;
    const fs::path relPath = fs::relative(path, root, ec);
    if (ec) {
        return wide2Ascii(path.filename());
    }
    return wide2Ascii(relPath);
}

std::vector<std::string> collectAgentCommonToolQueries(const json& arguments) {
    std::vector<std::string> queries;
    if (auto it = arguments.find("queries"); it != arguments.end() && it->is_array()) {
        for (const auto& query : *it) {
            if (query.is_string()) {
                const std::string value = query.get<std::string>();
                if (!value.empty()) {
                    queries.push_back(value);
                }
            }
        }
    }
    else if (it = arguments.find("query"); it != arguments.end() && it->is_string()) {
        std::string query = it->get<std::string>();
        if (!query.empty()) {
            queries.push_back(std::move(query));
        }
    }
    return queries;
}

json runAgentCommonListFilesTool(
    const std::vector<fs::path>& relFiles,
    const std::function<std::optional<int>(const fs::path&)>& getFileLineCount,
    int searchResultLimit,
    const json& arguments
) {
    const std::wstring spec = str2Lower(ascii2Wide(arguments.value("spec", "")));
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", searchResultLimit), searchResultLimit);
    json files = json::array();

    int matchCount = 0;
    for (const fs::path& relFile : relFiles) {
        if (!spec.empty()) {
            if (!str2Lower(relFile.wstring()).contains(spec) &&
#ifdef _WIN32
                !PathMatchSpecW(relFile.c_str(), spec.c_str())
#endif
                )
            {
                continue;
            }
        }
        ++matchCount;
        if (matchCount <= start || (int)files.size() >= limit) {
            continue;
        }

        json fileInfo = {
            {"file", wide2Ascii(relFile)}
        };
        if (getFileLineCount) {
            const std::optional<int> lineCount = getFileLineCount(relFile);
            if (lineCount.has_value()) {
                fileInfo["lines"] = lineCount.value();
            }
        }
        files.push_back(std::move(fileInfo));
    }
    return json{
        {"files", files},
        {"start", start},
        {"limit", limit},
        {"total", matchCount}
    };
}

json runAgentCommonGetProjectNoteTool(
    const fs::path& projectDir,
    const std::optional<fs::path>& projectNotePath,
    const json& arguments
) {
    if (!projectNotePath.has_value()) {
        return json{
            {"available", false},
            {"file", nullptr},
            {"content", ""}
        };
    }
    if (!fs::exists(projectNotePath.value())) {
        return json{
            {"available", false},
            {"file", safeRelativePath(projectNotePath.value(), projectDir)},
            {"content", ""}
        };
    }
    std::ifstream ifs(projectNotePath.value(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return json{
        {"file", safeRelativePath(projectNotePath.value(), projectDir)},
        {"content", content},
    };
}
