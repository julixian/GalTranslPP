#include "ProjectCachePage.h"

#include <algorithm>
#include <fstream>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCollator>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>

#include "ElaCheckBox.h"
#include "ElaComboBox.h"
#include "ElaDialog.h"
#include "ElaIconButton.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolTip.h"

import Tool;

namespace {
    constexpr int JsonRowRole = Qt::UserRole + 1;
    constexpr int HitIndexRole = Qt::UserRole + 2;
    constexpr int ProblemTextRole = Qt::UserRole + 3;
    constexpr int EntryIndexRole = Qt::UserRole + 4;
    constexpr int EntrySpeakerRole = Qt::UserRole + 5;
    constexpr int EntryProblemRole = Qt::UserRole + 6;
    constexpr int EntryEngineRole = Qt::UserRole + 7;
    constexpr int EntrySourceRole = Qt::UserRole + 8;
    constexpr int EntryDstRole = Qt::UserRole + 9;

    constexpr int LabelFontPx = 12;
    constexpr int BodyFontPx = 13;
    constexpr int TitleFontPx = 15;

    int sentenceIndexOf(const nlohmann::json& object, int fallback)
    {
        if (object.is_object() && object.contains("index") && object["index"].is_number_integer()) {
            return object["index"].get<int>();
        }
        return fallback;
    }

    QString compactPreview(QString text, int maxChars = 160)
    {
        text.replace("\r", "\\r");
        text.replace("\n", "\\n");
        if (text.size() <= maxChars) {
            return text;
        }
        return text.left(maxChars - 3) + "...";
    }

    QColor themeColor(ElaThemeType::ThemeColor color)
    {
        return eTheme->getThemeColor(eTheme->getThemeMode(), color);
    }

    QString colorName(ElaThemeType::ThemeColor color)
    {
        return themeColor(color).name(QColor::HexArgb);
    }

    QString auxiliaryTextStyle()
    {
        return QString("color:%1;").arg(colorName(ElaThemeType::BasicDetailsText));
    }

    void tuneTextEdit(ElaPlainTextEdit* edit, bool readOnly, int height)
    {
        edit->setMinimumHeight(height);
        edit->setMaximumHeight(height);
        edit->setReadOnly(readOnly);
        edit->setTabChangesFocus(true);
        edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QFont font = edit->font();
        font.setPixelSize(BodyFontPx);
        edit->setFont(font);
    }

    void tuneNavButton(ElaPushButton* button, bool active)
    {
        if (!button) {
            return;
        }
        button->setLightDefaultColor(active ? QColor(226, 241, 255) : QColor(248, 248, 248));
        button->setLightHoverColor(active ? QColor(214, 234, 255) : QColor(241, 241, 241));
        button->setLightPressColor(QColor(205, 229, 255));
        button->setDarkDefaultColor(active ? QColor(35, 62, 92) : QColor(42, 42, 42));
        button->setDarkHoverColor(active ? QColor(42, 72, 106) : QColor(52, 52, 52));
        button->setDarkPressColor(QColor(49, 82, 118));
        button->setLightTextColor(active ? QColor(15, 103, 173) : QColor(35, 35, 35));
        button->setDarkTextColor(active ? QColor(212, 232, 255) : QColor(235, 235, 235));
        button->update();
    }

    void setModelItemFont(QStandardItem* item, int px = BodyFontPx)
    {
        QFont font = item->font();
        font.setPixelSize(px);
        item->setFont(font);
    }

    ElaIconButton* createHeaderIconButton(ElaIconType::IconName icon, const QString& toolTip, QWidget* parent)
    {
        ElaIconButton* button = new ElaIconButton(icon, 22, 42, 38, parent);
        button->setBorderRadius(6);
        ElaToolTip* tip = new ElaToolTip(button);
        tip->setToolTip(toolTip);
        return button;
    }

    class CacheEntryDelegate : public QStyledItemDelegate
    {
    public:
        explicit CacheEntryDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            painter->save();
            painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

            const bool selected = option.state.testFlag(QStyle::State_Selected);
            const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
            const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
            const QString problem = index.data(EntryProblemRole).toString();

            QRectF cardRect = option.rect.adjusted(6, 4, -6, -5);
            QColor cardColor = themeColor(ElaThemeType::BasicBaseAlpha);
            if (selected) {
                cardColor = themeColor(ElaThemeType::BasicSelectedAlpha);
            }
            else if (hovered) {
                cardColor = themeColor(ElaThemeType::BasicHoverAlpha);
            }

            QColor borderColor = problem.isEmpty()
                ? themeColor(ElaThemeType::BasicBorder)
                : QColor(dark ? 143 : 207, dark ? 111 : 166, dark ? 45 : 65);
            painter->setPen(QPen(borderColor, problem.isEmpty() ? 1.0 : 1.3));
            painter->setBrush(cardColor);
            painter->drawRoundedRect(cardRect, 7, 7);

            if (selected) {
                QRectF indicator(cardRect.left() + 7, cardRect.top() + 14, 3, cardRect.height() - 28);
                painter->setPen(Qt::NoPen);
                painter->setBrush(themeColor(ElaThemeType::PrimaryNormal));
                painter->drawRoundedRect(indicator, 2, 2);
            }

            QRect contentRect = cardRect.toRect().adjusted(18, 8, -14, -8);
            int x = contentRect.left();
            int y = contentRect.top();

