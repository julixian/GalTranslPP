// PluginSettingsPage.h

#ifndef PLUGINSETTINGSPAGE_H
#define PLUGINSETTINGSPAGE_H

#include <QList>
#include <toml++/toml.hpp>
#include "BasePage.h"

class QVBoxLayout;
class PluginItemWidget;
class TLFCfgPage;
class PostFull2HalfCfgPage;

class PluginSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PluginSettingsPage(QWidget* mainWindow, toml::table& projectConfig, QWidget* parent = nullptr);
    ~PluginSettingsPage();
    virtual void apply2Config() override;

private Q_SLOTS:
    void _onPreMoveUp(PluginItemWidget* item);
    void _onPreMoveDown(PluginItemWidget* item);
    void _onPreSettings(PluginItemWidget* item);
    void _onPostMoveUp(PluginItemWidget* item);
    void _onPostMoveDown(PluginItemWidget* item);
    void _onPostSettings(PluginItemWidget* item);

private:
    void _setupUI();
    void _updatePreMoveButtonStates();
    void _updatePostMoveButtonStates(); // 更新所有项的上下移动按钮状态

    QVBoxLayout* _prePluginListLayout; // 容纳所有译前 PluginItemWidget 的布局
    QList<PluginItemWidget*> _prePluginItems; // 按顺序存储所有项的指针

    QVBoxLayout* _postPluginListLayout; // 容纳所有译后 PluginItemWidget 的布局
    QList<PluginItemWidget*> _postPluginItems; // 按顺序存储所有项的指针

    toml::table& _projectConfig;
    QWidget* _mainWindow;

private:
    // 以下为各个插件的设置页面
    TLFCfgPage* _tlfCfgPage;
    PostFull2HalfCfgPage* _pf2hCfgPage;
};

#endif // PLUGINSETTINGSPAGE_H
