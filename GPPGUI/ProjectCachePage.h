#ifndef PROJECTCACHEPAGE_H
#define PROJECTCACHEPAGE_H


#include "BasePage.h"
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QSet>
#include <QStandardItemModel>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <toml.hpp>

class ElaPlainTextEdit;
class ElaIconButton;
class ElaPushButton;
class ElaToolButton;
class ElaText;
class ElaListView;
class ElaLineEdit;
class QAction;
class QRegularExpression;
class QButtonGroup;
class QSplitter;
class QStackedWidget;
class QVBoxLayout;
class QWidget;

namespace fs = std::filesystem;
using json = nlohmann::json;

class ProjectCachePage : public BasePage
{
    Q_OBJECT

public:
    explicit ProjectCachePage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent = nullptr);

    void ensureCacheFilesLoaded();
    void refreshCacheFiles();

private:
    struct CacheFileInfo {
        QString relativeName;
        std::uintmax_t size{};
        QDateTime modified;
        int entries{};
        int problems{};
        bool parseOk = true;
        QString error;
    };

    struct SearchHit {
        QString filename;
        int row = -1;
        int sentenceIndex = -1;
        bool matchSrc{};
        bool matchDst{};
        bool matchProblem{};
        QString sourcePreview;
        QString dstPreview;
        QString problemPreview;
    };

    struct ReplacePreviewEntry {
        QString filename;
        int row = -1;
        int sentenceIndex = -1;
        QString field;
        QString before;
        QString after;
    };

    struct ReplaceDetail {
        QString filename;
        int matches{};
        QList<ReplacePreviewEntry> previews;
    };

    void setupUi();

    // ProjectCachePageFiles.cpp：发现缓存 JSON 文件、按 Windows 自然顺序排序，
    // 并负责 UTF-8 JSON 数组的读写。
    void loadCacheFiles(bool discardDirty = false);
    void loadCacheFile(const QString& filename, bool forceReload = false);
    void renderFileList();
    fs::path getCacheDir() const;
    fs::path cachePathForRelativeName(const QString& filename) const;
    bool readCacheFile(const QString& filename, json& entries, QString* errorMessage = nullptr) const;
    bool writeCacheFile(const QString& filename, const json& entries, QString* errorMessage = nullptr) const;

    // ProjectCachePageEntries.cpp：构建右侧句子概览和单句编辑弹窗；
    // 可见模型里的行号始终映射回原始 JSON 数组行。
    void renderEntries();
    void syncSelectedEntryRows();
    void updateEntryListItem(int row);
    void updateEntryField(int row, const char* key, const QString& value);
    void openEntryEditor(int row);
    void deleteEntryRows(QList<int> rows);
    static QString jsonString(const json& object, const char* key);
    static QString speakerString(const json& object);
    static QString problemString(const json& object, const QString& separator = "\n");
    static QString entrySource(const json& object);
    static QString entryDst(const json& object);
    static QString entryOriginal(const json& object);
    static QString entryPreview(const json& object);
    static QString entryTransby(const json& object);
    static QString truncateForList(const QString& text, int maxChars = 120);
    QString entryListText(const json& object, int row) const;
    static QStringList problemsFromEditorText(const QString& text);
    static int countOccurrences(const QString& text, const QString& query,
        const QRegularExpression* regex = nullptr);
    static int replaceInString(QString& text, const QString& query, const QString& replacement,
        const QRegularExpression* regex = nullptr);

    // ProjectCachePageSearch.cpp：全局搜索、问题聚合和批量替换；
    // 搜索优先读取内存中的未保存编辑，再回退到磁盘文件。
    QList<ReplaceDetail> collectReplaceDetails(const QString& query, const QString& replacement,
        const QString& field,
        const QRegularExpression* regex = nullptr, int* totalMatches = nullptr) const;
    int applyReplaceToEntries(json& entries, const QString& query, const QString& replacement,
        const QString& field, const QRegularExpression* regex = nullptr) const;
    bool prepareRegex(const QString& query, bool enabled, bool caseInsensitive,
        QRegularExpression& regex, ElaLineEdit* edit, QAction* errorAction) const;
    void setRegexError(ElaLineEdit* edit, QAction* errorAction, const QString& message) const;
    void runGlobalSearch();
    void previewReplace();
    void executeReplace();
    void loadProblems();
    void jumpToHit(int hitIndex);
    void selectEntryByRow(int row);
    int currentJsonRow() const;

    // ProjectCachePageActions.cpp：集中维护页面状态、Ela 风格反馈、
    // 项目运行时的只读锁定，以及 ElaContentDialog 确认框。
    void markDirty(const QString& filename);
    void setInfo(const QString& message);
    void setError(const QString& message);
    void updateCurrentSummary();
    void updateActionStates();
    void setSidebarPage(int index);
    void refreshThemeStyles();
    void setReplacePanelVisible(bool visible);
    bool isProjectRunning() const;
    bool ensureWritableAction(const QString& actionName) const;
    bool confirmAction(const QString& title, const QString& message);

