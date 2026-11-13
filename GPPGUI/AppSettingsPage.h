#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"

class AppSettingsPage : public BasePage
{
    Q_OBJECT

public:
	explicit AppSettingsPage(tomlkit::Item& globalConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void clearLogShortcutChangedSignal(const QString& shortcut);

private:
    void setupUi();

    tomlkit::Item& m_globalConfig;
};
