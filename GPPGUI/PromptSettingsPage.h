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

class QStackedWidget;

class PromptSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PromptSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent = nullptr);
    ~PromptSettingsPage() override;
    void apply2Config();

private:

    void _setupUI();
    toml::table _promptConfig;
    toml::table& _projectConfig;
    fs::path& _projectDir;

    std::function<void()> _applyFunc;
};

#endif // PROMPTSSETTINGSPAGE_H
