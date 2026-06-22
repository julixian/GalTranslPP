#ifndef PROMPTSSETTINGSPAGE_H
#define PROMPTSSETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "GptDictModel.h"
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class PromptSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PromptSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~PromptSettingsPage() override;

private:

    void setupUi();
    toml::ordered_value m_promptConfig;
    toml::ordered_value& m_projectConfig;
    fs::path& m_projectDir;

};

#endif // PROMPTSSETTINGSPAGE_H
