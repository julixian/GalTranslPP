// PASettingsPage.h

#ifndef PASETTINGSPAGE_H
#define PASETTINGSPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class PASettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PASettingsPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~PASettingsPage() override;

private:

    void _setupUI();
    toml::table& _projectConfig;
};

#endif // COMMONSETTINGSPAGE_H
