// ProjectSettingsPage.cpp

#include "ProjectSettingsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>

#include "ElaToggleButton.h"
#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaText.h"
#include "ElaScrollPageArea.h"

#include "APISettingsPage.h"
#include "PluginSettingsPage.h"
#include "CommonSettingsPage.h"
#include "PASettingsPage.h"
#include "DictSettingsPage.h"
#include "DictExSettingsPage.h"
#include "StartSettingsPage.h"
#include "NameTableSettingsPage.h"
#include "OtherSettingsPage.h"
#include "PromptSettingsPage.h"

import Tool;

ProjectSettingsPage::ProjectSettingsPage(toml::table& globalConfig, const fs::path& projectDir, QWidget* parent)
    : BasePage(parent), _projectDir(projectDir), _globalConfig(globalConfig), _mainWindow(parent)
{
    setWindowTitle("项目设置主页");
    setTitleVisible(false);
    setContentsMargins(5, 10, 10, 10);

    std::ifstream ifs(_projectDir / L"config.toml");
    try {
        _projectConfig = toml::parse(ifs);
    }
    catch (...) {
        ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "项目 " + QString(_projectDir.filename().wstring()) + " 的配置文件不符合规范", 3000);
    }
    ifs.close();
    insertToml(_projectConfig, "GUIConfig.isRunning", false);

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
    _dictExSettingsPage->apply2Config();
    _startSettingsPage->apply2Config();
    _otherSettingsPage->apply2Config();
    _promptSettingsPage->apply2Config();

    std::ofstream ofs(_projectDir / L"config.toml");
    ofs << _projectConfig;
    ofs.close();
}

void ProjectSettingsPage::refreshCommonDicts()
{
    _dictExSettingsPage->refreshCommonDictsList();
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

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(0);

    QWidget* navigationWidget = new QWidget(this);
    navigationWidget->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout* navigationLayout = new QHBoxLayout(navigationWidget);
    ElaText* settingsTitle = new ElaText("API设置", navigationWidget);
    settingsTitle->setContentsMargins(0, 10, 0, 0);
    settingsTitle->setTextPixelSize(18);
    settingsTitle->setFixedWidth(85);
    navigationLayout->addSpacing(30);
    navigationLayout->addWidget(settingsTitle);
    navigationLayout->addStretch();

    ElaMenu* foundamentalSettingMenu = new ElaMenu(navigationWidget);
    QAction* apiSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::MagnifyingGlassPlus, "API设置");
    QAction* commonSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::BoxCheck, "一般设置");
    QAction* paSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::Question, "问题分析");

    ElaToolButton* foundamentalSettingButton = new ElaToolButton(navigationWidget);
    foundamentalSettingButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    foundamentalSettingButton->setElaIcon(ElaIconType::Broom);
    foundamentalSettingButton->setText("基本设置");
    foundamentalSettingButton->setMenu(foundamentalSettingMenu);

    ElaMenu* transMenu = new ElaMenu(navigationWidget);
    QAction* nameTableSettingAction = transMenu->addElaIconAction(ElaIconType::User, "人名表");
    QAction* dictSettingAction = transMenu->addElaIconAction(ElaIconType::Book, "项目字典");
    QAction* dictExSettingAction = transMenu->addElaIconAction(ElaIconType::BookOpen, "字典设置");
    QAction* promptSettingAction = transMenu->addElaIconAction(ElaIconType::Bell, "提示词");

    ElaToolButton* transButton = new ElaToolButton(navigationWidget);
    transButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    transButton->setElaIcon(ElaIconType::YenSign);
    transButton->setText("翻译设置");
    transButton->setMenu(transMenu);

    ElaMenuBar* menuBar = new ElaMenuBar(navigationWidget);
    menuBar->setContentsMargins(0, 0, 0, 0);
    QAction* pluginSettingAction = menuBar->addElaIconAction(ElaIconType::Plug, "插件管理");
    QAction* startTransAction = menuBar->addElaIconAction(ElaIconType::Play, "开始翻译");
    QAction* otherSettingAction = menuBar->addElaIconAction(ElaIconType::Copy, "其他设置");

    navigationLayout->addWidget(foundamentalSettingButton);
    navigationLayout->addWidget(transButton);
    navigationLayout->addWidget(menuBar);
    navigationLayout->addStretch();
    navigationLayout->addStretch();

    _stackedWidget = new QStackedWidget(this);
    _stackedWidget->setContentsMargins(0, 0, 0, 0);

    _createPages();

    auto pageNavigation = [=]()
        {
            BasePage* page = qobject_cast<BasePage*>(_stackedWidget->currentWidget());
            if (page) {
                page->navigation(0);
            }
        };

    connect(apiSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(0);
            settingsTitle->setText("API设置");
        });
    connect(commonSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(1);
            settingsTitle->setText("一般设置");
        });
    connect(paSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(2);
            settingsTitle->setText("问题分析");
        });
    connect(nameTableSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(3);
            settingsTitle->setText("人名表");
        });
    connect(dictSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(4);
            settingsTitle->setText("项目字典");
        });
    connect(dictExSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(5);
            settingsTitle->setText("字典设置");
        });
    connect(promptSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(6);
            settingsTitle->setText("提示词");
        });
    connect(pluginSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(7);
            pageNavigation();
            settingsTitle->setText("插件管理");
        });
    connect(startTransAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(8);
            pageNavigation();
            settingsTitle->setText("开始翻译");
        });
    connect(otherSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(9);
            settingsTitle->setText("其他设置");
        });

    mainLayout->addWidget(navigationWidget);
    mainLayout->addWidget(_stackedWidget, 1);
    
    addCentralWidget(centralWidget, true, true, 0);
}

