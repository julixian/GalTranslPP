module;

#define PYBIND11_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <sol/sol.hpp>

module ITranslator;

import Tool;
import NormalJsonTranslator;
import EpubTranslator;
import PDFTranslator;
import LuaTranslator;
import PythonTranslator;

namespace fs = std::filesystem;

namespace
{
    std::string compactRuntimePreview(std::string text, size_t limit)
    {
        for (char& ch : text) {
            if (ch == '\r' || ch == '\n' || ch == '\t') {
                ch = ' ';
            }
        }
        return truncateUtf8Prefix(text, limit);
    }
}

IController::IController()
{

}

IController::~IController()
{

}

void IController::makeBar(int totalSentences, int totalThreads)
{
    flush();
    m_totalSentences = totalSentences;
    m_completedSentences = 0;
    m_workersConfigured = totalThreads;
    m_workersActive = 0;
    onMakeBar(totalSentences, totalThreads);
}

void IController::addThreadNum()
{
    const int workersActive = ++m_workersActive;
    onAddThreadNum(workersActive);
}

void IController::reduceThreadNum()
{
    int workersActive = --m_workersActive;
    if (workersActive < 0) {
        m_workersActive = 0;
        workersActive = 0;
    }
    onReduceThreadNum(workersActive);
}

void IController::updateBar(int ticks)
{
    const int completed = (m_completedSentences += ticks);
    onUpdateBar(ticks, completed, m_totalSentences.load());
}

void IController::setRuntimeFiles(const std::map<std::string, int>& fileTotals)
{
    std::vector<RuntimeFileProgress> filesProgress;
    {
        std::lock_guard<std::mutex> lock(m_runtimeMutex);
        m_runtimeFiles.clear();
        for (const auto& [filename, total] : fileTotals) {
            RuntimeFileProgress progress;
            progress.filename = filename;
            progress.total = total;
            m_runtimeFiles[filename] = progress;
            filesProgress.push_back(progress);
        }
    }
    onRuntimeFilesReset(filesProgress);
}

void IController::setRuntimeStage(const std::string& stage, const std::string& currentFile)
{
    // 没什么大用，processFile 开头调用一下，用于展示最新开始翻译的文件
    onRuntimeStageChanged(stage, currentFile);
}

void IController::recordFileSentenceDone(const std::string& runtimeFile, bool hasProblem)
{
    if (runtimeFile.empty()) {
        return;
    }
    RuntimeFileProgress progress;
    {
        std::lock_guard<std::mutex> lock(m_runtimeMutex);
        RuntimeFileProgress& target = m_runtimeFiles[runtimeFile];
        target.filename = runtimeFile;
        ++target.completed;
        if (hasProblem) {
            ++target.problems;
        }
        progress = target;
    }
    onRuntimeFileProgress(progress);
}

void IController::recordRuntimeSuccess(RuntimeSuccessEvent event)
{
    event.sourcePreview = compactRuntimePreview(std::move(event.sourcePreview), 60);
    event.translationPreview = compactRuntimePreview(std::move(event.translationPreview), 60);
    if (event.timestamp.empty()) {
        event.timestamp = nowTimestampString();
    }
    onRuntimeSuccess(event);
}

void IController::recordRuntimeError(RuntimeErrorEvent event)
{
    event.message = compactRuntimePreview(std::move(event.message), 180);
    if (event.timestamp.empty()) {
        event.timestamp = nowTimestampString();
    }
    onRuntimeError(event);
}

ITranslator::ITranslator()
{

}

ITranslator::~ITranslator()
{

}


template<typename Mutex>
class ControllerSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit ControllerSink(const std::shared_ptr<IController>& controller)
        : m_controller(controller) {
    }

protected:

    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        m_controller->writeLog(fmt::to_string(formatted));
    }

    void flush_() override {
        m_controller->flush();
    }

private:
    std::shared_ptr<IController> m_controller;
};

