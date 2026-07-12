module;

#define SOL2_HEADERS
#include "GPPMacros.hpp"

module LuaTextPlugin;

namespace fs = std::filesystem;

LuaTextPlugin::LuaTextPlugin(const fs::path& projectDir, const std::string& scriptPath,
	const std::unique_ptr<LuaManager>& luaManager, const std::shared_ptr<spdlog::logger>& logger)
	: m_logger(logger), m_scriptPath(scriptPath)
{
	std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = luaManager->registerFunction(m_scriptPath, "init");
	if (!luaStateOpt.has_value()) {
		throw std::runtime_error(gppTr("LuaTextPlugin.LuaTextPlugin",
			"LuaTextPlugin [%1] 获取 init 函数失败")
		    .arg(m_scriptPath)
		    .toStdString());
	}
	m_luaState = luaStateOpt.value();

	auto registerFunctionFunc = [&](const std::string& funcName, sol::function*& pFunc)
		{
			luaStateOpt = luaManager->registerFunction(m_scriptPath, funcName);
			if (luaStateOpt.has_value()) {
				pFunc = m_luaState->m_functions[funcName].get();
				m_logger->info(gppTr("LuaTextPlugin.LuaTextPlugin",
					"注册 LuaTextPlugin [%1] 中的 %2 函数成功")
				    .arg(m_scriptPath)
				    .arg(funcName)
				    .toStdString());
			}
		};
	registerFunctionFunc("dPreRun", m_luaDPreRunFunc);
	registerFunctionFunc("preRun", m_luaPreRunFunc);
	registerFunctionFunc("postRun", m_luaPostRunFunc);
	registerFunctionFunc("dPostRun", m_luaDPostRunFunc);
	registerFunctionFunc("unload", m_luaUnloadFunc);

	m_luaState->submitTask([&]()
		{
			try {
				const sol::protected_function_result result = (*m_luaState->m_functions["init"])(projectDir);
				if (!result.valid()) {
					const sol::error error = result;
					throw error;
				}
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr(
					"LuaTextPlugin.LuaTextPlugin",
					"调用 LuaTextPlugin [%1] init 函数时出现异常: %2")
					.arg(m_scriptPath)
					.arg(e.what())
					.toStdString());
			}
		}).get();

	m_logger->info(gppTr("LuaTextPlugin.LuaTextPlugin", "LuaTextPlugin [%1] 初始化完毕")
		.arg(m_scriptPath)
		.toStdString());
}

LuaTextPlugin::~LuaTextPlugin() {
	if (!m_luaUnloadFunc) {
		return;
	}
	m_luaState->submitTask([&]()
		{
			try {
				const sol::protected_function_result result = (*m_luaUnloadFunc)();
				if (!result.valid()) {
					const sol::error error = result;
					throw error;
				}
			}
			catch (const sol::error& e) {
				m_logger->error(gppTr("LuaTextPlugin.~LuaTextPlugin",
					"调用 LuaTextPlugin [%1] unload 函数时出现异常: %2")
					.arg(m_scriptPath)
					.arg(e.what())
					.toStdString());
			}
		}).get();
}

void LuaTextPlugin::dPreRun(Sentence* se) {
	if (!m_luaDPreRunFunc) {
		return;
	}
	m_luaState->submitTask([&]()
		{
			try {
				const sol::protected_function_result result = (*m_luaDPreRunFunc)(se);
				if (!result.valid()) {
					const sol::error error = result;
					throw error;
				}
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr("LuaTextPlugin.dPreRun",
					"调用 LuaTextPlugin [%1] dPreRun 函数时出现异常: %2")
					.arg(m_scriptPath)
					.arg(e.what())
					.toStdString());
			}
		}).get();
}

void LuaTextPlugin::preRun(Sentence* se) {
	if (!m_luaPreRunFunc) {
		return;
	}
	m_luaState->submitTask([&]()
		{
			try {
				const sol::protected_function_result result = (*m_luaPreRunFunc)(se);
				if (!result.valid()) {
					const sol::error error = result;
					throw error;
				}
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr("LuaTextPlugin.preRun",
					"调用 LuaTextPlugin [%1] preRun 函数时出现异常: %2")
					.arg(m_scriptPath)
					.arg(e.what())
					.toStdString());
			}
		}).get();
}

void LuaTextPlugin::postRun(Sentence* se) {
	if (!m_luaPostRunFunc) {
		return;
	}
	m_luaState->submitTask([&]()
		{
			try {
				const sol::protected_function_result result = (*m_luaPostRunFunc)(se);
				if (!result.valid()) {
					const sol::error error = result;
					throw error;
				}
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr("LuaTextPlugin.postRun",
					"调用 LuaTextPlugin [%1] postRun 函数时出现异常: %2")
					.arg(m_scriptPath)
					.arg(e.what())
					.toStdString());
			}
		}).get();
}

void LuaTextPlugin::dPostRun(Sentence* se) {
	if (!m_luaDPostRunFunc) {
		return;
	}
	m_luaState->submitTask([&]()
		{
			try {
				const sol::protected_function_result result = (*m_luaDPostRunFunc)(se);
				if (!result.valid()) {
					const sol::error error = result;
					throw error;
				}
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr("LuaTextPlugin.dPostRun",
					"调用 LuaTextPlugin [%1] dPostRun 函数时出现异常: %2")
					.arg(m_scriptPath)
					.arg(e.what())
					.toStdString());
			}
		}).get();
}
