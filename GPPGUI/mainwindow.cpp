#include "mainwindow.h"

#include <Windows.h>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>

#include "ElaContentDialog.h"
#include "ElaDockWidget.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaProgressBar.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolBar.h"
#include "ElaToolButton.h"
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
namespace fs = std::filesystem;

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    if (fs::exists("BaseConfig/globalConfig.toml")) {
        try {
            _globalConfig = toml::parse_file("BaseConfig/globalConfig.toml");
        }
        catch (...) {
            QMessageBox::critical(this, "解析错误", "基本配置文件不符合规范！", QMessageBox::Ok);
        }
    }
    else {
        QMessageBox::critical(this, "不是哥们", "有病吧，你把我软件的配置文件删了！？", QMessageBox::Ok);
    }

    setIsAllowPageOpenInNewWindow(false);

    initWindow();

    //额外布局
    initEdgeLayout();

    //中心窗口
    initContent();

    // 拦截默认关闭事件
    _closeDialog = new ElaContentDialog(this);
    connect(_closeDialog, &ElaContentDialog::rightButtonClicked, this, &MainWindow::_on_closeWindow_clicked);
    connect(_closeDialog, &ElaContentDialog::middleButtonClicked, this, [=]() {
        _closeDialog->close();
        showMinimized();
        });
    this->setIsDefaultClosed(false);
    connect(this, &MainWindow::closeButtonClicked, this, [=]()
        {
            if (
                !(_globalConfig["allowCloseWhenRunning"].value_or(false)) &&
                std::any_of(_projectPages.begin(), _projectPages.end(), [](const auto& page)
                    {
                        if (page->getIsRunning()) {
                            ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                                "项目 " + page->getProjectName() + " 仍在运行，请先停止运行！", 3000);
                            return true;
                        }
                        return false;
                    })
                )
            {
                return;
            }
            _closeDialog->exec();
        });

    // 初始化提示
    ElaMessageBar::success(ElaMessageBarType::BottomRight, "Success", "初始化成功!", 2000);
}

MainWindow::~MainWindow()
{
    delete this->_aboutPage;
}

void MainWindow::initWindow()
{
    // 详见 ElaWidgetTool 开源项目的示例
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(QIcon(":/GPPGUI/Resource/Image/julixian_s.jpeg"));
    int width = _globalConfig["windowWidth"].value_or(1450);
    int height = _globalConfig["windowHeight"].value_or(770);
    resize(width, height - 30);
    int x = _globalConfig["windowPosX"].value_or(0);
    int y = _globalConfig["windowPosY"].value_or(0);
    if (x < 0)x = 0;
    if (y < 0)y = 0;
    move(x, y);
    setUserInfoCardPixmap(QPixmap(":/GPPGUI/Resource/Image/julixian_s.jpeg"));
    setUserInfoCardTitle(QString::fromStdString("Galtransl++ v" + GPPVERSION));
    setUserInfoCardSubTitle("tianquyesss@gmail.com");
    setWindowTitle("Galtransl++");
    setNavigationBarWidth(275);
}

