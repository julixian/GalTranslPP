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

#include "ApiSettingsPage.h"
#include "PluginSettingsPage.h"
#include "CommonSettingsPage.h"
#include "PASettingsPage.h"
#include "DictSettingsPage.h"
#include "DictExSettingsPage.h"
#include "StartSettingsPage.h"
#include "NameTableSettingsPage.h"
#include "OtherSettingsPage.h"
#include "PromptSettingsPage.h"
#include "ProjectCachePage.h"
#include "ProblemOverviewTracker.h"

import Tool;

ProjectSettingsPage::ProjectSettingsPage(const fs::path& projectDir, toml::ordered_value& globalConfig, QWidget* parent)
    : BasePage(parent), m_projectDir(projectDir), m_globalConfig(globalConfig)
{
    setWindowTitle(tr("项目设置主页"));
    setTitleVisible(false);

    try {
        m_projectConfig = toml::uoparse(m_projectDir / L"Config.toml");
    }
    catch (...) {
        m_projectConfig = toml::ordered_table{};
        ElaMessageBar::error(ElaMessageBarType::TopLeft,
            tr("解析失败"), tr("项目 %1 的配置文件不符合 toml 规范")
            .arg(QString::fromStdWString(m_projectDir.filename().wstring())), 3000);
    }
    insertToml(m_projectConfig, "GUIConfig.isRunning", false);
    m_dictExSettingsPage = new DictExSettingsPage(m_globalConfig, m_projectConfig, this);
}


void ProjectSettingsPage::apply2Config()
{
    if (m_apiSettingsPage) m_apiSettingsPage->apply2Config();
    if (m_pluginSettingsPage) m_pluginSettingsPage->apply2Config();
    if (m_commonSettingsPage) m_commonSettingsPage->apply2Config();
    if (m_paSettingsPage) m_paSettingsPage->apply2Config();
    if (m_nameTableSettingsPage) m_nameTableSettingsPage->apply2Config();
    if (m_dictSettingsPage) m_dictSettingsPage->apply2Config();
    if (m_dictExSettingsPage) m_dictExSettingsPage->apply2Config();
    if (m_startSettingsPage) m_startSettingsPage->apply2Config();
    if (m_otherSettingsPage) m_otherSettingsPage->apply2Config();
    if (m_promptSettingsPage) m_promptSettingsPage->apply2Config();

    saveProjectConfig();
}

void ProjectSettingsPage::saveProjectConfig()
{
    try {
        atomicOutputFile(m_projectDir / L"Config.toml", toml::format(m_projectConfig));
    }
    catch (const toml::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, ascii2Wide(e.what()).c_str(),
            tr("Toml 格式化错误").toStdWString().c_str(),MB_ICONERROR);
#endif
    }
}

void ProjectSettingsPage::refreshCommonDicts()
{
    m_dictExSettingsPage->refreshCommonDictsList();
}

QString ProjectSettingsPage::getProjectName()
{
    return QString::fromStdWString(m_projectDir.filename().wstring());
}

fs::path ProjectSettingsPage::getProjectDir()
{
    return m_projectDir;
}

void ProjectSettingsPage::clearLog(bool forceClear) {
    if (forceClear ||
        (m_stackedWidget->currentWidget() == m_startSettingsPage && m_startSettingsPage->isMainPageVisible())) {
        m_startSettingsPage->clearLog();
        ElaMessageBar::success(ElaMessageBarType::Bottom,
            tr("清理成功"), tr("已清空项目 %1 的日志输出窗口").arg(getProjectName()), 3000);
    }
}

