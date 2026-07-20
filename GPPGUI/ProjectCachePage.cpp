#include "ProjectCachePage.h"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>

#include "ElaIcon.h"
#include "ElaIconButton.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaMenu.h"
#include "ElaPushButton.h"
#include "ElaToolButton.h"
#include "ElaText.h"

#include "ProjectCachePage_p.h"

using namespace ProjectCachePagePrivate;

ProjectCachePage::ProjectCachePage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectDir(projectDir), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("缓存管理"));
    setTitleVisible(false);
    setupUi();
}

void ProjectCachePage::ensureCacheFilesLoaded()
{
    if (m_cacheFilesLoaded) {
        return;
    }
    loadCacheFiles();
}

void ProjectCachePage::refreshCacheFiles()
{
    loadCacheFiles(true);
}

void ProjectCachePage::setupUi()
{
    // 缓存页是一个偏高密度的工具界面：左侧放文件/搜索/问题导航和批量操作，
    // 右侧留给当前缓存文件的句子概览，便于快速扫描和跳转编辑。
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(10, 10, 10, 0);
    mainLayout->setSpacing(8);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    m_cacheDirLabel = new ElaText(QString::fromStdWString(getCacheDir().wstring()), BodyFontPx, mainWidget);
    m_cacheDirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cacheDirLabel->setStyleSheet(auxiliaryTextStyle());
    topLayout->addWidget(m_cacheDirLabel, 1);

    ElaIconButton* openCacheButton = createHeaderIconButton(ElaIconType::FolderOpen, tr("打开缓存文件夹"), mainWidget);
    connect(openCacheButton, &ElaIconButton::clicked, this, [=]()
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(getCacheDir().wstring())));
        });
    topLayout->addWidget(openCacheButton);

    ElaIconButton* refreshButton = createHeaderIconButton(ElaIconType::Rotate, tr("刷新"), mainWidget);
    connect(refreshButton, &ElaIconButton::clicked, this, [=]()
        {
            if (!m_dirtyFiles.isEmpty()
                && !confirmAction(tr("确认刷新"), tr("刷新会放弃所有未保存的缓存修改，确定要继续吗？"))) {
                return;
            }
            loadCacheFiles(true);
        });
    topLayout->addWidget(refreshButton);

    m_saveButton = createHeaderIconButton(ElaIconType::FloppyDisk, tr("保存当前文件"), mainWidget);
    connect(m_saveButton, &ElaIconButton::clicked, this, [=]()
        {
            if (!ensureWritableAction(tr("保存缓存"))) {
                return;
            }
            QString error;
            if (writeCacheFile(m_currentFile, m_entries, &error)) {
                m_loadedEntriesByFile[m_currentFile] = m_entries;
                m_dirtyFiles.remove(m_currentFile);
                renderFileList();
                m_currentFileLabel->setText(m_currentFile);
                setInfo(tr("已保存 %1").arg(m_currentFile));
            }
            else {
                setError(error);
            }
            updateActionStates();
        });
    topLayout->addWidget(m_saveButton);

    m_saveAllButton = createHeaderIconButton(ElaIconType::FloppyDisks, tr("保存全部"), mainWidget);
    connect(m_saveAllButton, &ElaIconButton::clicked, this, [=]()
        {
            if (!ensureWritableAction(tr("保存缓存"))) {
                return;
            }
            int saved = 0;
            QString lastError;
            const QStringList files = m_dirtyFiles.values();
            for (const QString& filename : files) {
                const auto it = m_loadedEntriesByFile.find(filename);
                if (it == m_loadedEntriesByFile.end()) {
                    continue;
                }
                QString error;
                if (writeCacheFile(filename, it.value(), &error)) {
                    m_dirtyFiles.remove(filename);
                    ++saved;
                }
                else {
                    lastError = error;
                }
            }
            renderFileList();
            if (!m_currentFile.isEmpty()) {
                m_currentFileLabel->setText((m_dirtyFiles.contains(m_currentFile) ? "*" : "") + m_currentFile);
            }
            if (!lastError.isEmpty()) {
                setError(lastError);
            }
            else {
                setInfo(tr("已保存 %1 个缓存文件").arg(QString::number(saved)));
            }
            updateActionStates();
        });
    topLayout->addWidget(m_saveAllButton);
    mainLayout->addLayout(topLayout);

    m_mainSplitter = new QSplitter(Qt::Horizontal, mainWidget);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(8);
    m_mainSplitter->setStyleSheet(splitterStyle());

    QWidget* sidebarWidget = new QWidget(m_mainSplitter);
    sidebarWidget->setMinimumWidth(270);
    sidebarWidget->setMaximumWidth(330);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(0, 0, 6, 0);
    sidebarLayout->setSpacing(5);

    QHBoxLayout* navLayout = new QHBoxLayout();
    navLayout->setSpacing(4);
    m_sidebarButtonGroup = new QButtonGroup(this);
    m_sidebarButtonGroup->setExclusive(true);
    auto addNavButton = [&](const QString& text, int index)
        {
            ElaPushButton* button = new ElaPushButton(text, sidebarWidget);
            button->setCheckable(true);
            button->setMinimumHeight(30);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            QFont font = button->font();
            font.setPixelSize(BodyFontPx);
            button->setFont(font);
            m_sidebarButtonGroup->addButton(button, index);
            navLayout->addWidget(button);
            return button;
        };
    m_filesNavButton = addNavButton(tr("文件"), 0);
    m_searchNavButton = addNavButton(tr("搜索"), 1);
    m_problemsNavButton = addNavButton(tr("问题"), 2);
    connect(m_sidebarButtonGroup, &QButtonGroup::idClicked, this, [=](int index)
        {
            setSidebarPage(index);
        });
    sidebarLayout->addLayout(navLayout);

    m_sidebarStack = new QStackedWidget(sidebarWidget);

    QWidget* filesTab = new QWidget(m_sidebarStack);
    QVBoxLayout* filesLayout = new QVBoxLayout(filesTab);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(4);

    m_deleteFilesButton = new ElaToolButton(filesTab);
    m_deleteFilesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_deleteFilesButton->setElaIcon(ElaIconType::Trash);
    m_deleteFilesButton->setText(tr("删除选中文件"));
    connect(m_deleteFilesButton, &ElaToolButton::clicked, this, [=]()
        {
            if (!ensureWritableAction(tr("删除缓存文件"))) {
                return;
            }
            const QModelIndexList selected = m_fileList->selectionModel()->selectedRows();
            if (selected.empty()) {
                return;
            }
            if (!confirmAction(tr("确认删除"),
                tr("确定要删除选中的 %1 个缓存文件吗？").arg(QString::number(selected.size())))) {
                return;
            }
            int deleted = 0;
            for (const QModelIndex& index : selected) {
                const QString filename = index.data(Qt::UserRole).toString();
                std::error_code ec;
                fs::remove(cachePathForRelativeName(filename), ec);
                if (!ec) {
                    m_loadedEntriesByFile.remove(filename);
                    m_dirtyFiles.remove(filename);
                    if (m_currentFile == filename) {
                        m_currentFile.clear();
                        m_entries = json::array();
                        m_selectedEntryRows.clear();
                        renderEntries();
                    }
                    ++deleted;
                }
            }
            loadCacheFiles();
            setInfo(tr("已删除 %1 个缓存文件").arg(QString::number(deleted)));
        });
    filesLayout->addWidget(m_deleteFilesButton);

    m_fileModel = new QStandardItemModel(this);
    m_fileList = new ElaListView(filesTab);
    m_fileList->setModel(m_fileModel);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileList->setItemHeight(42);
    connect(m_fileList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            if (!index.isValid()) {
                return;
            }
            loadCacheFile(index.data(Qt::UserRole).toString());
            updateActionStates();
        });
    connect(m_fileList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]()
        {
            updateActionStates();
        });
    filesLayout->addWidget(m_fileList, 1);
    m_sidebarStack->addWidget(filesTab);

    QWidget* searchTab = new QWidget(m_sidebarStack);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchTab);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(4);

    QHBoxLayout* globalSearchOptionsLayout = new QHBoxLayout();
    globalSearchOptionsLayout->setContentsMargins(0, 0, 0, 0);
    globalSearchOptionsLayout->setSpacing(4);
    QTimer* globalSearchTimer = new QTimer(searchTab);
    globalSearchTimer->setSingleShot(true);
    globalSearchTimer->setInterval(200);

    m_globalSearchEdit = new ElaLineEdit(searchTab);
    m_globalSearchEdit->setPlaceholderText(tr("搜索内容..."));
    m_globalSearchEdit->setIsClearButtonEnable(true);
    m_globalRegexErrorAction = m_globalSearchEdit->addAction(
        ElaIcon::getInstance()->getElaIcon(ElaIconType::TriangleExclamation),
        QLineEdit::TrailingPosition);
    m_globalRegexErrorAction->setVisible(false);
    searchLayout->addWidget(m_globalSearchEdit);

    m_globalSearchField = new ElaToolButton(searchTab);
    m_globalSearchField->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_globalSearchField->setElaIcon(ElaIconType::BarsFilter);
    m_globalSearchField->setText(tr("全部"));
    m_globalSearchField->setMinimumWidth(135);
    m_globalSearchField->setPopupMode(QToolButton::InstantPopup);
    ElaMenu* globalSearchMenu = new ElaMenu(m_globalSearchField);
    globalSearchMenu->setFixedWidth(135);
    QActionGroup* globalSearchGroup = new QActionGroup(globalSearchMenu);
    globalSearchGroup->setExclusive(true);
    const QList<QPair<QString, QString>> globalSearchFields = {
        { tr("全部"), "all" },
        { "preproc", "src" },
        { "transraw", "dst" },
        { "problems", "problems" },
    };
    for (int i = 0; i < globalSearchFields.size(); ++i) {
        QAction* action = globalSearchMenu->addAction(globalSearchFields.at(i).first);
        action->setCheckable(true);
        action->setChecked(i == 0);
        action->setData(globalSearchFields.at(i).second);
        globalSearchGroup->addAction(action);
    }
    m_globalSearchField->setMenu(globalSearchMenu);
    connect(globalSearchGroup, &QActionGroup::triggered, this, [=](QAction* action)
        {
            m_globalSearchFieldKey = action->data().toString();
            m_globalSearchField->setText(action->text());
            globalSearchTimer->start();
        });

    m_globalRegexButton = new ElaToolButton(searchTab);
    m_globalRegexButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_globalRegexButton->setText(".*");
    m_globalRegexButton->setCheckable(true);
    m_globalRegexButton->setFixedWidth(25);
    m_globalRegexButton->setToolTip(tr("正则搜索"));
    globalSearchOptionsLayout->addWidget(m_globalSearchField, 1);
    globalSearchOptionsLayout->addWidget(m_globalRegexButton);
    searchLayout->addLayout(globalSearchOptionsLayout);

    connect(globalSearchTimer, &QTimer::timeout, this, &ProjectCachePage::runGlobalSearch);
    connect(m_globalSearchEdit, &ElaLineEdit::returnPressed, this, [=]()
        {
            globalSearchTimer->stop();
            runGlobalSearch();
        });
    connect(m_globalSearchEdit, &ElaLineEdit::textChanged, this, [=]()
        {
            globalSearchTimer->start();
        });
    connect(m_globalRegexButton, &ElaToolButton::toggled, this, [=](bool)
        {
            globalSearchTimer->start();
        });

    // 批量替换默认收起，日常浏览时优先把空间留给搜索结果列表。
    m_replaceToggleButton = new ElaToolButton(searchTab);
    m_replaceToggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_replaceToggleButton->setElaIcon(ElaIconType::AngleDown);
    m_replaceToggleButton->setText(tr("展开批量替换"));
    connect(m_replaceToggleButton, &ElaToolButton::clicked, this, [=]()
        {
            setReplacePanelVisible(!m_replacePanel->isVisible());
        });
    searchLayout->addWidget(m_replaceToggleButton);

    m_replacePanel = new QWidget(searchTab);
    QVBoxLayout* replaceLayout = new QVBoxLayout(m_replacePanel);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);

    m_replaceQueryEdit = new ElaLineEdit(searchTab);
    m_replaceQueryEdit->setPlaceholderText(tr("查找"));
    m_replaceQueryEdit->setIsClearButtonEnable(true);
    m_replaceRegexErrorAction = m_replaceQueryEdit->addAction(
        ElaIcon::getInstance()->getElaIcon(ElaIconType::TriangleExclamation),
        QLineEdit::TrailingPosition);
    m_replaceRegexErrorAction->setVisible(false);
    replaceLayout->addWidget(m_replaceQueryEdit);

    m_replaceWithEdit = new ElaLineEdit(searchTab);
    m_replaceWithEdit->setPlaceholderText(tr("替换为"));
    m_replaceWithEdit->setIsClearButtonEnable(true);
    replaceLayout->addWidget(m_replaceWithEdit);

    QHBoxLayout* replaceFieldLayout = new QHBoxLayout();
    replaceFieldLayout->setContentsMargins(0, 0, 0, 0);
    replaceFieldLayout->setSpacing(4);
    m_replaceField = new ElaToolButton(searchTab);
    m_replaceField->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_replaceField->setElaIcon(ElaIconType::BarsFilter);
    m_replaceField->setText(tr("transraw"));
    m_replaceField->setMinimumWidth(135);
    m_replaceField->setPopupMode(QToolButton::InstantPopup);
    ElaMenu* replaceMenu = new ElaMenu(m_replaceField);
    replaceMenu->setFixedWidth(135);
    QActionGroup* replaceGroup = new QActionGroup(replaceMenu);
    replaceGroup->setExclusive(true);
    const QList<QPair<QString, QString>> replaceFields = {
        { tr("transraw"), "dst" },
        { tr("preproc"), "src" },
        { tr("全部"), "all" },
    };
    for (int i = 0; i < replaceFields.size(); ++i) {
        QAction* action = replaceMenu->addAction(replaceFields.at(i).first);
        action->setCheckable(true);
        action->setChecked(i == 0);
        action->setData(replaceFields.at(i).second);
        replaceGroup->addAction(action);
    }
    m_replaceField->setMenu(replaceMenu);
    connect(replaceGroup, &QActionGroup::triggered, this, [=](QAction* action)
        {
            m_replaceFieldKey = action->data().toString();
            m_replaceField->setText(action->text());
        });
    replaceFieldLayout->addWidget(m_replaceField, 1);
    m_replaceRegexButton = new ElaToolButton(searchTab);
    m_replaceRegexButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_replaceRegexButton->setText(".*");
    m_replaceRegexButton->setCheckable(true);
    m_replaceRegexButton->setFixedWidth(25);
    m_replaceRegexButton->setToolTip(tr("正则替换"));
    replaceFieldLayout->addWidget(m_replaceRegexButton);
    replaceLayout->addLayout(replaceFieldLayout);

    QHBoxLayout* replaceButtonLayout = new QHBoxLayout();
    ElaToolButton* replacePreviewButton = new ElaToolButton(searchTab);
    replacePreviewButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    replacePreviewButton->setElaIcon(ElaIconType::Eye);
    replacePreviewButton->setText(tr("预览"));
    connect(replacePreviewButton, &ElaToolButton::clicked, this, &ProjectCachePage::previewReplace);
    replaceButtonLayout->addWidget(replacePreviewButton);
    m_replaceExecuteButton = new ElaToolButton(searchTab);
    m_replaceExecuteButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_replaceExecuteButton->setElaIcon(ElaIconType::Repeat);
    m_replaceExecuteButton->setText(tr("替换"));
    connect(m_replaceExecuteButton, &ElaToolButton::clicked, this, &ProjectCachePage::executeReplace);
    replaceButtonLayout->addWidget(m_replaceExecuteButton);
    replaceLayout->addLayout(replaceButtonLayout);

    m_replacePreviewLabel = new ElaText("", BodyFontPx, searchTab);
    m_replacePreviewLabel->setWordWrap(true);
    m_replacePreviewLabel->setStyleSheet(auxiliaryTextStyle());
    replaceLayout->addWidget(m_replacePreviewLabel);
    connect(m_replaceRegexButton, &ElaToolButton::toggled, this, [=](bool)
        {
            setRegexError(m_replaceQueryEdit, m_replaceRegexErrorAction, {});
            m_replacePreviewLabel->clear();
        });
    searchLayout->addWidget(m_replacePanel);

    m_searchStatusLabel = new ElaText("", BodyFontPx, searchTab);
    m_searchStatusLabel->setStyleSheet(auxiliaryTextStyle());
    searchLayout->addWidget(m_searchStatusLabel);

    m_searchModel = new QStandardItemModel(this);
    m_searchResultList = new ElaListView(searchTab);
    m_searchResultList->setModel(m_searchModel);
    m_searchResultList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_searchResultList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 高度需要和 CacheSearchDelegate 的四行卡片布局保持同步。
    m_searchResultList->setItemHeight(120);
    m_searchResultList->setItemDelegate(createCacheSearchDelegate(m_searchResultList));
    m_searchResultList->setMinimumHeight(520);
    connect(m_searchResultList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            if (index.isValid()) {
                jumpToHit(index.data(HitIndexRole).toInt());
            }
        });
    searchLayout->addWidget(m_searchResultList, 1);
    m_sidebarStack->addWidget(searchTab);

    QWidget* problemsTab = new QWidget(m_sidebarStack);
    QVBoxLayout* problemsLayout = new QVBoxLayout(problemsTab);
    problemsLayout->setContentsMargins(0, 0, 0, 0);
    problemsLayout->setSpacing(4);

    ElaToolButton* refreshProblemsButton = new ElaToolButton(problemsTab);
    refreshProblemsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    refreshProblemsButton->setElaIcon(ElaIconType::Rotate);
    refreshProblemsButton->setText(tr("刷新问题"));
    connect(refreshProblemsButton, &ElaToolButton::clicked, this, &ProjectCachePage::loadProblems);
    problemsLayout->addWidget(refreshProblemsButton);
    // 问题会随缓存文件刷新自动聚合，暂时保留手动刷新实现但不在界面显示。
    refreshProblemsButton->hide();

    m_problemModel = new QStandardItemModel(this);
    m_problemList = new ElaListView(problemsTab);
    m_problemList->setModel(m_problemModel);
    m_problemList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_problemList->setItemHeight(32);
    connect(m_problemList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            if (!index.isValid()) {
                return;
            }
            setSidebarPage(1);
            m_globalSearchFieldKey = "problems";
            for (QAction* action : globalSearchGroup->actions()) {
                if (action->data().toString() == m_globalSearchFieldKey) {
                    action->setChecked(true);
                    m_globalSearchField->setText(action->text());
                    break;
                }
            }
            m_globalSearchEdit->setText(index.data(ProblemTextRole).toString());
            runGlobalSearch();
        });
    problemsLayout->addWidget(m_problemList, 1);
    m_sidebarStack->addWidget(problemsTab);

    sidebarLayout->addWidget(m_sidebarStack, 1);
    m_mainSplitter->addWidget(sidebarWidget);

    QWidget* editorWidget = new QWidget(m_mainSplitter);
    QVBoxLayout* editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setContentsMargins(8, 0, 0, 0);
    editorLayout->setSpacing(8);

    QHBoxLayout* currentLayout = new QHBoxLayout();
    m_currentFileLabel = new ElaText(tr("未选择缓存文件"), TitleFontPx, editorWidget);
    m_currentFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont currentFont = m_currentFileLabel->font();
    currentFont.setBold(true);
    m_currentFileLabel->setFont(currentFont);
    currentLayout->addWidget(m_currentFileLabel, 1);
    m_currentSummaryLabel = new ElaText("", BodyFontPx, editorWidget);
    m_currentSummaryLabel->setStyleSheet(auxiliaryTextStyle());
    m_currentSummaryLabel->setWordWrap(false);
    m_currentSummaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_currentSummaryLabel->setMinimumWidth(360);
    currentLayout->addWidget(m_currentSummaryLabel);
    editorLayout->addLayout(currentLayout);

    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);
    m_localSearchEdit = new ElaLineEdit(editorWidget);
    m_localSearchEdit->setPlaceholderText(tr("在当前文件中搜索 preproc/transraw/problems..."));
    m_localSearchEdit->setIsClearButtonEnable(true);
    m_localRegexErrorAction = m_localSearchEdit->addAction(
        ElaIcon::getInstance()->getElaIcon(ElaIconType::TriangleExclamation),
        QLineEdit::TrailingPosition);
    m_localRegexErrorAction->setVisible(false);
    QTimer* localSearchTimer = new QTimer(editorWidget);
    localSearchTimer->setSingleShot(true);
    localSearchTimer->setInterval(200);
    connect(localSearchTimer, &QTimer::timeout, this, [=]()
        {
            renderEntries();
        });
    connect(m_localSearchEdit, &ElaLineEdit::textChanged, this, [=]()
        {
            localSearchTimer->start();
        });
    filterLayout->addWidget(m_localSearchEdit, 1);
    m_localRegexButton = new ElaToolButton(editorWidget);
    m_localRegexButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_localRegexButton->setText(".*");
    m_localRegexButton->setCheckable(true);
    m_localRegexButton->setFixedWidth(25);
    m_localRegexButton->setToolTip(tr("正则搜索"));
    connect(m_localRegexButton, &ElaToolButton::toggled, this, [=](bool)
        {
            localSearchTimer->start();
        });
    filterLayout->addWidget(m_localRegexButton);

    m_filterProblemsCheck = new ElaToolButton(editorWidget);
    m_filterProblemsCheck->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_filterProblemsCheck->setElaIcon(ElaIconType::CircleExclamation);
    m_filterProblemsCheck->setFixedWidth(36);
    m_filterProblemsCheck->setCheckable(true);
    m_filterProblemsCheck->setToolTip(tr("只看问题句"));
    connect(m_filterProblemsCheck, &ElaToolButton::toggled, this, [=]()
        {
            renderEntries();
        });
    filterLayout->addWidget(m_filterProblemsCheck);

    m_editEntryButton = new ElaToolButton(editorWidget);
    m_editEntryButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_editEntryButton->setElaIcon(ElaIconType::PenToSquare);
    m_editEntryButton->setText(tr("编辑选中条目"));
    connect(m_editEntryButton, &ElaToolButton::clicked, this, [=]()
        {
            openEntryEditor(currentJsonRow());
        });
    filterLayout->addWidget(m_editEntryButton);

    m_deleteEntriesButton = new ElaToolButton(editorWidget);
    m_deleteEntriesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_deleteEntriesButton->setElaIcon(ElaIconType::Trash);
    m_deleteEntriesButton->setText(tr("删除选中条目"));
    connect(m_deleteEntriesButton, &ElaToolButton::clicked, this, [=]()
        {
            if (!ensureWritableAction(tr("删除缓存条目"))) {
                return;
            }
            syncSelectedEntryRows();
            if (m_selectedEntryRows.isEmpty()) {
                return;
            }
            if (!confirmAction(tr("确认删除"),
                tr("确定要删除选中的 %1 个缓存条目吗？").arg(QString::number(m_selectedEntryRows.size())))) {
                return;
            }
            deleteEntryRows(m_selectedEntryRows.values());
        });
    filterLayout->addWidget(m_deleteEntriesButton);
    editorLayout->addLayout(filterLayout);

    m_entryModel = new QStandardItemModel(this);
    m_entryList = new ElaListView(editorWidget);
    m_entryList->setModel(m_entryModel);
    m_entryList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_entryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_entryList->setItemHeight(112);
    m_entryList->setItemDelegate(createCacheEntryDelegate(m_entryList));
    connect(m_entryList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]()
        {
            syncSelectedEntryRows();
            updateCurrentSummary();
            updateActionStates();
        });
    connect(m_entryList, &ElaListView::doubleClicked, this, [=](const QModelIndex& index)
        {
            if (index.isValid()) {
                openEntryEditor(index.data(JsonRowRole).toInt());
            }
        });
    editorLayout->addWidget(m_entryList, 1);

    m_mainSplitter->addWidget(editorWidget);
    m_mainSplitter->setSizes({ 300, 1160 });

    mainLayout->addWidget(m_mainSplitter, 1);
    addCentralWidget(mainWidget, true, false, 0);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode)
        {
            refreshThemeStyles();
            renderEntries();
        });
    setSidebarPage(0);
    refreshThemeStyles();
    setReplacePanelVisible(false);
    updateActionStates();
}
