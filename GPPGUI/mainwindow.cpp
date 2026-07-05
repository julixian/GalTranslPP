#include "mainwindow.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QFileDialog>
#include <QProcess>
#include <QApplication>
#include <QDesktopServices>
#include <QShortcut>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "ElaContentDialog.h"
#include "ElaDockWidget.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaMessageBar.h"
#include "ElaApplication.h"
#include "ElaInputDialog.h"
#include "AboutDialog.h"
#include "UpdateWidget.h"
#include "UpdateChecker.h"

#include "HomePage.h"
#include "DefaultPromptPage.h"
#include "CommonGptDictPage.h"
#include "CommonNormalDictPage.h"
#include "ProjectSettingsPage.h"
#include "AppSettingsPage.h"

import Tool;
import PythonManager;

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    if (fs::exists(L"BaseConfig/globalConfig.toml")) {
        try {
            m_globalConfig = toml::uoparse(fs::path(L"BaseConfig/globalConfig.toml"));
        }
        catch (...) {
#ifdef Q_OS_WIN
            MessageBoxW(nullptr, tr("解析错误").toStdWString().c_str(), tr("基本配置文件不符合 toml 规范！").toStdWString().c_str(), MB_ICONERROR);
#endif
        }
    }
    else {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, tr("不是哥们").toStdWString().c_str(), tr("有病吧，你把我软件的配置文件删了！？").toStdWString().c_str(), MB_ICONERROR);
#endif
    }

    setIsAllowPageOpenInNewWindow(false);

    initWindow();

    //额外布局
    initEdgeLayout();

    //中心窗口
    initContent();

    // 拦截默认关闭事件
    m_closeDialog = new ElaContentDialog(this);

    m_closeDialog->setLeftButtonText(tr("取消"));
    m_closeDialog->setMiddleButtonText(tr("最小化"));
    m_closeDialog->setRightButtonText(tr("确认"));
    QWidget* widget = new QWidget(m_closeDialog);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(15, 25, 15, 10);
    ElaText* confirmText = new ElaText(tr("退出"), widget);
    confirmText->setTextStyle(ElaTextType::Title);
    confirmText->setWordWrap(false);
    layout->addWidget(confirmText);
    layout->addSpacing(2);
    ElaText* subTitle = new ElaText(tr("确定要退出程序吗"), widget);
    subTitle->setTextStyle(ElaTextType::Body);
    layout->addWidget(subTitle);
    layout->addStretch();
    m_closeDialog->setCentralWidget(widget);

    connect(m_closeDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
        {
            onCloseWindowClicked(false);
        });
    connect(m_closeDialog, &ElaContentDialog::middleButtonClicked, this, [=]()
        {
            m_closeDialog->close();
            showMinimized();
        });
    this->setIsDefaultClosed(false);

    auto closeCallback = [=](bool closeByMessageCard)
        {
            if (
                !(toml::find_or(m_globalConfig, "allowCloseWhenRunning", false)) &&
                std::ranges::any_of(m_projectPages, [](const auto& page)
                    {
						if (page->getIsRunning()) {
							ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("警告"),
								tr("项目 %1 仍在运行，请先停止运行！").arg(page->getProjectName()), 3000);
							return true;
						}
                        return false;
                    })
                )
            {
                return;
            }
            if (closeByMessageCard) {
                onCloseWindowClicked(true);
            }
            else {
                m_closeDialog->exec();
            }
        };
    connect(this, &MainWindow::closeButtonClicked, this, [=]()
        {
            closeCallback(false);
        });

    connect(m_updateChecker, &UpdateChecker::applyUpdateAndRestartSignal, this, [=]()
        {
            closeCallback(true);
        });
    connect(m_updateChecker, &UpdateChecker::checkCompleteSignal, this, [=](bool hasNewVersion)
        {
            m_aboutDialog->setDownloadButtonEnabled(hasNewVersion && m_updateChecker->shouldDownloadButtonEnabled());
        });

    // 初始化提示
    ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("成功"), tr("初始化成功!"), 2000);
    if (fs::exists(L"cache")) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("检测到异常退出"),
            tr("请注意备份相关翻译缓存"), 3000);
    }
    else {
        fs::create_directories(L"cache");
    }
}

