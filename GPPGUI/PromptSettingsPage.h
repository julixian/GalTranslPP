#pragma once

#include "BasePage.h"
#include "NormalDictModel.h"
#include "GppTomlFwd.h"
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

class PromptSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PromptSettingsPage(fs::path& projectDir, tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    ~PromptSettingsPage() override;

private:

    void setupUi();
    std::unique_ptr<tomlkit::Item> m_promptConfig;
    tomlkit::Item& m_projectConfig;
    fs::path& m_projectDir;

};
