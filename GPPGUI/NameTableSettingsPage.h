#ifndef NAMETABLESETTINGSPAGE_H
#define NAMETABLESETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include <filesystem>
#include "BasePage.h"
#include "NameTableModel.h"

namespace fs = std::filesystem;

class NameTableSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit NameTableSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~NameTableSettingsPage() override;
    void refreshTable();

private:

    void setupUi();
    QList<NameTableEntry> readNameTable();
    QString readNameTableStr();
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;
    std::function<void()> m_refreshFunc;

    QList<NameTableEntry> m_withdrawList;
};

#endif // NAMETABLESETTINGSPAGE_H
