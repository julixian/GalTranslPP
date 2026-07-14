module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <cpp-base64/base64.h>
#include <toml.hpp>

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
            m_logger->error(gppTr("SkipTrans.SkipTrans", "SkipTrans 不支持 %1 阶段运行")
                .arg(pluginRunTime2Names[m_runTime])
                .toStdString());
            return;
        }

        bool reversePriority = false;
        const fs::path pluginConfigPath = [&]()
	        {
                fs::path ret = textPluginConfigPath / std::format(L"SkipTrans-{}.toml", ascii2Wide(pluginRunTime2Names[m_runTime]));
                if (!fs::exists(ret)) {
                    ret = textPluginConfigPath / L"SkipTrans.toml";
                }
                else {
                    reversePriority = true;
                }
                return ret;
            }();
        const auto pluginConfig = toml::uparse(pluginConfigPath);

        m_skipH = parseToml<bool>(projectConfig, pluginConfig, "plugins.SkipTrans.skipH", reversePriority);
        if (m_skipH) {
            const auto& hKeysBase64 = parseToml<std::string>(projectConfig, pluginConfig,
                "plugins.SkipTrans.hKeys", reversePriority);
            const std::string hKeysStr = base64_decode(hKeysBase64);
            m_hKeys = splitString(hKeysStr, '\n');
        }

        const auto& skipKeys = parseToml<toml::array>(projectConfig, pluginConfig,
            "plugins.SkipTrans.skipKeys", reversePriority);
        for (const auto& elem : skipKeys) {
            if (elem.is_string()) {
                GppConditionPattern pattern;
                pattern.conditionTarget = CachePart::Preproc;
                pattern.conditionReg.setPattern(elem.as_string()).setModifier(defaultRegCompileModifier).compile();
                if (!pattern.conditionReg) {
                    throw std::runtime_error(gppTr(
                        "SkipTrans.SkipTrans",
                        "skipKeys 正则表达式 `%1` 编译失败")
                        .arg(elem.as_string())
                        .toStdString());
                }
                GPPCondition gppCondition{ std::move(pattern) };
                CheckSeCondNormalFunc checkFunc = [condr = std::move(gppCondition)](Sentence* se) -> bool
                    {
                        return checkGppCondition(condr, se);
                    };
                m_skipKeys.push_back(std::move(checkFunc));
            }
            else if (elem.is_array() || elem.is_table()) {
                CheckSeCondNormalFunc checkFunc = getCheckSeCondFunc(elem, projectDir, pythonManager, luaManager, m_logger);
                m_skipKeys.push_back(std::move(checkFunc));
            }
            else {
                throw std::invalid_argument(gppTr("SkipTrans.SkipTrans", "skipKeys 元素必须是字符串、表或表数组")
                    .toStdString());
            }
        }
        m_logger->info(gppTr("SkipTrans.SkipTrans", "插件 SkipTrans-%1 已加载, skipH: %2")
            .arg(pluginRunTime2Names[m_runTime])
            .arg(m_skipH ? "true" : "false")
            .toStdString());
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("SkipTrans.SkipTrans", "SkipTrans-%1 配置文件解析错误: %2")
            .arg(pluginRunTime2Names[m_runTime])
            .arg(e.what())
            .toStdString());
    }
}

void SkipTrans::processSkippedSentence(Sentence* se, const std::string& info) {
    se->transraw = se->preproc;
    se->otherinfo["SkipTrans"] = info;
    se->transCompleted = true;
    se->problemAnalyzeDisabled = true;
}

void SkipTrans::skipImpl(Sentence* se) {
    if (
        m_skipH &&
        std::ranges::any_of(m_hKeys, [&](const auto& key)
            {
                if (se->preproc.contains(key)) {
                    processSkippedSentence(se, "skipH: " + key);
                    return true;
                }
                return false;
            })
        ) {
        return;
    }

    for (const auto& [index, key] : m_skipKeys | std::views::enumerate) {
        if (key(se)) {
            processSkippedSentence(se, gppTr("SkipTrans.skipImpl", "被第 %1 个 skipKeys 条件匹配到")
                .arg(index + 1)
                .toStdString());
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
