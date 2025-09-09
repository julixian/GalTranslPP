// ProjectSettingsPage.cpp

#include "ProjectSettingsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>

#include "ElaToggleButton.h"
#include "ElaIcon.h"
#include "ElaMessageBar.h"
#include "ElaScrollPageArea.h"

#include "APISettingsPage.h"
#include "PluginSettingsPage.h"
#include "CommonSettingsPage.h"
#include "PASettingsPage.h"
#include "DictSettingsPage.h"
#include "StartSettingsPage.h"
#include "NameTableSettingsPage.h"
#include "OtherSettingsPage.h"
#include "PromptSettingsPage.h"

import std;

ProjectSettingsPage::ProjectSettingsPage(toml::table& globalConfig, const fs::path& projectDir, QWidget* parent)
    : BasePage(parent), _projectDir(projectDir), _globalConfig(globalConfig)
{
    setWindowTitle("ProjectSettings");
    setTitleVisible(false);

    std::ifstream ifs(_projectDir / L"config.toml");
    try {
        _projectConfig = toml::parse(ifs);
    }
    catch (...) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, "解析失败", "项目配置文件不符合规范", 3000);
    }
    ifs.close();

    _setupUI();
}

ProjectSettingsPage::~ProjectSettingsPage()
{

}

void ProjectSettingsPage::apply2Config()
{
    _apiSettingsPage->apply2Config();
    _pluginSettingsPage->apply2Config();
    _commonSettingsPage->apply2Config();
    _paSettingsPage->apply2Config();
    _nameTableSettingsPage->apply2Config();
    _dictSettingsPage->apply2Config();
    _startSettingsPage->apply2Config();
    _otherSettingsPage->apply2Config();
    _promptSettingsPage->apply2Config();

    _nameTableSettingsPage->refreshTable();
    _dictSettingsPage->refreshDicts();

    std::ofstream ofs(_projectDir / L"config.toml");
    ofs << _projectConfig;
    ofs.close();
}

QString ProjectSettingsPage::getProjectName()
{
    return QString(_projectDir.filename().wstring());
}

fs::path ProjectSettingsPage::getProjectDir()
{
    return _projectDir;
}

void ProjectSettingsPage::_setupUI()
{
    setWindowTitle("项目设置总页");
    setTitleVisible(false);
    QHBoxLayout* mainLayout = new QHBoxLayout();

    QWidget* navigationWidget = new QWidget(this);
    _navigationLayout = new QVBoxLayout(navigationWidget);
    _navigationLayout->addSpacing(130);

    ElaScrollPageArea* buttonsArea = new ElaScrollPageArea(navigationWidget);
    buttonsArea->setFixedWidth(100);
    buttonsArea->setFixedHeight(430);
    _buttonsLayout = new QVBoxLayout(buttonsArea);

    _navigationLayout->addWidget(buttonsArea);
    _navigationLayout->addStretch();

    _stackedWidget = new QStackedWidget(this);
    _stackedWidget->setContentsMargins(0, 0, 20, 0);
    _stackedWidget->setObjectName("SettingsStackedWidget");
    _stackedWidget->setStyleSheet("#SettingsStackedWidget { background-color: transparent; }");

    _createNavigation();
    _createPages();

    mainLayout->addWidget(buttonsArea);
    mainLayout->addWidget(_stackedWidget, 1);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    addCentralWidget(centralWidget, true, true, 0);

    // 初始状态设置：手动触发第一个按钮的点击逻辑
    if (!_navigationButtons.isEmpty())
    {
        _navigationButtons.first()->setIsToggled(true);
        _stackedWidget->setCurrentIndex(0);
    }
}

void ProjectSettingsPage::_createNavigation()
{
    // 创建导航按钮
    ElaToggleButton* apiButton = new ElaToggleButton("API 设置", this);
    apiButton->setFixedHeight(35);
    ElaToggleButton* pluginButton = new ElaToggleButton("插件管理", this);
    pluginButton->setFixedHeight(35);
    ElaToggleButton* commonButton = new ElaToggleButton("一般设置", this);
    commonButton->setFixedHeight(35);
    ElaToggleButton* paButton = new ElaToggleButton("问题分析", this);
    paButton->setFixedHeight(35);
    ElaToggleButton* nameTableButton = new ElaToggleButton("人名表", this);
    nameTableButton->setFixedHeight(35);
    ElaToggleButton* dictButton = new ElaToggleButton("项目字典", this);
    dictButton->setFixedHeight(35);
    ElaToggleButton* promptButton = new ElaToggleButton("提示词", this);
    promptButton->setFixedHeight(35);
    ElaToggleButton* startButton = new ElaToggleButton("开始翻译", this);
    startButton->setFixedHeight(35);
    ElaToggleButton* otherButton = new ElaToggleButton("其他设置", this);
    otherButton->setFixedHeight(35);

    // 将按钮添加到 QList 和布局中
    _navigationButtons.append(apiButton);
    _navigationButtons.append(pluginButton);
    _navigationButtons.append(commonButton);
    _navigationButtons.append(paButton);
    _navigationButtons.append(nameTableButton);
    _navigationButtons.append(dictButton);
    _navigationButtons.append(startButton);
    _navigationButtons.append(otherButton);
    _navigationButtons.append(promptButton);

    for (ElaToggleButton* button : _navigationButtons)
    {
        _buttonsLayout->addWidget(button);
        // 连接每个按钮的 toggled 信号到同一个槽函数
        connect(button, &ElaToggleButton::toggled, this, &ProjectSettingsPage::_onNavigationButtonToggled);
    }
}

