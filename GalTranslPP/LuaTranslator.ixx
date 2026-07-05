module;

#include "GPPMacros.hpp"
#include <sol/sol.hpp>

export module LuaTranslator;

export import Tool;
export import NormalJsonTranslator;
export import EpubTranslator;
export import PDFTranslator;
export import LuaManager;

namespace fs = std::filesystem;

export
{
	template<typename BaseTranslator>
	class LuaTranslator : public BaseTranslator {

	private:
		std::shared_ptr<LuaStateInstance> m_luaState;
		sol::function* m_luaRunFunc;
		std::string m_scriptPath;
		std::string m_translatorName;

	public:
		virtual void run() override
		{
			this->m_logger->info(gppTr("LuaTranslator.run", "开始运行 LuaTranslator..."));
			try {
				(*m_luaRunFunc)();
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr("LuaTranslator.run", "LuaTranslator 运行时异常: %1", e.what()));
			}
		}

		template <typename... Args>
		LuaTranslator(const std::string& scriptPath, Args&&... args) :
			BaseTranslator(std::forward<Args>(args)...), m_scriptPath(scriptPath)
		{
			m_translatorName = wide2Ascii(fs::path(ascii2Wide(m_scriptPath)).filename());
			// m_inputDir = L"cache" / projectDir.filename() / (ascii2Wide(m_translatorName) + L"_json_input");
			// m_outputDir = L"cache" / projectDir.filename() / (ascii2Wide(m_translatorName) + L"_json_output");
			std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = this->m_luaManager->registerFunction(m_scriptPath, "init");
			if (!luaStateOpt.has_value()) {
				throw std::runtime_error(gppTr("LuaTranslator.LuaTranslator", "LuaTranslator 获取 init 函数失败。"));
			}
			luaStateOpt = this->m_luaManager->registerFunction(m_scriptPath, "run");
			if (!luaStateOpt.has_value()) {
				throw std::runtime_error(gppTr("LuaTranslator.LuaTranslator", "LuaTranslator 获取 run 函数失败。"));
			}
			m_luaState = luaStateOpt.value();
			m_luaRunFunc = m_luaState->functions["run"].get();
			this->m_luaManager->registerFunction(m_scriptPath, "unload");

			sol::state& luaState = *(m_luaState->lua);
			luaState["luaTranslator"] = (BaseTranslator*)this;

			sol::function* initFunc = m_luaState->functions["init"].get();
			try {
				(*initFunc)();
			}
			catch (const sol::error& e) {
				throw std::runtime_error(gppTr("LuaTranslator.LuaTranslator", "初始化 LuaTranslator 时出现异常: %1", e.what()));
			}
		}

		virtual ~LuaTranslator() override
		{
			try {
				if (auto& unloadFunc = m_luaState->functions["unload"]; unloadFunc.operator bool() && unloadFunc->valid()) {
					(*unloadFunc)();
				}
			}
			catch (const sol::error& e) {
				this->m_logger->error(gppTr("LuaTranslator.~LuaTranslator", "卸载 LuaTranslator 时出现异常: %1", e.what()));
			}
			this->m_logger->info(gppTr("LuaTranslator.~LuaTranslator", "所有任务已完成！LuaTranslator %1 结束。", m_translatorName));
		}
	};
}
