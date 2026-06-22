#ifndef DICTSETTINGSPAGE_H
#define DICTSETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "GptDictModel.h"
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class QStackedWidget;

class DictSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~DictSettingsPage() override;
    void refreshDicts();

private:

    void setupUi();
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;

    std::function<void()> m_refreshFunc;

    QList<GptDictEntry> m_withdrawGptList;
    QList<NormalDictEntry> m_withdrawPreList;
    QList<NormalDictEntry> m_withdrawPostList;
};

#endif // COMMONSETTINGSPAGE_H
