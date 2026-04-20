// ProjectSettingsPage.cpp

#include "ProjectSettingsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaText.h"

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

ProjectSettingsPage::ProjectSettingsPage(const fs::path& projectDir, toml::ordered_value& globalConfig, QWidget* parent)
    : BasePage(parent), _mainWindow(parent), _globalConfig(globalConfig), _projectDir(projectDir)
{
    setWindowTitle(tr("项目设置主页"));
    setTitleVisible(false);

    try {
        _projectConfig = toml::uoparse(_projectDir / L"config.toml");
    }
    catch (...) {
        _projectConfig = toml::ordered_table{};
        ElaMessageBar::error(ElaMessageBarType::TopLeft, 
            tr("解析失败"), tr("项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的配置文件不符合 toml 规范"), 3000);
    }
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

    try {
        std::string configStr = toml::format(_projectConfig);
        std::ofstream ofs(_projectDir / L"config.toml", std::ios::binary);
        ofs << configStr;
        ofs.close();
    }
    catch (const toml::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, ascii2Wide(std::string_view(e.what())).c_str(), L"toml 格式化错误", MB_ICONERROR | MB_OK);
#endif
    }
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

void ProjectSettingsPage::clearLog(bool forceClear) {
    if (forceClear || _stackedWidget->currentIndex() == 8) {
        _startSettingsPage->clearLog();
        ElaMessageBar::success(ElaMessageBarType::Bottom, 
            tr("清理成功"), tr("已清空项目 ") + getProjectName() + tr(" 的日志输出窗口"), 3000);
    }
}

void ProjectSettingsPage::_setupUI()
{

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 15, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout* navigationLayout = new QHBoxLayout(centralWidget);
    _settingsTitle = new ElaText(tr("API设置"), centralWidget);
    _settingsTitle->setContentsMargins(0, 10, 0, 0);
    _settingsTitle->setTextPixelSize(18);
    _settingsTitle->setFixedWidth(110);
    navigationLayout->addSpacing(30);
    navigationLayout->addWidget(_settingsTitle);
    navigationLayout->addStretch();

    ElaMenu* foundamentalSettingMenu = new ElaMenu(centralWidget);
    QAction* apiSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::MagnifyingGlassPlus, tr("API设置"));
    QAction* commonSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::BoxCheck, tr("一般设置"));
    QAction* paSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::Question, tr("问题分析"));

    ElaToolButton* foundamentalSettingButton = new ElaToolButton(centralWidget);
    foundamentalSettingButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    foundamentalSettingButton->setElaIcon(ElaIconType::Broom);
    foundamentalSettingButton->setText(tr("基本设置"));
    foundamentalSettingButton->setMenu(foundamentalSettingMenu);

    ElaMenu* transMenu = new ElaMenu(centralWidget);
    QAction* nameTableSettingAction = transMenu->addElaIconAction(ElaIconType::User, tr("人名表"));
    QAction* dictSettingAction = transMenu->addElaIconAction(ElaIconType::Book, tr("项目字典"));
    QAction* dictExSettingAction = transMenu->addElaIconAction(ElaIconType::BookQuran, tr("字典设置"));
    QAction* promptSettingAction = transMenu->addElaIconAction(ElaIconType::Bell, tr("提示词"));

    ElaToolButton* transButton = new ElaToolButton(centralWidget);
    transButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    transButton->setElaIcon(ElaIconType::YenSign);
    transButton->setText(tr("翻译设置"));
    transButton->setMenu(transMenu);

    ElaMenuBar* menuBar = new ElaMenuBar(centralWidget);
    QAction* pluginSettingAction = menuBar->addElaIconAction(ElaIconType::Plug, tr("插件管理"));
    QAction* startTransAction = menuBar->addElaIconAction(ElaIconType::Play, tr("开始翻译"));
    QAction* otherSettingAction = menuBar->addElaIconAction(ElaIconType::Copy, tr("其他设置"));

    navigationLayout->addWidget(foundamentalSettingButton);
    navigationLayout->addWidget(transButton);
    navigationLayout->addWidget(menuBar);
    navigationLayout->addStretch();
    navigationLayout->addStretch();

    _stackedWidget = new QStackedWidget(centralWidget);

    _createPages();

    auto pageNavigation = [=]()
        {
            if (BasePage* page = qobject_cast<BasePage*>(_stackedWidget->currentWidget())) {
                page->navigation(0);
            }
        };

    connect(apiSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(0);
            _settingsTitle->setText(tr("API设置"));
        });
    connect(commonSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(1);
            _settingsTitle->setText(tr("一般设置"));
        });
    connect(paSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(2);
            _settingsTitle->setText(tr("问题分析"));
        });
    connect(nameTableSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(3);
            _settingsTitle->setText(tr("人名表"));
        });
    connect(dictSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(4);
            _settingsTitle->setText(tr("项目字典"));
        });
    connect(dictExSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(5);
            _settingsTitle->setText(tr("字典设置"));
        });
    connect(promptSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(6);
            _settingsTitle->setText(tr("提示词"));
        });
    connect(pluginSettingAction, &QAction::triggered, this, [=]()
        {
            if (_stackedWidget->currentIndex() == 7) {
                pageNavigation();
            }
            _stackedWidget->setCurrentIndex(7);
            _settingsTitle->setText(tr("插件管理"));
        });
    connect(startTransAction, &QAction::triggered, this, [=]()
        {
            if (_stackedWidget->currentIndex() == 8) {
                pageNavigation();
            }
            _stackedWidget->setCurrentIndex(8);
            _settingsTitle->setText(tr("开始翻译"));
        });
    connect(otherSettingAction, &QAction::triggered, this, [=]()
        {
            _stackedWidget->setCurrentIndex(9);
            _settingsTitle->setText(tr("其他设置"));
        });

    mainLayout->addLayout(navigationLayout);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(_stackedWidget);
    
    addCentralWidget(centralWidget, true, false, 0);
}

