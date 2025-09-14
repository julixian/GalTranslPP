module;

#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>
module ITranslator;

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
    std::string transEngineStr = configData["plugins"]["transEngine"].value_or("ForGalJson");

    TransEngine transEngine;
    if (transEngineStr == "ForGalJson") {
        transEngine = TransEngine::ForGalJson;
    }
    else if (transEngineStr == "ForGalTsv") {
        transEngine = TransEngine::ForGalTsv;
    }
    else if (transEngineStr == "ForNovelTsv") {
        transEngine = TransEngine::ForNovelTsv;
    }
    else if (transEngineStr == "DeepseekJson") {
        transEngine = TransEngine::DeepseekJson;
    }
    else if (transEngineStr == "Sakura") {
        transEngine = TransEngine::Sakura;
    }
    else if (transEngineStr == "DumpName") {
        transEngine = TransEngine::DumpName;
    }
    else if (transEngineStr == "GenDict") {
        transEngine = TransEngine::GenDict;
    }
    else if (transEngineStr == "Rebuild") {
        transEngine = TransEngine::Rebuild;
    }
    else {
        throw std::runtime_error("Invalid trans engine");
    }

    if (filePlugin == "NormalJson") {
        std::unique_ptr<ITranslator> translator = std::make_unique<NormalJsonTranslator>(projectDir, transEngine, controller);
        return translator;
    }
    else if (filePlugin == "Epub") {
        std::unique_ptr<ITranslator> translator = std::make_unique<EpubTranslator>(projectDir, transEngine, controller);
        return translator;
    }

    return nullptr;
}