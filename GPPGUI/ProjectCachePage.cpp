#include "ProjectCachePage.h"

#include <QButtonGroup>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QUrl>

#include "ElaCheckBox.h"
#include "ElaComboBox.h"
#include "ElaIconButton.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "NoWheelComboBox.h"

#include "ProjectCachePage_p.h"

using namespace ProjectCachePagePrivate;

ProjectCachePage::ProjectCachePage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), _projectDir(projectDir), _projectConfig(projectConfig)
{
    setWindowTitle(tr("缓存管理"));
    setTitleVisible(false);
    _setupUI();
}

ProjectCachePage::~ProjectCachePage() = default;

void ProjectCachePage::ensureCacheFilesLoaded()
{
    if (_cacheFilesLoaded) {
        return;
    }
    _loadCacheFiles();
}

void ProjectCachePage::refreshCacheFiles()
{
    _loadCacheFiles(true);
}

void ProjectCachePage::_setupUI()
{
    // 缓存页是一个偏高密度的工具界面：左侧放文件/搜索/问题导航和批量操作，
    // 右侧留给当前缓存文件的句子概览，便于快速扫描和跳转编辑。
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(10, 10, 10, 0);
    mainLayout->setSpacing(8);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    _cacheDirLabel = new ElaText(QString::fromStdWString(_cacheDir().wstring()), BodyFontPx, mainWidget);
    _cacheDirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    _cacheDirLabel->setStyleSheet(auxiliaryTextStyle());
    topLayout->addWidget(_cacheDirLabel, 1);

    ElaIconButton* openCacheButton = createHeaderIconButton(ElaIconType::FolderOpen, tr("打开缓存文件夹"), mainWidget);
    connect(openCacheButton, &ElaIconButton::clicked, this, [=]()
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(_cacheDir().wstring())));
        });
    topLayout->addWidget(openCacheButton);

    ElaIconButton* refreshButton = createHeaderIconButton(ElaIconType::Rotate, tr("刷新"), mainWidget);
    connect(refreshButton, &ElaIconButton::clicked, this, [=]()
        {
            if (!_dirtyFiles.isEmpty()
                && !_confirmAction(tr("确认刷新"), tr("刷新会放弃所有未保存的缓存修改，确定要继续吗？"))) {
                return;
            }
            _loadCacheFiles(true);
        });
    topLayout->addWidget(refreshButton);

    _saveButton = createHeaderIconButton(ElaIconType::FloppyDisk, tr("保存当前文件"), mainWidget);
    connect(_saveButton, &ElaIconButton::clicked, this, [=]()
        {
            if (!_ensureWritableAction(tr("保存缓存"))) {
                return;
            }
            QString error;
            if (_writeCacheFile(_currentFile, _entries, &error)) {
                _loadedEntriesByFile[_currentFile] = _entries;
                _dirtyFiles.remove(_currentFile);
                _renderFileList();
                _currentFileLabel->setText(_currentFile);
                _setInfo(tr("已保存 ") + _currentFile);
            }
            else {
                _setError(error);
            }
            _updateActionStates();
        });
    topLayout->addWidget(_saveButton);

    _saveAllButton = createHeaderIconButton(ElaIconType::FloppyDisks, tr("保存全部"), mainWidget);
    connect(_saveAllButton, &ElaIconButton::clicked, this, [=]()
        {
            if (!_ensureWritableAction(tr("保存缓存"))) {
                return;
            }
            int saved = 0;
            QString lastError;
            const QStringList files = _dirtyFiles.values();
            for (const QString& filename : files) {
                const auto it = _loadedEntriesByFile.find(filename);
                if (it == _loadedEntriesByFile.end()) {
                    continue;
                }
                QString error;
                if (_writeCacheFile(filename, it.value(), &error)) {
                    _dirtyFiles.remove(filename);
                    ++saved;
                }
                else {
                    lastError = error;
                }
            }
            _renderFileList();
            if (!_currentFile.isEmpty()) {
                _currentFileLabel->setText((_dirtyFiles.contains(_currentFile) ? "*" : "") + _currentFile);
            }
            if (!lastError.isEmpty()) {
                _setError(lastError);
            }
            else {
                _setInfo(tr("已保存 ") + QString::number(saved) + tr(" 个缓存文件"));
            }
            _updateActionStates();
        });
    topLayout->addWidget(_saveAllButton);
    mainLayout->addLayout(topLayout);

    _mainSplitter = new QSplitter(Qt::Horizontal, mainWidget);
    _mainSplitter->setChildrenCollapsible(false);
    _mainSplitter->setHandleWidth(8);
    _mainSplitter->setStyleSheet(splitterStyle());

    QWidget* sidebarWidget = new QWidget(_mainSplitter);
    sidebarWidget->setMinimumWidth(260);
    sidebarWidget->setMaximumWidth(330);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(0, 0, 6, 0);
    sidebarLayout->setSpacing(5);

    QHBoxLayout* navLayout = new QHBoxLayout();
    navLayout->setSpacing(4);
    _sidebarButtonGroup = new QButtonGroup(this);
    _sidebarButtonGroup->setExclusive(true);
    auto addNavButton = [&](const QString& text, int index)
        {
            ElaPushButton* button = new ElaPushButton(text, sidebarWidget);
            button->setCheckable(true);
            button->setMinimumHeight(30);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            QFont font = button->font();
            font.setPixelSize(BodyFontPx);
            button->setFont(font);
            _sidebarButtonGroup->addButton(button, index);
            navLayout->addWidget(button);
            return button;
        };
    _filesNavButton = addNavButton(tr("文件"), 0);
    _searchNavButton = addNavButton(tr("搜索"), 1);
    _problemsNavButton = addNavButton(tr("问题"), 2);
    connect(_sidebarButtonGroup, &QButtonGroup::idClicked, this, [=](int index)
        {
            _setSidebarPage(index);
        });
    sidebarLayout->addLayout(navLayout);

    _sidebarStack = new QStackedWidget(sidebarWidget);

    QWidget* filesTab = new QWidget(_sidebarStack);
    QVBoxLayout* filesLayout = new QVBoxLayout(filesTab);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(4);

    _deleteFilesButton = new ElaPushButton(tr("删除选中文件"), filesTab);
    connect(_deleteFilesButton, &ElaPushButton::clicked, this, [=]()
        {
            if (!_ensureWritableAction(tr("删除缓存文件"))) {
                return;
            }
            const QModelIndexList selected = _fileList->selectionModel()->selectedRows();
            if (selected.empty()) {
                return;
            }
            if (!_confirmAction(tr("确认删除"),
                tr("确定要删除选中的 ") + QString::number(selected.size()) + tr(" 个缓存文件吗？"))) {
                return;
            }
            int deleted = 0;
            for (const QModelIndex& index : selected) {
                const QString filename = index.data(Qt::UserRole).toString();
                std::error_code ec;
                fs::remove(_cachePathForRelativeName(filename), ec);
                if (!ec) {
                    _loadedEntriesByFile.remove(filename);
                    _dirtyFiles.remove(filename);
                    if (_currentFile == filename) {
                        _currentFile.clear();
                        _entries = json::array();
                        _selectedEntryRows.clear();
                        _renderEntries();
                    }
                    ++deleted;
                }
            }
            _loadCacheFiles();
            _setInfo(tr("已删除 ") + QString::number(deleted) + tr(" 个缓存文件"));
        });
    filesLayout->addWidget(_deleteFilesButton);

    _fileModel = new QStandardItemModel(this);
    _fileList = new ElaListView(filesTab);
    _fileList->setModel(_fileModel);
    _fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _fileList->setItemHeight(42);
    connect(_fileList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            if (!index.isValid()) {
                return;
            }
            _loadCacheFile(index.data(Qt::UserRole).toString());
            _updateActionStates();
        });
    connect(_fileList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]()
        {
            _updateActionStates();
        });
    filesLayout->addWidget(_fileList, 1);
    _sidebarStack->addWidget(filesTab);

    QWidget* searchTab = new QWidget(_sidebarStack);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchTab);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(4);

    _globalSearchEdit = new ElaLineEdit(searchTab);
    _globalSearchEdit->setPlaceholderText(tr("搜索内容..."));
    _globalSearchEdit->setIsClearButtonEnable(true);
    connect(_globalSearchEdit, &ElaLineEdit::returnPressed, this, &ProjectCachePage::_runGlobalSearch);
    connect(_globalSearchEdit, &ElaLineEdit::textChanged, this, [=]()
        {
            _runGlobalSearch();
        });
    searchLayout->addWidget(_globalSearchEdit);

    _globalSearchField = new NoWheelComboBox(searchTab);
    _globalSearchField->addItem(tr("全部"), "all");
    _globalSearchField->addItem(tr("原文 pre_processed_text"), "src");
    _globalSearchField->addItem(tr("译文 pre_translated_text"), "dst");
    _globalSearchField->addItem(tr("问题 problems"), "problems");
    connect(_globalSearchField, &ElaComboBox::currentIndexChanged, this, [=](int)
        {
            _runGlobalSearch();
        });
    searchLayout->addWidget(_globalSearchField);

    // 批量替换默认收起，日常浏览时优先把空间留给搜索结果列表。
    _replaceToggleButton = new ElaPushButton(tr("展开批量替换"), searchTab);
    _replaceToggleButton->setCheckable(true);
    connect(_replaceToggleButton, &ElaPushButton::toggled, this, [=](bool checked)
        {
            _setReplacePanelVisible(checked);
        });
    searchLayout->addWidget(_replaceToggleButton);

    _replacePanel = new QWidget(searchTab);
    QVBoxLayout* replaceLayout = new QVBoxLayout(_replacePanel);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);

    _replaceQueryEdit = new ElaLineEdit(searchTab);
    _replaceQueryEdit->setPlaceholderText(tr("查找"));
    _replaceQueryEdit->setIsClearButtonEnable(true);
    replaceLayout->addWidget(_replaceQueryEdit);

    _replaceWithEdit = new ElaLineEdit(searchTab);
    _replaceWithEdit->setPlaceholderText(tr("替换为"));
    _replaceWithEdit->setIsClearButtonEnable(true);
    replaceLayout->addWidget(_replaceWithEdit);

    _replaceField = new ElaComboBox(searchTab);
    _replaceField->addItem(tr("译文 pre_translated_text"), "dst");
    _replaceField->addItem(tr("原文 pre_processed_text"), "src");
    _replaceField->addItem(tr("全部"), "all");
    replaceLayout->addWidget(_replaceField);

    QHBoxLayout* replaceButtonLayout = new QHBoxLayout();
    ElaPushButton* replacePreviewButton = new ElaPushButton(tr("预览"), searchTab);
    connect(replacePreviewButton, &ElaPushButton::clicked, this, &ProjectCachePage::_previewReplace);
    replaceButtonLayout->addWidget(replacePreviewButton);
    _replaceExecuteButton = new ElaPushButton(tr("替换"), searchTab);
    connect(_replaceExecuteButton, &ElaPushButton::clicked, this, &ProjectCachePage::_executeReplace);
    replaceButtonLayout->addWidget(_replaceExecuteButton);
    replaceLayout->addLayout(replaceButtonLayout);

    _replacePreviewLabel = new ElaText("", BodyFontPx, searchTab);
    _replacePreviewLabel->setWordWrap(true);
    _replacePreviewLabel->setStyleSheet(auxiliaryTextStyle());
    replaceLayout->addWidget(_replacePreviewLabel);
    searchLayout->addWidget(_replacePanel);

    _searchStatusLabel = new ElaText("", BodyFontPx, searchTab);
    _searchStatusLabel->setStyleSheet(auxiliaryTextStyle());
    searchLayout->addWidget(_searchStatusLabel);

    _searchModel = new QStandardItemModel(this);
    _searchResultList = new ElaListView(searchTab);
    _searchResultList->setModel(_searchModel);
    _searchResultList->setSelectionMode(QAbstractItemView::SingleSelection);
    _searchResultList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 高度需要和 CacheSearchDelegate 的四行卡片布局保持同步。
    _searchResultList->setItemHeight(120);
    _searchResultList->setItemDelegate(createCacheSearchDelegate(_searchResultList));
    _searchResultList->setMinimumHeight(520);
    connect(_searchResultList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            if (index.isValid()) {
                _jumpToHit(index.data(HitIndexRole).toInt());
            }
        });
    searchLayout->addWidget(_searchResultList, 1);
    _sidebarStack->addWidget(searchTab);

    QWidget* problemsTab = new QWidget(_sidebarStack);
    QVBoxLayout* problemsLayout = new QVBoxLayout(problemsTab);
    problemsLayout->setContentsMargins(0, 0, 0, 0);
    problemsLayout->setSpacing(4);

    ElaPushButton* refreshProblemsButton = new ElaPushButton(tr("刷新问题"), problemsTab);
    connect(refreshProblemsButton, &ElaPushButton::clicked, this, &ProjectCachePage::_loadProblems);
    problemsLayout->addWidget(refreshProblemsButton);

    _problemModel = new QStandardItemModel(this);
    _problemList = new ElaListView(problemsTab);
    _problemList->setModel(_problemModel);
    _problemList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _problemList->setItemHeight(32);
    connect(_problemList, &ElaListView::clicked, this, [=](const QModelIndex& index)
        {
            if (!index.isValid()) {
                return;
            }
            _setSidebarPage(1);
            _globalSearchField->setCurrentIndex(_globalSearchField->findData("problems"));
            _globalSearchEdit->setText(index.data(ProblemTextRole).toString());
            _runGlobalSearch();
        });
    problemsLayout->addWidget(_problemList, 1);
    _sidebarStack->addWidget(problemsTab);

    sidebarLayout->addWidget(_sidebarStack, 1);
    _mainSplitter->addWidget(sidebarWidget);

    QWidget* editorWidget = new QWidget(_mainSplitter);
    QVBoxLayout* editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setContentsMargins(8, 0, 0, 0);
    editorLayout->setSpacing(8);

    QHBoxLayout* currentLayout = new QHBoxLayout();
    _currentFileLabel = new ElaText(tr("未选择缓存文件"), TitleFontPx, editorWidget);
    _currentFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont currentFont = _currentFileLabel->font();
    currentFont.setBold(true);
    _currentFileLabel->setFont(currentFont);
    currentLayout->addWidget(_currentFileLabel, 1);
    _currentSummaryLabel = new ElaText("", BodyFontPx, editorWidget);
    _currentSummaryLabel->setStyleSheet(auxiliaryTextStyle());
    _currentSummaryLabel->setWordWrap(false);
    _currentSummaryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _currentSummaryLabel->setMinimumWidth(360);
    currentLayout->addWidget(_currentSummaryLabel);
    editorLayout->addLayout(currentLayout);

    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);
    _localSearchEdit = new ElaLineEdit(editorWidget);
    _localSearchEdit->setPlaceholderText(tr("在当前文件中搜索 pre_processed_text / pre_translated_text / problems..."));
    _localSearchEdit->setIsClearButtonEnable(true);
    connect(_localSearchEdit, &ElaLineEdit::textChanged, this, [=]()
        {
            _renderEntries();
        });
    filterLayout->addWidget(_localSearchEdit, 1);
    _filterProblemsCheck = new ElaCheckBox(tr("只看问题句"), editorWidget);
    connect(_filterProblemsCheck, &ElaCheckBox::toggled, this, [=]()
        {
            _renderEntries();
        });
    filterLayout->addWidget(_filterProblemsCheck);

    _editEntryButton = new ElaPushButton(tr("编辑选中条目"), editorWidget);
    connect(_editEntryButton, &ElaPushButton::clicked, this, [=]()
        {
            _openEntryEditor(_currentJsonRow());
        });
    filterLayout->addWidget(_editEntryButton);

    _deleteEntriesButton = new ElaPushButton(tr("删除选中条目"), editorWidget);
    connect(_deleteEntriesButton, &ElaPushButton::clicked, this, [=]()
        {
            if (!_ensureWritableAction(tr("删除缓存条目"))) {
                return;
            }
            _syncSelectedEntryRows();
            if (_selectedEntryRows.isEmpty()) {
                return;
            }
            if (!_confirmAction(tr("确认删除"),
                tr("确定要删除选中的 ") + QString::number(_selectedEntryRows.size()) + tr(" 个缓存条目吗？"))) {
                return;
            }
            _deleteEntryRows(_selectedEntryRows.values());
        });
    filterLayout->addWidget(_deleteEntriesButton);
    editorLayout->addLayout(filterLayout);

    _entryModel = new QStandardItemModel(this);
    _entryList = new ElaListView(editorWidget);
    _entryList->setModel(_entryModel);
    _entryList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _entryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _entryList->setItemHeight(112);
    _entryList->setItemDelegate(createCacheEntryDelegate(_entryList));
    connect(_entryList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=]()
        {
            _syncSelectedEntryRows();
            _updateCurrentSummary();
            _updateActionStates();
        });
    connect(_entryList, &ElaListView::doubleClicked, this, [=](const QModelIndex& index)
        {
            if (index.isValid()) {
                _openEntryEditor(index.data(JsonRowRole).toInt());
            }
        });
    editorLayout->addWidget(_entryList, 1);

    _mainSplitter->addWidget(editorWidget);
    _mainSplitter->setSizes({ 300, 1160 });

    mainLayout->addWidget(_mainSplitter, 1);
    addCentralWidget(mainWidget, true, false, 0);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode)
        {
            _refreshThemeStyles();
            _renderEntries();
        });
    _setSidebarPage(0);
    _refreshThemeStyles();
    _setReplacePanelVisible(false);
    _updateActionStates();
}
