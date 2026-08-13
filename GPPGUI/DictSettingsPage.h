#ifndef DICTSETTINGSPAGE_H
#define DICTSETTINGSPAGE_H

#include "BasePage.h"
#include "GptDictModel.h"
#include "NormalDictModel.h"
#include <QList>
#include <filesystem>
#include <toml.hpp>

class QStackedWidget;
namespace fs = std::filesystem;

class DictSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig,
        toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    void refreshGptDict();

private:

    void setupUi();
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;

    std::function<void()> m_refreshGptDictFunc;

    QList<GptDictEntry> m_withdrawGptList;
    QList<NormalDictEntry> m_withdrawPreList;
    QList<NormalDictEntry> m_withdrawPostList;
};

#endif