void ProjectSettingsPage::_createPages()
{
    _apiSettingsPage = new APISettingsPage(_projectConfig, this);
    _commonSettingsPage = new CommonSettingsPage(_projectConfig, this);
    _paSettingsPage = new PASettingsPage(_projectConfig, this);
    _nameTableSettingsPage = new NameTableSettingsPage(_projectDir, _globalConfig, _projectConfig, this);
    _dictSettingsPage = new DictSettingsPage(_projectDir, _globalConfig, _projectConfig, this);
    _dictExSettingsPage = new DictExSettingsPage(_globalConfig, _projectConfig, this);
    _promptSettingsPage = new PromptSettingsPage(_projectDir, _projectConfig, this);
    _pluginSettingsPage = new PluginSettingsPage(_mainWindow, _projectConfig, this);
    _startSettingsPage = new StartSettingsPage(_mainWindow, _projectDir, _projectConfig, this);
    _otherSettingsPage = new OtherSettingsPage(_projectDir, _projectConfig, this);

    _stackedWidget->addWidget(_apiSettingsPage);
    _stackedWidget->addWidget(_commonSettingsPage);
    _stackedWidget->addWidget(_paSettingsPage);
    _stackedWidget->addWidget(_nameTableSettingsPage);
    _stackedWidget->addWidget(_dictSettingsPage);
    _stackedWidget->addWidget(_dictExSettingsPage);
    _stackedWidget->addWidget(_promptSettingsPage);
    _stackedWidget->addWidget(_pluginSettingsPage);
    _stackedWidget->addWidget(_startSettingsPage);
    _stackedWidget->addWidget(_otherSettingsPage);

    connect(_startSettingsPage, &StartSettingsPage::startTranslating, this, &ProjectSettingsPage::_onStartTranslating);
    connect(_startSettingsPage, &StartSettingsPage::finishTranslating, this, &ProjectSettingsPage::_onFinishTranslating);
    connect(_otherSettingsPage, &OtherSettingsPage::saveConfigSignal, this, [=]()
        {
            this->apply2Config();
        });
}

void ProjectSettingsPage::_onStartTranslating()
{
    insertToml(_projectConfig, "GUIConfig.isRunning", true);
    this->apply2Config();
}

void ProjectSettingsPage::_onFinishTranslating(const QString& transEngine, int exitCode)
{
    if (
        exitCode == 0 &&
        _globalConfig["autoRefreshAfterTranslate"].value_or(true)
        ) {
        if (transEngine == "DumpName") {
            _nameTableSettingsPage->refreshTable();
        }
        else if (transEngine == "GenDict") {
            _dictSettingsPage->refreshDicts();
        }
    }
    Q_EMIT finishedTranslating(this->property("ElaPageKey").toString());
    insertToml(_projectConfig, "GUIConfig.isRunning", false);
}

bool ProjectSettingsPage::getIsRunning()
{
    return _projectConfig["GUIConfig"]["isRunning"].value_or(true);
}
