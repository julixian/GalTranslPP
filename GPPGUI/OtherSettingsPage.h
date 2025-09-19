// OtherSettingsPage.h

#ifndef OTHERSETTINGSPAGE_H
#define OTHERSETTINGSPAGE_H

#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
namespace fs = std::filesystem;

class OtherSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit OtherSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent = nullptr);
    ~OtherSettingsPage() override;

Q_SIGNALS:
    void saveConfigSignal();
    void refreshProjectConfigSignal();

private:

    void _setupUI();
    toml::table& _projectConfig;

    fs::path& _projectDir;
};

#endif // OTHERSETTINGSPAGE_H
