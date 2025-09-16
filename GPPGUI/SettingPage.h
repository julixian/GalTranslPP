#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class SettingPage : public BasePage
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit SettingPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~SettingPage() override;

private:

    toml::table& _globalConfig;
};

#endif // SETTINGPAGE_H
