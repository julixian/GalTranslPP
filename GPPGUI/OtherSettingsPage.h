#pragma once

#include "BasePage.h"
#include "GppTomlFwd.h"
#include <filesystem>

namespace fs = std::filesystem;

class OtherSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit OtherSettingsPage(fs::path& projectDir, tomlkit::Item& globalConfig,
        tomlkit::Item& projectConfig, QWidget* parent = nullptr);

Q_SIGNALS:
    void saveConfigSignal();
    void refreshProjectConfigSignal();
    void changeProjectNameSignal(const QString& newProjectName);

private:

    void setupUi();
    fs::path& m_projectDir;
    tomlkit::Item& m_globalConfig;
    tomlkit::Item& m_projectConfig;
};
