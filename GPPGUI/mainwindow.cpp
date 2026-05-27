#include "mainwindow.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QFileDialog>
#include <QProcess>
#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QShortcut>

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
#include "SettingPage.h"

import Tool;
import PythonManager;

MainWindow::MainWindow(std::unique_ptr<py::gil_scoped_release>& release, QWidget* parent)
    : ElaWindow(parent), _release(release)
{
    if (fs::exists(L"BaseConfig/globalConfig.toml")) {
        try {
            _globalConfig = toml::uoparse(fs::path(L"BaseConfig/globalConfig.toml"));
        }
        catch (...) {
            QMessageBox::critical(this, tr("解析错误"), tr("基本配置文件不符合 toml 规范！"), QMessageBox::Ok);
        }
    }
    else {
        QMessageBox::critical(this, tr("不是哥们"), tr("有病吧，你把我软件的配置文件删了！？"), QMessageBox::Ok);
    }

    setIsAllowPageOpenInNewWindow(false);

    initWindow();

    //额外布局
    initEdgeLayout();

    //中心窗口
    initContent();

    // 拦截默认关闭事件
    _closeDialog = new ElaContentDialog(this);

    _closeDialog->setLeftButtonText(tr("取消"));
    _closeDialog->setMiddleButtonText(tr("最小化"));
    _closeDialog->setRightButtonText(tr("确认"));
    QWidget* widget = new QWidget(_closeDialog);
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
    _closeDialog->setCentralWidget(widget);

    connect(_closeDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
        {
            _onCloseWindowClicked(false);
        });
    connect(_closeDialog, &ElaContentDialog::middleButtonClicked, this, [=]() 
        {
            _closeDialog->close();
            showMinimized();
        });
    this->setIsDefaultClosed(false);

    auto closeCallback = [=](bool closeByMessageCard)
        {
            if (
                !(toml::find_or(_globalConfig, "allowCloseWhenRunning", false)) &&
                std::ranges::any_of(_projectPages, [](const auto& page)
                    {
                        if (page->getIsRunning()) {
                            ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("警告"),
                                tr("项目 ") + page->getProjectName() + tr(" 仍在运行，请先停止运行！"), 3000);
                            return true;
                        }
                        return false;
                    })
                )
            {
                return;
            }
            if (closeByMessageCard) {
                _onCloseWindowClicked(true);
            }
            else {
                _closeDialog->exec();
            }
        };
    connect(this, &MainWindow::closeButtonClicked, this, [=]()
        {
            closeCallback(false);
        });

    connect(_updateChecker, &UpdateChecker::applyUpdateAndRestartSignal, this, [=]()
        {
            closeCallback(true);
        });
    connect(_updateChecker, &UpdateChecker::checkCompleteSignal, this, [=](bool hasNewVersion)
        {
            _aboutDialog->setDownloadButtonEnabled(hasNewVersion && !_updateChecker->getIsDownloading() && !_updateChecker->getIsDownloadSucceed());
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

ProjectSettingsPage* MainWindow::_createProjectSettingsPage(const fs::path& projectDir)
{
    QSharedPointer<ProjectSettingsPage> newPage(new ProjectSettingsPage(projectDir, _globalConfig, this));
    connect(newPage.get(), &ProjectSettingsPage::finishTranslatingSignal, this, &MainWindow::_onFinishTranslating);
    connect(newPage.get(), &ProjectSettingsPage::changeProjectNameSignal, this, [=](QString nodeKey, QString newProjectName)
        {
            setNavigationNodeTitle(nodeKey, newProjectName);
        });
    _projectPages.push_back(newPage);
    addPageNode(QString::fromStdWString(projectDir.filename().wstring()), newPage.get(), _projectExpanderKey, ElaIconType::Projector);
    return newPage.get();
}

MainWindow::~MainWindow()
{
    delete this->_aboutDialog;
}

void MainWindow::initWindow()
{
    // 详见 ElaWidgetTool 开源项目的示例
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(QIcon(":/GPPGUI/Resource/Image/julixian_s.jpeg"));

    int width = toml::find_or(_globalConfig, "windowWidth", 1450);
    int height = toml::find_or(_globalConfig, "windowHeight", 770);
    resize(width, height - 30);
    int x = toml::find_or(_globalConfig, "windowPosX", 0);
    int y = toml::find_or(_globalConfig, "windowPosY", 0);
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
    customWidget->setFixedWidth(595);
    QVBoxLayout* customLayout = new QVBoxLayout(customWidget);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->addWidget(menuBar);
    this->setCustomWidget(ElaAppBarType::MiddleArea, customWidget);

    QAction* newProjectAction = menuBar->addElaIconAction(ElaIconType::AtomSimple, tr("新建项目"));
    QAction* openProjectAction = menuBar->addElaIconAction(ElaIconType::FolderOpen, tr("打开项目"));
    QAction* saveProjectAction = menuBar->addElaIconAction(ElaIconType::FloppyDisk, tr("保存项目配置"));
    QAction* removeProjectAction = menuBar->addElaIconAction(ElaIconType::TrashCan, tr("移除项目"));
    QAction* deleteProjectAction = menuBar->addElaIconAction(ElaIconType::TrashXmark, tr("删除项目"));

    connect(newProjectAction, &QAction::triggered, this, &MainWindow::_onNewProjectTriggered);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::_onOpenProjectTriggered);
    connect(saveProjectAction, &QAction::triggered, this, &MainWindow::_onSaveProjectTriggered);
    connect(removeProjectAction, &QAction::triggered, this, &MainWindow::_onRemoveProjectTriggered);
    connect(deleteProjectAction, &QAction::triggered, this, &MainWindow::_onDeleteProjectTriggered);

    //状态栏
    ElaStatusBar* statusBar = new ElaStatusBar(this);
    ElaText* statusText = new ElaText(tr("初始化成功！"), this);
    statusText->setTextPixelSize(14);
    statusText->setWordWrap(false);
    statusBar->addWidget(statusText);
    this->setStatusBar(statusBar);

    _updateChecker = new UpdateChecker(_globalConfig, statusText, this);

    //停靠窗口
    ElaDockWidget* updateDockWidget = new ElaDockWidget(tr("更新内容"), this);
    updateDockWidget->setWidget(new UpdateWidget(this));
    this->addDockWidget(Qt::RightDockWidgetArea, updateDockWidget);
    resizeDocks({ updateDockWidget }, { 200 }, Qt::Horizontal);
    std::string gppversion = GPPVERSION;
    std::erase_if(gppversion, [](char ch) { return ch == '.'; });
    updateDockWidget->setVisible(toml::find_or(_globalConfig, "showDockWidget", gppversion, true));
    insertToml(_globalConfig, "showDockWidget", toml::ordered_table{});
    insertToml(_globalConfig, "showDockWidget." + gppversion, updateDockWidget->isVisible());
    connect(updateDockWidget, &ElaDockWidget::visibilityChanged, this, [=](bool visible)
        {
            insertToml(_globalConfig, "showDockWidget", toml::ordered_table{});
            insertToml(_globalConfig, "showDockWidget." + gppversion, visible);
        });

    // 右键菜单
    ElaMenu* appBarMenu = new ElaMenu(this);
    appBarMenu->setMenuItemHeight(27);
    // 召唤停靠窗口
    connect(appBarMenu->addElaIconAction(ElaIconType::BellConcierge, tr("召唤停靠窗口")), &QAction::triggered, this, [=]()
        {
            updateDockWidget->show();
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::GearComplex, tr("设置")), &QAction::triggered, this, [=]() 
        {
            navigation(_settingKey);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::MoonStars, tr("更改程序主题")), &QAction::triggered, this, [=]() 
        {
            eTheme->setThemeMode(eTheme->getThemeMode() == ElaThemeType::Light ? ElaThemeType::Dark : ElaThemeType::Light);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::BroomWide, tr("清空当前项目翻译日志")), &QAction::triggered, this, [=]()
	    {
            _onClearLog(true);
	    });

    setCustomMenu(appBarMenu);
}

void MainWindow::initContent()
{
    _homePage = new HomePage(_globalConfig, this);
    _defaultPromptPage = new DefaultPromptPage(this);

    _commonPreDictPage = new CommonNormalDictPage("pre", _globalConfig, this);
    _commonGptDictPage = new CommonGptDictPage(_globalConfig, this);
    _commonPostDictPage = new CommonNormalDictPage("post", _globalConfig, this);
    
    _settingPage = new SettingPage(_globalConfig, this);
    //connect(_settingPage, &SettingPage::restartPythonEnvSignal, this, [=](QString pyEnvPathQStr)
    //    {
    //        bool running = std::ranges::any_of(_projectPages, [](auto& page)
    //            {
    //                return page->getIsRunning();
    //            });
    //        if (running) {
    //            ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("警告"), tr("有项目正在运行，请先停止运行！"), 3000);
    //            return;
    //        }
    //        try {
    //            shutDownPythonEnv(_release);
    //            startUpPythonEnv(pyEnvPathQStr.toStdWString(), _release);
    //            ElaMessageBar::success(ElaMessageBarType::TopRight, tr("成功"), tr("Python虚拟环境已重启"), 3000);
    //        }
    //        catch (...) {
    //            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("失败"), tr("Python虚拟环境重启失败"), 3000);
    //        }
    //    });

    addPageNode(tr("主页"), _homePage, ElaIconType::House);

    addPageNode(tr("默认提示词管理"), _defaultPromptPage, ElaIconType::Text);

    addExpanderNode(tr("通用字典管理"), _commonDictExpanderKey, ElaIconType::FontCase);
    auto refreshCommonDicts = [=]()
        {
            for (auto& page : _projectPages) {
                page->refreshCommonDicts();
            }
        };
    addPageNode(tr("通用译前字典"), _commonPreDictPage, _commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(_commonPreDictPage, &CommonNormalDictPage::commonDictsChanged, this, refreshCommonDicts);
    addPageNode(tr("通用GPT字典"), _commonGptDictPage, _commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(_commonGptDictPage, &CommonGptDictPage::commonDictsChanged, this, refreshCommonDicts);
    addPageNode(tr("通用译后字典"), _commonPostDictPage, _commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(_commonPostDictPage, &CommonNormalDictPage::commonDictsChanged, this, refreshCommonDicts);

    addExpanderNode(tr("项目管理"), _projectExpanderKey, ElaIconType::BriefcaseBlank);
    const auto projects = toml::find_or_default<toml::array>(_globalConfig, "projects");
    for (const auto& project : projects) {
            if (project.is_string()) {
                fs::path projectDir(ascii2Wide(project.as_string()));
                if (!fs::exists(projectDir / L"config.toml")) {
                    continue;
                }
                _createProjectSettingsPage(projectDir);
            }
    }
    expandNavigationNode(_projectExpanderKey);

    addFooterNode(tr("使用说明"), nullptr, _transIllustrationKey, 0, ElaIconType::BookOpen);
    addFooterNode(tr("关于"), nullptr, _aboutKey, 0, ElaIconType::User);
    _aboutDialog = new AboutDialog();
    _aboutDialog->hide();
    addFooterNode(tr("设置"), _settingPage, _settingKey, 0, ElaIconType::GearComplex);

    connect(_aboutDialog, &AboutDialog::checkUpdateSignal, this, [=]()
        {
            ElaMessageBar::information(ElaMessageBarType::TopLeft, tr("请稍候"), tr("正在检查更新..."), 3000);
            _updateChecker->check();
        });
    connect(_aboutDialog, &AboutDialog::downloadUpdateSignal, this, [=]()
        {
            _aboutDialog->setDownloadButtonEnabled(false);
            ElaMessageBar::information(ElaMessageBarType::TopLeft, tr("请稍候"), tr("正在下载更新..."), 3000);
            _updateChecker->check(true);
        });

    connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) 
        {
            if (_transIllustrationKey == nodeKey) {
                QDesktopServices::openUrl(QUrl::fromLocalFile("BaseConfig/illustration/foundation.html"));
            }
            else if (_aboutKey == nodeKey)
            {
                _aboutDialog->setFixedSize(400, 400);
                _aboutDialog->moveToCenter();
                _aboutDialog->show();
            }
        });

    /*connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey)
        {
            auto it = std::ranges::find_if(_projectPages, [&](auto& page)
                {
                    return page->property("ElaPageKey").toString() == nodeKey;
                });
            if (it != _projectPages.end()) {
                setWindowTitle("Galtransl++ - " + (*it)->getProjectName());
            }
            else {
                setWindowTitle("Galtransl++");
            }
        });*/

    const std::string clearShortcut = toml::find_or(_globalConfig, "clearLogShortcut", "Ctrl+L");
    _clearLogShortcut = new QShortcut(QKeySequence(QString::fromStdString(clearShortcut)), this);
    connect(_clearLogShortcut, &QShortcut::activated, this, [=]()
	    {
            _onClearLog(false);
	    });
}

void MainWindow::_onClearLog(bool forceClear) {
    QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
        return;
    }
    it->get()->clearLog(forceClear);
}

void MainWindow::_onNewProjectTriggered()
{
    QString parentPath = QFileDialog::getExistingDirectory(this, tr("选择新项目的存放位置"), 
        QString::fromStdString(toml::find_or(_globalConfig, "lastProjectPath", "./Projects")));
    if (parentPath.isEmpty()) {
        return;
    }

    insertToml(_globalConfig, "lastProjectPath", parentPath.toStdString());
    QString projectName;
    bool ok;
    ElaInputDialog inputDialog(this, tr("请输入项目名称"), tr("新建项目"), projectName, &ok);
    inputDialog.exec();

    if (!ok) {
        return;
    }

    if (std::ranges::any_of(_projectPages, [&](auto& page)
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

    fs::path newProjectDir = fs::path(parentPath.toStdWString()) / projectName.toStdWString();
    if (fs::exists(newProjectDir)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("目录下存在同名文件或文件夹！"), 3000);
        return;
    }

    fs::create_directories(newProjectDir);

    QFile file(":/GPPGUI/Resource/sampleProject.zip");
    if (file.open(QIODevice::ReadOnly)) {
        QFile newFile(parentPath + "/" + projectName + "/sampleProject.zip");
        if (newFile.open(QIODevice::WriteOnly)) {
            newFile.write(file.readAll());
            newFile.close();
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
    fs::remove(newProjectDir / L"sampleProject.zip");

    if (fs::exists(L"BaseConfig/Prompt.toml")) {
        fs::copy(L"BaseConfig/Prompt.toml", newProjectDir / L"Prompt.toml", fs::copy_options::overwrite_existing);
    }

    try {
        toml::ordered_value configData = toml::uoparse(newProjectDir / L"config.toml");

        auto addCommonDictsToProjectConfig = [&](const std::string& globalConfigKey, const std::string& projectConfigKey)
            {
                auto& globalDictNames = _globalConfig[globalConfigKey]["dictNames"];
                auto& projectDictNames = configData["dictionary"][projectConfigKey];
                if (globalDictNames.is_array() && projectDictNames.is_array()) {
                    for (const auto& dictName : globalDictNames.as_array()) {
                        if (!dictName.is_string()) {
                            continue;
                        }
                        if (!toml::find_or(_globalConfig, globalConfigKey, "spec", dictName.as_string(), "defaultOn", true)) {
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

    ProjectSettingsPage* newPage = _createProjectSettingsPage(newProjectDir);
    this->navigation(newPage->property("ElaPageKey").toString());

    QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(newProjectDir.wstring()));
    QDesktopServices::openUrl(dirUrl);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("创建成功"), tr("请将待翻译的文件放入 gt_input 中！"), 8000);
}

void MainWindow::_onOpenProjectTriggered()
{
    QString projectPath = QFileDialog::getExistingDirectory(this, tr("选择已有项目的文件夹路径"), 
        QString::fromStdString(toml::find_or(_globalConfig, "lastProjectPath", "./Projects")));
    if (projectPath.isEmpty()) {
        return;
    }
    insertToml(_globalConfig, "lastProjectPath", projectPath.toStdString());
    fs::path projectDir(projectPath.toStdWString());
    if (!fs::exists(projectDir / L"config.toml")) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("打开失败"), tr("目录下不存在 config.toml 文件！"), 3000);
        return;
    }

    QString projectName = QString::fromStdWString(projectDir.filename().wstring());

    if (std::ranges::any_of(_projectPages, [&](auto& page)
        {
            return page->getProjectName() == projectName;
        }))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("打开失败"), tr("已存在同名项目！"), 3000);
        return;
    }

    ProjectSettingsPage* newPage = _createProjectSettingsPage(projectDir);
    this->navigation(newPage->property("ElaPageKey").toString());

    QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(projectDir.wstring()));
    QDesktopServices::openUrl(dirUrl);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("打开成功"), newPage->getProjectName() + tr(" 已纳入项目管理！"), 3000);
}

void MainWindow::_onRemoveProjectTriggered()
{
    QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
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

    connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
        {
            QString projectName = it->get()->getProjectName();
            it->get()->apply2Config();
            removeNavigationNode(pageKey);
            _projectPages.erase(it);
            if (getCurrentNavigationPageKey() == _settingPage->property("ElaPageKey").toString()) {
                if (_projectPages.empty()) {
                    this->navigation(_homePage->property("ElaPageKey").toString());
                }
                else {
                    this->navigation(_projectPages.back()->property("ElaPageKey").toString());
                }
            }
            ElaMessageBar::success(ElaMessageBarType::TopRight, tr("移除成功"), 
                tr("项目 ") + projectName + tr(" 已从项目管理中移除！"), 3000);
        });
    helpDialog.exec();
}