ProjectSettingsPage* MainWindow::createProjectSettingsPage(const fs::path& projectDir)
{
    QSharedPointer<ProjectSettingsPage> newPage(new ProjectSettingsPage(projectDir, m_globalConfig, this));
    connect(newPage.get(), &ProjectSettingsPage::finishTranslatingSignal, this, &MainWindow::onFinishTranslating);
    connect(newPage.get(), &ProjectSettingsPage::changeProjectNameSignal, this, [=](const QString& nodeKey, const QString& newProjectName)
        {
            setNavigationNodeTitle(nodeKey, newProjectName);
        });
    m_projectPages.push_back(newPage);
    addPageNode(QString::fromStdWString(projectDir.filename().wstring()), newPage.get(), m_projectExpanderKey, ElaIconType::Projector);
    return newPage.get();
}

MainWindow::~MainWindow()
{
    delete m_aboutDialog;
}

void MainWindow::initWindow()
{
    // 详见 ElaWidgetTool 开源项目的示例
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(QIcon(":/GPPGUI/Resource/Image/julixian_s.jpeg"));

    int width = toml::find_or(m_globalConfig, "windowWidth", 1450);
    int height = toml::find_or(m_globalConfig, "windowHeight", 770);
    resize(width, height - 30);
    int x = toml::find_or(m_globalConfig, "windowPosX", 0);
    int y = toml::find_or(m_globalConfig, "windowPosY", 0);
    if (x < 0)x = 0;
    if (y < 0)y = 0;
    move(x, y);

    setUserInfoCardPixmap(QPixmap(":/GPPGUI/Resource/Image/julixian_s.jpeg"));
    setUserInfoCardTitle(QString::fromStdString("Galtransl++ v" + GPPVERSION));
    setUserInfoCardSubTitle("tianquyesss@gmail.com");
    connect(this, &MainWindow::userInfoCardClicked, this, [=]()
        {
            QDesktopServices::openUrl(QUrl("https://github.com/julixian"));
        });

    setWindowTitle("Galtransl++");

    setNavigationBarWidth(250);
}

void MainWindow::initEdgeLayout()
{
    //菜单栏
    ElaMenuBar* menuBar = new ElaMenuBar(this);
    menuBar->setFixedHeight(30);
    QWidget* customWidget = new QWidget(this);
    int customWidgetWidth = 595;
    if (const std::string language = toml::find_or(m_globalConfig, "language", "zh_CN"); language == "en") {
        customWidgetWidth = 490;
    }
    customWidget->setFixedWidth(customWidgetWidth);
    QVBoxLayout* customLayout = new QVBoxLayout(customWidget);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->addWidget(menuBar);
    this->setCustomWidget(ElaAppBarType::MiddleArea, customWidget);

    QAction* newProjectAction = menuBar->addElaIconAction(ElaIconType::AtomSimple, tr("新建项目"));
    QAction* openProjectAction = menuBar->addElaIconAction(ElaIconType::FolderOpen, tr("打开项目"));
    QAction* saveProjectAction = menuBar->addElaIconAction(ElaIconType::FloppyDisk, tr("保存项目配置"));
    QAction* removeProjectAction = menuBar->addElaIconAction(ElaIconType::TrashCan, tr("移除项目"));
    QAction* deleteProjectAction = menuBar->addElaIconAction(ElaIconType::TrashXmark, tr("删除项目"));

    connect(newProjectAction, &QAction::triggered, this, &MainWindow::onNewProjectTriggered);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::onOpenProjectTriggered);
    connect(saveProjectAction, &QAction::triggered, this, &MainWindow::onSaveProjectTriggered);
    connect(removeProjectAction, &QAction::triggered, this, &MainWindow::onRemoveProjectTriggered);
    connect(deleteProjectAction, &QAction::triggered, this, &MainWindow::onDeleteProjectTriggered);

    //状态栏
    ElaStatusBar* statusBar = new ElaStatusBar(this);
    ElaText* statusText = new ElaText(tr("初始化成功！"), this);
    statusText->setTextPixelSize(14);
    statusText->setWordWrap(false);
    statusBar->addWidget(statusText);
    this->setStatusBar(statusBar);

    m_updateChecker = new UpdateChecker(m_globalConfig, statusText, this);

    //停靠窗口
    ElaDockWidget* updateDockWidget = new ElaDockWidget(tr("更新内容"), this);
    updateDockWidget->setWidget(new UpdateWidget(this));
    this->addDockWidget(Qt::RightDockWidgetArea, updateDockWidget);
    resizeDocks({ updateDockWidget }, { 200 }, Qt::Horizontal);
    std::string gppversion = GPPVERSION;
    std::erase_if(gppversion, [](char ch) { return ch == '.'; });
    updateDockWidget->setVisible(toml::find_or(m_globalConfig, "showDockWidget", gppversion, true));
    insertToml(m_globalConfig, "showDockWidget", toml::ordered_table{});
    insertToml(m_globalConfig, "showDockWidget." + gppversion, updateDockWidget->isVisible());
    connect(updateDockWidget, &ElaDockWidget::visibilityChanged, this, [=](bool visible)
        {
            insertToml(m_globalConfig, "showDockWidget", toml::ordered_table{});
            insertToml(m_globalConfig, "showDockWidget." + gppversion, visible);
        });

    // 右键菜单
    ElaMenu* appBarMenu = new ElaMenu(this);
    appBarMenu->setMenuItemHeight(27);
    appBarMenu->setFixedWidth(200);
    // 召唤停靠窗口
    connect(appBarMenu->addElaIconAction(ElaIconType::BellConcierge, tr("召唤停靠窗口")), &QAction::triggered, this, [=]()
        {
            updateDockWidget->show();
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::GearComplex, tr("应用设置")), &QAction::triggered, this, [=]()
        {
            navigation(m_appSettingsKey);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::MoonStars, tr("更改程序主题")), &QAction::triggered, this, [=]()
        {
            eTheme->setThemeMode(eTheme->getThemeMode() == ElaThemeType::Light ? ElaThemeType::Dark : ElaThemeType::Light);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::BroomWide, tr("清空当前项目翻译日志")), &QAction::triggered, this, [=]()
	    {
            onClearLog(true);
	    });

    setCustomMenu(appBarMenu);
}

