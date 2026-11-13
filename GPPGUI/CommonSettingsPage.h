#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class CommonSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonSettingsPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);

private:
    void setupUi();
    tomlkit::Item& m_projectConfig;
};
