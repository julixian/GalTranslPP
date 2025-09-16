#ifndef DICTEXSETTINGSPAGE_H
#define DICTEXSETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"

namespace fs = std::filesystem;

class DictExSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictExSettingsPage(toml::table& globalConfig, toml::table& projectConfig, QWidget* parent = nullptr);
    ~DictExSettingsPage() override;
    void refreshCommonDictsList();

private:

    void _setupUI();
    toml::table& _globalConfig;
    toml::table& _projectConfig;

    std::function<void()> _refreshCommonDictsListFunc;
};

#endif // DICTEXSETTINGSPAGE_H