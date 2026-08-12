module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <ctpl_stl.h>

module PythonManager;

import NormalJsonTranslator;
import EpubTranslator;
import PDFTranslator;
import NLPTool;

import ITranslator;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

namespace
{
    py::object jsonToPython(const json& value)
    {
        if (value.is_null()) {
            return py::none();
        }
        if (value.is_boolean()) {
            return py::bool_(value.get<bool>());
        }
        if (value.is_number_unsigned()) {
            return py::int_(value.get<json::number_unsigned_t>());
        }
        if (value.is_number_integer()) {
            return py::int_(value.get<json::number_integer_t>());
        }
        if (value.is_number_float()) {
            return py::float_(value.get<json::number_float_t>());
        }
        if (value.is_string()) {
            return py::str(value.get_ref<const std::string&>());
        }
        if (value.is_array()) {
            py::list result(value.size());
            for (size_t i = 0; i < value.size(); ++i) {
                result[i] = jsonToPython(value[i]);
            }
            return result;
        }

        py::dict result;
        for (auto it = value.cbegin(); it != value.cend(); ++it) {
            result[py::str(it.key())] = jsonToPython(it.value());
        }
        return result;
    }

    json pythonToJson(const py::handle& value, std::set<const PyObject*>& references)
    {
        if (!value || value.is_none()) {
            return nullptr;
        }
        if (py::isinstance<py::bool_>(value)) {
            return value.cast<bool>();
        }
        if (py::isinstance<py::int_>(value)) {
            try {
                const auto signedValue = value.cast<json::number_integer_t>();
                if (py::int_(signedValue).equal(value)) {
                    return signedValue;
                }
            }
            catch (...) { }
            try {
                const auto unsignedValue = value.cast<json::number_unsigned_t>();
                if (py::int_(unsignedValue).equal(value)) {
                    return unsignedValue;
                }
            }
            catch (...) { }
            throw std::runtime_error("Python integer is outside the nlohmann::json integer range");
        }
        if (py::isinstance<py::float_>(value)) {
            return value.cast<json::number_float_t>();
        }
        if (py::isinstance<py::bytes>(value)) {
            return py::module_::import("base64")
                .attr("b64encode")(value)
                .attr("decode")("utf-8")
                .cast<std::string>();
        }
        if (py::isinstance<py::str>(value)) {
            return value.cast<std::string>();
        }
        if (py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value)) {
            const auto [referenceIt, inserted] = references.insert(value.ptr());
            if (!inserted) {
                throw std::runtime_error("Circular reference detected while converting Python value to JSON");
            }
            json result = json::array();
            for (const py::handle item : value) {
                result.push_back(pythonToJson(item, references));
            }
            references.erase(referenceIt);
            return result;
        }
        if (py::isinstance<py::dict>(value)) {
            const auto [referenceIt, inserted] = references.insert(value.ptr());
            if (!inserted) {
                throw std::runtime_error("Circular reference detected while converting Python value to JSON");
            }
            const py::dict dictionary = py::reinterpret_borrow<py::dict>(value);
            json result = json::object();
            for (const auto& [key, item] : dictionary) {
                result[py::str(key).cast<std::string>()] = pythonToJson(item, references);
            }
            references.erase(referenceIt);
            return result;
        }
        throw std::runtime_error("Unsupported Python value while converting to JSON: "
            + py::repr(value).cast<std::string>());
    }

    json pythonToJson(const py::handle& value)
    {
        std::set<const PyObject*> references;
        return pythonToJson(value, references);
    }
}

namespace pybind11::detail
{
    template <typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
    struct type_caster<absl::flat_hash_map<Key, Value, Hash, Equal, Alloc>>
        : map_caster<absl::flat_hash_map<Key, Value, Hash, Equal, Alloc>, Key, Value> {};

    template <typename Key, typename Hash, typename Equal, typename Alloc>
    struct type_caster<absl::flat_hash_set<Key, Hash, Equal, Alloc>>
        : set_caster<absl::flat_hash_set<Key, Hash, Equal, Alloc>, Key> {};

    template <typename Key, typename Value, typename Compare, typename Alloc>
    struct type_caster<absl::btree_map<Key, Value, Compare, Alloc>>
        : map_caster<absl::btree_map<Key, Value, Compare, Alloc>, Key, Value> {};

    template <typename Key, typename Compare, typename Alloc>
    struct type_caster<absl::btree_set<Key, Compare, Alloc>>
        : set_caster<absl::btree_set<Key, Compare, Alloc>, Key> {};

    template <>
    struct type_caster<json>
    {
        PYBIND11_TYPE_CASTER(json, _("json"));

        bool load(handle src, bool)
        {
            try {
                value = pythonToJson(src);
                return true;
            }
            catch (...) {
                return false;
            }
        }

        static handle cast(json src, return_value_policy, handle)
        {
            return jsonToPython(src).release();
        }
    };

    template <>
    struct type_caster<ordered_json>
    {
        PYBIND11_TYPE_CASTER(ordered_json, _("json"));

        bool load(handle src, bool convert)
        {
            type_caster<json> jsonCaster;
            if (!jsonCaster.load(src, convert)) {
                return false;
            }
            value = ordered_json(cast_op<json&&>(std::move(jsonCaster)));
            return true;
        }

        static handle cast(ordered_json src, return_value_policy policy, handle parent)
        {
            return type_caster<json>::cast(json(std::move(src)), policy, parent);
        }
    };
}

static fs::path s_pythonExePath;

// PythonMainInterpreterManager
PythonMainInterpreterManager::PythonMainInterpreterManager() {
    if (Py_IsInitialized()) {
        m_daemonThread = std::thread(&PythonMainInterpreterManager::daemonThreadFunc, this);
    }
    else {
        throw std::runtime_error(gppTr(
            "PythonMainInterpreterManager.PythonMainInterpreterManager",
            "Python 环境未初始化")
            .toStdString());
    }
}

PythonMainInterpreterManager& PythonMainInterpreterManager::getInstance() {
    static PythonMainInterpreterManager instance;
    return instance;
}

std::future<void> PythonMainInterpreterManager::submitTask(std::function<void()> taskFunc) {
    auto task = std::make_unique<PythonTask>();
    task->taskFunc = std::move(taskFunc);
    auto future = task->promise.get_future();
    m_taskQueue.push(std::move(task));
    return future;
}


void pythonNLPFunctionDeleter(PythonNLPFunction* ptr) {
    auto deleteTaskFunc = [ptr]()
        {
            try {
                (void)ptr->close();
            }
            catch (...) { }
            delete ptr;
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(deleteTaskFunc));
}

