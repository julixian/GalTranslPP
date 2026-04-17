module;

#include "GPPMacros.hpp"
#include <toml.hpp>

module AgentToolCommon;

import NormalJsonTranslatorHelperTool;
import Tool;

namespace fs = std::filesystem;

struct AgentLoadedDictionaryEntry {
    std::string type;
    std::string file;
    std::string sourceTerm;
    std::string targetTerm;
    std::string note;
    std::string status;
    std::string mergeInto;
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

int sanitizeAgentToolLimit(int requested, int fallback, int maxLimit) {
    if (requested <= 0) {
        return fallback;
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
                    .type = "gpt_dict",
                    .file = safeRelativePath(dictPath, env.projectDir),
                    .sourceTerm = sourceTerm,
                    .targetTerm = targetTerm,
                    .note = note,
                    .status = "loaded",
                    .mergeInto = ""
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
    if (const auto it = arguments.find("queries"); it != arguments.end() && it->is_array()) {
        for (const auto& query : *it) {
            if (query.is_string()) {
                const std::string value = trimAgentToolValue(query.get<std::string>());
                if (!value.empty()) {
                    queries.push_back(value);
                }
            }
        }
    }
    if (queries.empty()) {
        const std::string query = trimAgentToolValue(arguments.value("query", ""));
        if (!query.empty()) {
            std::istringstream iss(query);
            for (std::string token; iss >> token;) {
                queries.push_back(token);
            }
            if (queries.empty()) {
                queries.push_back(query);
            }
        }
    }
    return queries;
}

json agentToolLedgerEntryToJson(const std::string& sourceTerm, const json& entry, std::string_view entryType) {
    return {
        {"type", entryType},
        {"source_term", sourceTerm},
        {"target_term", entry.value("target_term", "")},
        {"note", entry.value("note", "")},
        {"status", entry.value("status", "")},
        {"merge_into", entry.value("merge_into", "")},
        {"origin", entry.value("origin", "")}
    };
}

json runAgentCommonListFilesTool(const AgentSharedToolEnv& env, const json& arguments) {
    const std::string pattern = str2Lower(arguments.value("pattern", ""));
    const int limit = sanitizeAgentToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit);
    json files = json::array();
    if (env.relFiles == nullptr) {
        return { {"files", files} };
    }
    for (const fs::path& relFile : *env.relFiles) {
        const std::string relFileStr = wide2Ascii(relFile);
        if (!pattern.empty() && !str2Lower(relFileStr).contains(pattern)) {
            continue;
        }
        files.push_back(relFileStr);
        if ((int)files.size() >= limit) {
            break;
        }
    }
    return { {"files", files} };
}

json runAgentCommonSearchDictionaryTool(const AgentSharedToolEnv& env, const json& arguments) {
    const std::vector<std::string> queries = collectAgentToolQueries(arguments);
    if (queries.empty()) {
        return { {"error", "search_dictionary requires query or queries"} };
    }
    const std::string mode = str2Lower(arguments.value("mode", "fuzzy"));
    const int limit = sanitizeAgentToolLimit(arguments.value("limit", env.searchResultLimit), env.searchResultLimit, 200);
    json matches = json::array();

    const auto matchedByQuery = [&](const std::string& sourceTerm, const std::string& targetTerm,
        const std::string& note, const std::string& extra = {}) -> bool
        {
            const std::string haystack = str2Lower(sourceTerm + "\n" + targetTerm + "\n" + note + "\n" + extra);
            return std::ranges::any_of(queries, [&](const std::string& query)
                {
                    if (query.empty()) {
                        return false;
                    }
                    if (mode == "exact") {
                        return sourceTerm == query || targetTerm == query;
                    }
                    return haystack.contains(str2Lower(query));
                });
        };

    if (env.includeLoadedDictionaryEntriesInSearch) {
        for (const AgentLoadedDictionaryEntry& entry : loadAgentDictionaryEntries(env)) {
            if ((int)matches.size() >= limit) {
                break;
            }
            if (matchedByQuery(entry.sourceTerm, entry.targetTerm, entry.note, entry.file)) {
                matches.push_back({
                    {"type", entry.type},
                    {"file", entry.file},
                    {"source_term", entry.sourceTerm},
                    {"target_term", entry.targetTerm},
                    {"note", entry.note},
                    {"status", entry.status},
                    {"merge_into", entry.mergeInto}
                });
            }
        }
    }

    if (env.includeTermLedgerInSearchDictionary) {
        const json ledger = loadAgentToolLedger(env);
        for (const auto& item : ledger.items()) {
            if ((int)matches.size() >= limit) {
                break;
            }
            const json& entry = item.value();
            if (matchedByQuery(item.key(), entry.value("target_term", ""), entry.value("note", ""),
                entry.value("status", "") + "\n" + entry.value("merge_into", ""))) {
                matches.push_back(agentToolLedgerEntryToJson(item.key(), entry, env.ledgerEntryType));
            }
        }
    }

    return {
        {"queries", queries},
        {"mode", mode},
        {"matches", matches}
    };
}

json runAgentCommonGetDictionaryEntriesTool(const AgentSharedToolEnv& env, const json& arguments) {
    std::vector<std::string> terms;
    if (auto it = arguments.find("terms"); it != arguments.end() && it->is_array()) {
        for (const auto& term : *it) {
            if (term.is_string()) {
                const std::string value = trimAgentToolValue(term.get<std::string>());
                if (!value.empty()) {
                    terms.push_back(value);
                }
            }
        }
    }
    else if (it = arguments.find("term"); it != arguments.end() && it->is_string()) {
        const std::string value = trimAgentToolValue(it->get<std::string>());
        if (!value.empty()) {
            terms.push_back(value);
        }
    }

    const auto exactMatched = [&](const std::string& sourceTerm, const std::string& targetTerm) -> bool
        {
            if (terms.empty()) {
                return true;
            }
            return std::ranges::any_of(terms, [&](const std::string& term)
                {
                    return sourceTerm == term || targetTerm == term;
                });
        };

    const int limit = sanitizeAgentToolLimit(arguments.value("limit", 200), 200, 2000);
    json entries = json::array();
    int matchedEntries = 0;

    if (env.includeLoadedDictionaryEntriesInGetEntries) {
        for (const AgentLoadedDictionaryEntry& entry : loadAgentDictionaryEntries(env)) {
            if (!exactMatched(entry.sourceTerm, entry.targetTerm)) {
                continue;
            }
            ++matchedEntries;
            if ((int)entries.size() < limit) {
                entries.push_back({
                    {"type", entry.type},
                    {"file", entry.file},
                    {"source_term", entry.sourceTerm},
                    {"target_term", entry.targetTerm},
                    {"note", entry.note},
                    {"status", entry.status},
                    {"merge_into", entry.mergeInto}
                });
            }
        }
    }

    if (env.includeTermLedgerInGetDictionaryEntries) {
        const json ledger = loadAgentToolLedger(env);
        for (const auto& item : ledger.items()) {
            const json& entry = item.value();
            if (!exactMatched(item.key(), entry.value("target_term", ""))) {
                continue;
            }
            ++matchedEntries;
            if ((int)entries.size() < limit) {
                entries.push_back(agentToolLedgerEntryToJson(item.key(), entry, env.ledgerEntryType));
            }
        }
    }

    return {
        {"entries", entries},
        {"matched_entries", matchedEntries},
        {"returned_entries", (int)entries.size()},
        {"truncated", (int)entries.size() < matchedEntries}
    };
}

json runAgentCommonGetTermTool(const AgentSharedToolEnv& env, const json& arguments) {
    const std::string term = trimAgentToolValue(arguments.value("term", ""));
    if (term.empty()) {
        return {
            {"term", term},
            {"entry", nullptr},
            {"error", "get_term requires term"}
        };
    }
    const json ledger = loadAgentToolLedger(env);
    return {
        {"term", term},
        {"entry", ledger.contains(term) ? ledger.at(term) : json()}
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
    const int maxChars = sanitizeAgentToolLimit(arguments.value("max_chars", 20000), 20000, 120000);
    std::ifstream ifs(env.projectNotePath.value(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const bool truncated = (int)content.size() > maxChars;
    return {
        {"available", true},
        {"file", safeRelativePath(env.projectNotePath.value(), env.projectDir)},
        {"content", truncated ? content.substr(0, maxChars) : content},
        {"truncated", truncated},
        {"total_chars", (int)content.size()}
    };
}
