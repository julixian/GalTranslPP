#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class ElaWidget;

class PASettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PASettingsPage(tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    ~PASettingsPage() override;

private:

    void setupUi();
    tomlkit::Item& m_projectConfig;
    ElaWidget* m_compareConfigWidget = nullptr;
};