void MainWindow::initContent()
{
    m_homePage = new HomePage(m_globalConfig, this);
    m_defaultPromptPage = new DefaultPromptPage(this);

    m_commonPreDictPage = new CommonNormalDictPage("pre", m_globalConfig, this);
    m_commonGptDictPage = new CommonGptDictPage(m_globalConfig, this);
    m_commonPostDictPage = new CommonNormalDictPage("post", m_globalConfig, this);

    m_appSettingsPage = new AppSettingsPage(m_globalConfig, this);

    addPageNode(tr("主页"), m_homePage, ElaIconType::House);

    addPageNode(tr("默认提示词管理"), m_defaultPromptPage, ElaIconType::Text);

    addExpanderNode(tr("通用字典管理"), m_commonDictExpanderKey, ElaIconType::FontCase);
    auto refreshCommonDicts = [=]()
        {
            for (auto& page : m_projectPages) {
                page->refreshCommonDicts();
            }
        };
    addPageNode(tr("通用译前字典"), m_commonPreDictPage, m_commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(m_commonPreDictPage, &CommonNormalDictPage::commonDictsChangedSignal, this, refreshCommonDicts);
    addPageNode(tr("通用GPT字典"), m_commonGptDictPage, m_commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(m_commonGptDictPage, &CommonGptDictPage::commonDictsChangedSignal, this, refreshCommonDicts);
    addPageNode(tr("通用译后字典"), m_commonPostDictPage, m_commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(m_commonPostDictPage, &CommonNormalDictPage::commonDictsChangedSignal, this, refreshCommonDicts);

    addExpanderNode(tr("项目管理"), m_projectExpanderKey, ElaIconType::BriefcaseBlank);
    const auto projects = toml::find_or_default<toml::array>(m_globalConfig, "projects");
    for (const auto& project : projects) {
            if (project.is_string()) {
                fs::path projectDir(ascii2Wide(project.as_string()));
                if (!fs::exists(projectDir / L"config.toml")) {
                    continue;
                }
                createProjectSettingsPage(projectDir);
            }
    }
    expandNavigationNode(m_projectExpanderKey);

    addFooterNode(tr("使用说明"), nullptr, m_transIllustrationKey, 0, ElaIconType::BookOpen);
    addFooterNode(tr("关于"), nullptr, m_aboutKey, 0, ElaIconType::User);
    m_aboutDialog = new AboutDialog();
    m_aboutDialog->hide();
    addFooterNode(tr("应用设置"), m_appSettingsPage, m_appSettingsKey, 0, ElaIconType::GearComplex);

    connect(m_aboutDialog, &AboutDialog::checkUpdateSignal, this, [=]()
        {
            ElaMessageBar::information(ElaMessageBarType::TopLeft, tr("请稍候"), tr("正在检查更新..."), 3000);
            m_updateChecker->check();
        });
    connect(m_aboutDialog, &AboutDialog::downloadUpdateSignal, this, [=]()
        {
            m_aboutDialog->setDownloadButtonEnabled(false);
            ElaMessageBar::information(ElaMessageBarType::TopLeft, tr("请稍候"), tr("正在下载更新..."), 3000);
            m_updateChecker->check(true);
        });

    connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, const QString& nodeKey)
        {
            if (m_transIllustrationKey == nodeKey) {
                QDesktopServices::openUrl(QUrl::fromLocalFile("BaseConfig/illustration/foundation.html"));
            }
            else if (m_aboutKey == nodeKey)
            {
                m_aboutDialog->setFixedSize(400, 400);
                m_aboutDialog->moveToCenter();
                m_aboutDialog->show();
            }
        });

    const std::string clearShortcut = toml::find_or(m_globalConfig, "clearLogShortcut", "Ctrl+L");
    m_clearLogShortcut = new QShortcut(QKeySequence(QString::fromStdString(clearShortcut)), this);
    connect(m_clearLogShortcut, &QShortcut::activated, this, [=]()
	    {
            onClearLog(false);
	    });
}

void MainWindow::onClearLog(bool forceClear) {
    QString pageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == m_projectPages.end()) {
        return;
    }
    it->get()->clearLog(forceClear);
}

