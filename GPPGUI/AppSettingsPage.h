#ifndef APPSETTINGSPAGE_H
#define APPSETTINGSPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class AppSettingsPage : public BasePage
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit AppSettingsPage(toml::ordered_value& globalConfig, QWidget* parent = nullptr);
    ~AppSettingsPage() override;

Q_SIGNALS:
    void restartPythonEnvSignal(const QString& path);

private:
    void setupUi();

    toml::ordered_value& m_globalConfig;
};

#endif // APPSETTINGSPAGE_H
