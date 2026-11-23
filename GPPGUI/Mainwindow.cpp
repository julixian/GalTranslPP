#include "GppPlatform.h"
#include "Mainwindow.h"

#include <QHBoxLayout>
#include <QButtonGroup>
#include <QMouseEvent>
#include <QFileDialog>
#include <QProcess>
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFontMetrics>
#include <QShortcut>
#include <algorithm>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include "ElaContentDialog.h"
#include "ElaDockWidget.h"
#include "ElaLineEdit.h"
#include "ElaMenu.h"
#include "ElaMenuBar.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaMessageBar.h"
#include "ElaApplication.h"
#include "GppInputDialog.h"
#include "GppRadioButton.h"
#include "AboutDialog.h"
#include "UpdateWidget.h"
#include "UpdateChecker.h"
#include "TaskbarController.h"

#include "HomePage.h"
#include "DefaultPromptsPage.h"
#include "CommonGptDictsPage.h"
#include "CommonNormalDictsPage.h"
#include "ProjectSettingsPage.h"
#include "AppSettingsPage.h"

import GPPVersion;
import Tool;
import PythonManager;

using namespace gpp;

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent), m_globalConfig(std::make_unique<tomlkit::Item>(tomlkit::Item::document())),
    m_taskbarController(std::make_unique<TaskbarController>(this))
{
    if (fs::exists(L"BaseConfig/GlobalConfig.toml")) {
        try {
            *m_globalConfig = parseToml(fs::path(L"BaseConfig/GlobalConfig.toml"));
        }
        catch (...) {
            *m_globalConfig = tomlkit::Item::document();
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

    connect(m_closeDialog, &ElaContentDialog::rightButtonClicked, this, [=, this]()
        {
            onCloseWindowClicked(false);
        });
    connect(m_closeDialog, &ElaContentDialog::middleButtonClicked, this, [=, this]()
        {
            m_closeDialog->close();
            showMinimized();
        });
    this->setIsDefaultClosed(false);

    auto closeCallback = [=, this](bool closeByMessageCard)
        {
            if (
                !(tomlkit::findOr(*m_globalConfig, "allowCloseWhenRunning", false)) &&
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
    connect(m_updateChecker, &UpdateChecker::checkCompleteSignal, this, [=, this](bool hasNewVersion)
        {
            m_aboutDialog->setDownloadButtonEnabled(hasNewVersion && m_updateChecker->shouldDownloadButtonEnabled());
        });

    connect(this, &ElaWindow::navigationNodeKeyPointsChanged, this, [this](const QString&, int)
        {
            updateTaskbarBadge();
        });
    connect(this, &ElaWindow::navigationBarWidthDragFinished, this, [this](int width)
        {
            insertToml(*m_globalConfig, "navigationBarWidth", width);
        });
    connect(this, &ElaWindow::navigationNodeReparented, this, [this](const QString&, const QString&, const QString&, int, int)
        {
            syncProjectsToGlobalConfig();
        });
    connect(this, &ElaWindow::navigationNodeExpandedChanged, this, [this](const QString& expanderKey, bool)
        {
            if (isProjectCategoryKey(expanderKey)) {
                syncProjectsToGlobalConfig();
            }
        });
    connect(this, &ElaWindow::navigationNodeContextMenuRequested, this, [this](const QString& nodeKey, const QPoint& globalPos)
        {
            if (nodeKey == m_projectExpanderKey) {
                showProjectRootContextMenu(globalPos);
            }
            else if (isProjectCategoryKey(nodeKey)) {
                showProjectCategoryContextMenu(nodeKey, globalPos);
            }
            else if (findProjectPageByKey(nodeKey) != nullptr) {
                showProjectNodeContextMenu(nodeKey, globalPos);
            }
        });
}

ProjectSettingsPage* MainWindow::createProjectSettingsPage(const fs::path& projectDir, const QString& targetParentKey)
{
    QSharedPointer<ProjectSettingsPage> newPage(new ProjectSettingsPage(projectDir, *m_globalConfig, this));
    connect(newPage.get(), &ProjectSettingsPage::finishTranslatingSignal, this, &MainWindow::onFinishTranslating);
    connect(newPage.get(), &ProjectSettingsPage::changeProjectNameSignal, this, [=, this](const QString& nodeKey, const QString& newProjectName)
        {
            setNavigationNodeTitle(nodeKey, newProjectName);
        });
    if (!tomlkit::findOr(*m_globalConfig, "lazyProjectInitialization", true)) {
        newPage->initialize();
    }
    m_projectPages.push_back(newPage);
    const QString parentKey = targetParentKey.isEmpty() ? m_projectExpanderKey : targetParentKey;
    addPageNode(QString::fromStdWString(projectDir.filename().wstring()), newPage.get(), parentKey, ElaIconType::Projector);
    const QString nodeKey = newPage->property("ElaPageKey").toString();
    setNodeReorderable(nodeKey, true);
    connect(newPage.get(), &ProjectSettingsPage::runningStateChangedSignal, this,
        [this](const QString& changedKey, bool isRunning, int completed, int total)
        {
            if (isRunning && getNodeKeyPoints(changedKey) > 0) {
                setNodeKeyPoints(changedKey, 0);
            }
            const qreal progress = total > 0 ? static_cast<qreal>(completed) / static_cast<qreal>(total) : -1.0;
            setNodeRunningState(changedKey, isRunning, progress);
            updateTaskbarProgress();
        });
    ProjectSettingsPage* page = newPage.get();
    connect(this, &MainWindow::navigationNodeClicked, page, [page](ElaNavigationType::NavigationNodeType nodeType, const QString& nodeKey)
        {
            if (nodeType == ElaNavigationType::PageNode && page->property("ElaPageKey").toString() == nodeKey) {
                page->initialize();
            }
        });
    return newPage.get();
}

ProjectSettingsPage* MainWindow::findProjectPageByKey(const QString& nodeKey) const
{
    const auto it = std::ranges::find_if(m_projectPages, [&](const auto& page)
        {
            return page->property("ElaPageKey").toString() == nodeKey;
        });
    return it == m_projectPages.end() ? nullptr : it->get();
}

QStringList MainWindow::getOrderedProjectCategoryKeys() const
{
    QStringList ordered;
    for (const QString& childKey : getNavigationNodeChildrenKeys(m_projectExpanderKey)) {
        if (m_projectCategoryKeys.contains(childKey)) {
            ordered.append(childKey);
        }
    }
    return ordered;
}

bool MainWindow::isProjectCategoryKey(const QString& nodeKey) const
{
    return !nodeKey.isEmpty() && m_projectCategoryKeys.contains(nodeKey);
}

bool MainWindow::hasCategoryName(const QString& name, const QString& excludeCategoryKey) const
{
    for (const QString& catKey : m_projectCategoryKeys) {
        if (catKey == excludeCategoryKey) {
            continue;
        }
        if (getNavigationNodeTitle(catKey) == name) {
            return true;
        }
    }
    return false;
}

bool MainWindow::promptCategoryName(const QString& dialogTitle, const QString& initialText, const QString& placeholder, QString& outName)
{
    ElaContentDialog dialog(this);
    dialog.setLeftButtonText(tr("取消"));
    dialog.setMiddleButtonText(tr("重置"));
    dialog.setRightButtonText(tr("确定"));

    QWidget* centerWidget = new QWidget(&dialog);
    QVBoxLayout* layout = new QVBoxLayout(centerWidget);
    layout->setContentsMargins(15, 25, 15, 10);

    ElaText* titleText = new ElaText(dialogTitle, centerWidget);
    titleText->setTextStyle(ElaTextType::Title);
    titleText->setWordWrap(false);
    layout->addWidget(titleText);
    layout->addSpacing(8);

    ElaLineEdit* lineEdit = new ElaLineEdit(centerWidget);
    lineEdit->setPlaceholderText(placeholder);
    lineEdit->setText(initialText);
    lineEdit->selectAll();
    layout->addStretch();
    layout->addWidget(lineEdit);
    dialog.setCentralWidget(centerWidget);

    connect(&dialog, &ElaContentDialog::middleButtonClicked, lineEdit, [lineEdit, initialText]()
        {
            lineEdit->setText(initialText);
            lineEdit->selectAll();
        });
    connect(lineEdit, &ElaLineEdit::returnPressed, &dialog, [&dialog]()
        {
            dialog.accept();
        });
    lineEdit->setFocus();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    outName = lineEdit->text().trimmed();
    return true;
}

QString MainWindow::createProjectCategory(const QString& name)
{
    const int insertIdx = getOrderedProjectCategoryKeys().size();
    QString categoryKey;
    if (insertExpanderNode(name, categoryKey, m_projectExpanderKey, insertIdx, ElaIconType::Folder) != ElaNavigationType::Success) {
        return {};
    }
    setNodeReorderable(categoryKey, true);
    m_projectCategoryKeys.append(categoryKey);
    return categoryKey;
}

void MainWindow::handleNewProjectCategory(const QString& moveProjectKey)
{
    QString name;
    if (!promptCategoryName(tr("新建分类"), QString(), tr("请输入分类名称"), name)) {
        return;
    }
    if (name.isEmpty()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("分类名称不能为空！"), 3000);
        return;
    }
    if (hasCategoryName(name)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("创建失败"), tr("已存在同名分类！"), 3000);
        return;
    }
    const QString categoryKey = createProjectCategory(name);
    if (categoryKey.isEmpty()) {
        return;
    }
    expandNavigationNode(m_projectExpanderKey);
    if (!moveProjectKey.isEmpty()) {
        moveNavigationNode(moveProjectKey, categoryKey, 0);
    }
    expandNavigationNode(categoryKey);
    syncProjectsToGlobalConfig();
}

void MainWindow::handleRenameProjectCategory(const QString& categoryKey)
{
    if (!isProjectCategoryKey(categoryKey)) {
        return;
    }
    const QString oldName = getNavigationNodeTitle(categoryKey);
    QString newName;
    if (!promptCategoryName(tr("重命名分类"), oldName, tr("请输入新的分类名称"), newName)) {
        return;
    }
    if (newName.isEmpty()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("重命名失败"), tr("分类名称不能为空！"), 3000);
        return;
    }
    if (newName == oldName) {
        return;
    }
    if (hasCategoryName(newName, categoryKey)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("重命名失败"), tr("已存在同名分类！"), 3000);
        return;
    }
    setNavigationNodeTitle(categoryKey, newName);
    syncProjectsToGlobalConfig();
}

