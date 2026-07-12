#ifndef PROMPTSSETTINGSPAGE_H
#define PROMPTSSETTINGSPAGE_H

#include "BasePage.h"
#include "NormalDictModel.h"
#include <filesystem>
#include <toml.hpp>

namespace fs = std::filesystem;

class PromptSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PromptSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:

    void setupUi();
    toml::ordered_value m_promptConfig;
    toml::ordered_value& m_projectConfig;
    fs::path& m_projectDir;

};

#endif