void ProjectSettingsPage::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 15, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout* navigationLayout = new QHBoxLayout(centralWidget);
    m_settingsTitle = new ElaText(tr("Api设置"), centralWidget);
    m_settingsTitle->setContentsMargins(0, 10, 0, 0);
    m_settingsTitle->setTextPixelSize(18);
    m_settingsTitle->setFixedWidth(110);
    navigationLayout->addSpacing(30);
    navigationLayout->addWidget(m_settingsTitle);
    navigationLayout->addStretch();

    ElaMenu* foundamentalSettingMenu = new ElaMenu(centralWidget);
    QAction* apiSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::MagnifyingGlassPlus, tr("Api设置"));
    QAction* commonSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::BoxCheck, tr("一般设置"));
    QAction* paSettingAction = foundamentalSettingMenu->addElaIconAction(ElaIconType::Question, tr("问题分析"));

    ElaToolButton* foundamentalSettingButton = new ElaToolButton(centralWidget);
    foundamentalSettingButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    foundamentalSettingButton->setElaIcon(ElaIconType::Broom);
    foundamentalSettingButton->setText(tr("基本设置"));
    foundamentalSettingButton->setMenu(foundamentalSettingMenu);

    ElaMenu* transMenu = new ElaMenu(centralWidget);
    if (const std::string language = toml::find_or(m_globalConfig, "language", "zh_CN"); language == "en") {
        transMenu->setFixedWidth(145);
    }
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
    QAction* cacheProblemAction = menuBar->addElaIconAction(ElaIconType::BoxArchive, tr("缓存管理"));
    QAction* startTransAction = menuBar->addElaIconAction(ElaIconType::Play, tr("开始翻译"));
    QAction* otherSettingAction = menuBar->addElaIconAction(ElaIconType::Copy, tr("其他设置"));

    navigationLayout->addWidget(foundamentalSettingButton);
    navigationLayout->addWidget(transButton);
    navigationLayout->addWidget(menuBar);
    navigationLayout->addStretch();
    navigationLayout->addStretch();

    m_stackedWidget = new QStackedWidget(centralWidget);

    createPages();

    auto pageNavigation = [=]()
        {
            if (BasePage* page = qobject_cast<BasePage*>(m_stackedWidget->currentWidget())) {
                page->navigation(0);
            }
        };

    connect(apiSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(0);
            m_settingsTitle->setText(tr("Api设置"));
        });
    connect(commonSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(1);
            m_settingsTitle->setText(tr("一般设置"));
        });
    connect(paSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(2);
            m_settingsTitle->setText(tr("问题分析"));
        });
    connect(nameTableSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(3);
            m_settingsTitle->setText(tr("人名表"));
        });
    connect(dictSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(4);
            m_settingsTitle->setText(tr("项目字典"));
        });
    connect(dictExSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(5);
            m_settingsTitle->setText(tr("字典设置"));
        });
    connect(promptSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(6);
            m_settingsTitle->setText(tr("提示词"));
        });
    connect(pluginSettingAction, &QAction::triggered, this, [=]()
        {
            if (m_stackedWidget->currentIndex() == 7) {
                pageNavigation();
            }
            m_stackedWidget->setCurrentIndex(7);
            m_settingsTitle->setText(tr("插件管理"));
        });
    connect(cacheProblemAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(8);
            m_projectCachePage->ensureCacheFilesLoaded();
            m_settingsTitle->setText(tr("缓存管理"));
        });
    connect(startTransAction, &QAction::triggered, this, [=]()
        {
            if (m_stackedWidget->currentIndex() == 9) {
                pageNavigation();
            }
            m_stackedWidget->setCurrentIndex(9);
            m_settingsTitle->setText(tr("开始翻译"));
        });
    connect(otherSettingAction, &QAction::triggered, this, [=]()
        {
            m_stackedWidget->setCurrentIndex(10);
            m_settingsTitle->setText(tr("其他设置"));
        });

    mainLayout->addLayout(navigationLayout);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(m_stackedWidget);

    addCentralWidget(centralWidget, true, false, 0);
}

void ProjectSettingsPage::createPages()
{
    if (!m_apiSettingsPage) {
        m_apiSettingsPage = new ApiSettingsPage(m_projectConfig, m_stackedWidget);
    }
    if (!m_commonSettingsPage) {
        m_commonSettingsPage = new CommonSettingsPage(m_projectConfig, m_stackedWidget);
    }
    if (!m_paSettingsPage) {
        m_paSettingsPage = new PASettingsPage(m_projectConfig, m_stackedWidget);
    }
    if (!m_nameTableSettingsPage) {
        m_nameTableSettingsPage = new NameTableSettingsPage(m_projectDir, m_globalConfig, m_projectConfig, m_stackedWidget);
    }
    if (!m_dictSettingsPage) {
        m_dictSettingsPage = new DictSettingsPage(m_projectDir, m_globalConfig, m_projectConfig, m_stackedWidget);
    }
    if (!m_dictExSettingsPage) {
        m_dictExSettingsPage = new DictExSettingsPage(m_globalConfig, m_projectConfig, m_stackedWidget);
    }
    if (!m_promptSettingsPage) {
        m_promptSettingsPage = new PromptSettingsPage(m_projectDir, m_projectConfig, m_stackedWidget);
    }
    if (!m_pluginSettingsPage) {
        m_pluginSettingsPage = new PluginSettingsPage(m_projectDir, m_projectConfig, m_stackedWidget);
    }
    if (!m_projectCachePage) {
        m_projectCachePage = new ProjectCachePage(m_projectDir, m_projectConfig, m_stackedWidget);
    }
    if (!m_startSettingsPage) {
        m_startSettingsPage = new StartSettingsPage(m_projectDir, m_globalConfig, m_projectConfig, m_stackedWidget);
    }
    if (!m_otherSettingsPage) {
        m_otherSettingsPage = new OtherSettingsPage(m_projectDir, m_globalConfig, m_projectConfig, m_stackedWidget);
    }

    m_stackedWidget->addWidget(m_apiSettingsPage);
    m_stackedWidget->addWidget(m_commonSettingsPage);
    m_stackedWidget->addWidget(m_paSettingsPage);
    m_stackedWidget->addWidget(m_nameTableSettingsPage);
    m_stackedWidget->addWidget(m_dictSettingsPage);
    m_stackedWidget->addWidget(m_dictExSettingsPage);
    m_stackedWidget->addWidget(m_promptSettingsPage);
    m_stackedWidget->addWidget(m_pluginSettingsPage);
    m_stackedWidget->addWidget(m_projectCachePage);
    m_stackedWidget->addWidget(m_startSettingsPage);
    m_stackedWidget->addWidget(m_otherSettingsPage);

    if (m_startSettingsPage && m_otherSettingsPage) {
        connect(m_startSettingsPage, &StartSettingsPage::startTranslatingSignal, this, &ProjectSettingsPage::onStartTranslating);
        connect(m_startSettingsPage, &StartSettingsPage::finishTranslatingSignal, this, &ProjectSettingsPage::onFinishTranslating);
        connect(m_otherSettingsPage, &OtherSettingsPage::saveConfigSignal, this, &ProjectSettingsPage::apply2Config);
        connect(m_otherSettingsPage, &OtherSettingsPage::refreshProjectConfigSignal, this, &ProjectSettingsPage::onRefreshProjectConfig);
        connect(m_otherSettingsPage, &OtherSettingsPage::changeProjectNameSignal, this, [=](const QString& newProjectName)
            {
                Q_EMIT this->changeProjectNameSignal(this->property("ElaPageKey").toString(), newProjectName);
            });
    }
}

