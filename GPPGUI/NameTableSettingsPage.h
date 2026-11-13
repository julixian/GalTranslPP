#pragma once

#include "BasePage.h"
#include "NameTableModel.h"
#include <QList>
#include <filesystem>
#include "GppTomlFwd.h"

namespace fs = std::filesystem;

class NameTableSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit NameTableSettingsPage(fs::path& projectDir, tomlkit::Item& globalConfig,
        tomlkit::Item& projectConfig, QWidget* parent = nullptr);
    void refreshTable();

private:

    void setupUi();
    QList<NameTableEntry> readNameTable();
    QString readNameTableStr();
    fs::path& m_projectDir;
    tomlkit::Item& m_globalConfig;
    tomlkit::Item& m_projectConfig;
    std::function<void()> m_refreshFunc;

    QList<NameTableEntry> m_withdrawList;
};
