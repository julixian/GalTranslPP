#ifndef CUSTOMFILEPLUGINCFGPAGE_H
#define CUSTOMFILEPLUGINCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

namespace fs = std::filesystem;

class CustomFilePluginCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit CustomFilePluginCfgPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~CustomFilePluginCfgPage() override;

private:
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;
};

#endif // CUSTOMFILEPLUGINCFGPAGE_H