void MainWindow::_onDeleteProjectTriggered()
{
    const QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
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

    connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
        {
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
            _projectPages.erase(it);
            if (getCurrentNavigationPageKey() == _settingPage->property("ElaPageKey").toString()) {
                if (_projectPages.empty()) {
                    this->navigation(_homePage->property("ElaPageKey").toString());
                }
                else {
                    this->navigation(_projectPages.back()->property("ElaPageKey").toString());
                }
            }
            ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"), 
                tr("项目 ") + projectName + tr(" 已从项目管理和磁盘中移除！"), 3000);
        });
    helpDialog.exec();
}

void MainWindow::_onSaveProjectTriggered()
{
    const QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("保存失败"), tr("当前页面不是项目页面！"), 3000);
        return;
    }

    it->get()->apply2Config();
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"), 
        tr("项目 ") + it->get()->getProjectName() + tr(" 配置信息已保存！"), 3000);
}

void MainWindow::_onFinishTranslating(QString nodeKey)
{
    setNodeKeyPoints(nodeKey, getNodeKeyPoints(nodeKey) + 1);
}

void MainWindow::_onCloseWindowClicked(bool restart)
{
    _defaultPromptPage->apply2Config();
    _commonPreDictPage->apply2Config();
    _commonGptDictPage->apply2Config();
    _commonPostDictPage->apply2Config();

    toml::array projects;
    for (auto& page : _projectPages) {
        page->apply2Config();
        projects.push_back(wide2Ascii(page->getProjectDir()));
    }
    insertToml(_globalConfig, "projects", projects);

    _settingPage->apply2Config();
    insertToml(_globalConfig, "clearLogShortcut", _clearLogShortcut->key().toString().toStdString());

    std::ofstream ofs(L"BaseConfig/globalConfig.toml", std::ios::binary);
    ofs << _globalConfig;
    ofs.close();

    if (_updateChecker->getIsDownloadSucceed()) {
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
    _updateChecker->check();
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
