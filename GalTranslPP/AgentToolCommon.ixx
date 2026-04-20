module;

#include "GPPMacros.hpp"

export module AgentToolCommon;

import GPPDefines;

namespace fs = std::filesystem;

export {
    struct AgentToolJsonEnvelopeParseOptions {
        bool allowCodeFence = true;
        bool allowLightRepair = true;
        bool allowSubstringFallback = true;
    };

    struct AgentSharedToolEnv {
        fs::path projectDir;
        fs::path currentFile;
        const std::vector<fs::path>* relFiles = nullptr;
        const std::vector<fs::path>* dictionaryPaths = nullptr;
        std::optional<fs::path> projectNotePath;
        std::function<json()> loadTermLedger;
        std::function<std::optional<int>(const fs::path&)> getFileLineCount;
        std::shared_ptr<json> loadedDictionaryEntriesCache;
        int searchResultLimit = 80;
        bool includeLoadedDictionaryEntriesInSearchDictionary = true;
        bool includeTermLedgerInSearchDictionary = false;
    };

    std::string safeRelativePath(const fs::path& path, const fs::path& root);
    std::string trimAgentToolValue(const std::string& value);
    std::optional<json> tryParseAgentJsonEnvelope(const std::string& text, const AgentToolJsonEnvelopeParseOptions& options = {});
    int sanitizeAgentToolLimit(int requested, int maxLimit = 200);
    int sanitizeAgentContextLines(int requested, int maxLimit = 20);
    std::vector<std::string> collectAgentToolQueries(const json& arguments);
    json agentToolLedgerEntryToJson(const std::string& sourceTerm, const json& entry);

    json runAgentCommonListFilesTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonSearchDictionaryTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonGetProjectNoteTool(const AgentSharedToolEnv& env, const json& arguments);
}
