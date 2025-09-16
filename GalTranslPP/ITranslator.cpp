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

    if (filePlugin == "NormalJson") {
        std::unique_ptr<ITranslator> translator = std::make_unique<NormalJsonTranslator>(projectDir, controller);
        return translator;
    }
    else if (filePlugin == "Epub") {
        std::unique_ptr<ITranslator> translator = std::make_unique<EpubTranslator>(projectDir, controller);
        return translator;
    }

    return nullptr;
}