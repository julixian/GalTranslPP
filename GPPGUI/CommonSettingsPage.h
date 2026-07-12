#ifndef COMMONSETTINGSPAGE_H
#define COMMONSETTINGSPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class CommonSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonSettingsPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:
    void setupUi();
    toml::ordered_value& m_projectConfig;
};

#endif
