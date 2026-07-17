#ifndef APPSETTINGSPAGE_H
#define APPSETTINGSPAGE_H

#include "BasePage.h"
#include <toml.hpp>

class AppSettingsPage : public BasePage
{
    Q_OBJECT

public:
	explicit AppSettingsPage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void clearLogShortcutChangedSignal(const QString& shortcut);

private:
    void setupUi();

    toml::ordered_value& m_globalConfig;
};

#endif
