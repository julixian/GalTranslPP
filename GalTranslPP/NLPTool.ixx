module;

#include "GPPMacros.hpp"

export module NLPTool;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    NLPTokenizeFunc getMeCabTokenizeFunc(const std::string& mecabDictDir, const std::shared_ptr<spdlog::logger>& logger);

    NLPTokenizeFunc getPythonNLPTokenizeFunc(const std::vector<std::string>& dependencies, const std::string& moduleName,
        const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger);


    std::vector<std::string_view> splitIntoTokenViews(const WordPosVec& wordPosVec, std::string_view text);
    std::vector<std::string> splitIntoTokens(const WordPosVec& wordPosVec, std::string_view text);
}
