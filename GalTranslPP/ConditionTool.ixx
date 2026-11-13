module;

#include "GPPMacros.hpp"

export module ConditionTool;

export import LuaManager;
export import PythonManager;
export import Tool;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;
namespace py = pybind11;

template<typename T>
concept IsMapLike = requires {
    typename T::key_type;
    typename T::mapped_type;
};

struct GppConditionPattern {
    jpc::Regex conditionReg;
    CachePart conditionTarget = CachePart::None;
    int sentenceOffset = 0;
};
using GPPCondition = std::vector<GppConditionPattern>;

bool checkString(const jpc::Regex& conditionReg, const std::string& str);
bool checkGppCondition(const GPPCondition& gppCondition, Sentence* se);
GPPCondition createGppCondition(const toml& conditionPatterns);
ConditionType getConditionType(const toml& tbl);

template<typename ...Args>
CheckSeCondBaseFunc<Args...> getCheckSeCondFunc(const toml& condElem, const fs::path& projectDir,
    const std::unique_ptr<PythonManager>& pythonManager, const std::unique_ptr<LuaManager>& luaManager,
    const std::shared_ptr<spdlog::logger>& logger)
{
    using RetFuncType = CheckSeCondBaseFunc<Args...>;
    std::vector<RetFuncType> funcs;

    auto appendFunctionFunc = [&](const tomlkit::Item& tbl)
        {
            ConditionType condType = getConditionType(tbl);
            switch (condType)
            {
            case ConditionType::Gpp:
            {
                GPPCondition gppCondition = createGppCondition(tbl);
                if (gppCondition.empty()) {
                    return;
                }
                RetFuncType checkFunc = [condR = std::move(gppCondition)](Sentence* se, Args...) -> bool
                    {
                        return checkGppCondition(condR, se);
                    };
                funcs.emplace_back(std::move(checkFunc));
            }
            break;

            case ConditionType::Lua:
            {
                std::string conditionLuaStr = tbl.at("conditionScript").asString();
                replaceStrInplace(conditionLuaStr, "%PROJECT_DIR%", wide2Ascii(projectDir));
                const std::string conditionFuncStr = tbl.at("conditionFunc").asString();
                const std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = luaManager->registerFunction(
                    conditionLuaStr, conditionFuncStr);
                if (luaStateOpt) {
                    std::shared_ptr<LuaStateInstance> luaState = *luaStateOpt;
                    LuaFunction* pConditionFunc = luaState->m_functions[conditionFuncStr].get();
                    RetFuncType checkFunc = [luaState, pConditionFunc, conditionFuncStr](Sentence* se, Args... args) -> bool
                        {
                            bool result = false;
                            try {
                                luaState->submitTask([&]()
                                    {
                                        result = pConditionFunc->call<bool>(se, args...);
                                    }).get();
                            }
                            catch (const std::exception& e) {
                                throw std::runtime_error(gppTr(
                                    "ConditionTool.getCheckSeCondFunc",
                                    "执行 Lua 条件函数 %1 时发生错误: %2")
                                    .arg(conditionFuncStr)
                                    .arg(e.what())
                                    .toStdString());
                            }
                            return result;
                        };
                    funcs.emplace_back(std::move(checkFunc));
                    logger->info(gppTr(
                        "ConditionTool.getCheckSeCondFunc",
                        "注册 Lua 脚本 [%1] 中的条件函数 %2 成功")
                        .arg(conditionLuaStr)
                        .arg(conditionFuncStr)
                        .toStdString());
                }
                else {
                    throw std::runtime_error(gppTr(
                        "ConditionTool.getCheckSeCondFunc",
                        "注册 Lua 脚本 [%1] 中的条件函数 %2 失败")
                        .arg(conditionLuaStr)
                        .arg(conditionFuncStr)
                        .toStdString());
                }
            }
            break;

            case ConditionType::Python:
            {
                std::string conditionPythonStr = tbl.at("conditionScript").asString();
                replaceStrInplace(conditionPythonStr, "%PROJECT_DIR%", wide2Ascii(projectDir));
                const std::string conditionFuncStr = tbl.at("conditionFunc").asString();
                const std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt = pythonManager->registerFunction(
                    conditionPythonStr, conditionFuncStr);
                if (pythonInterpreterOpt) {
                    std::shared_ptr<PythonInterpreterInstance> pythonInterpreter = *pythonInterpreterOpt;
                    py::object* pConditionFunc = pythonInterpreter->functions[conditionFuncStr].get();
                    RetFuncType checkFunc = [pythonInterpreter, pConditionFunc, conditionFuncStr](Sentence* se, Args... args) -> bool
                        {
                            bool result;
                            pythonInterpreter->submitTask([&]()
                                {
                                    try {
                                        result = (*pConditionFunc)(se, args...).template cast<bool>();
                                    }
                                    catch (const py::error_already_set& e) {
                                        throw std::runtime_error(gppTr(
                                            "ConditionTool.getCheckSeCondFunc",
                                            "执行 Python 条件函数 %1 时发生错误: %2")
                                            .arg(conditionFuncStr)
                                            .arg(e.what())
                                            .toStdString());
                                    }
                                }).get();
                            return result;
                        };
                    funcs.emplace_back(std::move(checkFunc));
                    logger->info(gppTr(
                        "ConditionTool.getCheckSeCondFunc",
                        "注册 Python 脚本 [%1] 中的条件函数 %2 成功")
                        .arg(conditionPythonStr)
                        .arg(conditionFuncStr)
                        .toStdString());
                }
                else {
                    throw std::runtime_error(gppTr(
                        "ConditionTool.getCheckSeCondFunc",
                        "注册 Python 脚本 [%1] 中的条件函数 %2 失败")
                        .arg(conditionPythonStr)
                        .arg(conditionFuncStr)
                        .toStdString());
                }
            }
            break;

            default:
                return;
            }
        };

    if (condElem.isArray()) {
        for (const auto& condition : condElem
            | std::views::filter([](const auto& condition_) { return condition_.isTable(); }))
        {
            appendFunctionFunc(condition);
        }
    }
    else if (condElem.isTable()) {
        appendFunctionFunc(condElem);
    }

    RetFuncType resultFunc;
    if (funcs.empty()) {
        resultFunc = [](Sentence*, Args...) -> bool
            {
                return true;
            };
    }
    else {
        resultFunc = [funcsR = std::move(funcs)](Sentence* se, Args... args) -> bool
            {
                return std::ranges::all_of(funcsR, [&](const RetFuncType& func)
                    {
                        return func(se, args...);
                    });
            };
    }
    return resultFunc;
}