void MainWindow::handleDeleteProjectCategory(const QString& categoryKey)
{
    if (!isProjectCategoryKey(categoryKey)) {
        return;
    }
    const QString catName = getNavigationNodeTitle(categoryKey);
    const QStringList childKeys = getNavigationNodeChildrenKeys(categoryKey);

    // Empty category: delete directly without unnecessary choices.
    if (childKeys.isEmpty()) {
        m_projectCategoryKeys.removeAll(categoryKey);
        removeNavigationNode(categoryKey);
        syncProjectsToGlobalConfig();
        ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"), tr("分类「%1」已删除！").arg(catName), 3000);
        return;
    }

    auto hasRunningChild = [this](const QStringList& keys) -> bool
        {
            return std::ranges::any_of(keys, [this](const QString& key)
                {
                    ProjectSettingsPage* page = findProjectPageByKey(key);
                    return page && page->getIsRunning();
                });
        };

    const bool anyRunning = hasRunningChild(childKeys);

    ElaContentDialog confirmDialog(this);
    confirmDialog.setLeftButtonText(tr("取消"));
    confirmDialog.setMiddleButtonText(tr("思考人生"));
    confirmDialog.setRightButtonText(tr("确定"));

    QWidget* dialogWidget = new QWidget(&confirmDialog);
    QVBoxLayout* dialogLayout = new QVBoxLayout(dialogWidget);
    dialogLayout->setContentsMargins(15, 25, 15, 10);

    ElaText* titleText = new ElaText(tr("确定要删除分类「%1」吗？").arg(catName), dialogWidget);
    titleText->setTextStyle(ElaTextType::Title);
    titleText->setWordWrap(false);
    dialogLayout->addWidget(titleText);
    dialogLayout->addSpacing(10);

    QButtonGroup* optionGroup = new QButtonGroup(&confirmDialog);
    GppRadioButton* keepOnlyRadio = new GppRadioButton(
        tr("仅删除分类（将下属 %1 个项目移回未分类）").arg(childKeys.size()), dialogWidget);
    GppRadioButton* removeProjectsRadio = new GppRadioButton(
        tr("删除分类并移除下属所有项目（不删除项目文件夹）"), dialogWidget);
    GppRadioButton* deleteProjectsRadio = new GppRadioButton(
        tr("删除分类并彻底删除下属所有项目（同时删除磁盘上的项目文件夹）"), dialogWidget);

    keepOnlyRadio->setChecked(true);
    removeProjectsRadio->setEnabled(!anyRunning);
    deleteProjectsRadio->setEnabled(!anyRunning);

    optionGroup->addButton(keepOnlyRadio, 0);
    optionGroup->addButton(removeProjectsRadio, 1);
    optionGroup->addButton(deleteProjectsRadio, 2);

    dialogLayout->addWidget(keepOnlyRadio);
    dialogLayout->addSpacing(6);
    dialogLayout->addWidget(removeProjectsRadio);
    dialogLayout->addSpacing(6);
    dialogLayout->addWidget(deleteProjectsRadio);

    if (anyRunning) {
        dialogLayout->addSpacing(8);
        ElaText* runningHint = new ElaText(tr("分类下存在正在运行的项目，无法连带移除或删除项目。"), dialogWidget);
        runningHint->setTextStyle(ElaTextType::Caption);
        runningHint->setWordWrap(true);
        dialogLayout->addWidget(runningHint);
    }

    dialogLayout->addStretch();
    confirmDialog.setCentralWidget(dialogWidget);
    confirmDialog.resize(560, std::max(confirmDialog.sizeHint().height(), anyRunning ? 290 : 260));

    if (confirmDialog.exec() != QDialog::Accepted) {
        return;
    }

    const int selectedOption = optionGroup->checkedId();
    const QStringList currentChildKeys = getNavigationNodeChildrenKeys(categoryKey);

    if (selectedOption == 0) {
        for (const QString& childKey : currentChildKeys) {
            const int appendIdx = getNavigationNodeChildrenKeys(m_projectExpanderKey).size();
            moveNavigationNode(childKey, m_projectExpanderKey, appendIdx);
        }
        m_projectCategoryKeys.removeAll(categoryKey);
        removeNavigationNode(categoryKey);
        syncProjectsToGlobalConfig();
        ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"), tr("分类「%1」已删除！").arg(catName), 3000);
        return;
    }

    // Re-check running state right before executing option 1 or 2.
    if (hasRunningChild(currentChildKeys)) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("操作失败"),
            tr("分类下存在正在运行的项目，无法连带移除或删除项目！"), 3000);
        return;
    }

    if (selectedOption == 1) {
        const QString activeKeyBefore = getCurrentNavigationPageKey();
        const bool activeWasInCategory = currentChildKeys.contains(activeKeyBefore);
        if (activeWasInCategory) {
            this->navigation(m_homePage->property("ElaPageKey").toString());
        }
        int removedCount = 0;

        for (const QString& childKey : currentChildKeys) {
            const auto it = std::ranges::find_if(m_projectPages, [&](const auto& page)
                {
                    return page->property("ElaPageKey").toString() == childKey;
                });
            if (it == m_projectPages.end()) {
                const int appendIdx = getNavigationNodeChildrenKeys(m_projectExpanderKey).size();
                moveNavigationNode(childKey, m_projectExpanderKey, appendIdx);
                continue;
            }
            it->get()->apply2Config();
            removeNavigationNode(childKey);
            m_projectPages.erase(it);
            ++removedCount;
        }

        m_projectCategoryKeys.removeAll(categoryKey);
        removeNavigationNode(categoryKey);
        syncProjectsToGlobalConfig();
        updateTaskbarProgress();
        updateTaskbarBadge();

        if (activeWasInCategory && !m_projectPages.empty()) {
            this->navigation(m_projectPages.back()->property("ElaPageKey").toString());
        }
        ElaMessageBar::success(ElaMessageBarType::TopRight, tr("移除成功"),
            tr("分类「%1」及其下 %2 个项目已从项目管理中移除！").arg(catName).arg(removedCount), 3000);
        return;
    }

    if (selectedOption == 2) {
        ElaContentDialog secondConfirm(this);
        secondConfirm.setLeftButtonText(tr("否"));
        secondConfirm.setMiddleButtonText(tr("思考人生"));
        secondConfirm.setRightButtonText(tr("是"));

        QWidget* secondWidget = new QWidget(&secondConfirm);
        QVBoxLayout* secondLayout = new QVBoxLayout(secondWidget);
        secondLayout->setContentsMargins(15, 25, 15, 10);
        ElaText* secondTitle = new ElaText(tr("你确定要彻底删除分类「%1」下的所有项目吗？").arg(catName), secondWidget);
        secondTitle->setTextStyle(ElaTextType::Title);
        secondTitle->setWordWrap(true);
        secondLayout->addWidget(secondTitle);
        secondLayout->addSpacing(4);
        ElaText* secondBody = new ElaText(
            tr("将永久删除该分类下 %1 个项目的项目文件夹，如果不备份，再次翻译将必须从头开始！").arg(currentChildKeys.size()),
            secondWidget);
        secondBody->setTextStyle(ElaTextType::Body);
        secondBody->setWordWrap(true);
        secondLayout->addWidget(secondBody);
        secondLayout->addStretch();
        secondConfirm.setCentralWidget(secondWidget);
        secondConfirm.resize(520, std::max(secondConfirm.sizeHint().height(), 210));

        if (secondConfirm.exec() != QDialog::Accepted) {
            return;
        }

        const QStringList finalChildKeys = getNavigationNodeChildrenKeys(categoryKey);
        if (hasRunningChild(finalChildKeys)) {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"),
                tr("分类下存在正在运行的项目，无法连带删除项目！"), 3000);
            return;
        }

        const QString activeKeyBefore = getCurrentNavigationPageKey();
        bool activeWasDeleted = false;
        int deletedCount = 0;
        QStringList failedProjects;

        for (const QString& childKey : finalChildKeys) {
            const auto it = std::ranges::find_if(m_projectPages, [&](const auto& page)
                {
                    return page->property("ElaPageKey").toString() == childKey;
                });
            if (it == m_projectPages.end()) {
                const int appendIdx = getNavigationNodeChildrenKeys(m_projectExpanderKey).size();
                moveNavigationNode(childKey, m_projectExpanderKey, appendIdx);
                continue;
            }
            const fs::path projectDir = it->get()->getProjectDir();
            const QString projectName = it->get()->getProjectName();
            try {
                fs::remove_all(projectDir);
            }
            catch (const std::exception&) {
                const int appendIdx = getNavigationNodeChildrenKeys(m_projectExpanderKey).size();
                moveNavigationNode(childKey, m_projectExpanderKey, appendIdx);
                failedProjects.append(projectName);
                continue;
            }
            if (childKey == activeKeyBefore) {
                activeWasDeleted = true;
                this->navigation(m_homePage->property("ElaPageKey").toString());
            }
            removeNavigationNode(childKey);
            m_projectPages.erase(it);
            ++deletedCount;
        }

        m_projectCategoryKeys.removeAll(categoryKey);
        removeNavigationNode(categoryKey);
        syncProjectsToGlobalConfig();
        updateTaskbarProgress();
        updateTaskbarBadge();

        if (activeWasDeleted && !m_projectPages.empty()) {
            this->navigation(m_projectPages.back()->property("ElaPageKey").toString());
        }

        if (failedProjects.isEmpty()) {
            ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"),
                tr("分类「%1」及其下 %2 个项目已从项目管理和磁盘中移除！").arg(catName).arg(deletedCount), 3000);
        }
        else {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("部分删除失败"),
                tr("分类「%1」已删除（成功删除 %2 个项目）；以下项目文件夹删除失败，已移回未分类：%3")
                .arg(catName).arg(deletedCount).arg(failedProjects.join(tr("、"))), 6000);
        }
    }
}