void MainWindow::onNewProjectTriggered()
{
    const QString parentPath = QFileDialog::getExistingDirectory(this, tr("选择新项目的存放位置"),
        QString::fromStdString(toml::find_or(m_globalConfig, "lastProjectPath", "./Projects")));
    if (parentPath.isEmpty()) {
        return;
    }

    insertToml(m_globalConfig, "lastProjectPath", parentPath.toStdString());

    QString projectName;
    ElaInputDialog inputDialog(tr("请输入项目名称"), tr("新建项目"), projectName, this);
    if (inputDialog.exec() != QDialog::Accepted) {
        return;
    }

    if (std::ranges::any_of(m_projectPages, [&](auto& page)
        {
            return projectName == page->getProjectName();
        }))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("已存在同名项目！"), 3000);
        return;
    }

    if (projectName.isEmpty() || projectName.contains("/") || projectName.contains("\\"))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("项目名称不能为空，且不能包含斜杠或反斜杠！"), 3000);
        return;
    }

    const fs::path newProjectDir = fs::path(parentPath.toStdWString()) / projectName.toStdWString();
    if (fs::exists(newProjectDir)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("目录下存在同名文件或文件夹！"), 3000);
        return;
    }

    fs::create_directories(newProjectDir);

    QFile resourceFile(":/GPPGUI/Resource/sampleProject.zip");
    if (resourceFile.open(QIODevice::ReadOnly)) {
        QFile outputFile(parentPath + "/" + projectName + "/sampleProject.zip");
        if (outputFile.open(QIODevice::WriteOnly)) {
            outputFile.write(resourceFile.readAll());
            outputFile.close();
        }
        else {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("无法创建新文件！"), 3000);
            return;
        }
    }
    else {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("无法读取模板文件！"), 3000);
        return;
    }

    extractZip(newProjectDir / L"sampleProject.zip", newProjectDir);

    try {
        fs::remove(newProjectDir / L"sampleProject.zip");

        if (fs::exists(L"BaseConfig/Prompt.toml")) {
            fs::copy(L"BaseConfig/Prompt.toml", newProjectDir / L"Prompt.toml", fs::copy_options::overwrite_existing);
        }

        toml::ordered_value configData = toml::uoparse(newProjectDir / L"config.toml");

        auto addCommonDictsToProjectConfig = [&](const std::string& globalConfigKey, const std::string& projectConfigKey)
            {
                auto& globalDictNames = m_globalConfig[globalConfigKey]["dictNames"];
                auto& projectDictNames = configData["dictionary"][projectConfigKey];
                if (globalDictNames.is_array() && projectDictNames.is_array()) {
                    for (const auto& dictName : globalDictNames.as_array()) {
                        if (!dictName.is_string()) {
                            continue;
                        }
                        if (!toml::find_or(m_globalConfig, globalConfigKey, "spec", dictName.as_string(), "defaultOn", true)) {
                            continue;
                        }
                        projectDictNames.push_back(dictName.as_string() + ".toml");
                    }
                }
                else {
                    globalDictNames = toml::array{};
                    projectDictNames = toml::array{};
                }
            };

        addCommonDictsToProjectConfig("commonPreDicts", "preDict");
        addCommonDictsToProjectConfig("commonGptDicts", "gptDict");
        addCommonDictsToProjectConfig("commonPostDicts", "postDict");

        std::ofstream ofs(newProjectDir / L"config.toml", std::ios::binary);
        ofs << configData;
        ofs.close();
    }
    catch (...) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("无法写入配置文件！"), 3000);
        return;
    }

    auto renameDictFileFunc = [&](const QString& orgName, const QString& newName)
        {
            if (orgName == newName) {
                return;
            }
            try {
                fs::rename(newProjectDir / (orgName.toStdWString() + L".toml"), newProjectDir / (newName.toStdWString() + L".toml"));
            }
            catch (...) {

            }
        };
    renameDictFileFunc("项目GPT字典", tr("项目GPT字典"));
    renameDictFileFunc("项目译前字典", tr("项目译前字典"));
    renameDictFileFunc("项目译后字典", tr("项目译后字典"));

    ProjectSettingsPage* newPage = createProjectSettingsPage(newProjectDir);
    this->navigation(newPage->property("ElaPageKey").toString());

    const QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(newProjectDir.wstring()));
    QDesktopServices::openUrl(dirUrl);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("创建成功"), tr("请将待翻译的文件放入 gt_input 中！"), 8000);
}