            auto drawPill = [&](QString text, const QColor& fill, const QColor& penColor, bool bold)
                {
                    if (text.trimmed().isEmpty() || x >= contentRect.right() - 24) {
                        return;
                    }
                    QFont font = option.font;
                    font.setPixelSize(12);
                    font.setBold(bold);
                    painter->setFont(font);
                    QFontMetrics fm(font);

                    const int maxWidth = contentRect.right() - x;
                    text = fm.elidedText(text, Qt::ElideRight, qMax(36, maxWidth - 12));
                    const int width = qMin(maxWidth, fm.horizontalAdvance(text) + 18);
                    QRectF pillRect(x, y, width, 23);
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(fill);
                    painter->drawRoundedRect(pillRect, 11, 11);
                    painter->setPen(penColor);
                    painter->drawText(pillRect.adjusted(9, 0, -9, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
                    x += width + 8;
                };

            const QColor textColor = themeColor(ElaThemeType::BasicText);
            const QColor detailColor = themeColor(ElaThemeType::BasicDetailsText);
            QColor subtleFill = themeColor(ElaThemeType::BasicHoverAlpha);
            QColor problemFill = dark ? QColor(86, 66, 28) : QColor(247, 232, 181);
            QColor problemText = dark ? QColor(248, 219, 139) : QColor(105, 75, 15);
            QColor engineFill = themeColor(ElaThemeType::PrimaryNormal);
            engineFill.setAlpha(dark ? 65 : 32);

            drawPill(QString("#%1").arg(index.data(EntryIndexRole).toInt()), subtleFill, detailColor, true);
            drawPill(index.data(EntrySpeakerRole).toString(), subtleFill, textColor, true);
            drawPill(problem, problemFill, problemText, true);
            drawPill(index.data(EntryEngineRole).toString(), engineFill, themeColor(ElaThemeType::PrimaryNormal), true);

            auto drawLine = [&](int lineY, const QString& label, const QString& text)
                {
                    QFont labelFont = option.font;
                    labelFont.setPixelSize(12);
                    painter->setFont(labelFont);
                    painter->setPen(detailColor);
                    QRect labelRect(contentRect.left(), lineY, 42, 22);
                    painter->drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, label);

                    QFont bodyFont = option.font;
                    bodyFont.setPixelSize(BodyFontPx);
                    painter->setFont(bodyFont);
                    painter->setPen(textColor);
                    QFontMetrics fm(bodyFont);
                    QRect textRect(contentRect.left() + 46, lineY, contentRect.width() - 46, 22);
                    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                        fm.elidedText(text, Qt::ElideRight, textRect.width()));
                };

            drawLine(contentRect.top() + 29, QObject::tr("原文"), index.data(EntrySourceRole).toString());
            drawLine(contentRect.top() + 53, QObject::tr("译文"), index.data(EntryDstRole).toString());

            painter->restore();
        }
    };
}

ProjectCachePage::ProjectCachePage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), _projectDir(projectDir), _projectConfig(projectConfig)
{
    setWindowTitle(tr("缓存管理"));
    setTitleVisible(false);
    _setupUI();
    _loadCacheFiles();
}

ProjectCachePage::~ProjectCachePage() = default;

void ProjectCachePage::refreshCacheFiles()
{
    _loadCacheFiles();
}