namespace
{
    void adjustElaMenuWidth(ElaMenu* menu, int minWidth = 190)
    {
        QFont menuFont = menu->font();
        menuFont.setPixelSize(eApp->getFontPixelSize());
        const QFontMetrics fm(menuFont);
        int menuWidth = minWidth;
        const int extraPadding = menu->isHasChildMenu() ? 92 : 72;
        for (const QAction* action : menu->actions()) {
            if (!action->isSeparator()) {
                menuWidth = std::max(menuWidth, fm.horizontalAdvance(action->text()) + extraPadding);
            }
        }
        menu->setFixedWidth(menuWidth);
    }
}

void MainWindow::showProjectRootContextMenu(const QPoint& globalPos)
{
    ElaMenu menu(this);
    menu.setMenuItemHeight(27);
    QAction* newCategoryAction = menu.addElaIconAction(ElaIconType::FolderPlus, tr("新建分类"));
    connect(newCategoryAction, &QAction::triggered, this, [this]()
        {
            handleNewProjectCategory();
        });
    adjustElaMenuWidth(&menu);
    menu.exec(globalPos);
}

void MainWindow::showProjectCategoryContextMenu(const QString& categoryKey, const QPoint& globalPos)
{
    const QStringList categories = getOrderedProjectCategoryKeys();
    const int idx = categories.indexOf(categoryKey);
    const int count = categories.size();
    if (idx < 0) {
        return;
    }

    ElaMenu menu(this);
    menu.setMenuItemHeight(27);

    QAction* newCategoryAction = menu.addElaIconAction(ElaIconType::FolderPlus, tr("新建分类"));
    QAction* renameAction = menu.addElaIconAction(ElaIconType::PenToSquare, tr("重命名分类"));
    connect(newCategoryAction, &QAction::triggered, this, [this]() { handleNewProjectCategory(); });
    connect(renameAction, &QAction::triggered, this, [this, categoryKey]() { handleRenameProjectCategory(categoryKey); });

    menu.addSeparator();
    QAction* moveTopAction = menu.addElaIconAction(ElaIconType::ArrowUpToLine, tr("置顶"));
    QAction* moveUpAction = menu.addElaIconAction(ElaIconType::ArrowUp, tr("上移"));
    QAction* moveDownAction = menu.addElaIconAction(ElaIconType::ArrowDown, tr("下移"));
    QAction* moveBottomAction = menu.addElaIconAction(ElaIconType::ArrowDownToLine, tr("置底"));
    moveTopAction->setEnabled(idx > 0);
    moveUpAction->setEnabled(idx > 0);
    moveDownAction->setEnabled(idx + 1 < count);
    moveBottomAction->setEnabled(idx + 1 < count);
    connect(moveTopAction, &QAction::triggered, this, [this, categoryKey]() { moveNavigationNode(categoryKey, m_projectExpanderKey, 0); });
    connect(moveUpAction, &QAction::triggered, this, [this, categoryKey, idx]() { moveNavigationNode(categoryKey, m_projectExpanderKey, idx - 1); });
    connect(moveDownAction, &QAction::triggered, this, [this, categoryKey, idx]() { moveNavigationNode(categoryKey, m_projectExpanderKey, idx + 1); });
    connect(moveBottomAction, &QAction::triggered, this, [this, categoryKey, count]() { moveNavigationNode(categoryKey, m_projectExpanderKey, count - 1); });

    menu.addSeparator();
    QAction* deleteAction = menu.addElaIconAction(ElaIconType::TrashCan, tr("删除分类"));
    connect(deleteAction, &QAction::triggered, this, [this, categoryKey]() { handleDeleteProjectCategory(categoryKey); });

    adjustElaMenuWidth(&menu);
    menu.exec(globalPos);
}