// 最理想的情况当然是把 NLP 函数也放在子解释器里运行，但这些 NLP 模块都很娇气，不是在主解释里的导入就会崩溃。。。
std::shared_ptr<PythonNLPFunction> PythonMainInterpreterManager::registerNLPFunction
(const std::string& moduleName, const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger) {
    std::shared_ptr<PythonNLPFunction> pythonNLPModuleFunc;

    logger->info(gppTr("PythonMainInterpreterManager.registerNLPFunction", "正在加载模块 [%1] 的模型 %2")
        .arg(moduleName)
        .arg(modelName)
        .toStdString());
    auto loadModelTaskFunc = [&]()
        {
            try {
                py::module_ nlpModule = py::module_::import(moduleName.c_str());
                const bool modelReady = nlpModule.attr("ensure_model")(modelName).cast<bool>();
                if (!modelReady) {
                    throw std::runtime_error(gppTr(
                        "PythonMainInterpreterManager.registerNLPFunction",
                        "模块 [%1] 的模型 %2 不可用")
                        .arg(moduleName)
                        .arg(modelName)
                        .toStdString());
                }
                py::object processor = nlpModule.attr("NLPProcessor")(modelName);
                pythonNLPModuleFunc = std::shared_ptr<PythonNLPFunction>(
                    new PythonNLPFunction{
                        .proc = processor.attr("process_text"),
                        .close = processor.attr("close")
                    },
                    pythonNLPFunctionDeleter);
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(gppTr(
                    "PythonMainInterpreterManager.registerNLPFunction",
                    "加载模块 [%1] 的模型 %2 时出现异常: %3")
                    .arg(moduleName)
                    .arg(modelName)
                    .arg(e.what())
                    .toStdString());
            }
        };
    this->submitTask(std::move(loadModelTaskFunc)).get();
    logger->debug(gppTr("PythonMainInterpreterManager.registerNLPFunction", "模块 [%1] 的模型 %2 已加载")
        .arg(moduleName)
        .arg(modelName)
        .toStdString());
    return pythonNLPModuleFunc;
}

void PythonMainInterpreterManager::stop() {
    m_taskQueue.stop();
    if (m_daemonThread.joinable()) {
        m_daemonThread.join();
    }
}

void PythonMainInterpreterManager::daemonThreadFunc() {
    py::gil_scoped_acquire acquire;
    try {
        py::module_::import("gpp_plugin_api");
    }
    catch (const py::error_already_set& e) {
        throw std::runtime_error(gppTr(
            "PythonMainInterpreterManager.daemonThreadFunc",
            "PythonMainInterpreterManager 导入 gpp_plugin_api 时出现异常: %1")
            .arg(e.what())
            .toStdString());
    }
    while (true) {
        const auto taskOpt = m_taskQueue.pop();
        if (!taskOpt) {
            break;
        }
        const std::unique_ptr<PythonTask>& task = taskOpt.value();
        try {
            task->taskFunc();
            task->promise.set_value();
        }
        catch (const py::error_already_set& e) {
            // python 异常不能带出守护线程作用域，因为 .what() 时需要获取 GIL
            // 如果 taskFunc 没有捕获就在这里手动转成 runtime_error
            task->promise.set_exception(
                std::make_exception_ptr(
                    std::runtime_error(gppTr(
                        "PythonMainInterpreterManager.daemonThreadFunc",
                        "PythonMainInterpreterManager 异常: %1")
                        .arg(e.what())
                        .toStdString())
                )
            );
        }
        catch (...) {
            // 如果是我们自己抛的异常的话就直接转发
            task->promise.set_exception(std::current_exception());
        }
    }
}




// PythonInterpreterInstance
PythonInterpreterInstance::PythonInterpreterInstance() {
    auto createSubInterpreterTaskFunc = [&]()
        {
            try {
                PyInterpreterConfig cfg = { 0 };
                cfg.allow_daemon_threads = 1;
                cfg.allow_threads = 1;
                cfg.check_multi_interp_extensions = 1;
                cfg.gil = PyInterpreterConfig_OWN_GIL;
                subInterpreter = std::make_unique<py::subinterpreter>(py::subinterpreter::create(cfg));
            }
            catch (...) { }
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(createSubInterpreterTaskFunc)).get();
    if (subInterpreter) {
        m_daemonThread = std::thread(&PythonInterpreterInstance::daemonThreadFunc, this);
    }
}

PythonInterpreterInstance::~PythonInterpreterInstance() {
    auto functionClearTaskFunc = [this]()
        {
            this->functions.clear(); // noexcept
        };
    this->submitTask(std::move(functionClearTaskFunc)).get();
    m_taskQueue.stop();
    if (m_daemonThread.joinable()) {
        m_daemonThread.join();
    }
    auto destroySubInterpreterTaskFunc = [this]()
        {
            this->subInterpreter.reset(); // noexcept
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(destroySubInterpreterTaskFunc)).get();
}

std::future<void> PythonInterpreterInstance::submitTask(std::function<void()> taskFunc) {
    auto task = std::make_unique<PythonTask>();
    task->taskFunc = std::move(taskFunc);
    auto future = task->promise.get_future();
    m_taskQueue.push(std::move(task));
    return future;
}

bool PythonInterpreterInstance::isEffective() const {
    return this->subInterpreter.operator bool();
}

void PythonInterpreterInstance::daemonThreadFunc() {
    py::subinterpreter_scoped_activate activate(*subInterpreter);
    try {
        py::module_::import("sys").attr("path").attr("append")(wide2Ascii(fs::absolute(L"BaseConfig/PythonScripts")));
        py::module_::import("gpp_plugin_api");
    }
    catch (const py::error_already_set& e) {
        throw std::runtime_error(gppTr(
            "PythonInterpreterInstance.daemonThreadFunc",
            "PythonInterpreterInstance 导入 gpp_plugin_api 时出现异常: %1")
            .arg(e.what())
            .toStdString());
    }
    while (true) {
        const auto taskOpt = m_taskQueue.pop();
        if (!taskOpt) {
            break;
        }
        const std::unique_ptr<PythonTask>& task = taskOpt.value();
        try {
            task->taskFunc();
            task->promise.set_value();
        }
        catch (const py::error_already_set& e) {
            task->promise.set_exception(
                std::make_exception_ptr(
                    std::runtime_error(gppTr(
                        "PythonInterpreterInstance.daemonThreadFunc",
                        "PythonInterpreterInstance 异常: %1")
                        .arg(e.what())
                        .toStdString())
                )
            );
        }
        catch (...) {
            task->promise.set_exception(std::current_exception());
        }
    }
}




