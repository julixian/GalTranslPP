module;

#include "GPPMacros.hpp"

export module AgentToolCommon;

import GPPDefines;

namespace fs = std::filesystem;

export {
    struct AgentSharedToolEnv {
        fs::path projectDir;
        const std::vector<fs::path>* relFiles = nullptr;
        const std::vector<fs::path>* dictionaryPaths = nullptr;
        std::optional<fs::path> projectNotePath;
        std::function<json()> loadTermLedger;
        int searchResultLimit = 80;
        bool includeLoadedDictionaryEntriesInSearch = true;
        bool includeLoadedDictionaryEntriesInGetEntries = true;
        bool includeTermLedgerInSearchDictionary = false;
        bool includeTermLedgerInGetDictionaryEntries = false;
        std::string ledgerEntryType = "term_ledger";
    };

    std::string safeRelativePath(const fs::path& path, const fs::path& root);
    std::vector<std::string> collectAgentToolQueries(const json& arguments);
    json agentToolLedgerEntryToJson(const std::string& sourceTerm, const json& entry, std::string_view entryType = "term_ledger");

    json runAgentCommonListFilesTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonSearchDictionaryTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonGetDictionaryEntriesTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonGetTermTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonGetProjectNoteTool(const AgentSharedToolEnv& env, const json& arguments);
}
