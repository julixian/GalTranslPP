// PromptSettingsPage.h

#ifndef PROMPTSSETTINGSPAGE_H
#define PROMPTSSETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "DictionaryModel.h"
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class PromptSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PromptSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent = nullptr);
    ~PromptSettingsPage() override;

private:

    void _setupUI();
    toml::table _promptConfig;
    toml::table& _projectConfig;
    fs::path& _projectDir;

};

#endif // PROMPTSSETTINGSPAGE_H