void ProjectSettingsPage::_createPages()
{
    _apiSettingsPage = new APISettingsPage(_projectConfig, _stackedWidget);
    _commonSettingsPage = new CommonSettingsPage(_projectConfig, _stackedWidget);
    _paSettingsPage = new PASettingsPage(_projectConfig, _stackedWidget);
    _nameTableSettingsPage = new NameTableSettingsPage(_projectDir, _globalConfig, _projectConfig, _stackedWidget);
    _dictSettingsPage = new DictSettingsPage(_projectDir, _globalConfig, _projectConfig, _stackedWidget);
    _dictExSettingsPage = new DictExSettingsPage(_globalConfig, _projectConfig, _stackedWidget);
    _promptSettingsPage = new PromptSettingsPage(_projectDir, _projectConfig, _stackedWidget);
    _pluginSettingsPage = new PluginSettingsPage(_mainWindow, _projectDir, _projectConfig, _stackedWidget);
    _startSettingsPage = new StartSettingsPage(_mainWindow, _projectDir, _globalConfig, _projectConfig, _stackedWidget);
    _otherSettingsPage = new OtherSettingsPage(_mainWindow, _projectDir, _globalConfig, _projectConfig, _stackedWidget);

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
    connect(_startSettingsPage, &StartSettingsPage::finishTranslatingSignal, this, &ProjectSettingsPage::_onFinishTranslating);
    connect(_otherSettingsPage, &OtherSettingsPage::saveConfigSignal, this, &ProjectSettingsPage::apply2Config);
    connect(_otherSettingsPage, &OtherSettingsPage::refreshProjectConfigSignal, this, &ProjectSettingsPage::_onRefreshProjectConfig);
    connect(_otherSettingsPage, &OtherSettingsPage::changeProjectNameSignal, this, [=](QString newProjectName)
        {
            Q_EMIT changeProjectNameSignal(this->property("ElaPageKey").toString(), newProjectName);
        });
}

void ProjectSettingsPage::_onRefreshProjectConfig()
{
    bool isRunning = toml::find_or(_projectConfig, "GUIConfig", "isRunning", true);
    if (isRunning) {
        ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("正在运行"), tr("项目仍在运行中，无法刷新配置"), 3000);
        return;
    }
    try {
        _projectConfig = toml::uoparse(_projectDir / L"config.toml");
    }
    catch (...) {
        ElaMessageBar::error(ElaMessageBarType::TopLeft, 
            tr("解析失败"), tr("项目 ") + QString(_projectDir.filename().wstring()) + tr(" 的配置文件不符合规范"), 3000);
        return;
    }
    insertToml(_projectConfig, "GUIConfig.isRunning", false);
    while (_stackedWidget->count() > 0) {
        QWidget* widget = _stackedWidget->currentWidget();
        _stackedWidget->removeWidget(widget);
        widget->deleteLater();
    }
    _createPages();
    _stackedWidget->setCurrentIndex(9);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("刷新成功"), tr("项目配置刷新成功"), 3000);
}

void ProjectSettingsPage::_onStartTranslating()
{
    insertToml(_projectConfig, "GUIConfig.isRunning", true);
    this->apply2Config();
}

void ProjectSettingsPage::_onFinishTranslating(const QString& transEngine, int exitCode)
{
    if (
        exitCode >= 0 &&
        toml::find_or(_globalConfig, "autoRefreshAfterTranslate", true)
        ) {
        if (transEngine == "DumpName" || transEngine == "NameTrans") {
            _nameTableSettingsPage->refreshTable();
        }
        else if (transEngine == "GenDict") {
            _dictSettingsPage->refreshDicts();
        }
    }
    Q_EMIT finishTranslatingSignal(this->property("ElaPageKey").toString());
    insertToml(_projectConfig, "GUIConfig.isRunning", false);
}

bool ProjectSettingsPage::getIsRunning()
{
    return toml::find_or(_projectConfig, "GUIConfig", "isRunning", true);
}