void ProjectCachePage::_setupUI()
{
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(14, 12, 14, 0);
    mainLayout->setSpacing(8);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    _cacheDirLabel = new ElaText(QString(_cacheDir().wstring()), BodyFontPx, mainWidget);
    _cacheDirLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    _cacheDirLabel->setStyleSheet(auxiliaryTextStyle());
    topLayout->addWidget(_cacheDirLabel, 1);

    ElaIconButton* openCacheButton = createHeaderIconButton(ElaIconType::FolderOpen, tr("打开缓存文件夹"), mainWidget);
    connect(openCacheButton, &ElaIconButton::clicked, this, [=]()
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QString(_cacheDir().wstring())));
        });
    topLayout->addWidget(openCacheButton);

    ElaIconButton* refreshButton = createHeaderIconButton(ElaIconType::Rotate, tr("刷新"), mainWidget);
    connect(refreshButton, &ElaIconButton::clicked, this, &ProjectCachePage::_loadCacheFiles);
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

    _messageLabel = new ElaText("", BodyFontPx, mainWidget);
    _messageLabel->setVisible(false);
    _messageLabel->setWordWrap(true);
    _messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(_messageLabel);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, mainWidget);
    mainSplitter->setChildrenCollapsible(false);

    QWidget* sidebarWidget = new QWidget(mainSplitter);
    sidebarWidget->setMinimumWidth(280);
    sidebarWidget->setMaximumWidth(360);
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
            if (QMessageBox::question(this, tr("确认删除"),
                tr("确定要删除选中的 ") + QString::number(selected.size()) + tr(" 个缓存文件吗？")) != QMessageBox::Yes) {
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
                        _entries = nlohmann::json::array();
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

    _globalSearchField = new ElaComboBox(searchTab);
    _globalSearchField->addItem(tr("全部"), "all");
    _globalSearchField->addItem(tr("原文 pre_processed_text"), "src");
    _globalSearchField->addItem(tr("译文 pre_translated_text"), "dst");
    _globalSearchField->addItem(tr("问题 problems"), "problems");
    connect(_globalSearchField, &ElaComboBox::currentIndexChanged, this, [=](int)
        {
            _runGlobalSearch();
        });
    searchLayout->addWidget(_globalSearchField);

    ElaText* replaceTitle = new ElaText(tr("批量替换"), LabelFontPx, searchTab);
    searchLayout->addWidget(replaceTitle);

    _replaceQueryEdit = new ElaLineEdit(searchTab);
    _replaceQueryEdit->setPlaceholderText(tr("查找"));
    _replaceQueryEdit->setIsClearButtonEnable(true);
    searchLayout->addWidget(_replaceQueryEdit);

    _replaceWithEdit = new ElaLineEdit(searchTab);
    _replaceWithEdit->setPlaceholderText(tr("替换为"));
    _replaceWithEdit->setIsClearButtonEnable(true);
    searchLayout->addWidget(_replaceWithEdit);

    _replaceField = new ElaComboBox(searchTab);
    _replaceField->addItem(tr("译文 pre_translated_text"), "dst");
    _replaceField->addItem(tr("原文 pre_processed_text"), "src");
    _replaceField->addItem(tr("全部"), "all");
    searchLayout->addWidget(_replaceField);

    QHBoxLayout* replaceButtonLayout = new QHBoxLayout();
    ElaPushButton* replacePreviewButton = new ElaPushButton(tr("预览"), searchTab);
    connect(replacePreviewButton, &ElaPushButton::clicked, this, &ProjectCachePage::_previewReplace);
    replaceButtonLayout->addWidget(replacePreviewButton);
    _replaceExecuteButton = new ElaPushButton(tr("替换"), searchTab);
    connect(_replaceExecuteButton, &ElaPushButton::clicked, this, &ProjectCachePage::_executeReplace);
    replaceButtonLayout->addWidget(_replaceExecuteButton);
    searchLayout->addLayout(replaceButtonLayout);

    _replacePreviewLabel = new ElaText("", BodyFontPx, searchTab);
    _replacePreviewLabel->setWordWrap(true);
    _replacePreviewLabel->setStyleSheet(auxiliaryTextStyle());
    searchLayout->addWidget(_replacePreviewLabel);

    _searchStatusLabel = new ElaText("", BodyFontPx, searchTab);
    _searchStatusLabel->setStyleSheet(auxiliaryTextStyle());
    searchLayout->addWidget(_searchStatusLabel);

    _searchModel = new QStandardItemModel(this);
    _searchResultList = new ElaListView(searchTab);
    _searchResultList->setModel(_searchModel);
    _searchResultList->setSelectionMode(QAbstractItemView::SingleSelection);
    _searchResultList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _searchResultList->setItemHeight(72);
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
    mainSplitter->addWidget(sidebarWidget);

    QWidget* editorWidget = new QWidget(mainSplitter);
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
            if (QMessageBox::question(this, tr("确认删除"),
                tr("确定要删除选中的 ") + QString::number(_selectedEntryRows.size()) + tr(" 个缓存条目吗？")) != QMessageBox::Yes) {
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
    _entryList->setItemDelegate(new CacheEntryDelegate(_entryList));
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

    mainSplitter->addWidget(editorWidget);
    mainSplitter->setSizes({ 320, 1140 });

    mainLayout->addWidget(mainSplitter, 1);
    addCentralWidget(mainWidget, true, false, 0);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode)
        {
            _refreshThemeStyles();
            _renderEntries();
    });
    _setSidebarPage(0);
    _refreshThemeStyles();
    _updateActionStates();
}

