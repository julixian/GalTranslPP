#ifndef PLUGINITEMWIDGET_H
#define PLUGINITEMWIDGET_H

#include "ElaScrollPageArea.h"

class ElaDoubleText;
class ElaToggleSwitch;
class ElaIconButton;
class ElaNoWheelComboBox;

class PluginItemWidget : public ElaScrollPageArea
{
    Q_OBJECT

public:
    explicit PluginItemWidget(const QString& pluginName, const QString& runTimeStr, QWidget* parent = nullptr);

    // 公共方法，用于获取当前项的状态
    QString getPluginName() const;
    bool getIsToggled() const;
    void setIsToggled(bool enabled);

    // 控制上下移动按钮的可用性
    void setMoveUpButtonEnabled(bool enabled);
    void setMoveDownButtonEnabled(bool enabled);

Q_SIGNALS:
    // 信号，当用户点击移动按钮时，通知父窗口
    void moveUpRequestedSignal(PluginItemWidget* item);
    void moveDownRequestedSignal(PluginItemWidget* item);
    void settingsRequestedSignal(PluginItemWidget* item);

private:
    // 内部控件
    ElaNoWheelComboBox* m_pluginRunTimeBox = nullptr;
    ElaDoubleText* m_pluginNameLabel = nullptr;
    ElaToggleSwitch* m_enableSwitch = nullptr;
    ElaIconButton* m_moveUpButton = nullptr;
    ElaIconButton* m_moveDownButton = nullptr;
    ElaIconButton* m_settingsButton = nullptr;
};

#endif
