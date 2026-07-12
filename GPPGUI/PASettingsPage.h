#ifndef PASETTINGSPAGE_H
#define PASETTINGSPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class PASettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PASettingsPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);

private:

    void setupUi();
    toml::ordered_value& m_projectConfig;
};

#endif
