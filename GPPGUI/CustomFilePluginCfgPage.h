#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"
#include <filesystem>

namespace fs = std::filesystem;

class CustomFilePluginCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit CustomFilePluginCfgPage(fs::path& projectDir, tomlkit::Item& globalConfig,
        tomlkit::Item& projectConfig, QWidget* parent = nullptr);

private:
    fs::path& m_projectDir;
    tomlkit::Item& m_globalConfig;
    tomlkit::Item& m_projectConfig;
};
