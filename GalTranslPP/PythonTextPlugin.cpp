module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"

module PythonTextPlugin;

namespace fs = std::filesystem;
namespace py = pybind11;

PythonTextPlugin::PythonTextPlugin(const fs::path& projectDir, const std::string& modulePath,
    const std::unique_ptr<PythonManager>& pythonManager, const std::shared_ptr<spdlog::logger>& logger)
    : m_logger(logger), m_modulePath(modulePath)
{
    std::optional<std::shared_ptr<PythonInterpreterInstance>> pythonInterpreterOpt =
        pythonManager->registerFunction(m_modulePath, "init");
    if (!pythonInterpreterOpt.has_value()) {
        throw std::runtime_error(gppTr("PythonTextPlugin.PythonTextPlugin",
            "PythonTextPlugin [%1] 获取 init 函数失败")
            .arg(m_modulePath)
            .toStdString());
    }
    m_pythonInterpreter = pythonInterpreterOpt.value();

    auto registerFunctionFunc = [&](const std::string& funcName, py::object*& func)
        {
            pythonInterpreterOpt = pythonManager->registerFunction(m_modulePath, funcName);
            if (pythonInterpreterOpt.has_value()) {
                func = m_pythonInterpreter->functions[funcName].get();
                m_logger->info(gppTr("PythonTextPlugin.PythonTextPlugin",
                    "注册 PythonTextPlugin [%1] 中的 %2 函数成功")
                    .arg(m_modulePath)
                    .arg(funcName)
                    .toStdString());
            }
        };
    registerFunctionFunc("dPreRun", m_pythonDPreRunFunc);
    registerFunctionFunc("preRun", m_pythonPreRunFunc);
    registerFunctionFunc("postRun", m_pythonPostRunFunc);
    registerFunctionFunc("dPostRun", m_pythonDPostRunFunc);
    registerFunctionFunc("unload", m_pythonUnloadFunc);

    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (void)(*(m_pythonInterpreter->functions["init"]))(projectDir);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(gppTr(
                    "PythonTextPlugin.PythonTextPlugin",
                    "调用 PythonTextPlugin [%1] init 函数时出现异常: %2")
                    .arg(m_modulePath)
                    .arg(e.what())
                    .toStdString());
            }
        }).get();

    m_logger->info(gppTr("PythonTextPlugin.PythonTextPlugin", "PythonTextPlugin [%1] 初始化完毕")
        .arg(m_modulePath)
        .toStdString());
}

PythonTextPlugin::~PythonTextPlugin()
{
    if (!m_pythonUnloadFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (void)(*m_pythonUnloadFunc)();
            }
            catch (const py::error_already_set& e) {
                m_logger->error(gppTr("PythonTextPlugin.~PythonTextPlugin",
                    "调用 PythonTextPlugin [%1] unload 函数时出现异常: %2")
                    .arg(m_modulePath)
                    .arg(e.what())
                    .toStdString());
            }
        }).get();
}

void PythonTextPlugin::dPreRun(Sentence* se) {
    if (!m_pythonDPreRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonDPreRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(gppTr("PythonTextPlugin.dPreRun",
                    "调用 PythonTextPlugin [%1] dPreRun 函数时出现异常: %2")
                    .arg(m_modulePath)
                    .arg(e.what())
                    .toStdString());
            }
        }).get();
}

void PythonTextPlugin::preRun(Sentence* se) {
    if (!m_pythonPreRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonPreRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(gppTr("PythonTextPlugin.preRun",
                    "调用 PythonTextPlugin [%1] preRun 函数时出现异常: %2")
                    .arg(m_modulePath)
                    .arg(e.what())
                    .toStdString());
            }
        }).get();
}

void PythonTextPlugin::postRun(Sentence* se) {
    if (!m_pythonPostRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonPostRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(gppTr("PythonTextPlugin.postRun",
                    "调用 PythonTextPlugin [%1] postRun 函数时出现异常: %2")
                    .arg(m_modulePath)
                    .arg(e.what())
                    .toStdString());
            }
        }).get();
}

void PythonTextPlugin::dPostRun(Sentence* se) {
    if (!m_pythonDPostRunFunc) {
        return;
    }
    m_pythonInterpreter->submitTask([&]()
        {
            try {
                (*m_pythonDPostRunFunc)(se);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(gppTr("PythonTextPlugin.dPostRun",
                    "调用 PythonTextPlugin [%1] dPostRun 函数时出现异常: %2")
                    .arg(m_modulePath)
                    .arg(e.what())
                    .toStdString());
            }
        }).get();
}