void MainWindow::showProjectNodeContextMenu(const QString& nodeKey, const QPoint& globalPos)
{
    ProjectSettingsPage* page = findProjectPageByKey(nodeKey);
    if (!page) {
        return;
    }

    const QString parentKey = getNavigationNodeParentKey(nodeKey);
    const QStringList siblings = getNavigationNodeChildrenKeys(parentKey);
    const int actualIdx = siblings.indexOf(nodeKey);
    if (actualIdx < 0) {
        return;
    }

    const QStringList categories = getOrderedProjectCategoryKeys();
    const bool isUncategorized = (parentKey == m_projectExpanderKey);
    const int minIdx = isUncategorized ? categories.size() : 0;
    const int maxIdx = siblings.size() - 1;

    ElaMenu menu(this);
    menu.setMenuItemHeight(27);
    QAction* moveTopAction = menu.addElaIconAction(ElaIconType::ArrowUpToLine, tr("置顶"));
    QAction* moveUpAction = menu.addElaIconAction(ElaIconType::ArrowUp, tr("上移"));
    QAction* moveDownAction = menu.addElaIconAction(ElaIconType::ArrowDown, tr("下移"));
    QAction* moveBottomAction = menu.addElaIconAction(ElaIconType::ArrowDownToLine, tr("置底"));
    moveTopAction->setEnabled(actualIdx > minIdx);
    moveUpAction->setEnabled(actualIdx > minIdx);
    moveDownAction->setEnabled(actualIdx < maxIdx);
    moveBottomAction->setEnabled(actualIdx < maxIdx);
    connect(moveTopAction, &QAction::triggered, this, [this, nodeKey, parentKey, minIdx]() { moveNavigationNode(nodeKey, parentKey, minIdx); });
    connect(moveUpAction, &QAction::triggered, this, [this, nodeKey, parentKey, actualIdx]() { moveNavigationNode(nodeKey, parentKey, actualIdx - 1); });
    connect(moveDownAction, &QAction::triggered, this, [this, nodeKey, parentKey, actualIdx]() { moveNavigationNode(nodeKey, parentKey, actualIdx + 1); });
    connect(moveBottomAction, &QAction::triggered, this, [this, nodeKey, parentKey, maxIdx]() { moveNavigationNode(nodeKey, parentKey, maxIdx); });

    menu.addSeparator();
    ElaMenu* moveToCategoryMenu = menu.addMenu(ElaIconType::ArrowsTurnRight, tr("移动到分类"));
    moveToCategoryMenu->setMenuItemHeight(27);

    QAction* uncategorizedAction = moveToCategoryMenu->addElaIconAction(
        isUncategorized ? ElaIconType::Check : ElaIconType::BriefcaseBlank, tr("未分类"));
    uncategorizedAction->setEnabled(!isUncategorized);
    connect(uncategorizedAction, &QAction::triggered, this, [this, nodeKey]()
        {
            const int appendIdx = getNavigationNodeChildrenKeys(m_projectExpanderKey).size();
            moveNavigationNode(nodeKey, m_projectExpanderKey, appendIdx);
        });

    if (!categories.isEmpty()) {
        moveToCategoryMenu->addSeparator();
        for (const QString& catKey : categories) {
            const bool isCurrent = (parentKey == catKey);
            QAction* catAction = moveToCategoryMenu->addElaIconAction(
                isCurrent ? ElaIconType::Check : ElaIconType::Folder, getNavigationNodeTitle(catKey));
            catAction->setEnabled(!isCurrent);
            connect(catAction, &QAction::triggered, this, [this, nodeKey, catKey]()
                {
                    const int appendIdx = getNavigationNodeChildrenKeys(catKey).size();
                    if (moveNavigationNode(nodeKey, catKey, appendIdx)) {
                        expandNavigationNode(catKey);
                    }
                });
        }
    }

    moveToCategoryMenu->addSeparator();
    QAction* newCategoryAndMoveAction = moveToCategoryMenu->addElaIconAction(ElaIconType::FolderPlus, tr("新建分类并移入..."));
    connect(newCategoryAndMoveAction, &QAction::triggered, this, [this, nodeKey]()
        {
            handleNewProjectCategory(nodeKey);
        });
    adjustElaMenuWidth(moveToCategoryMenu, 180);

    menu.addSeparator();
    QAction* openDirAction = menu.addElaIconAction(ElaIconType::FolderOpen, tr("打开项目文件夹"));
    const fs::path projectDir = page->getProjectDir();
    connect(openDirAction, &QAction::triggered, this, [projectDir]()
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(projectDir.wstring())));
        });

    adjustElaMenuWidth(&menu);
    menu.exec(globalPos);
}

