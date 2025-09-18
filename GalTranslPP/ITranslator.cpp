module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

module ITranslator;

import <toml++/toml.hpp>;
import Tool;
import NormalJsonTranslator;
import EpubTranslator;

namespace fs = std::filesystem;

IController::IController()
{

}

IController::~IController()
{

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
    explicit ControllerSink(std::shared_ptr<IController> controller)
        : m_controller(controller) {
    }

protected:

    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        m_controller->writeLog(fmt::to_string(formatted));
    }

    void flush_() override {
        // 也可以在IController里再写一个虚flush，不过感觉没什么必要了
    }

private:
    std::shared_ptr<IController> m_controller;
};

std::unique_ptr<ITranslator> createTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller)
{
    fs::path configFilePath = projectDir / L"config.toml";
    if (!fs::exists(configFilePath)) {
        throw std::runtime_error("Config file not found");
    }
    std::ifstream ifs(configFilePath);
    auto configData = toml::parse(ifs);
    ifs.close();

    std::string filePlugin = configData["plugins"]["filePlugin"].value_or("NormalJson");
    // 日志配置
    spdlog::level::level_enum logLevel;
    bool saveLog = configData["common"]["saveLog"].value_or(true);
    std::string logLevelStr = configData["common"]["logLevel"].value_or("info");
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
        throw std::runtime_error("Invalid log level");
    }

    std::shared_ptr<ControllerSink<std::mutex>> controllerSink = std::make_shared<ControllerSink<std::mutex>>(controller);
    std::vector<spdlog::sink_ptr> sinks = { controllerSink };
    if (saveLog) {
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(wide2Ascii(projectDir, 0) + "/log.txt", true));
    }
    std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>(wide2Ascii(projectDir) + "-Logger", sinks.begin(), sinks.end());
    logger->set_level(logLevel);
    logger->set_pattern("[%H:%M:%S.%e %^%l%$] %v");
    logger->info("Logger initialized.");
    // 日志配置结束

    if (filePlugin == "NormalJson") {
        std::unique_ptr<ITranslator> translator = std::make_unique<NormalJsonTranslator>(projectDir, controller, logger);
        return translator;
    }
    else if (filePlugin == "Epub") {
        std::unique_ptr<ITranslator> translator = std::make_unique<EpubTranslator>(projectDir, controller, logger);
        return translator;
    }

    return nullptr;
}