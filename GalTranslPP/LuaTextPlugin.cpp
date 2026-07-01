module;

#include "GPPMacros.hpp"
#include <sol/sol.hpp>

module LuaTextPlugin;

namespace fs = std::filesystem;

LuaTextPlugin::LuaTextPlugin(const fs::path& projectDir, const std::string& scriptPath, const std::unique_ptr<LuaManager>& luaManager, const std::shared_ptr<spdlog::logger>& logger)
	: m_logger(logger), m_scriptPath(scriptPath)
{
	m_logger->info("正在初始化 Lua 插件 {}", m_scriptPath);
	std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = luaManager->registerFunction(m_scriptPath, "init");
	if (!luaStateOpt.has_value()) {
		throw std::runtime_error(std::format("{} init函数初始化失败", m_scriptPath));
	}
	m_luaState = luaStateOpt.value();

	auto registerFunctionFunc = [&](const std::string& funcName, sol::function*& pFunc)
		{
			luaStateOpt = luaManager->registerFunction(m_scriptPath, funcName);
			if (luaStateOpt.has_value()) {
				pFunc = m_luaState->functions[funcName].get();
				m_logger->info("{} {} 函数注册成功", m_scriptPath, funcName);
			}
		};
	registerFunctionFunc("dPreRun", m_luaDPreRunFunc);
	registerFunctionFunc("preRun", m_luaPreRunFunc);
	registerFunctionFunc("postRun", m_luaPostRunFunc);
	registerFunctionFunc("dPostRun", m_luaDPostRunFunc);
	registerFunctionFunc("unload", m_luaUnloadFunc);

	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		(*(m_luaState->functions["init"]))(projectDir);
    }
    catch (const sol::error& e) {
        throw std::runtime_error(std::format("{} init 函数执行失败: {}", m_scriptPath, e.what()));
	}

	m_logger->info("{} 初始化成功", m_scriptPath);
}

LuaTextPlugin::~LuaTextPlugin() {
	if (!m_luaUnloadFunc) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		(*m_luaUnloadFunc)();
	}
	catch (const sol::error&) {
		m_logger->error("{} unload 函数执行失败", m_scriptPath);
	}
}

void LuaTextPlugin::dPreRun(Sentence* se) {
	if (!m_luaDPreRunFunc) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		(*m_luaDPreRunFunc)(se);
	}
    catch (const sol::error& e) {
        throw std::runtime_error(std::format("{} dPreRun 函数执行失败: {}", m_scriptPath, e.what()));
	}
}

void LuaTextPlugin::preRun(Sentence* se) {
	if (!m_luaPreRunFunc) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		(*m_luaPreRunFunc)(se);
	}
    catch (const sol::error& e) {
        throw std::runtime_error(std::format("{} preRun 函数执行失败: {}", m_scriptPath, e.what()));
	}
}

void LuaTextPlugin::postRun(Sentence* se) {
	if (!m_luaPostRunFunc) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		(*m_luaPostRunFunc)(se);
	}
    catch (const sol::error& e) {
        throw std::runtime_error(std::format("{} postRun 函数执行失败: {}", m_scriptPath, e.what()));
	}
}

void LuaTextPlugin::dPostRun(Sentence* se) {
	if (!m_luaDPostRunFunc) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(m_luaState->executionMutex);
		(*m_luaDPostRunFunc)(se);
	}
    catch (const sol::error& e) {
        throw std::runtime_error(std::format("{} dPostRun函数执行失败: {}", m_scriptPath, e.what()));
    }
}
