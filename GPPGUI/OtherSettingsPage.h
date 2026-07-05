#ifndef OTHERSETTINGSPAGE_H
#define OTHERSETTINGSPAGE_H

#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"
namespace fs = std::filesystem;

class OtherSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit OtherSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~OtherSettingsPage() override;

Q_SIGNALS:
    void saveConfigSignal();
    void refreshProjectConfigSignal();
    void changeProjectNameSignal(const QString& newProjectName);

private:

    void setupUi();
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;
};

#endif // OTHERSETTINGSPAGE_H
