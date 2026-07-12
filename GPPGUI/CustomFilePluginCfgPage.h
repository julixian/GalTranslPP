#ifndef CUSTOMFILEPLUGINCFGPAGE_H
#define CUSTOMFILEPLUGINCFGPAGE_H

#include "BasePage.h"
#include <toml.hpp>

namespace fs = std::filesystem;

class CustomFilePluginCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit CustomFilePluginCfgPage(fs::path& projectDir, toml::ordered_value& globalConfig,
        toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;
};

#endif
