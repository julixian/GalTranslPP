#ifndef COMMONSETTINGSPAGE_H
#define COMMONSETTINGSPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class CommonSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit CommonSettingsPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~CommonSettingsPage() override;

private:
    void setupUi();
    toml::ordered_value& m_projectConfig;
};

#endif // COMMONSETTINGSPAGE_H
