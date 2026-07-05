module;

#define PYBIND11_HEADERS
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
			this->m_logger->info(gppTr("PythonTranslator.run", "开始运行 PythonTranslator..."));
			m_pythonInterpreter->submitTask([&]()
				{
					try {
						(*m_pythonRunFunc)();
					}
					catch (const py::error_already_set& e) {
						throw std::runtime_error(gppTr("PythonTranslator.run", "PythonTranslator 运行时异常: %1", e.what()));
					}
				}).get();
		}

		template<typename... Args>
		PythonTranslator(const std::string& modulePath, Args&&... args) :
			BaseTranslator(std::forward<Args>(args)...), m_modulePath(modulePath)
		{
			this->m_pythonTranslator = true;
			m_translatorName = wide2Ascii(fs::path(ascii2Wide(m_modulePath)).stem());
			std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt = this->m_pythonManager->registerFunction(m_modulePath, "init");
			if (!pythonInterpreterOpt.has_value()) {
				throw std::runtime_error(gppTr("PythonTranslator.PythonTranslator", "PythonTranslator 获取 init 函数失败！"));
			}
			pythonInterpreterOpt = this->m_pythonManager->registerFunction(m_modulePath, "run");
			if (!pythonInterpreterOpt.has_value()) {
				throw std::runtime_error(gppTr("PythonTranslator.PythonTranslator", "PythonTranslator 获取 run 函数失败！"));
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
						(*(m_pythonInterpreter->functions["init"]))();
					}
					catch (const py::error_already_set& e) {
						throw std::runtime_error(gppTr("PythonTranslator.PythonTranslator", "初始化 PythonTranslator 时出现异常: %1", e.what()));
					}
				}).get();
			this->m_logger->info(gppTr("PythonTranslator.PythonTranslator", "PythonTranslator 已加载模块: %1", m_translatorName));
		}

		virtual ~PythonTranslator() override
		{
			m_pythonInterpreter->submitTask([&]()
				{
					try {

						this->m_onFileProcessed = nullptr;
						this->m_onPerformApi = nullptr;
						this->m_onDictProcessed = nullptr;

						if (auto& unloadFuncPtr = m_pythonInterpreter->functions["unload"]; unloadFuncPtr.operator bool() && py::isinstance<py::function>(*unloadFuncPtr)) {
							(*unloadFuncPtr)();
						}
					}
					catch (const py::error_already_set& e) {
						// 析构不抛异常
						this->m_logger->error(gppTr("PythonTranslator.~PythonTranslator", "卸载 PythonTranslator 时出现异常: %1", e.what()));
					}
				}).get();
			this->m_logger->info(gppTr("PythonTranslator.~PythonTranslator", "所有任务已完成！PythonTranslator %1 结束。", m_translatorName));
		}

	};
}
