module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

export module PythonTranslator;

export import Tool;
export import NormalJsonTranslator;
export import EpubTranslator;
export import PDFTranslator;
export import PythonManager;

namespace fs = std::filesystem;
namespace py = pybind11;

export
{
	template<typename BaseTranslator>
	class PythonTranslator : public BaseTranslator {
	private:
		std::shared_ptr<PythonInterpreterInstance> m_pythonInterpreter;
		py::object* m_pythonRunFunc;
		std::string m_modulePath;
		std::string m_translatorName;

	public:
		virtual void run() override
		{
			m_pythonInterpreter->submitTask([&]()
				{
					try {
						(void)(*m_pythonRunFunc)();
					}
					catch (const py::error_already_set& e) {
						throw std::runtime_error(gppTr("PythonTranslator.run",
							"调用 PythonTranslator [%1] run 函数时出现异常: %2")
							.arg(m_translatorName)
						    .arg(e.what())
						    .toStdString());
					}
				}).get();
		}

		template<typename... Args>
		explicit PythonTranslator(const std::string& modulePath, Args&&... args) :
			BaseTranslator(std::forward<Args>(args)...), m_modulePath(modulePath)
		{
			this->m_pythonTranslator = true;
			m_translatorName = wide2Ascii(fs::path(ascii2Wide(m_modulePath)).stem());
			std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt =
				this->m_pythonManager->registerFunction(m_modulePath, "init");
			if (!pythonInterpreterOpt.has_value()) {
				throw std::runtime_error(gppTr(
				    "PythonTranslator.PythonTranslator",
				    "PythonTranslator [%1] 获取 init 函数失败！")
					.arg(m_translatorName)
				    .toStdString());
			}
			pythonInterpreterOpt = this->m_pythonManager->registerFunction(m_modulePath, "run");
			if (!pythonInterpreterOpt.has_value()) {
				throw std::runtime_error(gppTr(
				    "PythonTranslator.PythonTranslator",
				    "PythonTranslator [%1] 获取 run 函数失败！")
					.arg(m_translatorName)
				    .toStdString());
			}
			m_pythonInterpreter = pythonInterpreterOpt.value();
			m_pythonRunFunc = m_pythonInterpreter->functions["run"].get();
			this->m_pythonManager->registerFunction(m_modulePath, "unload");

			m_pythonInterpreter->submitTask([&]()
				{
					try {
						const fs::path stdModulePath = fs::weakly_canonical(ascii2Wide(m_modulePath));
						const std::string moduleName = wide2Ascii(stdModulePath.stem());
						py::module_ pythonTranslatorModule = py::module_::import(moduleName.c_str());
						pythonTranslatorModule.attr("pythonTranslator") = (BaseTranslator*)this;
						(void)(*(m_pythonInterpreter->functions["init"]))();
					}
					catch (const py::error_already_set& e) {
						throw std::runtime_error(gppTr(
						    "PythonTranslator.PythonTranslator",
						    "调用 PythonTranslator [%1] init 函数时出现异常: %2")
							.arg(m_translatorName)
						    .arg(e.what())
						    .toStdString());
					}
				}).get();
			this->m_logger->info(gppTr("PythonTranslator.PythonTranslator", "PythonTranslator [%1] 初始化完毕")
			    .arg(m_translatorName)
			    .toStdString());
		}

		virtual ~PythonTranslator() override
		{
			m_pythonInterpreter->submitTask([&]()
				{
					try {
						if (const auto& unloadFuncPtr = m_pythonInterpreter->functions["unload"];
							unloadFuncPtr.operator bool() && py::isinstance<py::function>(*unloadFuncPtr))
						{
							(void)(*unloadFuncPtr)();
						}
					}
					catch (const py::error_already_set& e) {
						// 析构不抛异常
						this->m_logger->error(gppTr(
						    "PythonTranslator.~PythonTranslator",
						    "调用 PythonTranslator [%1] unload 函数时出现异常: %2")
							.arg(m_translatorName)
						    .arg(e.what())
						    .toStdString());
					}
					// python 闭包析构时需要有 GIL
					this->m_onFileProcessed = nullptr;
					this->m_onPerformApi = nullptr;
					this->m_onDictProcessed = nullptr;
				}).get();
			this->m_logger->info(gppTr(
			    "PythonTranslator.~PythonTranslator",
			    "所有任务已完成！PythonTranslator [%1] 结束")
			    .arg(m_translatorName)
			    .toStdString());
		}

	};
}
