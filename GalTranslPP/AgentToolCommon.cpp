module;

#include "GPPMacros.hpp"

#ifdef _WIN32
#include <Shlwapi.h>
#endif

module AgentToolCommon;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

std::optional<json> tryParseAgentJsonEnvelope(const std::string& text, const AgentToolJsonEnvelopeParseOptions& options) {
    std::string newText = text;
    if (newText.empty()) {
        return std::nullopt;
    }

    if (options.allowCodeFence) {
        const size_t fencedStart = newText.find("```");
        if (fencedStart != std::string::npos) {
            const size_t lineEnd = newText.find('\n', fencedStart);
            const size_t fencedEnd = newText.rfind("```");
            if (lineEnd != std::string::npos && fencedEnd != std::string::npos && fencedEnd > lineEnd) {
                newText = newText.substr(lineEnd + 1, fencedEnd - lineEnd - 1);
            }
        }
    }

    try {
        return json::parse(newText);
    }
    catch (...) { }

    if (options.allowLightRepair) {
        newText = lightRepairJsonText(newText);
        try {
            return json::parse(newText);
        }
        catch (...) { }
    }

    if (!options.allowSubstringFallback) {
        return std::nullopt;
    }

    const size_t jsonStart = newText.find('{');
    const size_t jsonEnd = newText.rfind('}');
    if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
        return std::nullopt;
    }

    const std::string jsonSlice = newText.substr(jsonStart, jsonEnd - jsonStart + 1);
    try {
        return json::parse(jsonSlice);
    }
    catch (...) {
        if (!options.allowLightRepair) {
            return std::nullopt;
        }
        try {
            return json::parse(lightRepairJsonText(jsonSlice));
        }
        catch (...) {
            return std::nullopt;
        }
    }
}

int sanitizeAgentToolLimit(int requested, int maxLimit) {
    if (requested <= 0) {
        return maxLimit;
    }
    return std::min(requested, maxLimit);
}

int sanitizeAgentContextLines(int requested, int maxLimit) {
    if (requested < 0) {
        return 0;
    }
    return std::min(requested, maxLimit);
}

std::string safeRelativePath(const fs::path& path, const fs::path& root) {
    try {
        return wide2Ascii(fs::relative(path, root));
    }
    catch (...) {
        return wide2Ascii(path.filename());
    }
}

std::vector<std::string> collectAgentToolQueries(const json& arguments) {
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

json agentToolLedgerEntryToJson(const std::string& sourceTerm, const json& entry) {
    return json{
        {"source_term", sourceTerm},
        {"target_term", entry.value("target_term", "")},
        {"category", entry.value("category", "")},
        {"note", entry.value("note", "")},
        {"status", entry.value("status", "")},
        {"merge_into", entry.value("merge_into", "")},
        {"origin", entry.value("origin", "")},
        {"occurrences", entry.value("occurrences", json::array())}
    };
}

json runAgentCommonListFilesTool(const AgentSharedToolEnv& env, const json& arguments) {
    const std::wstring spec = str2Lower(ascii2Wide(arguments.value("spec", "")));
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    json files = json::array();
    if (env.relFiles == nullptr) {
        return json{
            {"files", files},
            {"start", start},
            {"limit", limit},
            {"total", 0}
        };
    }

    int matchCount = 0;
    for (const fs::path& relFile : *env.relFiles) {
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
        if (env.getFileLineCount) {
            const std::optional<int> lineCount = env.getFileLineCount(relFile);
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

json runAgentCommonGetProjectNoteTool(const AgentSharedToolEnv& env, const json& arguments) {
    if (!env.projectNotePath.has_value()) {
        return json{
            {"available", false},
            {"file", nullptr},
            {"content", ""}
        };
    }
    std::ifstream ifs(env.projectNotePath.value(), std::ios::binary);
    if (!ifs) {
        return json{
            {"available", false},
            {"file", safeRelativePath(env.projectNotePath.value(), env.projectDir)},
            {"content", ""}
        };
    }
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return json{
        {"file", safeRelativePath(env.projectNotePath.value(), env.projectDir)},
        {"content", content},
    };
}
