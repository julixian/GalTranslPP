#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <toml++/toml.hpp>
#include <functional>
#include "BasePage.h"

class SettingPage : public BasePage
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit SettingPage(toml::table& globalConfig, QWidget* parent = nullptr);
    ~SettingPage() override;
    void apply2Config();

private:

    toml::table& _globalConfig;
    std::function<void()> _applyFunc;
};

#endif // SETTINGPAGE_H
