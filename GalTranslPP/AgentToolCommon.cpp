module;

#include <Shlwapi.h>

#include "GPPMacros.hpp"
#include <toml.hpp>

module AgentToolCommon;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

struct AgentLoadedDictionaryEntry {
    std::string sourceTerm;
    std::string targetTerm;
    std::string note;
};

std::string trimAgentToolValue(const std::string& value) {
    const auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
    const auto begin = std::ranges::find_if(value, isNotSpace);
    if (begin == value.end()) {
        return {};
    }
    const auto end = std::ranges::find_if(value | std::views::reverse, isNotSpace).base();
    return std::string(begin, end);
}

std::optional<json> tryParseAgentJsonEnvelope(const std::string& text, const AgentToolJsonEnvelopeParseOptions& options) {
    std::string newText = trimAgentToolValue(text);
    if (newText.empty()) {
        return std::nullopt;
    }

    if (options.allowCodeFence) {
        const size_t fencedStart = newText.find("```");
        if (fencedStart != std::string::npos) {
            const size_t lineEnd = newText.find('\n', fencedStart);
            const size_t fencedEnd = newText.rfind("```");
            if (lineEnd != std::string::npos && fencedEnd != std::string::npos && fencedEnd > lineEnd) {
                newText = trimAgentToolValue(newText.substr(lineEnd + 1, fencedEnd - lineEnd - 1));
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

std::vector<AgentLoadedDictionaryEntry> loadAgentDictionaryEntries(const AgentSharedToolEnv& env) {
    std::vector<AgentLoadedDictionaryEntry> entries;
    if (env.dictionaryPaths == nullptr) {
        return entries;
    }
    for (const fs::path& dictPath : *env.dictionaryPaths) {
        try {
            const auto dictData = toml::uparse(dictPath);
            if (!dictData.contains("gptDict")) {
                continue;
            }
            const auto& dictTbls = dictData.at("gptDict").as_array();
            for (const auto& el : dictTbls) {
                const std::string sourceTerm = el.contains("org")
                    ? el.at("org").as_string()
                    : (el.contains("searchStr") ? el.at("searchStr").as_string() : "");
                const std::string targetTerm = el.contains("rep")
                    ? el.at("rep").as_string()
                    : (el.contains("replaceStr") ? el.at("replaceStr").as_string() : "");
                const std::string note = toml::find_or(el, "note", "");
                if (sourceTerm.empty() && targetTerm.empty() && note.empty()) {
                    continue;
                }
                entries.push_back({
                    .sourceTerm = sourceTerm,
                    .targetTerm = targetTerm,
                    .note = note
                });
            }
        }
        catch (...) { }
    }
    return entries;
}

json loadAgentToolLedger(const AgentSharedToolEnv& env) {
    json ledger = env.loadTermLedger ? env.loadTermLedger() : json::object();
    if (!ledger.is_object()) {
        return json::object();
    }
    return ledger;
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
                const std::string value = trimAgentToolValue(query.get<std::string>());
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
                !PathMatchSpecW(relFile.c_str(), spec.c_str())
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

json runAgentCommonSearchDictionaryTool(const AgentSharedToolEnv& env, const json& arguments) {
    const std::vector<std::string> queries = collectAgentToolQueries(arguments);
    const std::vector<std::string> queryLowers = queries
        | std::views::transform([](const std::string& query) { return str2Lower(query); })
        | std::ranges::to<std::vector>();
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    json matches = json::array();
    int matchCount = 0;

    auto matchedByQueryFunc = [&](const std::string& sourceTerm, const std::string& targetTerm, const std::string& note) -> bool
        {
            if (queryLowers.empty()) {
                return true;
            }
            const std::string haystack = str2Lower(sourceTerm + "\n" + targetTerm + "\n" + note);
            return std::ranges::any_of(queryLowers, [&](const std::string& queryLower)
                {
                    return !queryLower.empty() && haystack.contains(queryLower);
                });
        };

    if (env.includeLoadedDictionaryEntriesInSearchDictionary) {
        if (env.loadedDictionaryEntriesCache && env.loadedDictionaryEntriesCache->is_null()) {
            *env.loadedDictionaryEntriesCache = json::array();
            for (const AgentLoadedDictionaryEntry& entry : loadAgentDictionaryEntries(env)) {
                env.loadedDictionaryEntriesCache->push_back({
                    {"source_term", entry.sourceTerm},
                    {"target_term", entry.targetTerm},
                    {"note", entry.note}
                });
            }
        }
        else if (env.loadedDictionaryEntriesCache && !env.loadedDictionaryEntriesCache->is_array()) {
            *env.loadedDictionaryEntriesCache = json::array();
        }

        if (env.loadedDictionaryEntriesCache) {
            for (const json& entry : *env.loadedDictionaryEntriesCache) {
                if (matchedByQueryFunc(
                    entry.value("source_term", ""),
                    entry.value("target_term", ""),
                    entry.value("note", "")
                )) {
                    ++matchCount;
                    if (matchCount <= start || (int)matches.size() >= limit) {
                        continue;
                    }
                    matches.push_back(entry);
                }
            }
        }
        else {
            for (const AgentLoadedDictionaryEntry& entry : loadAgentDictionaryEntries(env)) {
                if (matchedByQueryFunc(entry.sourceTerm, entry.targetTerm, entry.note)) {
                    ++matchCount;
                    if (matchCount <= start || (int)matches.size() >= limit) {
                        continue;
                    }
                    matches.push_back({
                        {"source_term", entry.sourceTerm},
                        {"target_term", entry.targetTerm},
                        {"note", entry.note}
                    });
                }
            }
        }
    }

    if (env.includeTermLedgerInSearchDictionary) {
        const json ledger = loadAgentToolLedger(env);
        for (const auto& item : ledger.items()) {
            const json& entry = item.value();
            if (matchedByQueryFunc(item.key(), entry.value("target_term", ""), entry.value("note", ""))) {
                ++matchCount;
                if (matchCount <= start || (int)matches.size() >= limit) {
                    continue;
                }
                matches.push_back(agentToolLedgerEntryToJson(item.key(), entry));
            }
        }
    }

    return json{
        {"queries", queries},
        {"start", start},
        {"limit", limit},
        {"total", matchCount},
        {"matches", matches}
    };
}


json runAgentCommonGetProjectNoteTool(const AgentSharedToolEnv& env, const json& arguments) {
    if (!env.projectNotePath.has_value()) {
        return {
            {"available", false},
            {"file", nullptr},
            {"content", ""}
        };
    }
    std::ifstream ifs(env.projectNotePath.value(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return json{
        {"file", safeRelativePath(env.projectNotePath.value(), env.projectDir)},
        {"content", content},
    };
}
