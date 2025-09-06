// CommonSettingsPage.h

#ifndef COMMONSETTINGSPAGE_H
#define COMMONSETTINGSPAGE_H

#include <toml++/toml.hpp>
#include "BasePage.h"

class CommonSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonSettingsPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~CommonSettingsPage() override;
    void apply2Config();

private:

    void _setupUI();
    toml::table& _projectConfig;
    std::function<void()> _applyFunc;
};

#endif // COMMONSETTINGSPAGE_H
