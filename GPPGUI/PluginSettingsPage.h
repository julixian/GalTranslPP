// PluginSettingsPage.h

#ifndef PLUGINSETTINGSPAGE_H
#define PLUGINSETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include "BasePage.h"

class QVBoxLayout;
class PluginItemWidget;

class PluginSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PluginSettingsPage(toml::table& projectConfig, QWidget* parent = nullptr);
    ~PluginSettingsPage();
    void apply2Config();

private Q_SLOTS:
    void _onMoveUp(PluginItemWidget* item);
    void _onMoveDown(PluginItemWidget* item);
    void _onSettings(PluginItemWidget* item);

private:
    void _setupUI();
    void _updateMoveButtonStates(); // 更新所有项的上下移动按钮状态

    QVBoxLayout* _pluginListLayout; // 容纳所有 PluginItemWidget 的布局
    QList<PluginItemWidget*> _pluginItems; // 按顺序存储所有项的指针

    toml::table& _projectConfig;
};

#endif // PLUGINSETTINGSPAGE_H
