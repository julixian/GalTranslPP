// PluginSettingsPage.cpp

#include "PluginSettingsPage.h"
#include "PluginItemWidget.h" // 引入自定义控件

#include <QVBoxLayout>
#include <QDebug>
#include "ElaText.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaContentDialog.h"
#include "TLFCfgDialog.h"
#include "PostFull2HalfCfgDialog.h"

import Tool;

PluginSettingsPage::PluginSettingsPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
    setWindowTitle("插件设置");
    setTitleVisible(false);
    _setupUI();
}

PluginSettingsPage::~PluginSettingsPage()
{
}

void PluginSettingsPage::apply2Config()
{
    toml::array plugins;
    for (PluginItemWidget* item : _pluginItems) {
        if (!item->isEnabled()) {
            continue;
        }
        plugins.push_back(item->getPluginName().toStdString());
    }
    insertToml(_projectConfig, "plugins.textPostPlugins", plugins);
}

void PluginSettingsPage::_setupUI()
{
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

    //创建一个提示标题
    ElaText* title = new ElaText(mainWidget);
    title->setText("后处理插件设置(由上至下执行)");
    title->setTextPixelSize(18);

    // 创建一个容器用于放置列表
    QWidget* listContainer = new QWidget(mainWidget);

    _pluginListLayout = new QVBoxLayout(listContainer);
    _pluginListLayout->setSpacing(20);
    _pluginListLayout->setContentsMargins(5, 5, 5, 5);

    // 插件名称列表
    QStringList pluginNames = { "TextPostFull2Half", "TextLinebreakFix" };

    // 遍历名称列表，创建并添加 PluginItemWidget
    for (const QString& name : pluginNames)
    {
        PluginItemWidget* item = new PluginItemWidget(name, this);
        _pluginItems.append(item); // 添加到列表中
        _pluginListLayout->addWidget(item); // 添加到布局中

        // 连接信号
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onSettings);
    }

    _pluginListLayout->addStretch(); // 弹簧将列表项向上推

    // 初始化按钮状态
    _updateMoveButtonStates();

    mainLayout->addWidget(title);
    mainLayout->addWidget(listContainer);

    addCentralWidget(mainWidget);
}

void PluginSettingsPage::_onMoveUp(PluginItemWidget* item)
{
    int index = _pluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        _pluginListLayout->removeWidget(item);
        _pluginItems.removeOne(item);

        // 插入到新位置
        _pluginListLayout->insertWidget(index - 1, item);
        _pluginItems.insert(index - 1, item);

        _updateMoveButtonStates();
    }
}

void PluginSettingsPage::_onMoveDown(PluginItemWidget* item)
{
    int index = _pluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < _pluginItems.count() - 1)
    {
        _pluginListLayout->removeWidget(item);
        _pluginItems.removeOne(item);

        _pluginListLayout->insertWidget(index + 1, item);
        _pluginItems.insert(index + 1, item);

        _updateMoveButtonStates();
    }
}

void PluginSettingsPage::_updateMoveButtonStates()
{
    for (int i = 0; i < _pluginItems.count(); ++i)
    {
        PluginItemWidget* item = _pluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < _pluginItems.count() - 1);
    }
}

void PluginSettingsPage::_onSettings(PluginItemWidget* item)
{
    if (!item) {
        return;
    }
    QString pluginName = item->getPluginName();

    if (pluginName == "TextPostFull2Half") {
        PostFull2HalfCfgDialog dlg(_projectConfig, this);
        dlg.exec();
    }
    else if (pluginName == "TextLinebreakFix") {
        TLFCfgDialog dlg(_projectConfig, this);
        dlg.exec();
    }
}
