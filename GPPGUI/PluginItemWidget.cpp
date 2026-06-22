#include "PluginItemWidget.h"

#include <QHBoxLayout>
#include <QMap>
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaIconButton.h"
#include "ElaDoubleText.h"
#include "ElaComboBox.h"

PluginItemWidget::PluginItemWidget(const QString& pluginName, const QString& runTimeStr, QWidget* parent)
    : ElaScrollPageArea(parent)
{
    const QMap<QString, QString> toolTipMap =
    {
        { "SkipTrans", tr("滤过插件") },
        { "TextFull2Half", tr("全角半角转换插件") },
        { "TextLinebreakFix", tr("换行修复插件") },
    };
    const QMap<QString, QStringList> boxItemMap =
    {
        { "SkipTrans", { "dprerun", "prerun" } },
        { "TextFull2Half", { "dprerun", "prerun", "postrun", "dpostrun" } },
        { "TextLinebreakFix", { "dpostrun" } },
    };
    // 主水平布局
    QHBoxLayout* mainLayout = new QHBoxLayout(this);

    // 插件名称
    m_pluginNameLabel = new ElaDoubleText(this,
        pluginName, 16, toolTipMap[pluginName], 10, "");

    m_pluginRunTimeBox = new ElaComboBox(this);
    m_pluginRunTimeBox->setFixedWidth(150);
    m_pluginRunTimeBox->addItems(boxItemMap[pluginName]);
    m_pluginRunTimeBox->setCurrentText(runTimeStr);

    // 新增设置按钮
    m_settingsButton = new ElaIconButton(ElaIconType::Gear, this);
    connect(m_settingsButton, &ElaIconButton::clicked, this, [=]()
        {
            Q_EMIT settingsRequestedSignal(this);
        });

    // 启用/禁用开关
    m_enableSwitch = new ElaToggleSwitch(this);
    m_enableSwitch->setIsToggled(false);

    // 上移按钮
    m_moveUpButton = new ElaIconButton(ElaIconType::AngleUp, this);
    connect(m_moveUpButton, &ElaIconButton::clicked, this, [=]()
        {
            Q_EMIT moveUpRequestedSignal(this);
        });

    // 下移按钮
    m_moveDownButton = new ElaIconButton(ElaIconType::AngleDown, this);
    connect(m_moveDownButton, &ElaIconButton::clicked, this, [=]()
        {
            Q_EMIT moveDownRequestedSignal(this);
        });

    // 组合布局
    mainLayout->addWidget(m_pluginNameLabel);
    mainLayout->addStretch();
    mainLayout->addWidget(m_enableSwitch);
    mainLayout->addWidget(m_pluginRunTimeBox);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(m_settingsButton);
    mainLayout->addWidget(m_moveUpButton);
    mainLayout->addWidget(m_moveDownButton);
}

PluginItemWidget::~PluginItemWidget()
{
}

QString PluginItemWidget::getPluginName() const
{
    return m_pluginRunTimeBox->currentText() + ":" + m_pluginNameLabel->getFirstLineText();
}

bool PluginItemWidget::getIsToggled() const
{
    return m_enableSwitch->getIsToggled();
}

void PluginItemWidget::setIsToggled(bool enabled)
{
    m_enableSwitch->setIsToggled(enabled);
}

void PluginItemWidget::setMoveUpButtonEnabled(bool enabled)
{
    m_moveUpButton->setEnabled(enabled);
}

void PluginItemWidget::setMoveDownButtonEnabled(bool enabled)
{
    m_moveDownButton->setEnabled(enabled);
}