NAMESPACE_END(gpp)

module :private;

NAMESPACE_BEGIN(gpp)

GPPCondition createGppCondition(const toml& conditionPatterns) {
    GPPCondition patterns;

    auto appendPatternFunc = [&](const toml& conditionTbl)
        {
            if (!conditionTbl.contains("conditionTarget") || !conditionTbl.at("conditionTarget").isString()
                || !conditionTbl.contains("conditionReg") || !conditionTbl.at("conditionReg").isString()
                || (conditionTbl.contains("compileModifier") && !conditionTbl.at("compileModifier").isString()))
            {
                return;
            }

            GppConditionPattern pattern;
            std::string_view conditionTargetStr = conditionTbl.at("conditionTarget").asString();
            while (true) {
                if (conditionTargetStr.starts_with("prev_")) {
                    --pattern.sentenceOffset;
                    conditionTargetStr.remove_prefix(5);
                }
                else if (conditionTargetStr.starts_with("next_")) {
                    ++pattern.sentenceOffset;
                    conditionTargetStr.remove_prefix(5);
                }
                else {
                    break;
                }
            }

            pattern.conditionTarget = chooseCachePart(conditionTargetStr);
            const std::string& conditionRegStr = conditionTbl.at("conditionReg").asString();
            if (conditionRegStr.empty()) {
                return;
            }
            const std::string modifier = tomlkit::findOr(conditionTbl, "compileModifier", defaultRegCompileModifier);
            pattern.conditionReg.setPattern(conditionRegStr).setModifier(modifier).compile();
            if (!pattern.conditionReg) {
                return;
            }
            patterns.emplace_back(std::move(pattern));
        };
    if (conditionPatterns.isArray()) {
        for (const auto& condition : conditionPatterns
            | std::views::filter([](const auto& condition_) { return condition_.isTable(); }))
        {
            appendPatternFunc(condition);
        }
    }
    else if (conditionPatterns.isTable()) {
        appendPatternFunc(conditionPatterns);
    }

    return patterns;
}