void ProjectCachePage::_loadCacheFiles()
{
    _cacheFiles.clear();
    const fs::path cacheDir = _cacheDir();
    if (!fs::exists(cacheDir)) {
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
    }

    if (fs::exists(cacheDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(cacheDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != L".json") {
                continue;
            }
            CacheFileInfo info;
            info.relativeName = QString(fs::relative(entry.path(), cacheDir).generic_wstring());
            std::error_code ec;
            info.size = entry.file_size(ec);
            QFileInfo fileInfo(QString(entry.path().wstring()));
            info.modified = fileInfo.lastModified();

            nlohmann::json data;
            QString error;
            if (_readCacheFile(info.relativeName, data, &error) && data.is_array()) {
                info.entries = (int)data.size();
                for (const auto& item : data) {
                    if (item.is_object() && !_problemString(item).isEmpty()) {
                        ++info.problems;
                    }
                }
            }
            else {
                info.parseOk = false;
                info.error = error;
            }
            _cacheFiles.push_back(info);
        }
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(_cacheFiles.begin(), _cacheFiles.end(), [&collator](const CacheFileInfo& a, const CacheFileInfo& b)
        {
            return collator.compare(a.relativeName, b.relativeName) < 0;
        });

    _cacheDirLabel->setText(QString(cacheDir.wstring()));
    _renderFileList();
    _loadProblems();
    _runGlobalSearch();
    _updateActionStates();
}

void ProjectCachePage::_renderFileList()
{
    _fileModel->clear();
    for (const CacheFileInfo& file : _cacheFiles) {
        QString title = file.relativeName;
        if (_dirtyFiles.contains(file.relativeName)) {
            title = "*" + title;
        }
        QString subtitle = tr("%1 句 · %2 问题").arg(file.entries).arg(file.problems);
        if (!file.parseOk) {
            subtitle = tr("解析失败");
        }
        QStandardItem* item = new QStandardItem(title + "\n" + subtitle);
        item->setData(file.relativeName, Qt::UserRole);
        item->setToolTip(file.parseOk ? file.relativeName : file.error);
        item->setEditable(false);
        setModelItemFont(item, BodyFontPx);
        _fileModel->appendRow(item);
        if (_currentFile == file.relativeName) {
            const QModelIndex index = _fileModel->indexFromItem(item);
            _fileList->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            _fileList->setCurrentIndex(index);
        }
    }
    if (_filesNavButton) {
        _filesNavButton->setText(tr("文件 (%1)").arg(_cacheFiles.size()));
    }
}

void ProjectCachePage::_loadCacheFile(const QString& filename, bool forceReload)
{
    if (filename.isEmpty()) {
        return;
    }
    const bool fileChanged = _currentFile != filename;
    if (!forceReload && _loadedEntriesByFile.contains(filename)) {
        _entries = _loadedEntriesByFile.value(filename);
    }
    else {
        QString error;
        if (!_readCacheFile(filename, _entries, &error)) {
            _setError(error);
            return;
        }
        if (!_entries.is_array()) {
            _setError(tr("缓存文件不是 JSON 数组: ") + filename);
            return;
        }
        _loadedEntriesByFile[filename] = _entries;
    }
    _currentFile = filename;
    if (fileChanged) {
        _selectedEntryRows.clear();
    }
    _currentFileLabel->setText((_dirtyFiles.contains(filename) ? "*" : "") + filename);
    _renderEntries();
    _updateCurrentSummary();
    _updateActionStates();
}

void ProjectCachePage::_renderEntries()
{
    if (!_entryModel) {
        return;
    }
    _renderingEntries = true;
    _entryModel->clear();
    _selectedEntryRows.clear();

    if (_currentFile.isEmpty()) {
        _currentFileLabel->setText(tr("未选择缓存文件"));
        _renderingEntries = false;
        _updateCurrentSummary();
        _updateActionStates();
        return;
    }

    const QString query = _localSearchEdit ? _localSearchEdit->text().trimmed() : QString();
    const bool onlyProblems = _filterProblemsCheck && _filterProblemsCheck->isChecked();

    for (int i = 0; i < (int)_entries.size(); ++i) {
        const auto& item = _entries[i];
        if (!item.is_object()) {
            continue;
        }
        const QString source = _entrySource(item);
        const QString dst = _entryDst(item);
        const QString problems = _problemString(item, " | ");
        if (onlyProblems && problems.isEmpty()) {
            continue;
        }
        if (!query.isEmpty()
            && !source.contains(query, Qt::CaseInsensitive)
            && !dst.contains(query, Qt::CaseInsensitive)
            && !problems.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        QStandardItem* itemRow = new QStandardItem(_entryListText(item, i));
        itemRow->setData(i, JsonRowRole);
        itemRow->setData(sentenceIndexOf(item, i), EntryIndexRole);
        itemRow->setData(_speakerString(item), EntrySpeakerRole);
        itemRow->setData(problems, EntryProblemRole);
        itemRow->setData(_entryTranslatedBy(item), EntryEngineRole);
        itemRow->setData(source, EntrySourceRole);
        itemRow->setData(dst, EntryDstRole);
        itemRow->setEditable(false);
        setModelItemFont(itemRow, BodyFontPx);
        _entryModel->appendRow(itemRow);
    }

    _renderingEntries = false;
    _currentFileLabel->setText(_currentFile.isEmpty() ? tr("未选择缓存文件") : ((_dirtyFiles.contains(_currentFile) ? "*" : "") + _currentFile));
    _updateCurrentSummary();
    _updateActionStates();
}

void ProjectCachePage::_syncSelectedEntryRows()
{
    _selectedEntryRows.clear();
    if (!_entryList || !_entryList->selectionModel()) {
        return;
    }
    const QModelIndexList selected = _entryList->selectionModel()->selectedRows();
    for (const QModelIndex& index : selected) {
        const int row = index.data(JsonRowRole).toInt();
        if (row >= 0) {
            _selectedEntryRows.insert(row);
        }
    }
}

void ProjectCachePage::_updateEntryListItem(int row)
{
    if (!_entryModel) {
        return;
    }
    for (int modelRow = 0; modelRow < _entryModel->rowCount(); ++modelRow) {
        QStandardItem* item = _entryModel->item(modelRow);
        if (item && item->data(JsonRowRole).toInt() == row) {
            item->setText(_entryListText(_entries[row], row));
            item->setData(sentenceIndexOf(_entries[row], row), EntryIndexRole);
            item->setData(_speakerString(_entries[row]), EntrySpeakerRole);
            item->setData(_problemString(_entries[row], " | "), EntryProblemRole);
            item->setData(_entryTranslatedBy(_entries[row]), EntryEngineRole);
            item->setData(_entrySource(_entries[row]), EntrySourceRole);
            item->setData(_entryDst(_entries[row]), EntryDstRole);
            return;
        }
    }
}

void ProjectCachePage::_updateEntryField(int row, const char* key, const QString& value)
{
    if (_isProjectRunning() || _currentFile.isEmpty()) {
        return;
    }
    if (row < 0 || row >= (int)_entries.size() || !_entries[row].is_object()) {
        return;
    }
    if (_jsonString(_entries[row], key) == value) {
        return;
    }
    _entries[row][key] = value.toStdString();
    _markDirty(_currentFile);
    _updateEntryListItem(row);
    _updateCurrentSummary();
}

void ProjectCachePage::_openEntryEditor(int row)
{
    if (_currentFile.isEmpty() || row < 0 || row >= (int)_entries.size() || !_entries[row].is_object()) {
        return;
    }

    const auto item = _entries[row];
    const bool writable = !_isProjectRunning();

    ElaDialog dialog(this);
    const QString speaker = _speakerString(item);
    dialog.setWindowTitle(speaker.isEmpty()
        ? QString("#%1").arg(sentenceIndexOf(item, row))
        : QString("#%1  %2").arg(sentenceIndexOf(item, row)).arg(speaker));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    dialog.resize(860, 430);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 34, 16, 14);
    mainLayout->setSpacing(5);

    auto addEditor = [&](const QString& labelText, const QString& value, bool readOnly, int height)
        {
            ElaText* label = new ElaText(labelText, LabelFontPx, &dialog);
            label->setStyleSheet(auxiliaryTextStyle());
            mainLayout->addWidget(label);

            ElaPlainTextEdit* edit = new ElaPlainTextEdit(&dialog);
            tuneTextEdit(edit, readOnly, height);
            edit->setPlainText(value);
            mainLayout->addWidget(edit);
            return edit;
        };

    ElaPlainTextEdit* originalEdit = addEditor(tr("original_text（元信息，只读）"), _entryOriginal(item), true, 48);
    Q_UNUSED(originalEdit);
    ElaPlainTextEdit* sourceEdit = addEditor(tr("pre_processed_text（原文，可编辑）"), _entrySource(item), !writable, 58);
    ElaPlainTextEdit* dstEdit = addEditor(tr("pre_translated_text（译文，可编辑）"), _entryDst(item), !writable, 58);
    ElaPlainTextEdit* problemsEdit = addEditor(tr("problems（只读）"), _problemString(item), true, 46);
    Q_UNUSED(problemsEdit);
    ElaPlainTextEdit* previewEdit = addEditor(tr("translated_preview（只读）"), _entryPreview(item), true, 46);
    Q_UNUSED(previewEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    ElaPushButton* closeButton = new ElaPushButton(tr("关闭"), &dialog);
    connect(closeButton, &ElaPushButton::clicked, &dialog, &ElaDialog::reject);
    buttonLayout->addWidget(closeButton);

    ElaPushButton* saveButton = new ElaPushButton(tr("保存修改"), &dialog);
    saveButton->setEnabled(writable);
    connect(saveButton, &ElaPushButton::clicked, this, [=, &dialog]()
        {
            _updateEntryField(row, "pre_processed_text", sourceEdit->toPlainText());
            _updateEntryField(row, "pre_translated_text", dstEdit->toPlainText());
            dialog.accept();
        });
    buttonLayout->addWidget(saveButton);
    mainLayout->addLayout(buttonLayout);

    dialog.moveToCenter();
    dialog.exec();
}

void ProjectCachePage::_deleteEntryRows(QList<int> rows)
{
    if (_currentFile.isEmpty() || rows.isEmpty()) {
        return;
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    int deleted = 0;
    for (int row : rows) {
        if (row >= 0 && row < (int)_entries.size()) {
            _entries.erase(_entries.begin() + row);
            ++deleted;
        }
    }
    if (deleted <= 0) {
        return;
    }
    _selectedEntryRows.clear();
    _markDirty(_currentFile);
    _renderEntries();
    _runGlobalSearch();
    _loadProblems();
    _setInfo(tr("已删除 ") + QString::number(deleted) + tr(" 个条目，保存后生效"));
}

void ProjectCachePage::_markDirty(const QString& filename)
{
    if (filename.isEmpty()) {
        return;
    }
    const bool wasClean = !_dirtyFiles.contains(filename);
    _dirtyFiles.insert(filename);
    _loadedEntriesByFile[filename] = _entries;
    _currentFileLabel->setText("*" + filename);
    if (wasClean) {
        _renderFileList();
    }
    _updateActionStates();
}

void ProjectCachePage::_setInfo(const QString& message)
{
    _messageLabel->setText(message);
    _messageLabel->setStyleSheet("color:#127a36;");
    _messageLabel->setVisible(!message.isEmpty());
    if (!message.isEmpty()) {
        ElaMessageBar::success(ElaMessageBarType::TopRight, tr("完成"), message, 2500);
    }
}

void ProjectCachePage::_setError(const QString& message)
{
    _messageLabel->setText(message);
    _messageLabel->setStyleSheet("color:#b00020;");
    _messageLabel->setVisible(!message.isEmpty());
    if (!message.isEmpty()) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("失败"), message, 4000);
    }
}

void ProjectCachePage::_updateCurrentSummary()
{
    if (_currentFile.isEmpty()) {
        _currentSummaryLabel->clear();
        return;
    }
    int translated = 0;
    int problemCount = 0;
    for (const auto& item : _entries) {
        if (!item.is_object()) {
            continue;
        }
        if (!_entryDst(item).isEmpty()) {
            ++translated;
        }
        if (!_problemString(item).isEmpty()) {
            ++problemCount;
        }
    }
    _currentSummaryLabel->setText(tr("%1 句 · %2 已翻译 · %3 有问题 · %4 已选择")
        .arg((int)_entries.size()).arg(translated).arg(problemCount).arg(_selectedEntryRows.size()));
}

void ProjectCachePage::_updateActionStates()
{
    const bool hasFile = !_currentFile.isEmpty();
    const bool writable = !_isProjectRunning();
    if (_saveButton) {
        _saveButton->setEnabled(writable && hasFile && _dirtyFiles.contains(_currentFile));
    }
    if (_saveAllButton) {
        _saveAllButton->setEnabled(writable && !_dirtyFiles.isEmpty());
    }
    if (_deleteEntriesButton) {
        const int selectedCount = _selectedEntryRows.size();
        _deleteEntriesButton->setText(selectedCount > 0 ? tr("删除选中条目 (%1)").arg(selectedCount) : tr("删除选中条目"));
        _deleteEntriesButton->setEnabled(writable && hasFile && selectedCount > 0);
    }
    if (_editEntryButton) {
        _editEntryButton->setEnabled(hasFile && _currentJsonRow() >= 0);
    }
    if (_deleteFilesButton) {
        _deleteFilesButton->setEnabled(writable && _fileList && _fileList->selectionModel() && !_fileList->selectionModel()->selectedRows().empty());
    }
    if (_replaceExecuteButton) {
        _replaceExecuteButton->setEnabled(writable);
    }
}

void ProjectCachePage::_setSidebarPage(int index)
{
    if (!_sidebarStack || !_sidebarButtonGroup) {
        return;
    }
    _sidebarStack->setCurrentIndex(index);
    if (QAbstractButton* button = _sidebarButtonGroup->button(index)) {
        button->setChecked(true);
    }
    tuneNavButton(_filesNavButton, index == 0);
    tuneNavButton(_searchNavButton, index == 1);
    tuneNavButton(_problemsNavButton, index == 2);
    if (index == 2) {
        _loadProblems();
    }
}

void ProjectCachePage::_refreshThemeStyles()
{
    if (_cacheDirLabel) {
        _cacheDirLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (_replacePreviewLabel) {
        _replacePreviewLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (_searchStatusLabel) {
        _searchStatusLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (_currentSummaryLabel) {
        _currentSummaryLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (_sidebarStack && _sidebarButtonGroup) {
        _setSidebarPage(_sidebarStack->currentIndex());
    }
}

bool ProjectCachePage::_isProjectRunning() const
{
    return toml::find_or(_projectConfig, "GUIConfig", "isRunning", false);
}

bool ProjectCachePage::_ensureWritableAction(const QString& actionName) const
{
    if (_isProjectRunning()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, actionName, tr("项目正在运行中，只允许查看缓存。"), 3000);
        return false;
    }
    return true;
}

fs::path ProjectCachePage::_cacheDir() const
{
    return _projectDir / transCacheDirName;
}

fs::path ProjectCachePage::_cachePathForRelativeName(const QString& filename) const
{
    return _cacheDir() / fs::path(filename.toStdWString());
}

bool ProjectCachePage::_readCacheFile(const QString& filename, nlohmann::json& entries, QString* errorMessage) const
{
    try {
        std::ifstream ifs(_cachePathForRelativeName(filename), std::ios::binary);
        if (!ifs.is_open()) {
            if (errorMessage) {
                *errorMessage = tr("无法打开缓存文件: ") + filename;
            }
            return false;
        }
        entries = nlohmann::json::parse(ifs);
        return true;
    }
    catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = tr("解析缓存失败: ") + filename + "\n" + QString::fromStdString(e.what());
        }
        return false;
    }
}

bool ProjectCachePage::_writeCacheFile(const QString& filename, const nlohmann::json& entries, QString* errorMessage) const
{
    try {
        const fs::path path = _cachePathForRelativeName(filename);
        createParent(path);
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (errorMessage) {
                *errorMessage = tr("无法写入缓存文件: ") + filename;
            }
            return false;
        }
        ofs << entries.dump(2);
        return true;
    }
    catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = tr("写入缓存失败: ") + filename + "\n" + QString::fromStdString(e.what());
        }
        return false;
    }
}