private:
    fs::path& m_projectDir;
    toml::ordered_value& m_projectConfig;

    QList<CacheFileInfo> m_cacheFiles;
    QMap<QString, json> m_loadedEntriesByFile;
    QSet<QString> m_dirtyFiles;
    QList<SearchHit> m_searchHits;
    QString m_currentFile;
    json m_entries = json::array();

    bool m_cacheFilesLoaded{};
    bool m_problemsLoaded{};
    bool m_renderingEntries{};
    QSet<int> m_selectedEntryRows;

    QButtonGroup* m_sidebarButtonGroup = nullptr;
    QStackedWidget* m_sidebarStack = nullptr;
    ElaPushButton* m_filesNavButton = nullptr;
    ElaPushButton* m_searchNavButton = nullptr;
    ElaPushButton* m_problemsNavButton = nullptr;
    ElaListView* m_fileList = nullptr;
    ElaListView* m_searchResultList = nullptr;
    ElaListView* m_problemList = nullptr;
    ElaListView* m_entryList = nullptr;
    QStandardItemModel* m_fileModel = nullptr;
    QStandardItemModel* m_searchModel = nullptr;
    QStandardItemModel* m_problemModel = nullptr;
    QStandardItemModel* m_entryModel = nullptr;

    ElaLineEdit* m_localSearchEdit = nullptr;
    ElaToolButton* m_filterProblemsCheck = nullptr;
    QAction* m_localRegexErrorAction = nullptr;
    ElaToolButton* m_localRegexButton = nullptr;
    ElaLineEdit* m_globalSearchEdit = nullptr;
    ElaToolButton* m_globalSearchField = nullptr;
    QAction* m_globalRegexErrorAction = nullptr;
    ElaToolButton* m_globalRegexButton = nullptr;
    QString m_globalSearchFieldKey = "all";
    ElaLineEdit* m_replaceQueryEdit = nullptr;
    ElaLineEdit* m_replaceWithEdit = nullptr;
    ElaToolButton* m_replaceField = nullptr;
    QAction* m_replaceRegexErrorAction = nullptr;
    ElaToolButton* m_replaceRegexButton = nullptr;
    QString m_replaceFieldKey = "dst";

    ElaText* m_cacheDirLabel = nullptr;
    ElaText* m_currentFileLabel = nullptr;
    ElaText* m_currentSummaryLabel = nullptr;
    ElaText* m_searchStatusLabel = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QWidget* m_replacePanel = nullptr;

    ElaIconButton* m_saveButton = nullptr;
    ElaIconButton* m_saveAllButton = nullptr;
    ElaToolButton* m_deleteEntriesButton = nullptr;
    ElaToolButton* m_deleteFilesButton = nullptr;
    ElaToolButton* m_editEntryButton = nullptr;
    ElaToolButton* m_replaceToggleButton = nullptr;
    ElaToolButton* m_replaceExecuteButton = nullptr;
};

#endif
