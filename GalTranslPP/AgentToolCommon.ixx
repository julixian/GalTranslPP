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
        const std::vector<fs::path>* relFiles = nullptr;
        std::optional<fs::path> projectNotePath;
        std::function<std::optional<int>(const fs::path&)> getFileLineCount;
        int searchResultLimit = 80;
    };

    std::string safeRelativePath(const fs::path& path, const fs::path& root);
    std::optional<json> tryParseAgentJsonEnvelope(const std::string& text, const AgentToolJsonEnvelopeParseOptions& options = {});
    int sanitizeAgentToolLimit(int requested, int maxLimit = 200);
    int sanitizeAgentContextLines(int requested, int maxLimit = 20);
    std::vector<std::string> collectAgentToolQueries(const json& arguments);
    json agentToolLedgerEntryToJson(const std::string& sourceTerm, const json& entry);

    json runAgentCommonListFilesTool(const AgentSharedToolEnv& env, const json& arguments);
    json runAgentCommonGetProjectNoteTool(const AgentSharedToolEnv& env, const json& arguments);
}
