module;

#include "GPPMacros.hpp"

export module AgentPythonExecutor;

export import ITranslator;

export NAMESPACE_BEGIN(gpp)

// 通过 stdin 一次性送入代码，返回标准输出、标准错误和退出状态；不提供宿主调用协议。
json runAgentPython(
    const std::string& code, const json& paths,
    const std::shared_ptr<IController>& controller);

NAMESPACE_END(gpp)
