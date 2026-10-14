module;

#include "GPPMacros.hpp"

export module AgentPythonExecutor;

export import ITranslator;

export NAMESPACE_BEGIN(gpp)

// 通过 stdin 一次性送入代码，返回标准输出、标准错误和退出状态；不提供宿主调用协议。
// fastPause 开启时响应暂停并结束子进程，否则等待工具结束；120 秒超时始终生效。
json runAgentPython(
    const std::string& code, const json& paths,
    const std::shared_ptr<IController>& controller, bool fastPause);

NAMESPACE_END(gpp)
