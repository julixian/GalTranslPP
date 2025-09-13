// PluginSettingsPage.cpp

#include "PluginSettingsPage.h"
#include "PluginItemWidget.h" // 引入自定义控件

#include <QVBoxLayout>
#include "ElaText.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"

#include "TLFCfgPage.h"
#include "PostFull2HalfCfgPage.h"

import Tool;

PluginSettingsPage::PluginSettingsPage(QWidget* mainWindow, toml::table& projectConfig, QWidget* parent) :
    BasePage(parent), _projectConfig(projectConfig), _mainWindow(mainWindow)
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
    _tlfCfgPage->apply2Config();
    _pf2hCfgPage->apply2Config();

    toml::array plugins;
    for (PluginItemWidget* item : _postPluginItems) {
        if (!item->isToggled()) {
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

    // 前处理插件列表

    // 后处理插件列表
    ElaText* postTitle = new ElaText(mainWidget);
    postTitle->setText("后处理插件设置(由上至下执行)");
    postTitle->setTextPixelSize(18);

    // 创建一个容器用于放置列表
    QWidget* postListContainer = new QWidget(mainWidget);
    _postPluginListLayout = new QVBoxLayout(postListContainer);

    // 插件名称列表
    QStringList postPluginNames = { "TextPostFull2Half", "TextLinebreakFix" };
    // 先处理项目已经启用的插件
    auto postPluginsArr = _projectConfig["plugins"]["textPostPlugins"].as_array();
    if (postPluginsArr) {
        for (const auto& elem : *postPluginsArr) {
            auto pluginNameOpt = elem.value<std::string>();
            if (!pluginNameOpt.has_value()) {
                continue;
            }
            QString pluginName = QString::fromStdString(pluginNameOpt.value());
            if (!postPluginNames.contains(pluginName)) {
                continue;
            }
            postPluginNames.removeOne(pluginName);
            PluginItemWidget* item = new PluginItemWidget(pluginName, this);
            item->setIsToggled(true);
            _postPluginItems.append(item);
            _postPluginListLayout->addWidget(item);
            connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onPostMoveUp);
            connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onPostMoveDown);
            connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onPostSettings);
        }
    }

    // 遍历剩下的名称列表，创建并添加 PluginItemWidget
    for (const QString& name : postPluginNames)
    {
        PluginItemWidget* item = new PluginItemWidget(name, this);
        _postPluginItems.append(item); // 添加到列表中
        _postPluginListLayout->addWidget(item); // 添加到布局中

        // 连接信号
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onPostMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onPostMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onPostSettings);
    }

    // 初始化按钮状态
    _updatePreMoveButtonStates();
    _updatePostMoveButtonStates();

    mainLayout->addWidget(postTitle);
    mainLayout->addWidget(postListContainer);
    mainLayout->addStretch();

    addCentralWidget(mainWidget);

    // 这里的顺序和_onPre/PostSettings 中的navigation索引对应
    _pf2hCfgPage = new PostFull2HalfCfgPage(_projectConfig, this);
    addCentralWidget(_pf2hCfgPage);
    _tlfCfgPage = new TLFCfgPage(_projectConfig, this);
    addCentralWidget(_tlfCfgPage);
}

void PluginSettingsPage::_onPostSettings(PluginItemWidget* item)
{
    if (!item) {
        return;
    }
    QString pluginName = item->getPluginName();

    if (pluginName == "TextPostFull2Half") {
        this->navigation(1);
    }
    else if (pluginName == "TextLinebreakFix") {
        this->navigation(2);
    }
}

void PluginSettingsPage::_onPreSettings(PluginItemWidget* item)
{
    if (!item) {
        return;
    }
    QString pluginName = item->getPluginName();


}

// 下面不用看，没什么用

void PluginSettingsPage::_onPostMoveUp(PluginItemWidget* item)
{
    int index = _postPluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        _postPluginListLayout->removeWidget(item);
        _postPluginItems.removeOne(item);

        // 插入到新位置
        _postPluginListLayout->insertWidget(index - 1, item);
        _postPluginItems.insert(index - 1, item);

        _updatePostMoveButtonStates();
    }
}

void PluginSettingsPage::_onPostMoveDown(PluginItemWidget* item)
{
    int index = _postPluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < _postPluginItems.count() - 1)
    {
        _postPluginListLayout->removeWidget(item);
        _postPluginItems.removeOne(item);

        _postPluginListLayout->insertWidget(index + 1, item);
        _postPluginItems.insert(index + 1, item);

        _updatePostMoveButtonStates();
    }
}

void PluginSettingsPage::_onPreMoveUp(PluginItemWidget* item)
{
    int index = _prePluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        _prePluginListLayout->removeWidget(item);
        _prePluginItems.removeOne(item);

        // 插入到新位置
        _prePluginListLayout->insertWidget(index - 1, item);
        _prePluginItems.insert(index - 1, item);

        _updatePreMoveButtonStates();
    }
}

void PluginSettingsPage::_onPreMoveDown(PluginItemWidget* item)
{
    int index = _prePluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < _prePluginItems.count() - 1)
    {
        _prePluginListLayout->removeWidget(item);
        _prePluginItems.removeOne(item);

        _prePluginListLayout->insertWidget(index + 1, item);
        _prePluginItems.insert(index + 1, item);

        _updatePreMoveButtonStates();
    }
}

void PluginSettingsPage::_updatePostMoveButtonStates()
{
    for (int i = 0; i < _postPluginItems.count(); ++i)
    {
        PluginItemWidget* item = _postPluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < _postPluginItems.count() - 1);
    }
}

void PluginSettingsPage::_updatePreMoveButtonStates()
{
    for (int i = 0; i < _prePluginItems.count(); ++i)
    {
        PluginItemWidget* item = _prePluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < _prePluginItems.count() - 1);
    }
}