// PythonManager
std::optional<std::shared_ptr<PythonInterpreterInstance>> PythonManager::registerFunction
(const std::string& modulePath, const std::string& functionName) {

    const fs::path stdModulePath = fs::weakly_canonical(ascii2Wide(modulePath));
    if (!fs::exists(stdModulePath)) {
        m_logger->error(gppTr("PythonManager.registerFunction", "脚本 [%1] 不存在")
            .arg(modulePath)
            .toStdString());
        return std::nullopt;
    }

    const std::string moduleName = wide2Ascii(stdModulePath.stem());
    auto it = m_interpreters.find(stdModulePath);
    if (it == m_interpreters.end()) {

        auto pythonInterpreter = std::make_shared<PythonInterpreterInstance>();
        if (!pythonInterpreter->isEffective()) {
            throw std::runtime_error(gppTr(
                "PythonManager.registerFunction",
                "加载模块 [%1] 时出现异常，子解释器无法开启")
                .arg(moduleName)
                .toStdString());
        }

        pythonInterpreter->submitTask([&]()
            {
                try {
                    if (stdModulePath.has_parent_path()) {
                        const py::list sysPaths = py::module_::import("sys").attr("path");
                        if (!sysPaths.contains(wide2Ascii(stdModulePath.parent_path()))) {
                            py::module_::import("sys").attr("path").attr("append")(wide2Ascii(stdModulePath.parent_path()));
                        }
                    }
                    registerCustomTypes(moduleName);
                }
                catch (const py::error_already_set& e) {
                    throw std::runtime_error(gppTr(
                        "PythonManager.registerFunction",
                        "为模块 [%1] 加载自定义类型时出现异常: %2")
                        .arg(moduleName)
                        .arg(e.what())
                        .toStdString());
                }
            }).get();
        const auto [retIt, inserted] = m_interpreters.insert({ stdModulePath, pythonInterpreter });
        if (inserted) {
            it = retIt;
        }
        else {
            throw std::runtime_error(gppTr("PythonManager.registerFunction", "模块 [%1] 插入失败")
                .arg(moduleName)
                .toStdString());
        }
    }

    const auto pythonInterpreter = it->second;
    if (!pythonInterpreter->functions.contains(functionName)) {
        bool success = false;
        pythonInterpreter->submitTask([&]()
            {
                try {
                    const py::module_ pythonModule = py::module_::import(moduleName.c_str());
                    if (!py::hasattr(pythonModule, functionName.c_str())) {
                        m_logger->debug(gppTr("PythonManager.registerFunction", "从脚本 [%1] 加载函数 %2 失败")
                            .arg(modulePath)
                            .arg(functionName)
                            .toStdString());
                        return;
                    }
                    auto pFunc = std::make_unique<py::object>(pythonModule.attr(functionName.c_str()));
                    if (const py::object& func = *pFunc; !func || !py::isinstance<py::function>(func)) {
                        m_logger->debug(gppTr("PythonManager.registerFunction", "从脚本 [%1] 加载函数 %2 失败")
                            .arg(modulePath)
                            .arg(functionName)
                            .toStdString());
                        return;
                    }
                    pythonInterpreter->functions.insert({ functionName, std::move(pFunc) });
                    success = true;
                }
                catch (const py::error_already_set& e) {
                    throw std::runtime_error(gppTr(
                        "PythonManager.registerFunction",
                        "加载模块 [%1] 的函数 %2 时出现异常: %3")
                        .arg(moduleName)
                        .arg(functionName)
                        .arg(e.what())
                        .toStdString());
                }
            }).get();
        if (!success) {
            return std::nullopt;
        }
    }
    return pythonInterpreter;
}

