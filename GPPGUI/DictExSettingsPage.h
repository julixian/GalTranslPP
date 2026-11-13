#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

namespace fs = std::filesystem;

class DictExSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictExSettingsPage(tomlkit::Item& globalConfig, tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    void refreshCommonDictsList();

private:

    void setupUi();
    tomlkit::Item& m_globalConfig;
    tomlkit::Item& m_projectConfig;

    std::function<void()> m_refreshCommonDictsListFunc;
};
