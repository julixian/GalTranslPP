#ifndef PLUGINSETTINGSPAGE_H
#define PLUGINSETTINGSPAGE_H

#include <QList>
#include <toml.hpp>
#include "BasePage.h"

class QVBoxLayout;
class PluginItemWidget;
class TF2HCfgPage;
class TLFCfgPage;
class SkipTransCfgPage;

namespace fs = std::filesystem;

class PluginSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PluginSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* mainWindow, QWidget* parent = nullptr);
    ~PluginSettingsPage() override;
    virtual void apply2Config() override;

private Q_SLOTS:
    void onItemMoveUp(PluginItemWidget* item);
    void onItemMoveDown(PluginItemWidget* item);
    void onItemSettings(PluginItemWidget* item);

private:
    void setupUi();
    void updateMoveButtonStates(); // 更新所有项的上下移动按钮状态

    QVBoxLayout* m_pluginListLayout = nullptr; // 容纳所有 PluginItemWidget 的布局
    QList<PluginItemWidget*> m_pluginItems; // 按顺序存储所有项的指针

    fs::path& m_projectDir;
    toml::ordered_value& m_projectConfig;
    QWidget* m_mainWindow = nullptr;

private:
    // 以下为各个插件的设置页面
    TF2HCfgPage* m_tf2hCfgPage = nullptr;
    TLFCfgPage* m_tlfCfgPage = nullptr;
    SkipTransCfgPage* m_skipTransCfgPage = nullptr;
};

#endif // PLUGINSETTINGSPAGE_H