void ProjectSettingsPage::initialize()
{
    if (m_fullPagesInitialized) {
        return;
    }
    m_fullPagesInitialized = true;
    setupUi();
}

void ProjectSettingsPage::onRefreshProjectConfig()
{
    if (getIsRunning()) {
        ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("正在运行"), tr("项目仍在运行中，无法刷新配置"), 3000);
        return;
    }
    try {
        m_projectConfig = toml::uoparse(m_projectDir / L"Config.toml");
    }
    catch (...) {
        ElaMessageBar::error(ElaMessageBarType::TopLeft,
            tr("解析失败"), tr("项目 %1 的配置文件不符合规范")
            .arg(QString::fromStdWString(m_projectDir.filename().wstring())), 3000);
        return;
    }
    insertToml(m_projectConfig, "GUIConfig.isRunning", false);
    while (m_stackedWidget->count() > 0) {
        QWidget* widget = m_stackedWidget->currentWidget();
        m_stackedWidget->removeWidget(widget);
        widget->deleteLater();
    }
    m_apiSettingsPage = nullptr;
    m_pluginSettingsPage = nullptr;
    m_commonSettingsPage = nullptr;
    m_paSettingsPage = nullptr;
    m_nameTableSettingsPage = nullptr;
    m_dictSettingsPage = nullptr;
    m_dictExSettingsPage = nullptr;
    m_startSettingsPage = nullptr;
    m_otherSettingsPage = nullptr;
    m_promptSettingsPage = nullptr;
    m_projectCachePage = nullptr;
    createPages();
    m_stackedWidget->setCurrentIndex(10);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("刷新成功"), tr("项目配置刷新成功"), 3000);
}

void ProjectSettingsPage::onStartTranslating()
{
    insertToml(m_projectConfig, "GUIConfig.isRunning", true);
    this->apply2Config();
}

void ProjectSettingsPage::onFinishTranslating(const QString& transEngine, int exitCode)
{
    if (exitCode >= 0 &&
        toml::find_or(m_globalConfig, "autoRefreshAfterTranslate", true)
        )
    {
        if (transEngine == "DumpName" || transEngine == "NameTrans") {
            m_nameTableSettingsPage->refreshTable();
        }
        else if (transEngine == "GenDict") {
            m_dictSettingsPage->refreshGptDict();
        }
    }
    insertToml(m_projectConfig, "GUIConfig.isRunning", false);
    if (exitCode >= 0) {
        const auto writeTime = ProblemOverviewTracker::lastWriteTime(
            ProblemOverviewTracker::overviewPath(m_projectDir, m_projectConfig));
        if (writeTime) {
            insertToml(m_projectConfig, ProblemOverviewTracker::configKey(m_projectConfig), *writeTime);
        }
    }
    saveProjectConfig();
    Q_EMIT finishTranslatingSignal(this->property("ElaPageKey").toString());
}

bool ProjectSettingsPage::getIsRunning()
{
    return toml::find_or(m_projectConfig, "GUIConfig", "isRunning", true);
}
