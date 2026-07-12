#ifndef DICTEXSETTINGSPAGE_H
#define DICTEXSETTINGSPAGE_H

#include "BasePage.h"
#include <toml.hpp>

namespace fs = std::filesystem;

class DictExSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictExSettingsPage(toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    void refreshCommonDictsList();

private:

    void setupUi();
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;

    std::function<void()> m_refreshCommonDictsListFunc;
};

#endif
