module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>

export module PythonManager;

export import GPPDefines;
export import SafeQueue;

namespace fs = std::filesystem;
namespace py = pybind11;

export
{
    struct PythonTask {
        std::function<void()> taskFunc;
        std::promise<void> promise; // 用于返回结果
    };

    struct PythonNLPFunction {
        py::object proc;
        py::object close;
    };



    class PythonMainInterpreterManager {
    public:

        PythonMainInterpreterManager(PythonMainInterpreterManager&) = delete;
        PythonMainInterpreterManager(PythonMainInterpreterManager&&) = delete;

        ~PythonMainInterpreterManager(){}

        static PythonMainInterpreterManager& getInstance();

        std::future<void> submitTask(std::function<void()> taskFunc);

        std::shared_ptr<PythonNLPFunction> registerNLPFunction
        (const std::string& moduleName, const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger);

        void stop();

    private:

        PythonMainInterpreterManager();

        void daemonThreadFunc();

        std::thread m_daemonThread; // 守护线程
        SafeQueue<std::unique_ptr<PythonTask>> m_taskQueue;
    };



    struct PythonInterpreterInstance {

        PythonInterpreterInstance();
        ~PythonInterpreterInstance();

        std::future<void> submitTask(std::function<void()> taskFunc);

        bool isEffective() const;

        absl::btree_map<std::string, std::unique_ptr<py::object>> functions;

    private:

        void daemonThreadFunc();

        std::thread m_daemonThread;
        SafeQueue<std::unique_ptr<PythonTask>> m_taskQueue;
        std::unique_ptr<py::subinterpreter> subInterpreter;
    };



    class PythonManager {

    public:

        explicit PythonManager(const std::shared_ptr<spdlog::logger>& logger) : m_logger(logger) {}

        std::optional<std::shared_ptr<PythonInterpreterInstance>> registerFunction
        (const std::string& modulePath, const std::string& functionName);

    private:

        void registerCustomTypes(const std::string& moduleName);

        absl::btree_map<fs::path, std::shared_ptr<PythonInterpreterInstance>> m_interpreters;

        std::shared_ptr<spdlog::logger> m_logger;
    };



    void checkPythonDependencies(const std::vector<std::string>& dependencies, const std::shared_ptr<spdlog::logger>& logger);



    bool startUpPythonEnv(const fs::path& pythonEnvPath, std::unique_ptr<py::gil_scoped_release>& release);
    void shutDownPythonEnv(std::unique_ptr<py::gil_scoped_release>& release);

}