void ProjectSettingsPage::_createPages()
{
    _apiSettingsPage = new APISettingsPage(_projectConfig, this);
    _pluginSettingsPage = new PluginSettingsPage(_projectConfig, this);
    _commonSettingsPage = new CommonSettingsPage(_projectConfig, this);
    _paSettingsPage = new PASettingsPage(_projectConfig, this);
    _nameTableSettingsPage = new NameTableSettingsPage(_projectDir, _globalConfig, _projectConfig, this);
    _dictSettingsPage = new DictSettingsPage(_projectDir, _globalConfig, _projectConfig, this);
    _promptSettingsPage = new PromptSettingsPage(_projectDir, _projectConfig, this);
    _startSettingsPage = new StartSettingsPage(_projectDir, _projectConfig, this);
    _otherSettingsPage = new OtherSettingsPage(_projectDir, _projectConfig, this);
    _stackedWidget->addWidget(_apiSettingsPage);
    _stackedWidget->addWidget(_pluginSettingsPage);
    _stackedWidget->addWidget(_commonSettingsPage);
    _stackedWidget->addWidget(_paSettingsPage);
    _stackedWidget->addWidget(_nameTableSettingsPage);
    _stackedWidget->addWidget(_dictSettingsPage);
    _stackedWidget->addWidget(_promptSettingsPage);
    _stackedWidget->addWidget(_startSettingsPage);
    _stackedWidget->addWidget(_otherSettingsPage);

    connect(_startSettingsPage, &StartSettingsPage::startTranslating, this, &ProjectSettingsPage::_onStartTranslating);
    connect(_startSettingsPage, &StartSettingsPage::finishTranslating, this, &ProjectSettingsPage::_onFinishTranslating);
    connect(_otherSettingsPage, &OtherSettingsPage::saveConfigSignal, this, [this]()
        {
            this->apply2Config();
        });
}

// 槽函数，处理所有导航按钮的状态变化
void ProjectSettingsPage::_onNavigationButtonToggled(bool checked)
{
    // 1. 确定是哪个按钮触发了信号
    ElaToggleButton* triggeredButton = qobject_cast<ElaToggleButton*>(sender());
    if (!triggeredButton)
    {
        return;
    }

    if (!checked)
    {
        triggeredButton->blockSignals(true);
        triggeredButton->setIsToggled(true);
        triggeredButton->blockSignals(false);
        return;
    }

    // 2. 遍历所有导航按钮，实现互斥效果
    for (ElaToggleButton* button : _navigationButtons)
    {
        if (button != triggeredButton)
        {
            // 关键：在不触发信号的情况下，取消其他按钮的选中状态
            button->blockSignals(true);
            button->setIsToggled(false);
            button->blockSignals(false);
        }
    }

    // 3. 切换到对应的页面
    // 找到触发信号的按钮在列表中的索引
    int index = _navigationButtons.indexOf(triggeredButton);
    if (index != -1)
    {
        _stackedWidget->setCurrentIndex(index);
    }
}

void ProjectSettingsPage::_onStartTranslating()
{
    _isRunning = true;
    apply2Config();
}

void ProjectSettingsPage::_onFinishTranslating(const QString& transEngine, int exitCode)
{
    if (
        exitCode == 0 &&
        (transEngine == "DumpName" || transEngine == "GenDict") &&
        _globalConfig["autoRefreshAfterTranslate"].value_or(true)
        ) {
        _nameTableSettingsPage->refreshTable();
        _dictSettingsPage->refreshDicts();
    }
    Q_EMIT finishedTranslating(this->property("ElaPageKey").toString());
    _isRunning = false;
}

bool ProjectSettingsPage::getIsRunning()
{
    return _isRunning.load();
}
