module;

#include "GPPMacros.hpp"
#include <toml.hpp>

export module SkipTrans;

export import GPPDefines;
export import LuaManager;
export import PythonManager;

namespace fs = std::filesystem;

export
{
    class SkipTrans {
    private:
        std::vector<std::string> m_hKeys;
        std::vector<CheckSeCondFunc> m_skipKeys;
        std::shared_ptr<spdlog::logger> m_logger;
        bool m_skipH;

        PluginRunTime m_runTime;

        static void processSkipSentence(Sentence* se, const std::string& info);
        void skipImpl(Sentence* se);

    public:
        SkipTrans(const fs::path& projectDir, const toml::value& projectConfig, 
            const std::unique_ptr<PythonManager>& pythonManager, const std::unique_ptr<LuaManager>& luaManager,
            const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime);
        ~SkipTrans() = default;

        void dPreRun(Sentence* se);
        void preRun(Sentence* se);
    };
}
