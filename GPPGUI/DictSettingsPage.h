// DictSettingsPage.h

#ifndef DICTSETTINGSPAGE_H
#define DICTSETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "DictionaryModel.h"
#include "NormalDictModel.h"

namespace fs = std::filesystem;

class ElaPivot;
class QStackedWidget;

class DictSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictSettingsPage(fs::path& projectDir, toml::table& globalConfig, toml::table& projectConfig, QWidget* parent = nullptr);
    ~DictSettingsPage() override;
    void apply2Config();
    void refreshDicts();

private:
    ElaPivot* _pivot;
    QStackedWidget* _stackedWidget;

    QList<DictionaryEntry> readGptDicts();
    QString readGptDictsStr();
    QList<NormalDictEntry> readNormalDicts(const fs::path& dictPath);
    QString readNormalDictsStr(const fs::path& dictPath);

    void _setupUI();
    toml::table& _globalConfig;
    toml::table& _projectConfig;
    fs::path& _projectDir;

    std::function<void()> _applyFunc;
    std::function<void()> _refreshFunc;
};

#endif // COMMONSETTINGSPAGE_H
