module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>

export module PythonManager;

import std;
import spdlog;

namespace fs = std::filesystem;
namespace py = pybind11;

export
{
    template <typename T>
    class SafeQueue {
    public:
        void push(T value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(value));
            m_cond.notify_one();
        }

        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this] { return !m_queue.empty() || m_stopped; });
            if (m_stopped && m_queue.empty()) {
                return std::nullopt;
            }
            T value = std::move(m_queue.front());
            m_queue.pop();
            return value;
        }

        void stop() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
            m_cond.notify_all();
        }

    private:
        std::queue<T> m_queue;
        std::mutex m_mutex;
        std::condition_variable m_cond;
        bool m_stopped = false;
    };



    struct PythonTask {
        std::function<void()> taskFunc;
        std::promise<void> promise; // 用于返回结果
    };



    class PythonMainInterpreterManager {
    public:

        PythonMainInterpreterManager(PythonMainInterpreterManager&) = delete;
        PythonMainInterpreterManager(PythonMainInterpreterManager&&) = delete;

        ~PythonMainInterpreterManager(){}

        static PythonMainInterpreterManager& getInstance();

        std::future<void> submitTask(std::function<void()> taskFunc);

        std::shared_ptr<py::object> registerNLPFunction
        (const std::string& moduleName, const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger, bool& needReboot);

        void stop();

    private:

        PythonMainInterpreterManager();

        void daemonThreadFunc();

        std::mutex m_mutex;
        absl::btree_map<std::string, absl::btree_map<std::string, std::weak_ptr<py::object>>> m_nlpModuleFunctions;
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

        std::thread daemonThread;
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



    bool startUpPythonEnv(const fs::path& pyEnvPath, std::unique_ptr<py::gil_scoped_release>& release);
    void shutDownPythonEnv(std::unique_ptr<py::gil_scoped_release>& release);

}
