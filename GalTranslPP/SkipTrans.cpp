module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <cpp-base64/base64.h>
#include <toml.hpp>
#include <sol/sol.hpp>

module SkipTrans;

import ConditionTool;
import Tool;

namespace fs = std::filesystem;

SkipTrans::SkipTrans(const fs::path& projectDir, const toml::value& projectConfig,
    const std::unique_ptr<PythonManager>& pythonManager, const std::unique_ptr<LuaManager>& luaManager,
    const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime)
    : m_logger(logger), m_runTime(runTime)
{
    try {
        if (m_runTime != PluginRunTime::DPre && m_runTime != PluginRunTime::Pre) {
            m_logger->error("SkipTrans 不支持 {} 阶段运行", pluginRunTimeNames[m_runTime]);
            return;
        }

        const fs::path pluginConfigPath = [&]()
	        {
                fs::path ret = textPluginConfigPath / std::format(L"SkipTrans-{}.toml", ascii2Wide(pluginRunTimeNames[m_runTime]));
                if (!fs::exists(ret)) {
                    ret = textPluginConfigPath / L"SkipTrans.toml";
                }
                return ret;
            }();
        const auto pluginConfig = toml::uparse(pluginConfigPath);

        m_skipH = parseToml<bool>(projectConfig, pluginConfig, "plugins.SkipTrans.skipH");
        if (m_skipH) {
            const auto& hKeysBase64 = parseToml<std::string>(projectConfig, pluginConfig, "plugins.SkipTrans.hKeys");
            const std::string hKeysStr = base64_decode(hKeysBase64);
            m_hKeys = splitString(hKeysStr, '\n');
        }

        const auto& skipKeys = parseToml<toml::array>(projectConfig, pluginConfig, "plugins.SkipTrans.skipKeys");
        for (const auto& elem : skipKeys) {
            if (elem.is_string()) {
                GppConditionPattern pattern;
                pattern.conditionTarget = CachePart::PreprocText;
                pattern.conditionReg.setPattern(elem.as_string()).setModifier(defaultRegCompileModifier).compile();
                if (!pattern.conditionReg) {
                    throw std::runtime_error(std::format("skipKeys 正则表达式 [{}] 编译失败", elem.as_string()));
                }
                GPPCondition gppCondition{ std::move(pattern) };
                CheckSeCondFunc checkFunc = [condr = std::move(gppCondition)](const Sentence* se) -> bool
                    {
                        return checkGppCondition(condr, se);
                    };
                m_skipKeys.push_back(std::move(checkFunc));
            }
            else if (elem.is_array() || elem.is_table()) {
                CheckSeCondFunc checkFunc = getCheckSeCondFunc(elem, projectDir, pythonManager, luaManager, m_logger);
                m_skipKeys.push_back(std::move(checkFunc));
            }
            else {
                throw std::invalid_argument("skipKeys 元素必须是字符串、表或表数组");
            }
        }
        m_logger->info("插件 SkipTrans-{} 已加载, skipH: {}",
            pluginRunTimeNames[m_runTime], m_skipH);
    }
    catch (const toml::exception& e) {
        m_logger->critical("SkipTrans-{} 配置文件解析错误", pluginRunTimeNames[m_runTime]);
        throw std::runtime_error(e.what());
    }
}

void SkipTrans::processSkipSentence(Sentence* se, const std::string& info) {
    se->pre_translated_text = se->pre_processed_text;
    se->other_info["SkipTrans"] = info;
    se->complete = true;
    se->notAnalyzeProblem = true;
}

void SkipTrans::skipImpl(Sentence* se) {
    if (
        m_skipH &&
        std::ranges::any_of(m_hKeys, [&](const auto& key)
            {
                if (se->pre_processed_text.contains(key)) {
                    processSkipSentence(se, "skipH: " + key);
                    return true;
                }
                return false;
            })
        ) {
        return;
    }

    for (const auto& [index, key] : m_skipKeys | std::views::enumerate) {
        if (key(se)) {
            processSkipSentence(se, std::format("被第 {} 个 skipKeys 条件匹配到", index + 1));
            return;
        }
    }
}

void SkipTrans::dPreRun(Sentence* se) {
    if (m_runTime != PluginRunTime::DPre) {
        return;
    }
    skipImpl(se);
}

void SkipTrans::preRun(Sentence* se) {
    if (m_runTime != PluginRunTime::Pre) {
        return;
    }
    skipImpl(se);
}