void MainWindow::onOpenProjectTriggered()
{
    const QString projectPath = QFileDialog::getExistingDirectory(this, tr("选择已有项目的文件夹路径"),
        QString::fromStdString(toml::find_or(m_globalConfig, "lastProjectPath", "./Projects")));
    if (projectPath.isEmpty()) {
        return;
    }
    insertToml(m_globalConfig, "lastProjectPath", projectPath.toStdString());
    const fs::path projectDir(projectPath.toStdWString());
    if (!fs::exists(projectDir / L"config.toml")) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("打开失败"), tr("目录下不存在 config.toml 文件！"), 3000);
        return;
    }

    const QString projectName = QString::fromStdWString(projectDir.filename().wstring());

    if (std::ranges::any_of(m_projectPages, [&](auto& page)
        {
            return page->getProjectName() == projectName;
        }))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("打开失败"), tr("已存在同名项目！"), 3000);
        return;
    }

    ProjectSettingsPage* newPage = createProjectSettingsPage(projectDir);
    this->navigation(newPage->property("ElaPageKey").toString());

	const QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(projectDir.wstring()));
	QDesktopServices::openUrl(dirUrl);
	ElaMessageBar::success(ElaMessageBarType::TopRight, tr("打开成功"),
		tr("%1 已纳入项目管理！").arg(newPage->getProjectName()), 3000);
}

void MainWindow::onRemoveProjectTriggered()
{
    QString pageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == m_projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移除失败"), tr("当前页面不是项目页面！"), 3000);
        return;
    }
    if (it->get()->getIsRunning()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移除失败"), tr("当前项目正在运行，请先停止运行！"), 3000);
        return;
    }
    ElaContentDialog helpDialog(this);

    helpDialog.setRightButtonText(tr("是"));
    helpDialog.setMiddleButtonText(tr("思考人生"));
    helpDialog.setLeftButtonText(tr("否"));

    QWidget* widget = new QWidget(&helpDialog);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(15, 25, 15, 10);
    ElaText* confirmText = new ElaText(tr("你确定要移除当前项目吗？"), widget);
    confirmText->setTextStyle(ElaTextType::Title);
    confirmText->setWordWrap(false);
    layout->addWidget(confirmText);
    layout->addSpacing(2);
    ElaText* subTitle = new ElaText(tr("从项目管理中移除该项目，但不会删除其项目文件夹"), widget);
    subTitle->setTextStyle(ElaTextType::Body);
    layout->addWidget(subTitle);
    layout->addStretch();
    helpDialog.setCentralWidget(widget);

    if (helpDialog.exec() == QDialog::Accepted) {
        QString projectName = it->get()->getProjectName();
        it->get()->apply2Config();
        removeNavigationNode(pageKey);
        m_projectPages.erase(it);
        if (getCurrentNavigationPageKey() == m_appSettingsPage->property("ElaPageKey").toString()) {
            if (m_projectPages.empty()) {
                this->navigation(m_homePage->property("ElaPageKey").toString());
            }
            else {
                this->navigation(m_projectPages.back()->property("ElaPageKey").toString());
            }
		}
		ElaMessageBar::success(ElaMessageBarType::TopRight, tr("移除成功"),
			tr("项目 %1 已从项目管理中移除！").arg(projectName), 3000);
	}
}