MainWindow::~MainWindow()
{
    delete m_aboutDialog;
}

void MainWindow::showEvent(QShowEvent* event)
{
    ElaWindow::showEvent(event);
    resize(m_windowSize);
}

void MainWindow::hideEvent(QHideEvent* event)
{
    m_windowSize = size();
    ElaWindow::hideEvent(event);
}

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
#else
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
#endif
{
    if (m_taskbarController) {
        m_taskbarController->handleNativeEvent(message);
    }
    return ElaWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::changeEvent(QEvent* event)
{
    ElaWindow::changeEvent(event);
    if (event && event->type() == QEvent::ActivationChange && isActiveWindow() && !isMinimized() && !m_projectPages.isEmpty()) {
        const QString currentKey = getCurrentNavigationPageKey();
        if (!currentKey.isEmpty() && getNodeKeyPoints(currentKey) > 0) {
            setNodeKeyPoints(currentKey, 0);
        }
    }
}

void MainWindow::updateTaskbarProgress()
{
    if (!m_taskbarController) {
        return;
    }
    int runningCount = 0;
    unsigned long long sumCompleted = 0;
    unsigned long long sumTotal = 0;
    for (const auto& page : m_projectPages) {
        if (page->getIsRunning()) {
            ++runningCount;
            const int total = page->getTotalLines();
            if (total > 0) {
                sumCompleted += static_cast<unsigned long long>(std::clamp(page->getCompletedLines(), 0, total));
                sumTotal += static_cast<unsigned long long>(total);
            }
        }
    }
    if (runningCount == 0) {
        m_taskbarController->setProgress(TaskbarController::ProgressState::NoProgress);
    }
    else if (sumTotal == 0) {
        m_taskbarController->setProgress(TaskbarController::ProgressState::Indeterminate);
    }
    else {
        m_taskbarController->setProgress(TaskbarController::ProgressState::Normal, sumCompleted, sumTotal);
    }
}

void MainWindow::updateTaskbarBadge()
{
    if (!m_taskbarController) {
        return;
    }
    int totalKeyPoints = 0;
    for (const auto& page : m_projectPages) {
        const int kp = getNodeKeyPoints(page->property("ElaPageKey").toString());
        if (kp > 0) {
            totalKeyPoints += kp;
        }
    }
    m_taskbarController->setBadgeNumber(totalKeyPoints);
}

void MainWindow::syncProjectsToGlobalConfig()
{
    if (!m_projectsLoaded) {
        return;
    }

    tomlkit::Item uncategorizedProjects = tomlkit::Item::array();
    tomlkit::Item categoriesArray = tomlkit::Item::arrayOfTables();

    const QStringList rootChildren = getNavigationNodeChildrenKeys(m_projectExpanderKey);
    for (const QString& childKey : rootChildren) {
        if (isProjectCategoryKey(childKey)) {
            tomlkit::Item catTable = tomlkit::Item::table(tomlkit::TableStyle::ArrayOfTablesElement);
            catTable["name"] = getNavigationNodeTitle(childKey).toStdString();
            catTable["expanded"] = getNavigationNodeIsExpanded(childKey);
            tomlkit::Item catProjects = tomlkit::Item::array();
            for (const QString& projKey : getNavigationNodeChildrenKeys(childKey)) {
                if (ProjectSettingsPage* page = findProjectPageByKey(projKey)) {
                    catProjects.push_back(wide2Ascii(page->getProjectDir()));
                }
            }
            catTable["projects"] = std::move(catProjects);
            categoriesArray.push_back(std::move(catTable));
        }
        else if (ProjectSettingsPage* page = findProjectPageByKey(childKey)) {
            uncategorizedProjects.push_back(wide2Ascii(page->getProjectDir()));
        }
    }

    if (categoriesArray.empty()) {
        categoriesArray.push_back(tomlkit::Item::table(tomlkit::TableStyle::ArrayOfTablesElement));
    }

    insertToml(*m_globalConfig, "projects", uncategorizedProjects);
    insertToml(*m_globalConfig, "projectCategories", categoriesArray);
}

void MainWindow::initWindow()
{
    // 详见 ElaWidgetTool 开源项目的示例
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(QIcon(":/GPPGUI/Resource/images/julixian_s.jpeg"));

    const int width = tomlkit::findOr(*m_globalConfig, "windowWidth", 1308);
    const int height = tomlkit::findOr(*m_globalConfig, "windowHeight", 676);
    m_windowSize = QSize(width, height);
    resize(m_windowSize);
    int x = tomlkit::findOr(*m_globalConfig, "windowPosX", 84);
    int y = tomlkit::findOr(*m_globalConfig, "windowPosY", 53);
    if (x < 0)x = 0;
    if (y < 0)y = 0;
    move(x, y);

    setUserInfoCardPixmap(QPixmap(":/GPPGUI/Resource/images/julixian_s.jpeg"));
    setUserInfoCardTitle("Galtransl++ v" + QString::fromUtf8(GPPVERSION));
    setUserInfoCardSubTitle("tianquyesss@gmail.com");
    connect(this, &MainWindow::userInfoCardClicked, this, [=]()
        {
            QDesktopServices::openUrl(QUrl("https://github.com/julixian"));
        });

    setWindowTitle("Galtransl++");

    setNavigationBarMinimumWidth(255);
    const int savedNavBarWidth = tomlkit::findOr(*m_globalConfig, "navigationBarWidth", 255);
    setNavigationBarWidth(savedNavBarWidth);
}

void MainWindow::initEdgeLayout()
{
    //菜单栏
    ElaMenuBar* menuBar = new ElaMenuBar(this);
    menuBar->setFixedHeight(30);
    QWidget* customWidget = new QWidget(this);
    int customWidgetWidth = 595;
    if (const std::string language = tomlkit::findOr(*m_globalConfig, "language", "zh_CN"); language == "en") {
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

    m_updateChecker = new UpdateChecker(*m_globalConfig, statusText, this);

    //停靠窗口
    ElaDockWidget* updateDockWidget = new ElaDockWidget(tr("更新内容"), this);
    updateDockWidget->setWidget(new UpdateWidget(this));
    this->addDockWidget(Qt::RightDockWidgetArea, updateDockWidget);
    resizeDocks({ updateDockWidget }, { 200 }, Qt::Horizontal);
    std::string gppversion(GPPVERSION);
    std::erase_if(gppversion, [](char ch) { return ch == '.'; });
    updateDockWidget->setVisible(tomlkit::findOr(*m_globalConfig, "showDockWidget", gppversion, true));
    insertToml(*m_globalConfig, "showDockWidget", tomlkit::Item::table());
    insertToml(*m_globalConfig, "showDockWidget." + gppversion, updateDockWidget->isVisible());
    connect(updateDockWidget, &ElaDockWidget::visibilityChanged, this, [=, this](bool visible)
        {
            insertToml(*m_globalConfig, "showDockWidget", tomlkit::Item::table());
            insertToml(*m_globalConfig, "showDockWidget." + gppversion, visible);
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
    connect(appBarMenu->addElaIconAction(ElaIconType::GearComplex, tr("应用设置")), &QAction::triggered, this, [=, this]()
        {
            navigation(m_appSettingsKey);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::MoonStars, tr("更改程序主题")), &QAction::triggered, this, [=]()
        {
            eTheme->setThemeMode(eTheme->getThemeMode() == ElaThemeType::Light ? ElaThemeType::Dark : ElaThemeType::Light);
        });
    connect(appBarMenu->addElaIconAction(ElaIconType::BroomWide, tr("清空当前项目翻译日志")), &QAction::triggered, this, [=, this]()
	    {
            onClearLog(true);
	    });

    setCustomMenu(appBarMenu);
}

void MainWindow::initContent()
{
    m_homePage = new HomePage(*m_globalConfig, this);
    m_defaultPromptPage = new DefaultPromptsPage(this);

    m_commonPreDictsPage = new CommonNormalDictsPage("pre", *m_globalConfig, this);
    m_commonGptDictsPage = new CommonGptDictsPage(*m_globalConfig, this);
    m_commonPostDictsPage = new CommonNormalDictsPage("post", *m_globalConfig, this);

    m_appSettingsPage = new AppSettingsPage(*m_globalConfig, this);

    addPageNode(tr("主页"), m_homePage, ElaIconType::House);

    addPageNode(tr("默认提示词管理"), m_defaultPromptPage, ElaIconType::Text);

    addExpanderNode(tr("通用字典管理"), m_commonDictExpanderKey, ElaIconType::FontCase);
    auto refreshCommonDictsFunc = [=, this]()
        {
            for (const auto& page : m_projectPages) {
                page->refreshCommonDicts();
            }
        };
    addPageNode(tr("通用译前字典"), m_commonPreDictsPage, m_commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(m_commonPreDictsPage, &CommonNormalDictsPage::commonDictsChangedSignal, this, refreshCommonDictsFunc);
    addPageNode(tr("通用GPT字典"), m_commonGptDictsPage, m_commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(m_commonGptDictsPage, &CommonGptDictsPage::commonDictsChangedSignal, this, refreshCommonDictsFunc);
    addPageNode(tr("通用译后字典"), m_commonPostDictsPage, m_commonDictExpanderKey, ElaIconType::OctagonDivide);
    connect(m_commonPostDictsPage, &CommonNormalDictsPage::commonDictsChangedSignal, this, refreshCommonDictsFunc);

    addExpanderNode(tr("项目管理"), m_projectExpanderKey, ElaIconType::BriefcaseBlank);
    setNavigationNodeCrossContainerDragEnabled(m_projectExpanderKey, true);

    auto isProjectAlreadyLoaded = [&](const fs::path& projectDir) -> bool
        {
            return std::ranges::any_of(m_projectPages, [&](const auto& page)
                {
                    std::error_code ec;
                    if (fs::equivalent(page->getProjectDir(), projectDir, ec) && !ec) {
                        return true;
                    }
                    return page->getProjectDir().lexically_normal() == projectDir.lexically_normal();
                });
        };

    QList<QPair<QString, bool>> categoryExpandStates;
    const auto categories = tomlkit::findArrayOrEmpty(*m_globalConfig, "projectCategories");
    for (const auto& categoryItem : categories) {
        if (!categoryItem.isTable()) {
            continue;
        }
        const QString catName = QString::fromStdString(tomlkit::findOr(categoryItem, "name", "")).trimmed();
        if (catName.isEmpty() || hasCategoryName(catName)) {
            continue;
        }
        const bool expanded = tomlkit::findOr(categoryItem, "expanded", true);
        const QString catKey = createProjectCategory(catName);
        if (catKey.isEmpty()) {
            continue;
        }
        const auto catProjects = tomlkit::findArrayOrEmpty(categoryItem, "projects");
        for (const auto& project : catProjects) {
            if (!project.isString()) {
                continue;
            }
            const fs::path projectDir(ascii2Wide(project.asString()));
            if (!fs::exists(projectDir / L"Config.toml") || isProjectAlreadyLoaded(projectDir)) {
                continue;
            }
            createProjectSettingsPage(projectDir, catKey);
        }
        categoryExpandStates.append({ catKey, expanded });
    }

    const auto projects = tomlkit::findArrayOrEmpty(*m_globalConfig, "projects");
    for (const auto& project : projects) {
        if (!project.isString()) {
            continue;
        }
        const fs::path projectDir(ascii2Wide(project.asString()));
        if (!fs::exists(projectDir / L"Config.toml") || isProjectAlreadyLoaded(projectDir)) {
            continue;
        }
        createProjectSettingsPage(projectDir, m_projectExpanderKey);
    }

    expandNavigationNode(m_projectExpanderKey);
    for (const auto& [catKey, expanded] : categoryExpandStates) {
        if (expanded) {
            expandNavigationNode(catKey);
        }
        else {
            collapseNavigationNode(catKey);
        }
    }
    m_projectsLoaded = true;

    addFooterNode(tr("使用说明"), nullptr, m_illustrationKey, 0, ElaIconType::BookOpen);
    addFooterNode(tr("关于"), nullptr, m_aboutKey, 0, ElaIconType::User);
    m_aboutDialog = new AboutDialog();
    m_aboutDialog->hide();
    addFooterNode(tr("应用设置"), m_appSettingsPage, m_appSettingsKey, 0, ElaIconType::GearComplex);

    connect(m_aboutDialog, &AboutDialog::checkUpdateSignal, this, [=, this]()
        {
            ElaMessageBar::information(ElaMessageBarType::TopLeft, tr("请稍候"), tr("正在检查更新..."), 3000);
            m_updateChecker->check();
        });
    connect(m_aboutDialog, &AboutDialog::downloadUpdateSignal, this, [=, this]()
        {
            m_aboutDialog->setDownloadButtonEnabled(false);
            ElaMessageBar::information(ElaMessageBarType::TopLeft, tr("请稍候"), tr("正在下载更新..."), 3000);
            m_updateChecker->check(true);
        });

    connect(this, &MainWindow::navigationNodeClicked, this, [=, this](ElaNavigationType::NavigationNodeType nodeType, const QString& nodeKey)
        {
            if (m_illustrationKey == nodeKey) {
                QDesktopServices::openUrl(QUrl::fromLocalFile("BaseConfig/illustration/foundation.html"));
            }
            else if (m_aboutKey == nodeKey)
            {
                m_aboutDialog->setFixedSize(400, 400);
                m_aboutDialog->moveToCenter();
                m_aboutDialog->show();
            }
        });

    m_clearLogShortcut = new QShortcut(this);
    const std::string clearLogShortcut = tomlkit::findOr(*m_globalConfig, "clearLogShortcut", "Ctrl+L");
    if (const QKeySequence keySequence = QKeySequence::fromString(QString::fromStdString(clearLogShortcut), QKeySequence::PortableText);
        !keySequence.isEmpty())
    {
        m_clearLogShortcut->setKey(keySequence);
    }
    connect(m_appSettingsPage, &AppSettingsPage::clearLogShortcutChangedSignal, this, [=, this](const QString& shortcut)
        {
            m_clearLogShortcut->setKey(QKeySequence::fromString(shortcut, QKeySequence::PortableText));
        });
    connect(m_clearLogShortcut, &QShortcut::activated, this, [=, this]()
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
    const QString parentPathQStr = QFileDialog::getExistingDirectory(this, tr("选择新项目的存放位置"),
        QString::fromStdString(tomlkit::findOr(*m_globalConfig, "lastProjectPath", "./Projects")));
    if (parentPathQStr.isEmpty()) {
        return;
    }

    insertToml(*m_globalConfig, "lastProjectPath", parentPathQStr.toStdString());

    QString projectName;
    GppInputDialog inputDialog(tr("请输入项目名称"), tr("新建项目"), projectName, this);
    if (inputDialog.exec() != QDialog::Accepted || projectName.isEmpty()) {
        return;
    }

    if (std::ranges::any_of(m_projectPages, [&](auto& page)
        {
            return projectName == page->getProjectName();
        }))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), tr("已存在同名项目！"), 3000);
        return;
    }

    const fs::path parentPath = parentPathQStr.toStdWString();
    const fs::path newProjectDir = parentPath / projectName.toStdWString();

    try {
        if (!fs::equivalent(newProjectDir.parent_path(), parentPath)) {
            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), tr("非法的项目名！"), 3000);
            return;
        }
        if (fs::exists(newProjectDir)) {
            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), tr("目录下存在同名文件或文件夹！"), 3000);
            return;
        }
        fs::create_directory(newProjectDir);
    }
    catch (const std::exception& e) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), e.what(), 3000);
        return;
    }

    QFile resourceSampleProjectFile(":/GPPGUI/Resource/SampleProject.zip");
    if (resourceSampleProjectFile.open(QIODevice::ReadOnly)) {
        QFile outputSampleProjectFile(parentPathQStr + "/" + projectName + "/SampleProject.zip");
        if (outputSampleProjectFile.open(QIODevice::WriteOnly)) {
            outputSampleProjectFile.write(resourceSampleProjectFile.readAll());
            outputSampleProjectFile.close();
        }
        else {
            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), tr("无法创建新文件！"), 3000);
            return;
        }
    }
    else {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), tr("无法读取模板文件！"), 3000);
        return;
    }

    extractZip(newProjectDir / L"SampleProject.zip", newProjectDir);

    try {
        fs::remove(newProjectDir / L"SampleProject.zip");

        if (fs::exists(L"BaseConfig/Prompt.toml")) {
            fs::copy(L"BaseConfig/Prompt.toml", newProjectDir / L"Prompt.toml", fs::copy_options::overwrite_existing);
        }

        toml configData = parseToml(newProjectDir / L"Config.toml");

        auto addCommonDictsToProjectConfig = [&](const std::string& projectDictStdName, const std::string& globalConfigKey, const std::string& projectConfigKey)
            {
                auto& globalConfigDictNames = (*m_globalConfig)[globalConfigKey]["dictNames"];
                auto& projectDictFileNames = configData["dictionary"][projectConfigKey];
                if (!globalConfigDictNames.isArray()) {
                    globalConfigDictNames = tomlkit::Item::array();
                }

                projectDictFileNames = tomlkit::Item::array();
                projectDictFileNames.push_back(projectDictStdName);
                for (const auto& globalConfigDictName : globalConfigDictNames) {
                    if (!globalConfigDictName.isString()) {
                        continue;
                    }
                    if (!tomlkit::findOr(*m_globalConfig, globalConfigKey, "spec", globalConfigDictName.asString(), "defaultOn", true)) {
                        continue;
                    }
                    const std::string globalConfigDictFileName = globalConfigDictName.asString() + ".toml";
                    if (std::ranges::any_of(projectDictFileNames, [&](const auto& projectDictFileName)
	                    {
                            return projectDictFileName.isString() && projectDictFileName.asString() == globalConfigDictFileName;
	                    }))
                    {
                        continue;
                    }
                    projectDictFileNames.push_back(globalConfigDictFileName);
                }
            };

        addCommonDictsToProjectConfig("ProjPreDict.toml", "commonPreDicts", "preDicts");
        addCommonDictsToProjectConfig("ProjGptDict.toml", "commonGptDicts", "gptDicts");
        addCommonDictsToProjectConfig("ProjPostDict.toml", "commonPostDicts", "postDicts");

        atomicOutputFile(newProjectDir / L"Config.toml", tomlkit::format(configData));
    }
    catch (...) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("创建失败"), tr("无法写入配置文件！"), 3000);
        return;
    }

    ProjectSettingsPage* newPage = createProjectSettingsPage(newProjectDir);
    syncProjectsToGlobalConfig();
    this->navigation(newPage->property("ElaPageKey").toString());

    const QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(newProjectDir.wstring()));
    QDesktopServices::openUrl(dirUrl);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("创建成功"), tr("请将待翻译的文件放入 gt_input 中！"), 8000);
}

