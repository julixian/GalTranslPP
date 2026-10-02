module;

#include "GPPMacros.hpp"

export module PythonTextPlugin;

export import GPPDefines;
export import PythonManager;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;
namespace py = pybind11;

class PythonTextPlugin {
private:
    std::shared_ptr<PythonInterpreterInstance> m_pythonInterpreter;
    py::object* m_pythonDPreRunFunc = nullptr;
    py::object* m_pythonPreRunFunc = nullptr;
    py::object* m_pythonPostRunFunc = nullptr;
    py::object* m_pythonDPostRunFunc = nullptr;
    py::object* m_pythonUnloadFunc = nullptr;

    std::shared_ptr<spdlog::logger> m_logger;
    std::string m_modulePath;

public:

    PythonTextPlugin(const fs::path& projectDir, const std::string& modulePath,
        const std::unique_ptr<PythonManager>& pythonManager, const std::shared_ptr<spdlog::logger>& logger);

    ~PythonTextPlugin();

    void dPreRun(Sentence* se);
    void preRun(Sentence* se);
    void postRun(Sentence* se);
    void dPostRun(Sentence* se);
};

NAMESPACE_END(gpp)