void MainWindow::initEdgeLayout()
{
    //菜单栏
    ElaMenuBar* menuBar = new ElaMenuBar(this);
    menuBar->setFixedHeight(30);
    QWidget* customWidget = new QWidget(this);
    QHBoxLayout* customLayout = new QHBoxLayout(customWidget);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->addWidget(menuBar);
    customLayout->addStretch();
    this->setCustomWidget(ElaAppBarType::MiddleArea, customWidget);
    this->setCustomWidgetMaximumWidth(700);

    QAction* newProjectAction = menuBar->addElaIconAction(ElaIconType::AtomSimple, "新建项目");
    QAction* openProjectAction = menuBar->addElaIconAction(ElaIconType::FolderOpen, "打开项目");
    QAction* saveProjectAction = menuBar->addElaIconAction(ElaIconType::FloppyDisk, "保存项目配置");
    QAction* removeProjectAction = menuBar->addElaIconAction(ElaIconType::TrashCan, "移除项目");
    QAction* deleteProjectAction = menuBar->addElaIconAction(ElaIconType::TrashXmark, "删除项目");

    connect(newProjectAction, &QAction::triggered, this, &MainWindow::_on_newProject_triggered);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::_on_openProject_triggered);
    connect(saveProjectAction, &QAction::triggered, this, &MainWindow::_on_saveProject_triggered);
    connect(removeProjectAction, &QAction::triggered, this, &MainWindow::_on_removeProject_triggered);
    connect(deleteProjectAction, &QAction::triggered, this, &MainWindow::_on_deleteProject_triggered);

    //状态栏
    ElaStatusBar* statusBar = new ElaStatusBar(this);
    ElaText* statusText = new ElaText("初始化成功！", this);
    statusText->setTextPixelSize(14);
    statusBar->addWidget(statusText);
    this->setStatusBar(statusBar);

    //停靠窗口
    ElaDockWidget* updateDockWidget = new ElaDockWidget("更新内容", this);
    updateDockWidget->setWidget(new UpdateWidget(this));
    this->addDockWidget(Qt::RightDockWidgetArea, updateDockWidget);
    resizeDocks({ updateDockWidget }, { 200 }, Qt::Horizontal);
    std::string gppversion = GPPVERSION;
    std::erase_if(gppversion, [](char ch) { return ch == '.'; });
    updateDockWidget->setVisible(_globalConfig["showDockWidget"][gppversion].value_or(true));
    insertToml(_globalConfig, "showDockWidget", toml::table{});
    insertToml(_globalConfig, "showDockWidget." + gppversion, updateDockWidget->isVisible());
    connect(updateDockWidget, &ElaDockWidget::visibilityChanged, this, [=](bool visible)
        {
            insertToml(_globalConfig, "showDockWidget", toml::table{});
            insertToml(_globalConfig, "showDockWidget." + gppversion, visible);
        });

    // 右键菜单
    ElaMenu* appBarMenu = new ElaMenu(this);
    appBarMenu->setMenuItemHeight(27);
    // 召唤停靠窗口
    connect(appBarMenu->addElaIconAction(ElaIconType::BellConcierge, "召唤停靠窗口"), &QAction::triggered, this, [=]()
        {
            updateDockWidget->show();
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::GearComplex, "设置"), &QAction::triggered, this, [=]() {
        navigation(_settingKey);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::MoonStars, "更改程序主题"), &QAction::triggered, this, [=]() {
        eTheme->setThemeMode(eTheme->getThemeMode() == ElaThemeType::Light ? ElaThemeType::Dark : ElaThemeType::Light);
        });
    setCustomMenu(appBarMenu);
}

