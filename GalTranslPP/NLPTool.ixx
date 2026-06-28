module;

#include "GPPMacros.hpp"

export module NLPTool;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    std::function<NLPResult(const std::string&)> getMeCabTokenizeFunc(const std::string& mecabDictDir, const std::shared_ptr<spdlog::logger>& logger);

    std::function<NLPResult(const std::string&)> getNLPTokenizeFunc(const std::vector<std::string>& dependencies, const std::string& moduleName,
        const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger);
}
