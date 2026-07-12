#ifndef OTHERSETTINGSPAGE_H
#define OTHERSETTINGSPAGE_H

#include "BasePage.h"
#include <filesystem>
#include <toml.hpp>

namespace fs = std::filesystem;

class OtherSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit OtherSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig,
        toml::ordered_value& projectConfig, QWidget* parent = nullptr);

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

#endif
