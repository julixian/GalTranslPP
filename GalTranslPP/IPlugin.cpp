module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <proxy/proxy.h>

module IPlugin;

import Tool;
import SkipTrans;
import TextFull2Half;
import TextLinebreakFix;
import LuaTextPlugin;
import PythonTextPlugin;

namespace fs = std::filesystem;

void registerPlugins(std::vector<pro::proxy<PPlugin>>& plugins, const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
    const std::unique_ptr<PythonManager>& pythonManager, const std::unique_ptr<LuaManager>& luaManager, const std::shared_ptr<spdlog::logger>& logger,
    const toml::value& projectConfig, bool preProcOnly) {

    auto runTimeCheckFunc = [&](PluginRunTime runTime) -> bool
        {
            if (runTime == PluginRunTime::DPre || runTime == PluginRunTime::Pre) {
                if (preProcOnly) {
                    return true;
                }
            }
            if (runTime == PluginRunTime::Post || runTime == PluginRunTime::DPost) {
                if (!preProcOnly) {
                    return true;
                }
            }
            return false;
        };

    for (const auto& pluginName : pluginNames) {
        if (pluginName.starts_with('>')) {
            continue;
        }
        const std::string pluginNameLower = str2Lower(pluginName);
        if (pluginNameLower.ends_with(".lua") && preProcOnly) {
            plugins.push_back(pro::make_proxy<PPlugin, LuaTextPlugin>(projectDir, replaceStr(pluginName, "%PROJECT_DIR%", wide2Ascii(projectDir)),
                luaManager, logger));
        }
        else if (pluginNameLower.ends_with(".py") && preProcOnly) {
            plugins.push_back(pro::make_proxy<PPlugin, PythonTextPlugin>(projectDir, replaceStr(pluginName, "%PROJECT_DIR%", wide2Ascii(projectDir)),
                pythonManager, logger));
        }
        else if (pluginName.contains("SkipTrans")) {
            const PluginRunTime runTime = choosePluginRunTime(pluginNameLower, PluginRunTime::Pre);
            if (runTimeCheckFunc(runTime)) {
                plugins.push_back(pro::make_proxy<PPlugin, SkipTrans>(projectDir, projectConfig, pythonManager, luaManager, logger, runTime));
            }
        }
        else if (pluginName.contains("TextFull2Half")) {
            const PluginRunTime runTime = choosePluginRunTime(pluginNameLower, PluginRunTime::DPost);
            if (runTimeCheckFunc(runTime)) {
                plugins.push_back(pro::make_proxy<PPlugin, TextFull2Half>(projectConfig, logger, runTime));
            }
        }
        else if (pluginName.contains("TextLinebreakFix")) {
            const PluginRunTime runTime = choosePluginRunTime(pluginNameLower, PluginRunTime::DPost);
            if (runTimeCheckFunc(runTime)) {
                plugins.push_back(pro::make_proxy<PPlugin, TextLinebreakFix>(otherCacheDir, projectConfig, logger, runTime));
            }
        }
    }
}
