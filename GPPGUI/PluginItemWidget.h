// PluginItemWidget.h

#ifndef PLUGINITEMWIDGET_H
#define PLUGINITEMWIDGET_H

#include "ElaScrollPageArea.h"

class ElaText;
class ElaToggleSwitch;
class ElaIconButton;

class PluginItemWidget : public ElaScrollPageArea
{
    Q_OBJECT

public:
    explicit PluginItemWidget(const QString& pluginName, QWidget* parent = nullptr);
    ~PluginItemWidget();

    // 公共方法，用于获取当前项的状态
    QString getPluginName() const;
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // 控制上下移动按钮的可用性
    void setMoveUpButtonEnabled(bool enabled);
    void setMoveDownButtonEnabled(bool enabled);

Q_SIGNALS:
    // 信号，当用户点击移动按钮时，通知父窗口
    void moveUpRequested(PluginItemWidget* item);
    void moveDownRequested(PluginItemWidget* item);
    void settingsRequested(PluginItemWidget* item);

private:
    // 内部控件
    ElaText* _pluginNameLabel;
    ElaToggleSwitch* _enableSwitch;
    ElaIconButton* _moveUpButton;
    ElaIconButton* _moveDownButton;
    ElaIconButton* _settingsButton;
};

#endif // PLUGINITEMWIDGET_H