ConditionType getConditionType(const toml& tbl) {
    if (tbl.contains("conditionTarget") && tbl.at("conditionTarget").isString()
        && !tbl.at("conditionTarget").asString().empty()
        && tbl.contains("conditionReg") && tbl.at("conditionReg").isString()
        && !tbl.at("conditionReg").asString().empty())
    {
        return ConditionType::Gpp;
    }
    if (tbl.contains("conditionScript") && tbl.at("conditionScript").isString()
        && !tbl.at("conditionScript").asString().empty()
        && tbl.contains("conditionFunc") && tbl.at("conditionFunc").isString()
        && !tbl.at("conditionFunc").asString().empty())
    {
        const std::string conditionScriptStr = str2Lower(tbl.at("conditionScript").asString());
        if (conditionScriptStr.ends_with(".lua")) {
            return ConditionType::Lua;
        }
        else if (conditionScriptStr.ends_with(".py")) {
            return ConditionType::Python;
        }
    }
    return ConditionType::None;
}

bool checkString(const jpc::Regex& conditionReg, const std::string& str) {
    jpc::RegexMatch rm(&conditionReg);
    return rm.setSubject(&str).match() > 0;
}

bool checkGppCondition(const GPPCondition& gppCondition, Sentence* se) {
    return std::ranges::all_of(gppCondition, [&](const GppConditionPattern& pattern)
        {
            Sentence* sentenceToCheck = se;
            if (pattern.sentenceOffset > 0) {
                for (int i = 0; i < pattern.sentenceOffset; i++) {
                    sentenceToCheck = sentenceToCheck->next;
                    if (sentenceToCheck == nullptr) {
                        return false;
                    }
                }
            }
            else if (pattern.sentenceOffset < 0) {
                for (int i = 0; i > pattern.sentenceOffset; i--) {
                    sentenceToCheck = sentenceToCheck->prev;
                    if (sentenceToCheck == nullptr) {
                        return false;
                    }
                }
            }
            auto checkAnyOf = [&]<typename ContainerType>(const ContainerType& container) -> bool
            {
                return std::ranges::any_of(container, [&](const auto& item)
                    {
                        if constexpr (IsMapLike<ContainerType>) {
                            return checkString(pattern.conditionReg, item.first) || checkString(pattern.conditionReg, item.second);
                        }
                        else {
                            return checkString(pattern.conditionReg, item);
                        }
                    });
            };
            switch (pattern.conditionTarget)
    		    {
            case CachePart::Index:
                return checkString(pattern.conditionReg, std::to_string(sentenceToCheck->index));
            case CachePart::Names:
                return checkAnyOf(sentenceToCheck->names);
            case CachePart::NamesTrans:
                return checkAnyOf(sentenceToCheck->namestrans);
            case CachePart::Problems:
                return checkAnyOf(sentenceToCheck->problems);
            case CachePart::OtherInfo:
                return checkAnyOf(sentenceToCheck->otherinfo);
            default:
                return checkString(pattern.conditionReg, chooseStringRef(sentenceToCheck, pattern.conditionTarget));
            }
            return false;
        });
}

NAMESPACE_END(gpp)
