module;

#include <spdlog/spdlog.h>

module IPlugin;

import std;
import Tool;
import TextPostFull2Half;
import TextLinebreakFix;
namespace fs = std::filesystem;

IPlugin::IPlugin(const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger) :
	m_projectDir(projectDir),
	m_logger(logger)
{

}

IPlugin::~IPlugin() {}



std::vector<std::shared_ptr<IPlugin>> registerPlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger) {
	std::vector<std::shared_ptr<IPlugin>> plugins;

	for (const auto& pluginName : pluginNames) {
		if (pluginName == "TextPostFull2Half") {
			plugins.push_back(std::make_shared<TextPostFull2Half>(projectDir, logger));
		}
		else if (pluginName == "TextLinebreakFix") {
			plugins.push_back(std::make_shared<TextLinebreakFix>(projectDir, logger));
		}
	}

	return plugins;
}
