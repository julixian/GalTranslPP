// NameTableSettingsPage.h

#ifndef NAMETABLESETTINGSPAGE_H
#define NAMETABLESETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "NameTableModel.h"

namespace fs = std::filesystem;

class NameTableSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit NameTableSettingsPage(fs::path& projectDir, toml::table& globalConfig, toml::table& projectConfig, QWidget* parent = nullptr);
    ~NameTableSettingsPage() override;
    void apply2Config();
    void refreshTable();

private:

    void _setupUI();
    QList<NameTableEntry> readNameTable();
    QString readNameTableStr();
    toml::table& _projectConfig;
    toml::table& _globalConfig;
    fs::path& _projectDir;
    std::function<void()> _applyFunc;
    std::function<void()> _refreshFunc;
};

#endif // NAMETABLESETTINGSPAGE_H