// 这个函数是在子解释器的守护线程里执行的
void PythonManager::registerCustomTypes(const std::string& moduleName) {
    const py::module_ pythonModule = py::module_::import(moduleName.c_str());
    auto setupTokenizer = [&](const std::string& mode)
        {
            const std::string useTokenizerFlag = mode + "UseTokenizer";
            if (py::hasattr(pythonModule, useTokenizerFlag.c_str()) && pythonModule.attr(useTokenizerFlag.c_str()).cast<bool>()) {
                const std::string tokenizerBackend = pythonModule.attr((mode + "TokenizerBackend").c_str()).cast<std::string>();
                if (tokenizerBackend == "MeCab") {
                    const std::string mecabDictDir = pythonModule.attr((mode + "MecabDictDir").c_str()).cast<std::string>();
                    m_logger->info(gppTr(
                        "PythonManager.registerCustomTypes",
                        "[%1] 已配置 MeCab 分词器，首次使用时加载")
                        .arg(moduleName)
                        .toStdString());
                    pythonModule.attr((mode + "TokenizeFunc").c_str()) = getMeCabTokenizeFunc(mecabDictDir, m_logger);
                }
                else if (tokenizerBackend == "spaCy") {
                    const std::string spaCyModelName = pythonModule.attr((mode + "SpaCyModelName").c_str()).cast<std::string>();
                    m_logger->info(gppTr(
                        "PythonManager.registerCustomTypes",
                        "[%1] 已配置 spaCy 分词器，首次使用时加载")
                        .arg(moduleName)
                        .toStdString());
                    pythonModule.attr((mode + "TokenizeFunc").c_str()) = getPythonNLPTokenizeFunc(
                        { "click", "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger);
                }
                else if (tokenizerBackend == "Stanza") {
                    const std::string stanzaLang = pythonModule.attr((mode + "StanzaLang").c_str()).cast<std::string>();
                    m_logger->info(gppTr(
                        "PythonManager.registerCustomTypes",
                        "[%1] 已配置 Stanza 分词器，首次使用时加载")
                        .arg(moduleName)
                        .toStdString());
                    pythonModule.attr((mode + "TokenizeFunc").c_str()) = getPythonNLPTokenizeFunc(
                        { "stanza" }, "tokenizer_stanza", stanzaLang, m_logger);
                }
                else if (tokenizerBackend == "pkuseg") {
                    m_logger->info(gppTr(
                        "PythonManager.registerCustomTypes",
                        "[%1] 已配置 pkuseg 分词器，首次使用时加载")
                        .arg(moduleName)
                        .toStdString());
                    pythonModule.attr((mode + "TokenizeFunc").c_str()) = getPythonNLPTokenizeFunc(
                        { "setuptools", "nes-py", "cython", "pkuseg" }, "tokenizer_pkuseg", "default", m_logger);
                }
                else {
                    throw std::invalid_argument(gppTr(
                        "PythonManager.registerCustomTypes",
                        "[%1] 中注册了无效的 TokenizerBackend: %2")
                        .arg(moduleName)
                        .arg(tokenizerBackend)
                        .toStdString());
                }
            }
        };
    setupTokenizer("sourceLang");
    setupTokenizer("targetLang");
    pythonModule.attr("logger") = m_logger;
}




void checkPythonDependencies(const std::vector<std::string>& dependencies, const std::shared_ptr<spdlog::logger>& logger)
{
    for (const auto& dependency : dependencies) {
        logger->debug(gppTr("checkPythonDependencies", "正在检查依赖 %1").arg(dependency).toStdString());
        auto checkDependencyTaskFunc = [&]()
            {
                try {
                    (void)py::module_::import("importlib.metadata").attr("version")(dependency);
                    logger->debug(gppTr("checkPythonDependencies", "依赖 %1 已安装")
                        .arg(dependency)
                        .toStdString());
                }
                catch (const py::error_already_set& e) {

                    if (!e.matches(py::module_::import("importlib.metadata").attr("PackageNotFoundError"))) {
                        throw std::runtime_error(gppTr(
                            "checkPythonDependencies",
                            "检查依赖 %1 时出现异常: %2")
                            .arg(dependency)
                            .arg(e.what())
                            .toStdString());
                    }

                    logger->error(gppTr("checkPythonDependencies", "依赖 %1 未安装，正在尝试安装")
                        .arg(dependency)
                        .toStdString());
                    const std::string installCommand = "-m pip install " + dependency;
                    logger->info(gppTr("checkPythonDependencies", "将在 3s 后开始安装依赖，请勿关闭接下来出现的窗口！")
                        .toStdString());
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    logger->info(gppTr("checkPythonDependencies", "正在执行安装命令: %1")
                        .arg(installCommand)
                        .toStdString());

                    executeCommand(s_pythonExePath.wstring(), L"-m pip cache purge", true, 3);
                    if (!executeCommand(s_pythonExePath.wstring(), ascii2Wide(installCommand))) {
                        throw std::runtime_error(gppTr("checkPythonDependencies", "安装依赖 %1 的命令失败")
                            .arg(dependency)
                            .toStdString());
                    }

                    try {
                        (void)py::module_::import("importlib.metadata").attr("version")(dependency);
                        logger->info(gppTr("checkPythonDependencies", "依赖 %1 安装成功")
                            .arg(dependency)
                            .toStdString());
                    }
                    catch (const py::error_already_set& eReCheck) {
                        throw std::runtime_error(gppTr(
                            "checkPythonDependencies",
                            "依赖 %1 安装验证失败: %2")
                            .arg(dependency)
                            .arg(eReCheck.what())
                            .toStdString());
                    }
                }
            };
        PythonMainInterpreterManager::getInstance().submitTask(std::move(checkDependencyTaskFunc)).get();
        logger->debug(gppTr("checkPythonDependencies", "依赖 %1 检查完毕").arg(dependency).toStdString());
    }
    logger->debug(gppTr("checkPythonDependencies", "所有依赖均已安装").toStdString());
}




// 开启关闭 Python 解释器
static const fs::path pythonSysPathsTxtPath = L"BaseConfig/pythonSysPaths.txt";
bool startUpPythonEnv(const fs::path& pythonEnvPath, std::unique_ptr<py::gil_scoped_release>& release) {
    if (fs::exists(pythonEnvPath) && fs::exists(pythonEnvPath / L"python.exe")) {

        const fs::path envZipPath = [&]()
	        {
                for (const auto& entry : fs::directory_iterator(pythonEnvPath)) {
                    if (isSameExtension(entry.path(), L".zip") &&
                        str2Lower(entry.path().filename().wstring()).starts_with(L"python"))
                    {
                        return entry.path();
                    }
                }
                return fs::path{};
            }();

        if (!envZipPath.empty()) {
            s_pythonExePath = fs::canonical(pythonEnvPath / L"python.exe");
            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            PyConfig_SetString(&config, &config.home, fs::canonical(pythonEnvPath).c_str());
            PyConfig_SetString(&config, &config.executable, fs::canonical(pythonEnvPath / L"python.exe").c_str());
            PyConfig_SetString(&config, &config.pythonpath_env, envZipPath.c_str());
            py::initialize_interpreter(&config);
            {
                py::module_::import("importlib.metadata");
                py::module_::import("sys").attr("path").attr("append")
                    (wide2Ascii(fs::absolute(L"BaseConfig/PythonScripts")));
                py::list sysPaths = py::module_::import("sys").attr("path");
                std::string sysPathsText;
                for (const auto& path : sysPaths) {
                    sysPathsText += path.cast<std::string>() + "\n";
                }
                atomicOutputFile(pythonSysPathsTxtPath, sysPathsText);
            }
            release = std::make_unique<py::gil_scoped_release>();
            return true;
        }
    }
    return false;
}

void shutDownPythonEnv(std::unique_ptr<py::gil_scoped_release>& release) {
    if (release) {
        PythonMainInterpreterManager::getInstance().stop();
        release.reset();
        py::finalize_interpreter();
    }
    if (fs::exists(pythonSysPathsTxtPath)) {
        fs::remove(pythonSysPathsTxtPath);
    }
}




// gpp_plugin_api
// 定义一个 C++ 模块，它将被嵌入到 Python 解释器中
// 所有脚本都可以通过 `import gpp_plugin_api` 来使用这些功能
PYBIND11_EMBEDDED_MODULE(gpp_plugin_api, m, py::multiple_interpreters::per_interpreter_gil())
{
    m.doc() = "GalTransl++ C++ Api for Python-based plugins";

    py::enum_<NameType>(m, "NameType")
        .value("None", NameType::None)
        .value("Single", NameType::Single)
        .value("Multiple", NameType::Multiple);
        //.export_values(); // 允许在 Python 中直接使用 gpp_plugin_api.Single 这样的形式

        py::enum_<TransEngine>(m, "TransEngine")
        .value("None", TransEngine::None)
        .value("ForGalJson", TransEngine::ForGalJson)
        .value("ForGalTsv", TransEngine::ForGalTsv)
        .value("ForNovelTsv", TransEngine::ForNovelTsv)
        .value("Sakura", TransEngine::Sakura)
        .value("DumpName", TransEngine::DumpName)
        .value("NameTrans", TransEngine::NameTrans)
        .value("GenDict", TransEngine::GenDict)
        .value("Rebuild", TransEngine::Rebuild)
        .value("ShowNormal", TransEngine::ShowNormal);

    py::enum_<CachePart>(m, "CachePart")
        .value("None", CachePart::None)
        .value("Index", CachePart::Index)
        .value("FileName", CachePart::FileName)
        .value("Name", CachePart::Name)
        .value("NameTrans", CachePart::NameTrans)
        .value("Names", CachePart::Names)
        .value("NamesTrans", CachePart::NamesTrans)
        .value("Orig", CachePart::Orig)
        .value("Preproc", CachePart::Preproc)
        .value("Problems", CachePart::Problems)
        .value("OtherInfo", CachePart::OtherInfo)
        .value("TransBy", CachePart::TransBy)
        .value("TransRaw", CachePart::TransRaw)
        .value("Transview", CachePart::Transview);

    py::enum_<ApiProtocol>(m, "ApiProtocol")
        .value("OpenAI", ApiProtocol::OpenAI)
        .value("Claude", ApiProtocol::Claude)
        .value("Gemini", ApiProtocol::Gemini);

    py::class_<SentencePosition>(m, "SentencePosition")
        .def(py::init<>())
        .def_readwrite("file", &SentencePosition::file)
        .def_readwrite("index", &SentencePosition::index);

    // 绑定 Sentence 结构体
    // 如果指针是 nullptr，在 Python 中会是 None。
    py::class_<Sentence>(m, "Sentence")
        .def(py::init<>()) // 允许在 Python 中创建实例: s = gpp_plugin_api.Sentence()
        .def_readwrite("index", &Sentence::index)
        .def_readwrite("filename", &Sentence::filename)
        .def_readwrite("name", &Sentence::name)
        .def_readwrite("names", &Sentence::names) // std::vector<string> <=> list[str]
        .def_readwrite("nametrans", &Sentence::nametrans)
        .def_readwrite("namestrans", &Sentence::namestrans)
        .def_readwrite("orig", &Sentence::orig)
        .def_readwrite("preproc", &Sentence::preproc)
        .def_readwrite("problems", &Sentence::problems)
        .def_readwrite("transby", &Sentence::transby)
        .def_readwrite("transraw", &Sentence::transraw)
        .def_readwrite("transview", &Sentence::transview)
        .def_readwrite("linebreak", &Sentence::linebreak)
        .def_readwrite("otherinfo", &Sentence::otherinfo) // absl::btree_map<string, string> <=> dict[str, str]
        .def_readwrite("ref", &Sentence::ref)
        .def_readwrite("refBy", &Sentence::refBy)
        .def_readwrite("nameType", &Sentence::nameType)
        .def_readwrite("prev", &Sentence::prev) // Sentence* <=> Sentence or None
        .def_readwrite("next", &Sentence::next) // Sentence* <=> Sentence or None
        .def_readwrite("transCompleted", &Sentence::transCompleted)
        .def_readwrite("problemAnalyzeDisabled", &Sentence::problemAnalyzeDisabled)
        .def_readwrite("isRefPending", &Sentence::isRefPending)
        .def("getProblemByIndex", &Sentence::getProblemByIndex)
        .def("setProblemByIndex", &Sentence::setProblemByIndex);

    py::enum_<spdlog::level::level_enum>(m, "LogLevel")
        .value("trace", spdlog::level::trace)
        .value("debug", spdlog::level::debug)
        .value("info", spdlog::level::info)
        .value("warn", spdlog::level::warn)
        .value("err", spdlog::level::err)
        .value("critical", spdlog::level::critical);

    // 绑定 spdlog::logger 类型，以便 Python 知道 "logger" 是什么
    // 使用 std::shared_ptr 作为持有者类型，因为 m_logger 就是一个 shared_ptr
    py::class_<spdlog::logger, std::shared_ptr<spdlog::logger>>(m, "spdlogLogger")
        .def("name", &spdlog::logger::name)
        .def("level", &spdlog::logger::level)
        .def("set_level", &spdlog::logger::set_level)
        .def("set_pattern", [](spdlog::logger& logger, const std::string& pattern) { logger.set_pattern(pattern); })
        .def("trace", [](spdlog::logger& logger, const std::string& msg) { logger.trace(msg); })
        .def("debug", [](spdlog::logger& logger, const std::string& msg) { logger.debug(msg); })
        .def("info", [](spdlog::logger& logger, const std::string& msg) { logger.info(msg); })
        .def("warn", [](spdlog::logger& logger, const std::string& msg) { logger.warn(msg); })
        .def("error", [](spdlog::logger& logger, const std::string& msg) { logger.error(msg); })
        .def("critical", [](spdlog::logger& logger, const std::string& msg) { logger.critical(msg); });

    py::module_ utilsSubmodule = m.def_submodule("utils", "A submodule for utility m_functions");

    utilsSubmodule
        .def("splitIntoTokens", &splitIntoTokens)
        .def("splitIntoGraphemes", &splitIntoGraphemes)
        .def("countGraphemes", &countGraphemes)
        .def("getMostCommonChar", &getMostCommonChar)
        .def("hasPunctuation", &hasPunctuation)
        .def("hasWhitespace", &hasWhitespace)
        .def("isAllPunctuation", &isAllPunctuation)
        .def("isAllWhitespace", &isAllWhitespace)
        .def("removePunctuation", &removePunctuation)
        .def("removeWhitespace", &removeWhitespace)
        .def("hasKatakana", &hasKatakana)
        .def("hasKana", &hasKana)
        .def("hasLatin", &hasLatin)
        .def("hasHangul", &hasHangul)
        .def("hasCJK", &hasCJK)
        .def("extractKatakana", &extractKatakana)
        .def("extractKana", &extractKana)
        .def("extractLatin", &extractLatin)
        .def("extractHangul", &extractHangul)
        .def("extractCJK", &extractCJK)
        .def("getTraditionalChineseExtractor", &getTraditionalChineseExtractor)
        .def("getConsoleWidth", &getConsoleWidth)
        .def("loadTokenizeCache", [](const fs::path& cachePath, const std::shared_ptr<spdlog::logger>& logger)
	        {
                absl::flat_hash_map<std::string, WordPosVec> result;
                loadTokenizeCache(result, cachePath, logger);
                return result;
	        })
        .def("saveTokenizeCache", &saveTokenizeCache);

    py::class_<RuntimeTransSuccessEvent>(m, "RuntimeTransSuccessEvent")
        .def(py::init<>())
        .def_readwrite("timestamp", &RuntimeTransSuccessEvent::timestamp)
        .def_readwrite("filename", &RuntimeTransSuccessEvent::filename)
        .def_readwrite("index", &RuntimeTransSuccessEvent::index)
        .def_readwrite("speakers", &RuntimeTransSuccessEvent::speakers)
        .def_readwrite("problems", &RuntimeTransSuccessEvent::problems)
        .def_readwrite("sourcePreview", &RuntimeTransSuccessEvent::sourcePreview)
        .def_readwrite("translationPreview", &RuntimeTransSuccessEvent::translationPreview)
        .def_readwrite("transby", &RuntimeTransSuccessEvent::transby);

    py::class_<RuntimeTransErrorEvent>(m, "RuntimeTransErrorEvent")
        .def(py::init<>())
        .def_readwrite("timestamp", &RuntimeTransErrorEvent::timestamp)
        .def_readwrite("kind", &RuntimeTransErrorEvent::kind)
        .def_readwrite("level", &RuntimeTransErrorEvent::level)
        .def_readwrite("message", &RuntimeTransErrorEvent::message)
        .def_readwrite("filename", &RuntimeTransErrorEvent::filename)
        .def_readwrite("indexRange", &RuntimeTransErrorEvent::indexRange)
        .def_readwrite("requestCount", &RuntimeTransErrorEvent::requestCount)
        .def_readwrite("model", &RuntimeTransErrorEvent::model)
        .def_readwrite("sleepSeconds", &RuntimeTransErrorEvent::sleepSeconds);

    py::class_<RuntimeFileProgress>(m, "RuntimeFileProgress")
        .def(py::init<>())
        .def_readwrite("filename", &RuntimeFileProgress::filename)
        .def_readwrite("total", &RuntimeFileProgress::total)
        .def_readwrite("completed", &RuntimeFileProgress::completed)
        .def_readwrite("problems", &RuntimeFileProgress::problems);

    py::class_<IController, std::shared_ptr<IController>>(m, "IController")
        .def_property("m_totalSentences", [](IController& self) { return self.m_totalSentences.load(); },
            [](IController& self, int val) { self.m_totalSentences = val; })
        .def_property("m_completedSentences", [](IController& self) { return self.m_completedSentences.load(); },
            [](IController& self, int val) { self.m_completedSentences = val; })
        .def_property("m_activeThreads", [](IController& self) { return self.m_activeThreads.load(); },
            [](IController& self, int val) { self.m_activeThreads = val; })
        .def_property("m_totalThreads", [](IController& self) { return self.m_totalThreads.load(); },
            [](IController& self, int val) { self.m_totalThreads = val; })
        .def("makeBar", &IController::makeBar)
        .def("writeLog", &IController::writeLog)
        .def("addThreadNum", &IController::addThreadNum)
        .def("reduceThreadNum", &IController::reduceThreadNum)
        .def("updateBar", &IController::updateBar, py::arg("ticks") = 1)
        .def("setRuntimeFiles", &IController::setRuntimeFiles)
        .def("setRuntimeStage", &IController::setRuntimeStage, py::arg("stage"), py::arg("currentFile") = std::string{})
        .def("recordFileSentenceDone", &IController::recordFileSentenceDone)
        .def("recordRuntimeTransSuccess", &IController::recordRuntimeTransSuccess)
        .def("recordRuntimeTransError", &IController::recordRuntimeTransError)
        .def("shouldStop", &IController::shouldStop)
        .def("flush", &IController::flush);

    // ITranslator
    py::class_<ITranslator>(m, "ITranslator")
        // 不要绑定构造函数，因为 Python 不应该创建这个接口的实例
        .def("run", &ITranslator::run);

    py::class_<ctpl::thread_pool>(m, "ThreadPool")
        .def("resize", &ctpl::thread_pool::resize)
        .def("size", &ctpl::thread_pool::size);

    py::class_<ApiPool>(m, "ApiPool")
        .def("resortTokens", &ApiPool::resortTokens)
        .def("isEmpty", &ApiPool::isEmpty)
        .def("size", &ApiPool::size);

    py::class_<GptDictionary>(m, "GptDictionary")
        .def("sort", &GptDictionary::sort)
        .def("loadFromFile", &GptDictionary::loadFromFile);

    py::class_<NormalDictionary>(m, "NormalDictionary")
        .def("sort", &NormalDictionary::sort)
        .def("loadFromFile", &NormalDictionary::loadFromFile);

    py::class_<ProblemCompareObj>(m, "ProblemCompareObj")
        .def(py::init<>())
        .def_readwrite("use", &ProblemCompareObj::use)
        .def_readwrite("base", &ProblemCompareObj::base)
        .def_readwrite("check", &ProblemCompareObj::check);

    py::class_<Problems>(m, "Problems")
        .def(py::init<>())
        .def_readwrite("highFrequency", &Problems::highFrequency)
        .def_readwrite("punctsMiss", &Problems::punctsMiss)
        .def_readwrite("remainJp", &Problems::remainJp)
        .def_readwrite("introLatin", &Problems::introLatin)
        .def_readwrite("introHangul", &Problems::introHangul)
        .def_readwrite("introTraditionalChinese", &Problems::introTraditionalChinese)
        .def_readwrite("linebreakLost", &Problems::linebreakLost)
        .def_readwrite("linebreakAdded", &Problems::linebreakAdded)
        .def_readwrite("longer", &Problems::longer)
        .def_readwrite("strictlyLonger", &Problems::strictlyLonger)
        .def_readwrite("dictUnused", &Problems::dictUnused)
        .def_readwrite("notTargetLang", &Problems::notTargetLang)
        .def_readwrite("invalidChar", &Problems::invalidChar);

    py::class_<ProblemAnalyzer>(m, "ProblemAnalyzer")
        .def("setProblemRule", &ProblemAnalyzer::setProblemRule)
        .def("analyze", [](ProblemAnalyzer& self, Sentence& sentence) { self.analyze(&sentence); });

    py::class_<NameTranslator>(m, "NameTranslator")
        .def("run", &NameTranslator::run, py::call_guard<py::gil_scoped_release>());

    py::class_<DictionaryGenerator>(m, "DictionaryGenerator")
        .def("generate", &DictionaryGenerator::generate, py::call_guard<py::gil_scoped_release>());

    py::class_<NormalJsonTranslatorTransAgent>(m, "NormalJsonTranslatorTransAgent")
        .def("applyAgentSuggestions", &NormalJsonTranslatorTransAgent::applyAgentSuggestions);

    py::class_<NormalJsonTranslator, ITranslator>(m, "NormalJsonTranslator")
        .def_readwrite("m_transEngine", &NormalJsonTranslator::m_transEngine)
        .def_readwrite("m_controller", &NormalJsonTranslator::m_controller)
        .def_readwrite("m_logger", &NormalJsonTranslator::m_logger)
        .def_readwrite("m_inputDir", &NormalJsonTranslator::m_inputDir)
        .def_readwrite("m_inputCacheDir", &NormalJsonTranslator::m_inputCacheDir)
        .def_readwrite("m_outputDir", &NormalJsonTranslator::m_outputDir)
        .def_readwrite("m_outputCacheDir", &NormalJsonTranslator::m_outputCacheDir)
        .def_readwrite("m_transCacheDir", &NormalJsonTranslator::m_transCacheDir)
        .def_readwrite("m_otherCacheDir", &NormalJsonTranslator::m_otherCacheDir)
        .def_readwrite("m_nameTablePath", &NormalJsonTranslator::m_nameTablePath)
        .def_readwrite("m_rollingContextCachePath", &NormalJsonTranslator::m_rollingContextCachePath)
        .def_readwrite("m_projectDir", &NormalJsonTranslator::m_projectDir)
        .def_readwrite("m_agentRootDir", &NormalJsonTranslator::m_agentRootDir)
        .def_readwrite("m_agentTermLedgerPath", &NormalJsonTranslator::m_agentTermLedgerPath)
        .def_readwrite("m_agentFileNotesDir", &NormalJsonTranslator::m_agentFileNotesDir)
        .def_readwrite("m_rollingContextCacheMap", &NormalJsonTranslator::m_rollingContextCacheMap)
        .def_readwrite("m_systemPrompt", &NormalJsonTranslator::m_systemPrompt)
        .def_readwrite("m_userPrompt", &NormalJsonTranslator::m_userPrompt)
        .def_readwrite("m_agentSystemPrompt", &NormalJsonTranslator::m_agentSystemPrompt)
        .def_readwrite("m_agentUserPrompt", &NormalJsonTranslator::m_agentUserPrompt)
        .def_readwrite("m_genDictReviewSystemPrompt", &NormalJsonTranslator::m_genDictReviewSystemPrompt)
        .def_readwrite("m_genDictReviewUserPrompt", &NormalJsonTranslator::m_genDictReviewUserPrompt)
        .def_readwrite("m_targetLang", &NormalJsonTranslator::m_targetLang)
        .def_readwrite("m_threadsNum", &NormalJsonTranslator::m_threadsNum)
        .def_readwrite("m_nameTransBatchSize", &NormalJsonTranslator::m_nameTransBatchSize)
        .def_readwrite("m_batchSize", &NormalJsonTranslator::m_batchSize)
        .def_readwrite("m_contextHistorySize", &NormalJsonTranslator::m_contextHistorySize)
        .def_readwrite("m_inputBlockMaxLines", &NormalJsonTranslator::m_inputBlockMaxLines)
        .def_readwrite("m_problemMaxLines", &NormalJsonTranslator::m_problemMaxLines)
        .def_readwrite("m_glossaryMaxLines", &NormalJsonTranslator::m_glossaryMaxLines)
        .def_readwrite("m_maxRequestCount", &NormalJsonTranslator::m_maxRequestCount)
        .def_readwrite("m_saveCacheInterval", &NormalJsonTranslator::m_saveCacheInterval)
        .def_readwrite("m_apiTimeOutMs", &NormalJsonTranslator::m_apiTimeOutMs)
        .def_readwrite("m_checkQuota", &NormalJsonTranslator::m_checkQuota)
        .def_readwrite("m_smartRetry", &NormalJsonTranslator::m_smartRetry)
        .def_readwrite("m_retransAllWhenFail", &NormalJsonTranslator::m_retransAllWhenFail)
        .def_readwrite("m_usePreDictInName", &NormalJsonTranslator::m_usePreDictInName)
        .def_readwrite("m_usePostDictInName", &NormalJsonTranslator::m_usePostDictInName)
        .def_readwrite("m_usePreDictInMsg", &NormalJsonTranslator::m_usePreDictInMsg)
        .def_readwrite("m_usePostDictInMsg", &NormalJsonTranslator::m_usePostDictInMsg)
        .def_readwrite("m_useGptDictToReplaceName", &NormalJsonTranslator::m_useGptDictToReplaceName)
        .def_readwrite("m_outputWithSrc", &NormalJsonTranslator::m_outputWithSrc)
        .def_readwrite("m_agentEnabled", &NormalJsonTranslator::m_agentEnabled)
        .def_readwrite("m_reuseRepeatedBlocks", &NormalJsonTranslator::m_reuseRepeatedBlocks)
        .def_readwrite("m_apiStrategy", &NormalJsonTranslator::m_apiStrategy)
        .def_readwrite("m_sortMethod", &NormalJsonTranslator::m_sortMethod)
        .def_readwrite("m_splitFileMethod", &NormalJsonTranslator::m_splitFileMethod)
        .def_readwrite("m_problemOverviewFormat", &NormalJsonTranslator::m_problemOverviewFormat)
        .def_readwrite("m_splitFileNum", &NormalJsonTranslator::m_splitFileNum)
        .def_readwrite("m_repeatedBlockMinSize", &NormalJsonTranslator::m_repeatedBlockMinSize)
        .def_readwrite("m_cacheSearchDistance", &NormalJsonTranslator::m_cacheSearchDistance)
        .def_readwrite("m_linebreakSymbol", &NormalJsonTranslator::m_linebreakSymbol)
        .def_readwrite("m_agentMaxTurnsPerChunk", &NormalJsonTranslator::m_agentMaxTurnsPerChunk)
        .def_readwrite("m_agentCompactContextThresholdBytes", &NormalJsonTranslator::m_agentCompactContextThresholdBytes)
        .def_readwrite("m_agentSearchResultLimit", &NormalJsonTranslator::m_agentSearchResultLimit)
        .def_readwrite("m_agentContextLinesLimit", &NormalJsonTranslator::m_agentContextLinesLimit)
        .def_readwrite("m_splitFileEnabled", &NormalJsonTranslator::m_splitFileEnabled)
        .def_readwrite("m_splitFilePartsToJson", &NormalJsonTranslator::m_splitFilePartsToJson)
        .def_readwrite("m_jsonToSplitFileParts", &NormalJsonTranslator::m_jsonToSplitFileParts)
        .def_readwrite("m_gptDictionaryPaths", &NormalJsonTranslator::m_gptDictionaryPaths)
        .def_readwrite("m_agentProjectNotePath", &NormalJsonTranslator::m_agentProjectNotePath)
        .def_readwrite("m_nameMap", &NormalJsonTranslator::m_nameMap)
        .def_readwrite("m_currentRunRelFilePaths", &NormalJsonTranslator::m_currentRunRelFilePaths)
        .def_readwrite("m_inputJsonMap", &NormalJsonTranslator::m_inputJsonMap)
        .def_readwrite("m_savedTranslCacheMap", &NormalJsonTranslator::m_savedTranslCacheMap)
        .def_readwrite("m_repeatedBlockCompletedRelFilePaths", &NormalJsonTranslator::m_repeatedBlockCompletedRelFilePaths)
        .def_readwrite("m_repeatedBlockReferenceCount", &NormalJsonTranslator::m_repeatedBlockReferenceCount)
        .def_readwrite("m_onFileProcessed", &NormalJsonTranslator::m_onFileProcessed)
        .def_readwrite("m_onPerformApi", &NormalJsonTranslator::m_onPerformApi)
        .def_readwrite("m_onDictProcessed", &NormalJsonTranslator::m_onDictProcessed)
        .def_property("m_threadPool", [](NormalJsonTranslator& self) -> ctpl::thread_pool& 
            { return self.m_threadPool; }, nullptr, py::return_value_policy::reference_internal)
        .def_property_readonly("m_apiPool", [](NormalJsonTranslator& self) -> ApiPool*
            { return self.m_apiPool.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_gptDictionary", [](NormalJsonTranslator& self) -> GptDictionary* 
            { return self.m_gptDictionary.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_preDictionary", [](NormalJsonTranslator& self) -> NormalDictionary* 
            { return self.m_preDictionary.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_postDictionary", [](NormalJsonTranslator& self) -> NormalDictionary* 
            { return self.m_postDictionary.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_problemAnalyzer", [](NormalJsonTranslator& self) -> ProblemAnalyzer* 
            { return self.m_problemAnalyzer.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_nameTranslator", [](NormalJsonTranslator& self) -> NameTranslator* 
            { return self.m_nameTranslator.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_dictionaryGenerator", [](NormalJsonTranslator& self) -> DictionaryGenerator* 
            { return self.m_dictionaryGenerator.get(); }, py::return_value_policy::reference_internal)
        .def_property_readonly("m_transAgent", [](NormalJsonTranslator& self) -> NormalJsonTranslatorTransAgent* 
            { return self.m_transAgent.get(); }, py::return_value_policy::reference_internal)
        .def("preProcess", &NormalJsonTranslator::preProcess)
        .def("postProcess", &NormalJsonTranslator::postProcess)
        .def("processFile", &NormalJsonTranslator::processFile, py::call_guard<py::gil_scoped_release>())
        .def("resolveRepeatedBlockReferences", &NormalJsonTranslator::resolveRepeatedBlockReferences)
        .def("normalJsonInit", &NormalJsonTranslator::normalJsonInit)
        .def("normalJsonBeforeRun", &NormalJsonTranslator::normalJsonBeforeRun)
        .def("normalJsonProcessFiles", &NormalJsonTranslator::normalJsonProcessFiles, py::call_guard<py::gil_scoped_release>())
        .def("normalJsonProcess", &NormalJsonTranslator::normalJsonProcess, py::call_guard<py::gil_scoped_release>())
        .def("normalJsonAfterRun", &NormalJsonTranslator::normalJsonAfterRun)
        .def("normalJsonRun", [](NormalJsonTranslator& self) { self.NormalJsonTranslator::run(); },
            py::call_guard<py::gil_scoped_release>());

    py::class_<EpubTextNodeInfo>(m, "EpubTextNodeInfo")
        .def(py::init<>())
        .def_readwrite("offset", &EpubTextNodeInfo::offset)
        .def_readwrite("length", &EpubTextNodeInfo::length);

    py::class_<JsonInfo>(m, "JsonInfo")
        .def(py::init<>())
        .def_readwrite("metadata", &JsonInfo::metadata)
        .def_readwrite("htmlPath", &JsonInfo::htmlPath)
        .def_readwrite("epubPath", &JsonInfo::epubPath)
        .def_readwrite("normalPostPath", &JsonInfo::normalPostPath)
        .def_readwrite("content", &JsonInfo::content);

    py::class_<EpubTranslator, NormalJsonTranslator>(m, "EpubTranslator")
        .def_readwrite("m_epubInputDir", &EpubTranslator::m_epubInputDir)
        .def_readwrite("m_epubOutputDir", &EpubTranslator::m_epubOutputDir)
        .def_readwrite("m_tempUnpackDir", &EpubTranslator::m_tempUnpackDir)
        .def_readwrite("m_tempRebuildDir", &EpubTranslator::m_tempRebuildDir)
        .def_readwrite("m_bilingualOutput", &EpubTranslator::m_bilingualOutput)
        .def_readwrite("m_originalTextColor", &EpubTranslator::m_originalTextColor)
        .def_readwrite("m_originalTextScale", &EpubTranslator::m_originalTextScale)
        .def_readwrite("m_jsonToInfoMap", &EpubTranslator::m_jsonToInfoMap)
        .def_readwrite("m_epubToJsonsMap", &EpubTranslator::m_epubToJsonsMap)
        .def("epubInit", &EpubTranslator::epubInit)
        .def("epubBeforeRun", &EpubTranslator::epubBeforeRun)
        .def("epubRun", [](EpubTranslator& self) { self.EpubTranslator::run(); });

    py::class_<PDFTranslator, NormalJsonTranslator>(m, "PDFTranslator")
        .def_readwrite("m_pdfInputDir", &PDFTranslator::m_pdfInputDir)
        .def_readwrite("m_pdfOutputDir", &PDFTranslator::m_pdfOutputDir)
        .def_readwrite("m_bilingualOutput", &PDFTranslator::m_bilingualOutput)
        .def_readwrite("m_babeldocLangOut", &PDFTranslator::m_babeldocLangOut)
        .def_readwrite("m_jsonToPDFPathMap", &PDFTranslator::m_jsonToPDFPathMap)
        .def("pdfInit", &PDFTranslator::pdfInit)
        .def("pdfBeforeRun", &PDFTranslator::pdfBeforeRun)
        .def("pdfRun", [](PDFTranslator& self) { self.PDFTranslator::run(); });

}
