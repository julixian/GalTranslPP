module;

#define PYBIND11_HEADERS
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

namespace pybind11::detail
{
    template <typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
    struct type_caster<absl::flat_hash_map<Key, Value, Hash, Equal, Alloc>>
        : map_caster<absl::flat_hash_map<Key, Value, Hash, Equal, Alloc>, Key, Value> {};
}

static fs::path s_pythonExePath;

// PythonMainInterpreterManager
PythonMainInterpreterManager::PythonMainInterpreterManager() {
    if (Py_IsInitialized()) {
        m_daemonThread = std::thread(&PythonMainInterpreterManager::daemonThreadFunc, this);
    }
    else {
        throw std::runtime_error("Python 环境未初始化");
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


template <typename T>
void pythonMainInterpreterObjDeleter(T* ptr) {
    auto deleteTaskFunc = [ptr]()
        {
            delete ptr;
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(deleteTaskFunc));
}

// 最理想的情况当然是把 NLP 函数也放在子解释器里运行，但这些 NLP 模块都很娇气，不是在主解释里的导入就会崩溃。。。
std::shared_ptr<py::object> PythonMainInterpreterManager::registerNLPFunction
(const std::string& moduleName, const std::string& modelName, const std::shared_ptr<spdlog::logger>& logger, bool& needReboot) {

    std::lock_guard<std::mutex> lock(m_mutex);
    if (const auto moduleFuncLocked = m_nlpModuleFunctions[moduleName][modelName].lock()) {
        logger->debug("模块 {} 的模型 {} 已在内存中，直接获取", moduleName, modelName);
        return moduleFuncLocked;
    }

    std::shared_ptr<py::object> pythonNLPModuleFunc;

    logger->info("正在加载模块 {} 的模型 {}", moduleName, modelName);
    auto loadModelTaskFunc = [&]()
        {
            try {
                py::module_ nlpModule = py::module_::import(moduleName.c_str());
                bool modelInstalled = nlpModule.attr("check_model")(modelName).cast<bool>();
                if (!modelInstalled) {

                    logger->error("模块 {} 的模型 {} 未安装，正在尝试安装", moduleName, modelName);
                    const std::vector<std::string> installArgs = nlpModule.attr("get_download_command")(modelName).cast<std::vector<std::string>>();
                    const std::string installCommand = installArgs | std::views::join_with(' ') | std::ranges::to<std::string>();

                    // 这个安装时间长，搞个窗口出来显示进度条吧
                    logger->info("将在 3s 后开始安装模型，请勿关闭接下来出现的窗口！");
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    logger->info("正在执行安装命令: {}", installCommand);

                    if (!executeCommand(s_pythonExePath.wstring(), ascii2Wide(installCommand))) {
                        throw std::runtime_error(std::format("安装模型 {} 的命令失败", modelName));
                    }

                    modelInstalled = nlpModule.attr("check_model")(modelName).cast<bool>();
                    if (modelInstalled) {
                        logger->info("模块 {} 的模型 {} 安装成功", moduleName, modelName);
                        m_nlpModuleFunctions[moduleName].insert_or_assign(modelName, std::weak_ptr<py::object>{});
                        // 不重启会找不到新下载的模型，因为已导入的 nlp 库模块不会 reload
                        // 理论上强制 reload 一遍 nlp 库也可以不用重启，不过保险起见
                        needReboot = true;
                        return;
                    }
                    else {
                        throw std::runtime_error(std::format("模块 {} 的模型 {} 安装失败", moduleName, modelName));
                    }
                }
                pythonNLPModuleFunc = std::shared_ptr<py::object>(new py::object{ nlpModule.attr("NLPProcessor")(modelName).attr("process_text") },
                    pythonMainInterpreterObjDeleter<py::object>);
                m_nlpModuleFunctions[moduleName].insert_or_assign(modelName, std::weak_ptr<py::object>{pythonNLPModuleFunc});
            }
            catch (const py::error_already_set& e) {
                throw std::runtime_error(std::format("加载模块 {} 的模型 {} 时出现异常: {}", moduleName, modelName, e.what()));
            }
        };
    this->submitTask(std::move(loadModelTaskFunc)).get();
    logger->debug("模块 {} 的模型 {} 已加载", moduleName, modelName);
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
        throw std::runtime_error(std::format("import gpp_plugin_api 时出现异常: {}", e.what()));
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
                    std::runtime_error(std::format("PythonMainInterpreterManager exception: {}", e.what()))
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
            catch (...) {}
        };
    PythonMainInterpreterManager::getInstance().submitTask(std::move(createSubInterpreterTaskFunc)).get();
    if (subInterpreter) {
        daemonThread = std::thread(&PythonInterpreterInstance::daemonThreadFunc, this);
    }
}

PythonInterpreterInstance::~PythonInterpreterInstance() {
    auto functionClearTaskFunc = [this]()
        {
            this->functions.clear(); // noexcept
        };
    this->submitTask(std::move(functionClearTaskFunc)).get();
    m_taskQueue.stop();
    if (daemonThread.joinable()) {
        daemonThread.join();
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
        py::module_::import("sys").attr("path").attr("append")(wide2Ascii(fs::absolute(L"BaseConfig/pyScripts")));
        py::module_::import("gpp_plugin_api");
    }
    catch (const py::error_already_set& e) {
        throw std::runtime_error(std::format("import gpp_plugin_api 时出现异常: {}", e.what()));
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
                    std::runtime_error(std::format("PythonInterpreterInstance exception: {}", e.what()))
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
        m_logger->error("Script is not found: {}", modulePath);
        return std::nullopt;
    }

    const std::string moduleName = wide2Ascii(stdModulePath.stem());
    auto it = m_interpreters.find(stdModulePath);
    if (it == m_interpreters.end()) {

        auto pythonInterpreter = std::make_shared<PythonInterpreterInstance>();
        if (!pythonInterpreter->isEffective()) {
            throw std::runtime_error(std::format("加载模块 {} 时出现异常，子解释器无法开启", moduleName));
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
                    throw std::runtime_error(std::format("为模块 {} 加载自定义类型时出现异常: {}", moduleName, e.what()));
                }
            }).get();
        const auto [retIt, inserted] = m_interpreters.insert({ stdModulePath, pythonInterpreter });
        if (inserted) {
            it = retIt;
        }
        else {
            throw std::runtime_error(std::format("模块 {} 插入失败", moduleName));
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
                        m_logger->debug("Failed to load function {} from script {}", functionName, modulePath);
                        return;
                    }
                    auto pFunc = std::make_unique<py::object>(pythonModule.attr(functionName.c_str()));
                    if (const py::object& func = *pFunc; !func || !py::isinstance<py::function>(func)) {
                        m_logger->debug("Failed to load function {} from script {}", functionName, modulePath);
                        return;
                    }
                    pythonInterpreter->functions.insert({ functionName, std::move(pFunc) });
                    success = true;
                }
                catch (const py::error_already_set& e) {
                    throw std::runtime_error(std::format("加载模块 {} 的函数 {} 时出现异常: {}", moduleName, functionName, e.what()));
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
            const std::string useTokenizerFlag = mode + "useTokenizer";
            if (py::hasattr(pythonModule, useTokenizerFlag.c_str()) && pythonModule.attr(useTokenizerFlag.c_str()).cast<bool>()) {
                const std::string tokenizerBackend = pythonModule.attr((mode + "tokenizerBackend").c_str()).cast<std::string>();
                if (tokenizerBackend == "MeCab") {
                    const std::string mecabDictDir = pythonModule.attr((mode + "mecabDictDir").c_str()).cast<std::string>();
                    m_logger->info("{} 已配置 MeCab 分词器，首次使用时加载。", moduleName);
                    pythonModule.attr((mode + "tokenizeFunc").c_str()) = getMeCabTokenizeFunc(mecabDictDir, m_logger);
                }
                else if (tokenizerBackend == "spaCy") {
                    const std::string spaCyModelName = pythonModule.attr((mode + "spaCyModelName").c_str()).cast<std::string>();
                    m_logger->info("{} 已配置 spaCy 分词器，首次使用时加载。", moduleName);
                    pythonModule.attr((mode + "tokenizeFunc").c_str()) = getNLPTokenizeFunc({ "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger);
                }
                else if (tokenizerBackend == "Stanza") {
                    const std::string stanzaLang = pythonModule.attr((mode + "stanzaLang").c_str()).cast<std::string>();
                    m_logger->info("{} 已配置 Stanza 分词器，首次使用时加载。", moduleName);
                    pythonModule.attr((mode + "tokenizeFunc").c_str()) = getNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza", stanzaLang, m_logger);
                }
                else if (tokenizerBackend == "pkuseg") {
                    m_logger->info("{} 已配置 pkuseg 分词器，首次使用时加载。", moduleName);
                    pythonModule.attr((mode + "tokenizeFunc").c_str()) = getNLPTokenizeFunc({ "setuptools", "nes-py", "cython", "pkuseg" }, "tokenizer_pkuseg", "default", m_logger);
                }
                else {
                    throw std::invalid_argument(std::format("{} 中注册了无效的 tokenizerBackend: {}", moduleName, tokenizerBackend));
                }
            }
        };
    setupTokenizer("sourceLang_");
    setupTokenizer("targetLang_");
    pythonModule.attr("logger") = m_logger;
}




void checkPythonDependencies(const std::vector<std::string>& dependencies, const std::shared_ptr<spdlog::logger>& logger)
{
    for (const auto& dependency : dependencies) {
        logger->debug("正在检查依赖 {}", dependency);
        auto checkDependencyTaskFunc = [&]()
            {
                try {
                    py::module_::import("importlib.metadata").attr("version")(dependency);
                    logger->debug("依赖 {} 已安装", dependency);
                }
                catch (const py::error_already_set& e) {

                    if (!e.matches(py::module_::import("importlib.metadata").attr("PackageNotFoundError"))) {
                        throw std::runtime_error(std::format("检查依赖 {} 时出现异常: {}", dependency, e.what()));
                    }

                    logger->error("依赖 {} 未安装，正在尝试安装", dependency);
                    const std::string installCommand = "-m pip install " + dependency;
                    logger->info("将在 3s 后开始安装依赖，请勿关闭接下来出现的窗口！");
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    logger->info("正在执行安装命令: {}", installCommand);

                    if (!executeCommand(s_pythonExePath.wstring(), ascii2Wide(installCommand))) {
                        throw std::runtime_error(std::format("安装依赖 {} 的命令失败", dependency));
                    }

                    try {
                        py::module_::import("importlib.metadata").attr("version")(dependency);
                        logger->info("依赖 {} 安装成功", dependency);
                    }
                    catch (const py::error_already_set& eReCheck) {
                        throw std::runtime_error(std::format("依赖 {} 安装验证失败: {}", dependency, eReCheck.what()));
                    }
                }
            };
        PythonMainInterpreterManager::getInstance().submitTask(std::move(checkDependencyTaskFunc)).get();
        logger->debug("依赖 {} 检查完毕", dependency);
    }
    logger->debug("所有依赖均已安装");
}




// 开启关闭 Python 解释器
static const fs::path pythonSysPathsTxtPath = L"BaseConfig/pythonSysPaths.txt";
bool startUpPythonEnv(const fs::path& pyEnvPath, std::unique_ptr<py::gil_scoped_release>& release) {
    if (fs::exists(pyEnvPath) && fs::exists(pyEnvPath / L"python.exe")) {

        const fs::path envZipPath = [&]()
	        {
                for (const auto& entry : fs::directory_iterator(pyEnvPath)) {
                    if (isSameExtension(entry.path(), L".zip") && str2Lower(entry.path().filename().wstring()).starts_with(L"python")) {
                        return entry.path();
                    }
                }
                return fs::path{};
            }();

        if (!envZipPath.empty()) {
            s_pythonExePath = fs::canonical(pyEnvPath / L"python.exe");
            PyConfig config;
            PyConfig_InitPythonConfig(&config);
            PyConfig_SetString(&config, &config.home, fs::canonical(pyEnvPath).c_str());
            PyConfig_SetString(&config, &config.executable, fs::canonical(pyEnvPath / L"python.exe").c_str());
            PyConfig_SetString(&config, &config.pythonpath_env, envZipPath.c_str());
            py::initialize_interpreter(&config);
            {
                py::module_::import("importlib.metadata");
                py::module_::import("sys").attr("path").attr("append")(wide2Ascii(fs::absolute(L"BaseConfig/pyScripts")));
                py::list sysPaths = py::module_::import("sys").attr("path");
                std::ofstream ofs(pythonSysPathsTxtPath);
                if (ofs.is_open()) {
                    for (const auto& path : sysPaths) {
                        ofs << path.cast<std::string>() << "\n";
                    }
                }
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
PYBIND11_EMBEDDED_MODULE(gpp_plugin_api, m, py::multiple_interpreters::per_interpreter_gil()) {

    m.doc() = "C++ API for Python-based plugins";

    py::enum_<NameType>(m, "NameType")
        .value("None", NameType::None)
        .value("Single", NameType::Single)
        .value("Multiple", NameType::Multiple)
        .export_values(); // 允许在 Python 中直接使用 gpp_plugin_api.Single 这样的形式

    py::enum_<TransEngine>(m, "TransEngine")
        .value("None", TransEngine::None)
        .value("ForGalJson", TransEngine::ForGalJson)
        .value("ForGalTsv", TransEngine::ForGalTsv)
        .value("ForNovelTsv", TransEngine::ForNovelTsv)
        .value("DeepseekJson", TransEngine::DeepseekJson)
        .value("Sakura", TransEngine::Sakura)
        .value("DumpName", TransEngine::DumpName)
        .value("NameTrans", TransEngine::NameTrans)
        .value("GenDict", TransEngine::GenDict)
        .value("Rebuild", TransEngine::Rebuild)
        .value("ShowNormal", TransEngine::ShowNormal)
        .export_values();

    py::class_<SentencePosition>(m, "SentencePosition")
        .def(py::init<>())
        .def_readwrite("file", &SentencePosition::file)
        .def_readwrite("index", &SentencePosition::index);

    // 绑定 Sentence 结构体
    // 如果指针是 nullptr，在 Python 中会是 None。
    py::class_<Sentence>(m, "Sentence")
        .def(py::init<>()) // 允许在 Python 中创建实例: s = gpp_plugin_api.Sentence()
        .def_readwrite("index", &Sentence::index)
        .def_readwrite("fileName", &Sentence::fileName)
        .def_readwrite("name", &Sentence::name)
        .def_readwrite("names", &Sentence::names) // std::vector<string> <=> list[str]
        .def_readwrite("name_preview", &Sentence::name_preview)
        .def_readwrite("names_preview", &Sentence::names_preview)
        .def_readwrite("original_text", &Sentence::original_text)
        .def_readwrite("pre_processed_text", &Sentence::pre_processed_text)
        .def_readwrite("pre_translated_text", &Sentence::pre_translated_text)
        .def_readwrite("problems", &Sentence::problems)
        .def_readwrite("translated_by", &Sentence::translated_by)
        .def_readwrite("translated_preview", &Sentence::translated_preview)
        .def_readwrite("originalLinebreak", &Sentence::originalLinebreak)
        .def_readwrite("other_info", &Sentence::other_info) // std::map<string, string> <=> dict[str, str]
        .def_readwrite("repeatedBlockRefTo", &Sentence::repeatedBlockRefTo)
        .def_readwrite("repeatedBlockRefBy", &Sentence::repeatedBlockRefBy)
        .def_readwrite("nameType", &Sentence::nameType)
        .def_readwrite("prev", &Sentence::prev) // Sentence* <=> Sentence or None
        .def_readwrite("next", &Sentence::next) // Sentence* <=> Sentence or None
        .def_readwrite("complete", &Sentence::complete)
        .def_readwrite("notAnalyzeProblem", &Sentence::notAnalyzeProblem)
        .def_readwrite("repeatedBlockRefPending", &Sentence::repeatedBlockRefPending)
        .def("problems_get_by_index", &Sentence::problems_get_by_index)
        .def("problems_set_by_index", &Sentence::problems_set_by_index);

    py::enum_<spdlog::level::level_enum>(m, "LogLevel")
        .value("trace", spdlog::level::trace)
        .value("debug", spdlog::level::debug)
        .value("info", spdlog::level::info)
        .value("warn", spdlog::level::warn)
        .value("err", spdlog::level::err)
        .value("critical", spdlog::level::critical)
        .export_values();

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

    py::module_ utilsSubmodule = m.def_submodule("utils", "A submodule for utility functions");

    utilsSubmodule
        .def("getConsoleWidth", &getConsoleWidth)
        .def("removePunctuation", &removePunctuation)
        .def("removeWhitespace", &removeWhitespace)
        .def("getMostCommonChar", &getMostCommonChar)
        .def("splitIntoTokens", &splitIntoTokens)
        .def("splitIntoGraphemes", &splitIntoGraphemes)
        .def("extractKatakana", &extractKatakana)
        .def("extractKana", &extractKana)
        .def("extractLatin", &extractLatin)
        .def("extractHangul", &extractHangul)
        .def("extractCJK", &extractCJK)
        .def("getTraditionalChineseExtractor", &getTraditionalChineseExtractor)
        .def("isApiTranslationEngine", &isApiTranslationEngine);

    py::class_<RuntimeSuccessEvent>(m, "RuntimeSuccessEvent")
        .def(py::init<>())
        .def_readwrite("timestamp", &RuntimeSuccessEvent::timestamp)
        .def_readwrite("filename", &RuntimeSuccessEvent::filename)
        .def_readwrite("index", &RuntimeSuccessEvent::index)
        .def_readwrite("speakers", &RuntimeSuccessEvent::speakers)
        .def_readwrite("sourcePreview", &RuntimeSuccessEvent::sourcePreview)
        .def_readwrite("translationPreview", &RuntimeSuccessEvent::translationPreview)
        .def_readwrite("translatedBy", &RuntimeSuccessEvent::translatedBy);

    py::class_<RuntimeErrorEvent>(m, "RuntimeErrorEvent")
        .def(py::init<>())
        .def_readwrite("timestamp", &RuntimeErrorEvent::timestamp)
        .def_readwrite("kind", &RuntimeErrorEvent::kind)
        .def_readwrite("level", &RuntimeErrorEvent::level)
        .def_readwrite("message", &RuntimeErrorEvent::message)
        .def_readwrite("filename", &RuntimeErrorEvent::filename)
        .def_readwrite("indexRange", &RuntimeErrorEvent::indexRange)
        .def_readwrite("retryCount", &RuntimeErrorEvent::retryCount)
        .def_readwrite("model", &RuntimeErrorEvent::model)
        .def_readwrite("sleepSeconds", &RuntimeErrorEvent::sleepSeconds);

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
        .def_property("m_workersActive", [](IController& self) { return self.m_workersActive.load(); },
            [](IController& self, int val) { self.m_workersActive = val; })
        .def_property("m_workersConfigured", [](IController& self) { return self.m_workersConfigured.load(); },
            [](IController& self, int val) { self.m_workersConfigured = val; })
        .def("makeBar", &IController::makeBar)
        .def("writeLog", &IController::writeLog)
        .def("addThreadNum", &IController::addThreadNum)
        .def("reduceThreadNum", &IController::reduceThreadNum)
        .def("updateBar", &IController::updateBar, py::arg("ticks") = 1)
        .def("setRuntimeFiles", &IController::setRuntimeFiles)
        .def("setRuntimeStage", &IController::setRuntimeStage, py::arg("stage"), py::arg("currentFile") = std::string{})
        .def("recordFileSentenceDone", &IController::recordFileSentenceDone)
        .def("recordRuntimeSuccess", &IController::recordRuntimeSuccess)
        .def("recordRuntimeError", &IController::recordRuntimeError)
        .def("shouldStop", &IController::shouldStop)
        .def("flush", &IController::flush);

    // ITranslator
    py::class_<ITranslator>(m, "ITranslator")
        // 不要绑定构造函数，因为 Python 不应该创建这个接口的实例
        .def("run", &ITranslator::run);

    py::class_<ctpl::thread_pool>(m, "ThreadPool")
        .def("resize", &ctpl::thread_pool::resize)
        .def("size", &ctpl::thread_pool::size);

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
        .def_readwrite("m_backgroundTextCachePath", &NormalJsonTranslator::m_backgroundTextCachePath)
        .def_readwrite("m_projectDir", &NormalJsonTranslator::m_projectDir)
        .def_readwrite("m_agentRootDir", &NormalJsonTranslator::m_agentRootDir)
        .def_readwrite("m_agentTermLedgerPath", &NormalJsonTranslator::m_agentTermLedgerPath)
        .def_readwrite("m_agentFileNotesDir", &NormalJsonTranslator::m_agentFileNotesDir)
        .def_readwrite("m_agentSearchCatalogPath", &NormalJsonTranslator::m_agentSearchCatalogPath)
        .def_readwrite("m_backgroundTextCacheMap", &NormalJsonTranslator::m_backgroundTextCacheMap)
        .def_readwrite("m_systemPrompt", &NormalJsonTranslator::m_systemPrompt)
        .def_readwrite("m_userPrompt", &NormalJsonTranslator::m_userPrompt)
        .def_readwrite("m_agentSystemPrompt", &NormalJsonTranslator::m_agentSystemPrompt)
        .def_readwrite("m_agentUserPrompt", &NormalJsonTranslator::m_agentUserPrompt)
        .def_readwrite("m_genDictReviewSystemPrompt", &NormalJsonTranslator::m_genDictReviewSystemPrompt)
        .def_readwrite("m_genDictReviewUserPrompt", &NormalJsonTranslator::m_genDictReviewUserPrompt)
        .def_readwrite("m_targetLang", &NormalJsonTranslator::m_targetLang)
        .def_readwrite("m_pythonTranslator", &NormalJsonTranslator::m_pythonTranslator)
        .def_readwrite("m_threadsNum", &NormalJsonTranslator::m_threadsNum)
        .def_readwrite("m_nameTransBatchSize", &NormalJsonTranslator::m_nameTransBatchSize)
        .def_readwrite("m_batchSize", &NormalJsonTranslator::m_batchSize)
        .def_readwrite("m_contextHistorySize", &NormalJsonTranslator::m_contextHistorySize)
        .def_readwrite("m_maxRetries", &NormalJsonTranslator::m_maxRetries)
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
        .def_readwrite("m_useRepeatedBlockInputCache", &NormalJsonTranslator::m_useRepeatedBlockInputCache)
        .def_readwrite("m_apiStrategy", &NormalJsonTranslator::m_apiStrategy)
        .def_readwrite("m_sortMethod", &NormalJsonTranslator::m_sortMethod)
        .def_readwrite("m_splitFile", &NormalJsonTranslator::m_splitFile)
        .def_readwrite("m_splitFileNum", &NormalJsonTranslator::m_splitFileNum)
        .def_readwrite("m_repeatedBlockMinSize", &NormalJsonTranslator::m_repeatedBlockMinSize)
        .def_readwrite("m_cacheSearchDistance", &NormalJsonTranslator::m_cacheSearchDistance)
        .def_readwrite("m_linebreakSymbol", &NormalJsonTranslator::m_linebreakSymbol)
        .def_readwrite("m_agentMaxTurnsPerChunk", &NormalJsonTranslator::m_agentMaxTurnsPerChunk)
        .def_readwrite("m_agentSoftContextChars", &NormalJsonTranslator::m_agentSoftContextChars)
        .def_readwrite("m_agentHardContextChars", &NormalJsonTranslator::m_agentHardContextChars)
        .def_readwrite("m_agentSearchResultLimit", &NormalJsonTranslator::m_agentSearchResultLimit)
        .def_readwrite("m_needsCombining", &NormalJsonTranslator::m_needsCombining)
        .def_readwrite("m_splitFilePartsToJson", &NormalJsonTranslator::m_splitFilePartsToJson)
        .def_readwrite("m_jsonToSplitFileParts", &NormalJsonTranslator::m_jsonToSplitFileParts)
        .def_readwrite("m_agentKnownRelFiles", &NormalJsonTranslator::m_agentKnownRelFiles)
        .def_readwrite("m_agentDictionaryPaths", &NormalJsonTranslator::m_agentDictionaryPaths)
        .def_readwrite("m_agentProjectInfoPath", &NormalJsonTranslator::m_agentProjectInfoPath)
        .def_readwrite("m_nameMap", &NormalJsonTranslator::m_nameMap)
        .def_readwrite("m_currentRunRelFilePaths", &NormalJsonTranslator::m_currentRunRelFilePaths)
        .def_readwrite("m_onFileProcessed", &NormalJsonTranslator::m_onFileProcessed)
        .def_readwrite("m_onPerformApi", &NormalJsonTranslator::m_onPerformApi)
        .def_readwrite("m_onDictProcessed", &NormalJsonTranslator::m_onDictProcessed)
        .def_property("m_threadPool", [](NormalJsonTranslator& self) -> ctpl::thread_pool& { return self.m_threadPool; }, nullptr, py::return_value_policy::reference_internal)
        .def("preProcess", &NormalJsonTranslator::preProcess)
        .def("postProcess", &NormalJsonTranslator::postProcess)
        .def("processFile", &NormalJsonTranslator::processFile)
        .def("shouldReportRuntimeWorkbench", &NormalJsonTranslator::shouldReportRuntimeWorkbench)
        .def("applyAgentRetranslateSuggestions", &NormalJsonTranslator::applyAgentRetranslateSuggestions)
        .def("resolveRepeatedBlockReferences", &NormalJsonTranslator::resolveRepeatedBlockReferences)
        .def("normalJsonInit", &NormalJsonTranslator::normalJsonInit)
        .def("normalJsonBeforeRun", &NormalJsonTranslator::normalJsonBeforeRun)
        .def("normalJsonProcessFiles", &NormalJsonTranslator::normalJsonProcessFiles)
        .def("normalJsonProcess", &NormalJsonTranslator::normalJsonProcess)
        .def("normalJsonAfterRun", &NormalJsonTranslator::normalJsonAfterRun)
        .def("normalJsonRun", [](NormalJsonTranslator& self) { self.NormalJsonTranslator::run(); });

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
        .def_readwrite("m_jsonToPDFPathMap", &PDFTranslator::m_jsonToPDFPathMap)
        .def("pdfInit", &PDFTranslator::pdfInit)
        .def("pdfBeforeRun", &PDFTranslator::pdfBeforeRun)
        .def("pdfRun", [](PDFTranslator& self) { self.PDFTranslator::run(); });

}