QString ProjectCachePage::_jsonString(const nlohmann::json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key)) {
        return {};
    }
    const auto& value = object[key];
    if (value.is_string()) {
        return QString::fromStdString(value.get<std::string>());
    }
    if (value.is_null()) {
        return {};
    }
    return QString::fromStdString(value.dump());
}

QString ProjectCachePage::_speakerString(const nlohmann::json& object)
{
    if (object.contains("name_preview")) {
        const QString preview = _jsonString(object, "name_preview");
        if (!preview.isEmpty()) {
            return preview;
        }
    }
    if (object.contains("name")) {
        return _jsonString(object, "name");
    }
    if (object.contains("names") && object["names"].is_array()) {
        QStringList names;
        for (const auto& name : object["names"]) {
            if (name.is_string()) {
                names.push_back(QString::fromStdString(name.get<std::string>()));
            }
        }
        return names.join("/");
    }
    return {};
}

QString ProjectCachePage::_problemString(const nlohmann::json& object, const QString& separator)
{
    if (!object.contains("problems") || !object["problems"].is_array()) {
        return {};
    }
    QStringList problems;
    for (const auto& problem : object["problems"]) {
        if (problem.is_string()) {
            const QString text = QString::fromStdString(problem.get<std::string>()).trimmed();
            if (!text.isEmpty() && !problems.contains(text)) {
                problems.push_back(text);
            }
        }
    }
    return problems.join(separator);
}