void MainWindow::initContent()
{
    _homePage = new HomePage(this);
    _defaultPromptPage = new DefaultPromptPage(this);

    _commonPreDictPage = new CommonNormalDictPage("pre", _globalConfig, this);
    _commonGptDictPage = new CommonGptDictPage(_globalConfig, this);
    _commonPostDictPage = new CommonNormalDictPage("post", _globalConfig, this);
    
    _settingPage = new SettingPage(_globalConfig, this);

    addPageNode("主页", _homePage, ElaIconType::House);

    addPageNode("默认提示词管理", _defaultPromptPage, ElaIconType::Text);

    addExpanderNode("通用字典管理", _commonDictExpanderKey, ElaIconType::FontCase);
    auto refreshCommonDicts = [=]()
        {
            for (auto& page : _projectPages) {
                page->refreshCommonDicts();
            }
        };
    addPageNode("通用译前字典", _commonPreDictPage, _commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(_commonPreDictPage, &CommonNormalDictPage::commonDictsChanged, this, refreshCommonDicts);
    addPageNode("通用GPT字典", _commonGptDictPage, _commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(_commonGptDictPage, &CommonGptDictPage::commonDictsChanged, this, refreshCommonDicts);
    addPageNode("通用译后字典", _commonPostDictPage, _commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(_commonPostDictPage, &CommonNormalDictPage::commonDictsChanged, this, refreshCommonDicts);

    addExpanderNode("项目管理", _projectExpanderKey, ElaIconType::BriefcaseBlank);
    auto projects = _globalConfig["projects"].as_array();
    if (projects) {
        for (const auto& elem : *projects) {
            if (auto project = elem.value<std::string>()) {
                fs::path projectDir(ascii2Wide(*project));
                if (!fs::exists(projectDir / L"config.toml")) {
                    continue;
                }
                QSharedPointer<ProjectSettingsPage> newPage(new ProjectSettingsPage(_globalConfig, projectDir, this));
                connect(newPage.get(), &ProjectSettingsPage::finishedTranslating, this, &MainWindow::_on_finishTranslating);
                _projectPages.push_back(newPage);
                addPageNode(QString(projectDir.filename().wstring()), newPage.get(), _projectExpanderKey, ElaIconType::Projector);
            }
        }
    }
    expandNavigationNode(_projectExpanderKey);

    addFooterNode("检查更新", nullptr, _updateKey, 0, ElaIconType::Repeat);
    connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) 
        {
            if (_updateKey == nodeKey) {
                ElaMessageBar::information(ElaMessageBarType::TopLeft, "请稍候", "正在检查更新...", 3000);
                _updateChecker->check();
            }
        });

    addFooterNode("关于", nullptr, _aboutKey, 0, ElaIconType::User);
    _aboutPage = new AboutDialog();
    _aboutPage->hide();
    connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) 
        {
            if (_aboutKey == nodeKey)
            {
                _aboutPage->setFixedSize(400, 400);
                _aboutPage->moveToCenter();
                _aboutPage->show();
            }
        });

    addFooterNode("设置", _settingPage, _settingKey, 0, ElaIconType::GearComplex);
    connect(this, &MainWindow::userInfoCardClicked, this, [=]() 
        {
            this->navigation(_homePage->property("ElaPageKey").toString());
        });

    /*connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey)
        {
            auto it = std::find_if(_projectPages.begin(), _projectPages.end(), [&](auto& page)
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

    _updateChecker = new UpdateChecker(this);
}

void MainWindow::_on_newProject_triggered()
{
    QString parentPath = QFileDialog::getExistingDirectory(this, "选择新项目的存放位置", QDir::currentPath() + "/Projects");
    if (parentPath.isEmpty()) {
        return;
    }

    QString projectName;
    bool ok;
    ElaInputDialog inputDialog(this, "请输入项目名称", "新建项目", projectName, &ok);
    inputDialog.exec();

    if (!ok) {
        return;
    }

    if (std::ranges::any_of(_projectPages, [&](auto& page)
        {
            return projectName == page->getProjectName();
        }))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "已存在同名项目！", 3000);
        return;
    }

    if (projectName.isEmpty() || projectName.contains("/") || projectName.contains("\\"))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "项目名称不能为空，且不能包含斜杠或反斜杠！", 3000);
        return;
    }

    fs::path newProjectDir = fs::path(parentPath.toStdWString()) / projectName.toStdWString();
    if (fs::exists(newProjectDir)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "目录下存在同名文件或文件夹！", 3000);
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
            ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "无法创建新文件！", 3000);
            return;
        }
    }
    else {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "无法读取模板文件！", 3000);
        return;
    }

    if (!extractZip(newProjectDir / L"sampleProject.zip", newProjectDir)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "无法解压模板文件！", 3000);
        return;
    }
    else {
        fs::remove(newProjectDir / L"sampleProject.zip");
    }

    if (fs::exists(L"BaseConfig/Prompt.toml")) {
        fs::copy(L"BaseConfig/Prompt.toml", newProjectDir / L"Prompt.toml", fs::copy_options::overwrite_existing);
    }

    try {
        std::ifstream ifs(newProjectDir / L"config.toml");
        toml::table configData = toml::parse(ifs);
        ifs.close();

        auto preDictNamesArr = _globalConfig["commonPreDicts"]["dictNames"].as_array();
        auto preDictsArr = configData["dictionary"]["preDict"].as_array();
        if (preDictNamesArr && preDictsArr) {
            for (const auto& elem : *preDictNamesArr) {
                auto preDictNameOpt = elem.value<std::string>();
                if (!preDictNameOpt.has_value()) {
                    continue;
                }
                if (!_globalConfig["commonPreDicts"]["spec"][*preDictNameOpt]["defaultOn"].value_or(true)) {
                    continue;
                }
                preDictsArr->push_back(*preDictNameOpt + ".toml");
            }
        }

        auto gptDictNamesArr = _globalConfig["commonGptDicts"]["dictNames"].as_array();
        auto gptDictsArr = configData["dictionary"]["gptDict"].as_array();
        if (gptDictNamesArr && gptDictsArr) {
            for (const auto& elem : *gptDictNamesArr) {
                auto gptDictNameOpt = elem.value<std::string>();
                if (!gptDictNameOpt.has_value()) {
                    continue;
                }
                if (!_globalConfig["commonGptDicts"]["spec"][*gptDictNameOpt]["defaultOn"].value_or(true)) {
                    continue;
                }
                gptDictsArr->push_back(*gptDictNameOpt + ".toml");
            }
        }

        auto postDictNamesArr = _globalConfig["commonPostDicts"]["dictNames"].as_array();
        auto postDictsArr = configData["dictionary"]["postDict"].as_array();
        if (postDictNamesArr && postDictsArr) {
            for (const auto& elem : *postDictNamesArr) {
                auto postDictNameOpt = elem.value<std::string>();
                if (!postDictNameOpt.has_value()) {
                    continue;
                }
                if (!_globalConfig["commonPostDicts"]["spec"][*postDictNameOpt]["defaultOn"].value_or(true)) {
                    continue;
                }
                postDictsArr->push_back(*postDictNameOpt + ".toml");
            }
        }

        std::ofstream ofs(newProjectDir / L"config.toml");
        ofs << configData;
        ofs.close();
    }
    catch (...) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "创建失败", "无法写入配置文件！", 3000);
        return;
    }

    QSharedPointer<ProjectSettingsPage> newPage(new ProjectSettingsPage(_globalConfig, newProjectDir, this));
    connect(newPage.get(), &ProjectSettingsPage::finishedTranslating, this, &MainWindow::_on_finishTranslating);
    _projectPages.push_back(newPage);
    addPageNode(projectName, newPage.get(), _projectExpanderKey, ElaIconType::Projector);
    this->navigation(newPage->property("ElaPageKey").toString());

    QUrl dirUrl = QUrl::fromLocalFile(QString(newProjectDir.wstring()));
    QDesktopServices::openUrl(dirUrl);
    ElaMessageBar::success(ElaMessageBarType::TopRight, "创建成功", "请将待翻译的文件放入 gt_input 中！", 8000);
}

void MainWindow::_on_openProject_triggered()
{
    QString projectPath = QFileDialog::getExistingDirectory(this, "选择已有项目的文件夹路径", QDir::currentPath() + "/Projects");
    if (projectPath.isEmpty()) {
        return;
    }

    fs::path projectDir(projectPath.toStdWString());
    if (!fs::exists(projectDir / L"config.toml")) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "打开失败", "目录下不存在 config.toml 文件！", 3000);
        return;
    }

    QString projectName(projectDir.filename().wstring());

    if (std::ranges::any_of(_projectPages, [&](auto& page)
        {
            return page->getProjectName() == projectName;
        }))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "打开失败", "已存在同名项目！", 3000);
        return;
    }

    QSharedPointer<ProjectSettingsPage> newPage(new ProjectSettingsPage(_globalConfig, projectDir, this));
    connect(newPage.get(), &ProjectSettingsPage::finishedTranslating, this, &MainWindow::_on_finishTranslating);
    _projectPages.push_back(newPage);
    addPageNode(newPage->getProjectName(), newPage.get(), _projectExpanderKey, ElaIconType::Projector);
    this->navigation(newPage->property("ElaPageKey").toString());

    QUrl dirUrl = QUrl::fromLocalFile(QString(projectDir.wstring()));
    QDesktopServices::openUrl(dirUrl);
    ElaMessageBar::success(ElaMessageBarType::TopRight, "打开成功", newPage->getProjectName() + " 已纳入项目管理！", 3000);
}

void MainWindow::_on_removeProject_triggered()
{
    QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "移除失败", "当前页面不是项目页面！", 3000);
        return;
    }
    if (it->get()->getIsRunning()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "移除失败", "当前项目正在运行，请先停止运行！", 3000);
        return;
    }
    ElaContentDialog helpDialog(this);

    helpDialog.setRightButtonText("是");
    helpDialog.setMiddleButtonText("思考人生");
    helpDialog.setLeftButtonText("否");

    QWidget* widget = new QWidget(&helpDialog);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->addWidget(new ElaText("你确定要移除当前项目吗？", widget));
    ElaText* tip = new ElaText("从项目管理中移除该项目，但不会删除其项目文件夹", widget);
    tip->setTextPixelSize(14);
    layout->addWidget(tip);
    helpDialog.setCentralWidget(widget);

    connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
        {
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
            ElaMessageBar::success(ElaMessageBarType::TopRight, "移除成功", "项目 " + it->get()->getProjectName() + " 已从项目管理中移除！", 3000);
        });
    helpDialog.exec();
}

void MainWindow::_on_deleteProject_triggered()
{
    QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "删除失败", "当前页面不是项目页面！", 3000);
        return;
    }
    if (it->get()->getIsRunning()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "删除失败", "当前项目正在运行，请先停止运行！", 3000);
        return;
    }
    ElaContentDialog helpDialog(this);

    helpDialog.setRightButtonText("是");
    helpDialog.setMiddleButtonText("思考人生");
    helpDialog.setLeftButtonText("否");

    QWidget* widget = new QWidget(&helpDialog);
    widget->setFixedHeight(110);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->addWidget(new ElaText("你确定要删除当前项目吗？", widget));
    ElaText* tip = new ElaText("将删除该项目的项目文件夹，如果不备份，再次翻译将必须从头开始！", widget);
    tip->setTextPixelSize(14);
    layout->addWidget(tip);
    helpDialog.setCentralWidget(widget);

    connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
        {
            fs::path projectDir = it->get()->getProjectDir();
            try {
                fs::remove_all(projectDir);
            }
            catch (const fs::filesystem_error& e) {
                ElaMessageBar::warning(ElaMessageBarType::TopRight, "删除失败", e.what(), 3000);
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
            ElaMessageBar::success(ElaMessageBarType::TopRight, "删除成功", "项目 " + it->get()->getProjectName() + " 已从项目管理和磁盘中移除！", 3000);
        });
    helpDialog.exec();
}

void MainWindow::_on_saveProject_triggered()
{
    QString pageKey = getCurrentNavigationPageKey();
    auto it = std::ranges::find_if(_projectPages, [&](auto& page)
        {
            return page->property("ElaPageKey").toString() == pageKey;
        });
    if (it == _projectPages.end()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, "保存失败", "当前页面不是项目页面！", 3000);
        return;
    }

    it->get()->apply2Config();
    ElaMessageBar::success(ElaMessageBarType::TopRight, "保存成功", "项目 " + it->get()->getProjectName() + " 配置信息已保存！", 3000);
}

void MainWindow::_on_finishTranslating(QString nodeKey)
{
    setNodeKeyPoints(nodeKey, getNodeKeyPoints(nodeKey) + 1);
}

void MainWindow::_on_closeWindow_clicked()
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
    _globalConfig.insert_or_assign("projects", projects);

    _settingPage->apply2Config();

    std::ofstream ofs(L"BaseConfig/globalConfig.toml");
    ofs << _globalConfig;
    ofs.close();
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