void MainWindow::onOpenProjectTriggered()
{
    const QString projectPath = QFileDialog::getExistingDirectory(this, tr("选择已有项目的文件夹路径"),
        QString::fromStdString(tomlkit::findOr(*m_globalConfig, "lastProjectPath", "./Projects")));
    if (projectPath.isEmpty()) {
        return;
    }
    insertToml(*m_globalConfig, "lastProjectPath", projectPath.toStdString());
    const fs::path projectDir(projectPath.toStdWString());
    if (!fs::exists(projectDir / L"Config.toml")) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("打开失败"), tr("目录下不存在 Config.toml 文件！"), 3000);
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
    syncProjectsToGlobalConfig();
    this->navigation(newPage->property("ElaPageKey").toString());

	const QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(projectDir.wstring()));
	QDesktopServices::openUrl(dirUrl);
	ElaMessageBar::success(ElaMessageBarType::TopRight, tr("打开成功"),
		tr("%1 已纳入项目管理！").arg(newPage->getProjectName()), 3000);
}

void MainWindow::onRemoveProjectTriggered()
{
    QString currentPageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](const auto& projectPage)
        {
            return projectPage->property("ElaPageKey").toString() == currentPageKey;
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

    QWidget* helpDialogWidget = new QWidget(&helpDialog);
    QVBoxLayout* helpDialogLayout = new QVBoxLayout(helpDialogWidget);
    helpDialogLayout->setContentsMargins(15, 25, 15, 10);
    ElaText* helpDialogTitle = new ElaText(tr("你确定要移除当前项目吗？"), helpDialogWidget);
    helpDialogTitle->setTextStyle(ElaTextType::Title);
    helpDialogTitle->setWordWrap(false);
    helpDialogLayout->addWidget(helpDialogTitle);
    helpDialogLayout->addSpacing(2);
    ElaText* helpDialogBody = new ElaText(tr("从项目管理中移除该项目，但不会删除其项目文件夹"), helpDialogWidget);
    helpDialogBody->setTextStyle(ElaTextType::Body);
    helpDialogLayout->addWidget(helpDialogBody);
    helpDialogLayout->addStretch();
    helpDialog.setCentralWidget(helpDialogWidget);

    if (helpDialog.exec() == QDialog::Accepted) {
        const QString projectName = it->get()->getProjectName();
        it->get()->apply2Config();
        removeNavigationNode(currentPageKey);
        m_projectPages.erase(it);
        syncProjectsToGlobalConfig();
        updateTaskbarProgress();
        updateTaskbarBadge();
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
    const QString currentPageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](const auto& projectPage)
        {
            return projectPage->property("ElaPageKey").toString() == currentPageKey;
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

    QWidget* helpDialogWidget = new QWidget(&helpDialog);
    QVBoxLayout* helpDialogLayout = new QVBoxLayout(helpDialogWidget);
    helpDialogLayout->setContentsMargins(15, 25, 15, 10);
    ElaText* helpDialogTitle = new ElaText(tr("你确定要删除当前项目吗？                "), helpDialogWidget);
    helpDialogTitle->setTextStyle(ElaTextType::Title);
    helpDialogTitle->setWordWrap(false);
    helpDialogLayout->addWidget(helpDialogTitle);
    helpDialogLayout->addSpacing(2);
    ElaText* helpDialogBody = new ElaText(tr("将删除该项目的项目文件夹，如果不备份，再次翻译将必须从头开始！"), helpDialogWidget);
    helpDialogBody->setTextStyle(ElaTextType::Body);
    helpDialogLayout->addWidget(helpDialogBody);
    helpDialogLayout->addStretch();
    helpDialog.setCentralWidget(helpDialogWidget);

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
        removeNavigationNode(currentPageKey);
        m_projectPages.erase(it);
        syncProjectsToGlobalConfig();
        updateTaskbarProgress();
        updateTaskbarBadge();
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
    const QString currentPageKey = getCurrentNavigationPageKey();
    const auto it = std::ranges::find_if(m_projectPages, [&](const auto& projectPage)
        {
            return projectPage->property("ElaPageKey").toString() == currentPageKey;
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
    if (isActiveWindow() && !isMinimized() && getCurrentNavigationPageKey() == nodeKey) {
        return;
    }
    setNodeKeyPoints(nodeKey, getNodeKeyPoints(nodeKey) + 1);
}

void MainWindow::onCloseWindowClicked(bool restart)
{
    m_defaultPromptPage->apply2Config();
    m_commonPreDictsPage->apply2Config();
    m_commonGptDictsPage->apply2Config();
    m_commonPostDictsPage->apply2Config();

    for (const auto& projectPage : m_projectPages) {
        projectPage->apply2Config();
    }
    syncProjectsToGlobalConfig();

    m_appSettingsPage->apply2Config();
    insertToml(*m_globalConfig, "navigationBarWidth", getNavigationBarWidth());
    insertToml(*m_globalConfig, "clearLogShortcut",
        m_clearLogShortcut->key().toString(QKeySequence::PortableText).toStdString());

    atomicOutputFile(L"BaseConfig/GlobalConfig.toml", tomlkit::format(*m_globalConfig));

    if (m_updateChecker->shouldStartUpdater()) {
        QStringList arguments;
        arguments << "--pid" << QString::number(QApplication::applicationPid());
        arguments << "--source" << QApplication::applicationDirPath() + "/" + gppUpdatePackage;
        arguments << "--target" << QApplication::applicationDirPath();
        if (restart) {
            arguments << "--restart" << QApplication::applicationFilePath();
        }
        QProcess updater;
        updater.setProgram(QApplication::applicationDirPath() + "/" + gppUpdaterExecutable);
        updater.setArguments(arguments);
        updater.setWorkingDirectory(QApplication::applicationDirPath());
#ifdef Q_OS_WIN
        // startDetached 不会脱离 Windows Job，必须显式排除主进程退出时的子进程清理。
        updater.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args)
            {
                args->flags |= CREATE_BREAKAWAY_FROM_JOB;
            });
#endif
        updater.startDetached();
    }
    MainWindow::close();
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