QString ProjectCachePage::_entrySource(const nlohmann::json& object)
{
    return _jsonString(object, "pre_processed_text");
}

QString ProjectCachePage::_entryDst(const nlohmann::json& object)
{
    return _jsonString(object, "pre_translated_text");
}

QString ProjectCachePage::_entryOriginal(const nlohmann::json& object)
{
    return _jsonString(object, "original_text");
}

QString ProjectCachePage::_entryPreview(const nlohmann::json& object)
{
    return _jsonString(object, "translated_preview");
}

QString ProjectCachePage::_entryTranslatedBy(const nlohmann::json& object)
{
    return _jsonString(object, "translated_by");
}

QString ProjectCachePage::_truncateForList(const QString& text, int maxChars)
{
    return compactPreview(text, maxChars);
}

QString ProjectCachePage::_entryListText(const nlohmann::json& object, int row) const
{
    QStringList header;
    header << QString("#%1").arg(sentenceIndexOf(object, row));
    const QString speaker = _speakerString(object);
    if (!speaker.isEmpty()) {
        header << speaker;
    }
    const QString problems = _problemString(object, " | ");
    if (!problems.isEmpty()) {
        header << tr("问题: ") + compactPreview(problems, 120);
    }
    const QString engine = _entryTranslatedBy(object);
    if (!engine.isEmpty()) {
        header << engine;
    }
    return header.join("  ·  ")
        + "\n" + tr("原文: ") + compactPreview(_entrySource(object), 220)
        + "\n" + tr("译文: ") + compactPreview(_entryDst(object), 220);
}

