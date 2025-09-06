// PluginItemWidget.cpp

#include "PluginItemWidget.h"

#include <QHBoxLayout>
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "ElaIcon.h"
#include "ElaIconButton.h"

PluginItemWidget::PluginItemWidget(const QString& pluginName, QWidget* parent)
    : ElaScrollPageArea(parent)
{
    setFixedHeight(60);

    // 主水平布局
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 0, 10, 0);

    // 插件名称
    _pluginNameLabel = new ElaText(pluginName, this);
    _pluginNameLabel->setTextPixelSize(16);
    _pluginNameLabel->setWordWrap(false);

    // 新增设置按钮
    _settingsButton = new ElaIconButton(ElaIconType::Gear, this);
    connect(_settingsButton, &ElaIconButton::clicked, this, [=]() {
        Q_EMIT settingsRequested(this);
        });

    // 启用/禁用开关
    _enableSwitch = new ElaToggleSwitch(this);
    _enableSwitch->setIsToggled(false);

    // 上移按钮
    _moveUpButton = new ElaIconButton(ElaIconType::AngleUp, this);
    connect(_moveUpButton, &ElaIconButton::clicked, this, [=]() {
        Q_EMIT moveUpRequested(this);
        });

    // 下移按钮
    _moveDownButton = new ElaIconButton(ElaIconType::AngleDown, this);
    connect(_moveDownButton, &ElaIconButton::clicked, this, [=]() {
        Q_EMIT moveDownRequested(this);
        });

    // 组合布局
    mainLayout->addWidget(_pluginNameLabel);
    mainLayout->addStretch();
    mainLayout->addWidget(_enableSwitch);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(_settingsButton);
    mainLayout->addWidget(_moveUpButton);
    mainLayout->addWidget(_moveDownButton);
}

PluginItemWidget::~PluginItemWidget()
{
}

QString PluginItemWidget::getPluginName() const
{
    return _pluginNameLabel->text();
}

bool PluginItemWidget::isEnabled() const
{
    return _enableSwitch->getIsToggled();
}

void PluginItemWidget::setEnabled(bool enabled)
{
    _enableSwitch->setIsToggled(enabled);
}

void PluginItemWidget::setMoveUpButtonEnabled(bool enabled)
{
    _moveUpButton->setEnabled(enabled);
}

void PluginItemWidget::setMoveDownButtonEnabled(bool enabled)
{
    _moveDownButton->setEnabled(enabled);
}
