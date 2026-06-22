#ifndef DICTEXSETTINGSPAGE_H
#define DICTEXSETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"

namespace fs = std::filesystem;

class DictExSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit DictExSettingsPage(toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~DictExSettingsPage() override;
    void refreshCommonDictsList();

private:

    void setupUi();
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;

    std::function<void()> m_refreshCommonDictsListFunc;
};

#endif // DICTEXSETTINGSPAGE_H