QStringList ProjectCachePage::_problemsFromEditorText(const QString& text)
{
    QStringList result;
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            result.push_back(line);
        }
    }
    return result;
}

int ProjectCachePage::_countOccurrences(const QString& text, const QString& query)
{
    if (query.isEmpty()) {
        return 0;
    }
    int count = 0;
    int pos = 0;
    while ((pos = text.indexOf(query, pos, Qt::CaseSensitive)) >= 0) {
        ++count;
        pos += query.size();
    }
    return count;
}

int ProjectCachePage::_replaceInString(QString& text, const QString& query, const QString& replacement)
{
    const int count = _countOccurrences(text, query);
    if (count > 0) {
        text.replace(query, replacement, Qt::CaseSensitive);
    }
    return count;
}

QList<ProjectCachePage::ReplaceDetail> ProjectCachePage::_collectReplaceDetails(const QString& query, const QString& field, int* totalMatches) const
{
    int total = 0;
    QList<ReplaceDetail> details;
    for (const CacheFileInfo& file : _cacheFiles) {
        nlohmann::json entries;
        const auto loaded = _loadedEntriesByFile.find(file.relativeName);
        if (loaded != _loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!_readCacheFile(file.relativeName, entries, nullptr)) {
            continue;
        }
        int fileMatches = 0;
        for (const auto& item : entries) {
            if (!item.is_object()) {
                continue;
            }
            if (field == "src" || field == "all") {
                fileMatches += _countOccurrences(_entrySource(item), query);
            }
            if (field == "dst" || field == "all") {
                fileMatches += _countOccurrences(_entryDst(item), query);
            }
        }
        if (fileMatches > 0) {
            details.push_back({ file.relativeName, fileMatches });
            total += fileMatches;
        }
    }
    if (totalMatches) {
        *totalMatches = total;
    }
    return details;
}

int ProjectCachePage::_applyReplaceToEntries(nlohmann::json& entries, const QString& query, const QString& replacement, const QString& field) const
{
    int total = 0;
    for (auto& item : entries) {
        if (!item.is_object()) {
            continue;
        }
        if (field == "src" || field == "all") {
            QString source = _entrySource(item);
            const int matches = _replaceInString(source, query, replacement);
            if (matches > 0) {
                item["pre_processed_text"] = source.toStdString();
                total += matches;
            }
        }
        if (field == "dst" || field == "all") {
            QString dst = _entryDst(item);
            const int matches = _replaceInString(dst, query, replacement);
            if (matches > 0) {
                item["pre_translated_text"] = dst.toStdString();
                total += matches;
            }
        }
    }
    return total;
}

void ProjectCachePage::_runGlobalSearch()
{
    if (!_globalSearchEdit || !_searchResultList) {
        return;
    }
    const QString query = _globalSearchEdit->text().trimmed();
    const QString field = _globalSearchField->currentData().toString();
    _searchHits.clear();
    _searchModel->clear();
    if (query.isEmpty()) {
        _searchStatusLabel->clear();
        if (_searchNavButton) {
            _searchNavButton->setText(tr("搜索"));
        }
        return;
    }

    constexpr int maxResults = 2000;
    for (const CacheFileInfo& file : _cacheFiles) {
        nlohmann::json entries;
        const auto loaded = _loadedEntriesByFile.find(file.relativeName);
        if (loaded != _loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!_readCacheFile(file.relativeName, entries, nullptr)) {
            continue;
        }
        for (int i = 0; i < (int)entries.size(); ++i) {
            const auto& item = entries[i];
            if (!item.is_object()) {
                continue;
            }
            const QString src = _entrySource(item);
            const QString dst = _entryDst(item);
            const QString problem = _problemString(item, " | ");
            const bool matchSrc = src.contains(query, Qt::CaseInsensitive);
            const bool matchDst = dst.contains(query, Qt::CaseInsensitive);
            const bool matchProblem = problem.contains(query, Qt::CaseInsensitive);
            if ((field == "src" && !matchSrc)
                || (field == "dst" && !matchDst)
                || (field == "problems" && !matchProblem)
                || (field == "all" && !matchSrc && !matchDst && !matchProblem)) {
                continue;
            }
            _searchHits.push_back({
                file.relativeName,
                i,
                sentenceIndexOf(item, i),
                matchSrc,
                matchDst,
                matchProblem,
                _truncateForList(src),
                _truncateForList(dst),
                _truncateForList(problem),
            });
            if (_searchHits.size() >= maxResults) {
                break;
            }
        }
        if (_searchHits.size() >= maxResults) {
            break;
        }
    }

    for (int i = 0; i < _searchHits.size(); ++i) {
        const SearchHit& hit = _searchHits[i];
        QStringList badges;
        if (hit.matchSrc) badges << tr("原文");
        if (hit.matchDst) badges << tr("译文");
        if (hit.matchProblem) badges << tr("问题");
        QString text = QString("[%1] %2 #%3\n%4\n%5")
            .arg(badges.join("/"), hit.filename)
            .arg(hit.sentenceIndex)
            .arg(hit.sourcePreview)
            .arg(hit.dstPreview);
        if (!hit.problemPreview.isEmpty()) {
            text += "\n" + hit.problemPreview;
        }
        QStandardItem* item = new QStandardItem(text);
        item->setData(i, HitIndexRole);
        item->setEditable(false);
        setModelItemFont(item, BodyFontPx);
        _searchModel->appendRow(item);
    }
    _searchStatusLabel->setText(tr("%1 条结果").arg(_searchHits.size()));
    if (_searchNavButton) {
        _searchNavButton->setText(tr("搜索 (%1)").arg(_searchHits.size()));
    }
}

