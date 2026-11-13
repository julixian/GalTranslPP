#pragma once

#include "BasePage.h"
#include "GptDictModel.h"
#include "NormalDictModel.h"
#include <QList>
#include <filesystem>
#include "GppTomlFwd.h"

class QStackedWidget;
namespace fs = std::filesystem;

class DictSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictSettingsPage(fs::path& projectDir, tomlkit::Item& globalConfig,
        tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    void refreshGptDict();

private:

    void setupUi();
    fs::path& m_projectDir;
    tomlkit::Item& m_globalConfig;
    tomlkit::Item& m_projectConfig;

    std::function<void()> m_refreshGptDictFunc;

    QList<GuiGptDictEntry> m_withdrawGptList;
    QList<GuiNormalDictEntry> m_withdrawPreList;
    QList<GuiNormalDictEntry> m_withdrawPostList;
};