std::unique_ptr<ITranslator> createTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller)
{
    const fs::path configFilePath = projectDir / L"config.toml";
    if (!fs::exists(configFilePath)) {
        throw std::runtime_error(gppTr("createTranslator", "找不到配置文件"));
    }
    const auto configData = toml::uparse(configFilePath);

    const std::string filePlugin = toml::find_or(configData, "plugins", "filePlugin", "NormalJson");
    const std::string transEngine = toml::find_or(configData, "plugins", "transEngine", "ForGalTsv");
    // 日志配置
    spdlog::level::level_enum logLevel;
    bool saveLog = toml::find_or(configData, "common", "saveLog", true);
    const std::string logLevelStr = toml::find_or(configData, "common", "logLevel", "info");
    if (logLevelStr == "trace") {
        logLevel = spdlog::level::trace;
    }
    else if (logLevelStr == "debug") {
        logLevel = spdlog::level::debug;
    }
    else if (logLevelStr == "info") {
        logLevel = spdlog::level::info;
    }
    else if (logLevelStr == "warn") {
        logLevel = spdlog::level::warn;
    }
    else if (logLevelStr == "err") {
        logLevel = spdlog::level::err;
    }
    else if (logLevelStr == "critical") {
        logLevel = spdlog::level::critical;
    }
    else {
        throw std::runtime_error(gppTr("createTranslator", "无效的日志等级"));
    }

    constexpr size_t LOG_FILE_MAX_SIZE_DEFAULT = 1024 * 1024 * 10;
    const size_t logFileMaxSize = toml::find_or(configData, "common", "logFileMaxSize", LOG_FILE_MAX_SIZE_DEFAULT);
    const size_t maxRotateFiles = toml::find_or(configData, "common", "maxRotateFiles", 1);

    auto controllerSink = std::make_shared<ControllerSink<std::mutex>>(controller);
    std::vector<spdlog::sink_ptr> sinks = { controllerSink };
    if (saveLog) {
        fs::create_directories(projectDir / L"logs");
        for (size_t i = 5; i-- > 0;) {                      // NormalJson_4.log
            const fs::path logFilePath = projectDir / L"logs" / (ascii2Wide(transEngine) + L"_" + std::to_wstring(i) + L".log");
            const fs::path newLogFilePath = projectDir / L"logs" / (ascii2Wide(transEngine) + L"_" + std::to_wstring(i + 1) + L".log");
            if (!fs::exists(logFilePath)) {
                continue;
            }
            fs::rename(logFilePath, newLogFilePath);
        }
        const fs::path logFilePath = projectDir / L"logs" / (ascii2Wide(transEngine) + L"_0.log");
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath.wstring(), logFileMaxSize, maxRotateFiles));
    }

    const auto logger = std::make_shared<spdlog::logger>(wide2Ascii(projectDir) + "-" + transEngine + "-Logger", sinks.begin(), sinks.end());
    //spdlog::register_logger(logger);
    logger->set_level(logLevel);
    if (logLevel == spdlog::level::trace) {
        logger->flush_on(spdlog::level::trace);
    }
    else if (logLevel == spdlog::level::debug) {
        logger->flush_on(spdlog::level::debug);
    }
    logger->set_pattern("[%H:%M:%S.%e %^%l%$] %v");
    logger->info(gppTr("createTranslator", "日志器初始化完成。"));
    // 日志配置结束

    const std::string filePluginLower = str2Lower(filePlugin);
    if (filePluginLower.ends_with(".lua")) {
        const std::string baseClassName = toml::find_or(configData, "plugins", "baseClassName", "NormalJson");
        const std::string scriptFileName = replaceStr(filePlugin, "<PROJECT_DIR>", wide2Ascii(projectDir));
        if (baseClassName == "NormalJson") {
            std::unique_ptr<ITranslator> translator = std::make_unique<LuaTranslator<NormalJsonTranslator>>(
                scriptFileName, projectDir, controller, logger);
            return translator;
        }
        else if (baseClassName == "Epub") {
            std::unique_ptr<ITranslator> translator = std::make_unique<LuaTranslator<EpubTranslator>>(
                scriptFileName, projectDir, controller, logger);
            return translator;
        }
        else if (baseClassName == "PDF") {
            std::unique_ptr<ITranslator> translator = std::make_unique<LuaTranslator<PDFTranslator>>(
                scriptFileName, projectDir, controller, logger);
            return translator;
        }
        else {
            throw std::runtime_error(gppTr("createTranslator", "无效的基类名称: %1", baseClassName));
        }
    }
    else if (filePluginLower.ends_with(".py")) {
        const std::string baseClassName = toml::find_or(configData, "plugins", "baseClassName", "NormalJson");
        const std::string scriptFileName = replaceStr(filePlugin, "<PROJECT_DIR>", wide2Ascii(projectDir));
        if (baseClassName == "NormalJson") {
            std::unique_ptr<ITranslator> translator = std::make_unique<PythonTranslator<NormalJsonTranslator>>(
                scriptFileName, projectDir, controller, logger);
            return translator;
        }
        else if (baseClassName == "Epub") {
            std::unique_ptr<ITranslator> translator = std::make_unique<PythonTranslator<EpubTranslator>>(
                scriptFileName, projectDir, controller, logger);
            return translator;
        }
        else if (baseClassName == "PDF") {
            std::unique_ptr<ITranslator> translator = std::make_unique<PythonTranslator<PDFTranslator>>(
                scriptFileName, projectDir, controller, logger);
            return translator;
        }
        else {
            throw std::runtime_error(gppTr("createTranslator", "无效的基类名称: %1", baseClassName));
        }
    }
    else if (filePlugin == "NormalJson") {
        std::unique_ptr<ITranslator> translator = std::make_unique<NormalJsonTranslator>(projectDir, controller, logger);
        return translator;
    }
    else if (filePlugin == "Epub") {
        std::unique_ptr<ITranslator> translator = std::make_unique<EpubTranslator>(projectDir, controller, logger);
        return translator;
    }
    else if (filePlugin == "PDF") {
        std::unique_ptr<ITranslator> translator = std::make_unique<PDFTranslator>(projectDir, controller, logger);
        return translator;
    }

    return nullptr;
}
