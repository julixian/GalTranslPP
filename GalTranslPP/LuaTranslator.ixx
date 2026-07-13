module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

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
		LuaFunction* m_luaRunFunc = nullptr;
		std::string m_scriptPath;
		std::string m_translatorName;

	public:
		void run() override
		{
			m_luaState->submitTask([&]()
				{
					try {
						m_luaRunFunc->call();
					}
					catch (const std::exception& e) {
						throw std::runtime_error(gppTr("LuaTranslator.run",
							"调用 LuaTranslator [%1] run 函数时出现异常: %2")
							.arg(m_translatorName)
							.arg(e.what())
							.toStdString());
					}
				}).get();
		}

		template <typename... Args>
		explicit LuaTranslator(const std::string& scriptPath, Args&&... args) :
			BaseTranslator(std::forward<Args>(args)...), m_scriptPath(scriptPath)
		{
			m_translatorName = wide2Ascii(fs::path(ascii2Wide(m_scriptPath)).filename());
			// m_inputDir = L"cache" / projectDir.filename() / (ascii2Wide(m_translatorName) + L"_json_input");
			// m_outputDir = L"cache" / projectDir.filename() / (ascii2Wide(m_translatorName) + L"_json_output");
			std::optional<std::shared_ptr<LuaStateInstance>> luaStateOpt = this->m_luaManager->registerFunction(m_scriptPath, "init");
			if (!luaStateOpt.has_value()) {
				throw std::runtime_error(gppTr("LuaTranslator.LuaTranslator",
					"LuaTranslator [%1] 获取 init 函数失败。")
					.arg(m_translatorName)
				    .toStdString());
			}
			luaStateOpt = this->m_luaManager->registerFunction(m_scriptPath, "run");
			if (!luaStateOpt.has_value()) {
				throw std::runtime_error(gppTr("LuaTranslator.LuaTranslator",
					"LuaTranslator [%1] 获取 run 函数失败。")
					.arg(m_translatorName)
				    .toStdString());
			}
			m_luaState = luaStateOpt.value();
			m_luaRunFunc = m_luaState->m_functions["run"].get();
			this->m_luaManager->registerFunction(m_scriptPath, "unload");

			LuaFunction* initFunc = m_luaState->m_functions["init"].get();
			m_luaState->submitTask([this, initFunc]()
				{
					try {
						if (!luabridge::setGlobal(m_luaState->m_lua.get(), (BaseTranslator*)this, "luaTranslator")) {
							throw std::runtime_error("设置 luaTranslator 全局变量失败");
						}
						initFunc->call();
					}
					catch (const std::exception& e) {
						throw std::runtime_error(gppTr("LuaTranslator.LuaTranslator",
							"调用 LuaTranslator [%1] init 函数时出现异常: %2")
							.arg(m_translatorName)
							.arg(e.what())
							.toStdString());
					}
				}).get();
			this->m_logger->info(gppTr("LuaTranslator.LuaTranslator",
			    "LuaTranslator [%1] 初始化完毕")
				.arg(m_translatorName)
				.toStdString());
		}

		~LuaTranslator() override
		{
			try {
				if (const auto& unloadFunc = m_luaState->m_functions["unload"];
					unloadFunc.operator bool() && unloadFunc->valid())
				{
					m_luaState->submitTask([&]
						{
							try {
								unloadFunc->call();
							}
							catch (const std::exception& e) {
								this->m_logger->error(gppTr(
									"LuaTranslator.~LuaTranslator",
									"调用 LuaTranslator unload 函数时出现异常: %1")
									.arg(e.what())
									.toStdString());
							}
						}).get();
				}
			}
			catch (const std::exception& e) {
				this->m_logger->error(gppTr("LuaTranslator.~LuaTranslator",
					"调用 LuaTranslator unload 函数时出现异常: %1")
				    .arg(e.what())
				    .toStdString());
			}
			this->m_logger->info(gppTr("LuaTranslator.~LuaTranslator",
			    "所有任务已完成！LuaTranslator [%1] 结束")
			    .arg(m_translatorName)
			    .toStdString());
		}
	};
}
