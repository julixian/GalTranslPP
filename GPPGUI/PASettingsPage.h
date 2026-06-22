#ifndef PASETTINGSPAGE_H
#define PASETTINGSPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class PASettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PASettingsPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~PASettingsPage() override;

private:

    void setupUi();
    toml::ordered_value& m_projectConfig;
};

#endif // COMMONSETTINGSPAGE_H
