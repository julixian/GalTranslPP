#ifndef PROJECTCACHEPAGE_H
#define PROJECTCACHEPAGE_H

#include <filesystem>
#include <cstdint>
#include <toml.hpp>

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QStandardItemModel>
#include <QSet>

#include <nlohmann/json.hpp>

#include "BasePage.h"

class ElaPlainTextEdit;
class ElaIconButton;
class ElaPushButton;
class ElaText;
class ElaListView;
class ElaLineEdit;
class ElaComboBox;
class ElaCheckBox;
class QButtonGroup;
class QSplitter;
class QStackedWidget;
class QVBoxLayout;
class QWidget;

namespace fs = std::filesystem;

class ProjectCachePage : public BasePage
{
    Q_OBJECT

public:
    explicit ProjectCachePage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~ProjectCachePage() override;

    void refreshCacheFiles();

private:
    struct CacheFileInfo {
        QString relativeName;
        std::uintmax_t size{0};
        QDateTime modified;
        int entries{0};
        int problems{0};
        bool parseOk{true};
        QString error;
    };

    struct SearchHit {
        QString filename;
        int row{-1};
        int sentenceIndex{-1};
        bool matchSrc{false};
        bool matchDst{false};
        bool matchProblem{false};
        QString sourcePreview;
        QString dstPreview;
        QString problemPreview;
    };

    struct ReplaceDetail {
        QString filename;
        int matches{0};
    };

    void _setupUI();

    // ProjectCachePageFiles.cpp: discover cache JSON files, keep Windows-style
    // natural ordering, and read/write the backing UTF-8 JSON arrays.
    void _loadCacheFiles();
    void _loadCacheFile(const QString& filename, bool forceReload = false);
    void _renderFileList();
    fs::path _cacheDir() const;
    fs::path _cachePathForRelativeName(const QString& filename) const;
    bool _readCacheFile(const QString& filename, nlohmann::json& entries, QString* errorMessage = nullptr) const;
    bool _writeCacheFile(const QString& filename, const nlohmann::json& entries, QString* errorMessage = nullptr) const;

    // ProjectCachePageEntries.cpp: populate the right-side overview and edit a
    // single sentence while preserving row mappings back to the JSON array.
    void _renderEntries();
    void _syncSelectedEntryRows();
    void _updateEntryListItem(int row);
    void _updateEntryField(int row, const char* key, const QString& value);
    void _openEntryEditor(int row);
    void _deleteEntryRows(QList<int> rows);
    static QString _jsonString(const nlohmann::json& object, const char* key);
    static QString _speakerString(const nlohmann::json& object);
    static QString _problemString(const nlohmann::json& object, const QString& separator = "\n");
    static QString _entrySource(const nlohmann::json& object);
    static QString _entryDst(const nlohmann::json& object);
    static QString _entryOriginal(const nlohmann::json& object);
    static QString _entryPreview(const nlohmann::json& object);
    static QString _entryTranslatedBy(const nlohmann::json& object);
    static QString _truncateForList(const QString& text, int maxChars = 120);
    QString _entryListText(const nlohmann::json& object, int row) const;
    static QStringList _problemsFromEditorText(const QString& text);
    static int _countOccurrences(const QString& text, const QString& query);
    static int _replaceInString(QString& text, const QString& query, const QString& replacement);

    // ProjectCachePageSearch.cpp: global search, problem aggregation, and batch
    // replace. Search reads cached in-memory edits first, then falls back to disk.
    QList<ReplaceDetail> _collectReplaceDetails(const QString& query, const QString& field, int* totalMatches = nullptr) const;
    int _applyReplaceToEntries(nlohmann::json& entries, const QString& query, const QString& replacement, const QString& field) const;
    void _runGlobalSearch();
    void _previewReplace();
    void _executeReplace();
    void _loadProblems();
    void _jumpToHit(int hitIndex);
    void _selectEntryByRow(int row);
    int _currentJsonRow() const;

    // ProjectCachePageActions.cpp: shared page state, themed feedback, locking
    // while the project is running, and ElaContentDialog confirmations.
    void _markDirty(const QString& filename);
    void _setInfo(const QString& message);
    void _setError(const QString& message);
    void _updateCurrentSummary();
    void _updateActionStates();
    void _setSidebarPage(int index);
    void _refreshThemeStyles();
    void _setReplacePanelVisible(bool visible);
    bool _isProjectRunning() const;
    bool _ensureWritableAction(const QString& actionName) const;
    bool _confirmAction(const QString& title, const QString& message);

private:
    fs::path& _projectDir;
    toml::ordered_value& _projectConfig;

    QList<CacheFileInfo> _cacheFiles;
    QMap<QString, nlohmann::json> _loadedEntriesByFile;
    QSet<QString> _dirtyFiles;
    QList<SearchHit> _searchHits;
    QString _currentFile;
    nlohmann::json _entries = nlohmann::json::array();

    bool _renderingEntries{false};
    QSet<int> _selectedEntryRows;

    QButtonGroup* _sidebarButtonGroup{nullptr};
    QStackedWidget* _sidebarStack{nullptr};
    ElaPushButton* _filesNavButton{nullptr};
    ElaPushButton* _searchNavButton{nullptr};
    ElaPushButton* _problemsNavButton{nullptr};
    ElaListView* _fileList{nullptr};
    ElaListView* _searchResultList{nullptr};
    ElaListView* _problemList{nullptr};
    ElaListView* _entryList{nullptr};
    QStandardItemModel* _fileModel{nullptr};
    QStandardItemModel* _searchModel{nullptr};
    QStandardItemModel* _problemModel{nullptr};
    QStandardItemModel* _entryModel{nullptr};

    ElaLineEdit* _localSearchEdit{nullptr};
    ElaCheckBox* _filterProblemsCheck{nullptr};
    ElaLineEdit* _globalSearchEdit{nullptr};
    ElaComboBox* _globalSearchField{nullptr};
    ElaLineEdit* _replaceQueryEdit{nullptr};
    ElaLineEdit* _replaceWithEdit{nullptr};
    ElaComboBox* _replaceField{nullptr};

    ElaText* _cacheDirLabel{nullptr};
    ElaText* _currentFileLabel{nullptr};
    ElaText* _currentSummaryLabel{nullptr};
    ElaText* _searchStatusLabel{nullptr};
    ElaText* _replacePreviewLabel{nullptr};
    QSplitter* _mainSplitter{nullptr};
    QWidget* _replacePanel{nullptr};

    ElaIconButton* _saveButton{nullptr};
    ElaIconButton* _saveAllButton{nullptr};
    ElaPushButton* _deleteEntriesButton{nullptr};
    ElaPushButton* _deleteFilesButton{nullptr};
    ElaPushButton* _editEntryButton{nullptr};
    ElaPushButton* _replaceToggleButton{nullptr};
    ElaPushButton* _replaceExecuteButton{nullptr};
};

#endif // PROJECTCACHEPAGE_H