void MainWindow::onDeleteProjectTriggered()
{
    const QString pageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == m_projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), tr("当前页面不是项目页面！"), 3000);
        return;
    }
    if (it->get()->getIsRunning()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), tr("当前项目正在运行，请先停止运行！"), 3000);
        return;
    }
    ElaContentDialog helpDialog(this);

    helpDialog.setRightButtonText(tr("是"));
    helpDialog.setMiddleButtonText(tr("思考人生"));
    helpDialog.setLeftButtonText(tr("否"));

    QWidget* widget = new QWidget(&helpDialog);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(15, 25, 15, 10);
    ElaText* confirmText = new ElaText(tr("你确定要删除当前项目吗？                "), widget);
    confirmText->setTextStyle(ElaTextType::Title);
    confirmText->setWordWrap(false);
    layout->addWidget(confirmText);
    layout->addSpacing(2);
    ElaText* subTitle = new ElaText(tr("将删除该项目的项目文件夹，如果不备份，再次翻译将必须从头开始！"), widget);
    subTitle->setTextStyle(ElaTextType::Body);
    layout->addWidget(subTitle);
    layout->addStretch();
    helpDialog.setCentralWidget(widget);

    if (helpDialog.exec() == QDialog::Accepted) {
        const fs::path projectDir = it->get()->getProjectDir();
        const QString projectName = it->get()->getProjectName();
        try {
            fs::remove_all(projectDir);
        }
        catch (const fs::filesystem_error& e) {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), e.what(), 3000);
            return;
        }
        removeNavigationNode(pageKey);
        m_projectPages.erase(it);
        if (getCurrentNavigationPageKey() == m_appSettingsPage->property("ElaPageKey").toString()) {
            if (m_projectPages.empty()) {
                this->navigation(m_homePage->property("ElaPageKey").toString());
            }
            else {
                this->navigation(m_projectPages.back()->property("ElaPageKey").toString());
            }
		}
		ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"),
			tr("项目 %1 已从项目管理和磁盘中移除！").arg(projectName), 3000);
	}
}

void MainWindow::onSaveProjectTriggered()
{
    const QString pageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == m_projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("保存失败"), tr("当前页面不是项目页面！"), 3000);
        return;
    }

	it->get()->apply2Config();
	ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"),
		tr("项目 %1 配置信息已保存！").arg(it->get()->getProjectName()), 3000);
}

void MainWindow::onFinishTranslating(const QString& nodeKey)
{
    setNodeKeyPoints(nodeKey, getNodeKeyPoints(nodeKey) + 1);
}

void MainWindow::onCloseWindowClicked(bool restart)
{
    m_defaultPromptPage->apply2Config();
    m_commonPreDictPage->apply2Config();
    m_commonGptDictPage->apply2Config();
    m_commonPostDictPage->apply2Config();

    toml::array projects;
    for (auto& page : m_projectPages) {
        page->apply2Config();
        projects.push_back(wide2Ascii(page->getProjectDir()));
    }
    insertToml(m_globalConfig, "projects", projects);

    m_appSettingsPage->apply2Config();
    insertToml(m_globalConfig, "clearLogShortcut", m_clearLogShortcut->key().toString().toStdString());

    std::ofstream ofs(L"BaseConfig/globalConfig.toml", std::ios::binary);
    ofs << m_globalConfig;
    ofs.close();

    if (m_updateChecker->shouldStartUpdater()) {
        QStringList arguments;
        arguments << "--pid" << QString::number(QApplication::applicationPid());
        arguments << "--source" << QApplication::applicationDirPath() + "/GUICORE.7z";
        arguments << "--target" << QApplication::applicationDirPath();
        if (restart) {
            arguments << "--restart" << QApplication::applicationFilePath();
        }
        QProcess::startDetached("Updater.exe", arguments, QDir::currentPath());
    }
    MainWindow::closeWindow();
}

void MainWindow::checkUpdate()
{
    m_updateChecker->check();
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (getCurrentNavigationIndex() != 2)
    {
        switch (event->button())
        {
        case Qt::BackButton:
        {
            this->setCurrentStackIndex(0);
            break;
        }
        case Qt::ForwardButton:
        {
            this->setCurrentStackIndex(1);
            break;
        }
        default:
        {
            break;
        }
        }
    }
    ElaWindow::mouseReleaseEvent(event);
}