void ProjectCachePage::_previewReplace()
{
    const QString query = _replaceQueryEdit->text();
    if (query.isEmpty()) {
        _replacePreviewLabel->setText(tr("请输入查找内容"));
        return;
    }
    int total = 0;
    const QList<ReplaceDetail> details = _collectReplaceDetails(query, _replaceField->currentData().toString(), &total);
    QString text = tr("共 %1 处匹配，涉及 %2 个文件").arg(total).arg(details.size());
    const int limit = std::min(static_cast<int>(details.size()), 8);
    for (int i = 0; i < limit; ++i) {
        text += QString("\n%1: %2").arg(details[i].filename).arg(details[i].matches);
    }
    if (details.size() > limit) {
        text += tr("\n...");
    }
    _replacePreviewLabel->setText(text);
}

void ProjectCachePage::_executeReplace()
{
    if (!_ensureWritableAction(tr("批量替换"))) {
        return;
    }
    const QString query = _replaceQueryEdit->text();
    const QString replacement = _replaceWithEdit->text();
    const QString field = _replaceField->currentData().toString();
    if (query.isEmpty()) {
        _replacePreviewLabel->setText(tr("请输入查找内容"));
        return;
    }

    int total = 0;
    const QList<ReplaceDetail> details = _collectReplaceDetails(query, field, &total);
    if (total <= 0) {
        _replacePreviewLabel->setText(tr("无匹配内容"));
        return;
    }
    if (QMessageBox::question(this, tr("确认替换"),
        tr("确定要替换 %1 处内容吗？").arg(total)) != QMessageBox::Yes) {
        return;
    }

    int changedFiles = 0;
    int changedMatches = 0;
    for (const ReplaceDetail& detail : details) {
        nlohmann::json entries;
        const auto loaded = _loadedEntriesByFile.find(detail.filename);
        if (loaded != _loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!_readCacheFile(detail.filename, entries, nullptr)) {
            continue;
        }
        const int matches = _applyReplaceToEntries(entries, query, replacement, field);
        if (matches > 0) {
            _loadedEntriesByFile[detail.filename] = entries;
            _dirtyFiles.insert(detail.filename);
            if (_currentFile == detail.filename) {
                _entries = entries;
            }
            changedMatches += matches;
            ++changedFiles;
        }
    }

    _renderFileList();
    _renderEntries();
    _runGlobalSearch();
    _replacePreviewLabel->setText(tr("已替换 %1 处，涉及 %2 个文件；保存后落盘。").arg(changedMatches).arg(changedFiles));
    _setInfo(tr("批量替换完成，记得保存修改"));
    _updateActionStates();
}

void ProjectCachePage::_loadProblems()
{
    if (!_problemList) {
        return;
    }
    QMap<QString, int> counts;
    for (const CacheFileInfo& file : _cacheFiles) {
        nlohmann::json entries;
        const auto loaded = _loadedEntriesByFile.find(file.relativeName);
        if (loaded != _loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!_readCacheFile(file.relativeName, entries, nullptr)) {
            continue;
        }
        for (const auto& item : entries) {
            if (!item.is_object()) {
                continue;
            }
            for (const QString& problemText : _problemsFromEditorText(_problemString(item))) {
                counts[problemText] += 1;
            }
        }
    }

    QList<QPair<QString, int>> items;
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        items.push_back({ it.key(), it.value() });
    }
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b)
        {
            if (a.second != b.second) {
                return a.second > b.second;
            }
            return a.first.localeAwareCompare(b.first) < 0;
        });

    _problemModel->clear();
    for (const auto& itemPair : items) {
        const QString& problem = itemPair.first;
        const int count = itemPair.second;
        QStandardItem* item = new QStandardItem(QString("%1  (%2)").arg(problem).arg(count));
        item->setData(problem, ProblemTextRole);
        item->setToolTip(tr("点击搜索此问题"));
        item->setEditable(false);
        setModelItemFont(item, BodyFontPx);
        _problemModel->appendRow(item);
    }
    if (_problemsNavButton) {
        _problemsNavButton->setText(items.empty() ? tr("问题") : tr("问题 (%1)").arg(items.size()));
    }
}

void ProjectCachePage::_jumpToHit(int hitIndex)
{
    if (hitIndex < 0 || hitIndex >= _searchHits.size()) {
        return;
    }
    const SearchHit& hit = _searchHits[hitIndex];
    _loadCacheFile(hit.filename);
    _selectEntryByRow(hit.row);
}

void ProjectCachePage::_selectEntryByRow(int row)
{
    if (row < 0 || !_entryList || !_entryModel) {
        return;
    }
    if (_localSearchEdit && !_localSearchEdit->text().isEmpty()) {
        QSignalBlocker blocker(_localSearchEdit);
        _localSearchEdit->clear();
    }
    if (_filterProblemsCheck && _filterProblemsCheck->isChecked()) {
        QSignalBlocker blocker(_filterProblemsCheck);
        _filterProblemsCheck->setChecked(false);
    }
    _renderEntries();
    for (int modelRow = 0; modelRow < _entryModel->rowCount(); ++modelRow) {
        const QModelIndex index = _entryModel->index(modelRow, 0);
        if (index.data(JsonRowRole).toInt() == row) {
            _entryList->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            _entryList->setCurrentIndex(index);
            _entryList->scrollTo(index, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

int ProjectCachePage::_currentJsonRow() const
{
    if (!_entryList || !_entryList->selectionModel()) {
        return -1;
    }
    const QModelIndexList selected = _entryList->selectionModel()->selectedRows();
    if (selected.empty()) {
        return -1;
    }
    return selected.first().data(JsonRowRole).toInt();
}
