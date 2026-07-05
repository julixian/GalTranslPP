#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <algorithm>
#include <fstream>

#include <QCollator>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QStandardItem>
#include <QStackedWidget>

#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPushButton.h"
#include "ElaText.h"

import Tool;

using namespace ProjectCachePagePrivate;

void ProjectCachePage::loadCacheFiles(bool discardDirty)
{
    // 首次进入页面会保守保留脏文件内存副本；用户主动刷新时 discardDirty=true，
    // 表示放弃未保存编辑并以磁盘内容为准，文件名前的 * 也会随之清掉。
    m_cacheFiles.clear();
    m_cacheFilesLoaded = true;
    m_problemsLoaded = false;
    const QString previousFile = m_currentFile;
    if (discardDirty) {
        m_dirtyFiles.clear();
        m_loadedEntriesByFile.clear();
    }
    const bool keepPreviousFromMemory = !discardDirty && !previousFile.isEmpty() && m_dirtyFiles.contains(previousFile);
    const fs::path cacheDir = getCacheDir();
    if (!fs::exists(cacheDir)) {
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
    }

    if (fs::exists(cacheDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(cacheDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }
            CacheFileInfo info;
            info.relativeName = QString::fromStdWString(fs::relative(entry.path(), cacheDir).wstring());
            if (discardDirty || !m_dirtyFiles.contains(info.relativeName)) {
                m_loadedEntriesByFile.remove(info.relativeName);
            }
            std::error_code ec;
            info.size = entry.file_size(ec);
            QFileInfo fileInfo(QString::fromStdWString(entry.path().wstring()));
            info.modified = fileInfo.lastModified();

            json data;
            QString error;
            if (readCacheFile(info.relativeName, data, &error) && data.is_array()) {
                info.entries = (int)data.size();
                for (const auto& item : data) {
                    if (item.is_object() && !problemString(item).isEmpty()) {
                        ++info.problems;
                    }
                }
            }
            else {
                info.parseOk = false;
                info.error = error;
            }
            m_cacheFiles.push_back(info);
        }
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    // 使用 Windows 资源管理器式自然排序，例如 1、2、10，而不是 1、10、2。
    std::ranges::sort(m_cacheFiles, [&collator](const CacheFileInfo& a, const CacheFileInfo& b)
        {
            return collator.compare(a.relativeName, b.relativeName) < 0;
        });

    m_cacheDirLabel->setText(QString::fromStdWString(cacheDir.wstring()));
    renderFileList();
    const bool previousStillExists = std::ranges::any_of(m_cacheFiles, [&](const CacheFileInfo& file)
        {
            return file.relativeName == previousFile && file.parseOk;
        });
    if (!previousFile.isEmpty()) {
        if (previousStillExists) {
            loadCacheFile(previousFile, !keepPreviousFromMemory);
        }
        else {
            m_currentFile.clear();
            m_entries = json::array();
            m_selectedEntryRows.clear();
            renderEntries();
        }
    }
    // 如果用户当前正在问题页或搜索页，刷新磁盘文件后同步刷新可见结果。
    if (m_sidebarStack && m_sidebarStack->currentIndex() == 2) {
        loadProblems();
    }
    if (m_globalSearchEdit && !m_globalSearchEdit->text().isEmpty()) {
        runGlobalSearch();
    }
    updateActionStates();
}

void ProjectCachePage::renderFileList()
{
    // 文件列表只保存相对路径到 Qt::UserRole，点击时再加载对应 JSON。
    // 这样列表重建不会复制完整缓存内容。
    m_fileModel->clear();
    for (const CacheFileInfo& file : m_cacheFiles) {
        QString title = file.relativeName;
        if (m_dirtyFiles.contains(file.relativeName)) {
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
        m_fileModel->appendRow(item);
        if (m_currentFile == file.relativeName) {
            const QModelIndex index = m_fileModel->indexFromItem(item);
            m_fileList->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_fileList->setCurrentIndex(index);
        }
    }
    if (m_filesNavButton) {
        m_filesNavButton->setText(tr("文件 (%1)").arg(m_cacheFiles.size()));
    }
}

void ProjectCachePage::loadCacheFile(const QString& filename, bool forceReload)
{
    if (filename.isEmpty()) {
        return;
    }
    const bool fileChanged = m_currentFile != filename;
    // 先用内存副本，保证切换文件时不丢尚未保存的改动；
    // forceReload 用于刷新后重新读取磁盘上的干净文件。
    if (!forceReload && m_loadedEntriesByFile.contains(filename)) {
        m_entries = m_loadedEntriesByFile.value(filename);
    }
    else {
        QString error;
        if (!readCacheFile(filename, m_entries, &error)) {
            setError(error);
            return;
        }
        if (!m_entries.is_array()) {
            setError(tr("缓存文件不是 JSON 数组: %1").arg(filename));
            return;
        }
        m_loadedEntriesByFile[filename] = m_entries;
    }
    m_currentFile = filename;
    if (fileChanged) {
        m_selectedEntryRows.clear();
    }
    m_currentFileLabel->setText((m_dirtyFiles.contains(filename) ? "*" : "") + filename);
    renderEntries();
    updateCurrentSummary();
    updateActionStates();
}

fs::path ProjectCachePage::getCacheDir() const
{
    return m_projectDir / transCacheDirName;
}

fs::path ProjectCachePage::cachePathForRelativeName(const QString& filename) const
{
    return getCacheDir() / fs::path(filename.toStdWString());
}

bool ProjectCachePage::readCacheFile(const QString& filename, json& entries, QString* errorMessage) const
{
    try {
        // 缓存文件是翻译核心生成的 UTF-8 JSON 数组。
        std::ifstream ifs(cachePathForRelativeName(filename), std::ios::binary);
        if (!ifs.is_open()) {
            if (errorMessage) {
                *errorMessage = tr("无法打开缓存文件: %1").arg(filename);
            }
            return false;
        }
        entries = json::parse(ifs);
        return true;
    }
    catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = tr("解析缓存失败: %1\n%2").arg(filename).arg(QString::fromStdString(e.what()));
        }
        return false;
    }
}

bool ProjectCachePage::writeCacheFile(const QString& filename, const json& entries, QString* errorMessage) const
{
    try {
        const fs::path path = cachePathForRelativeName(filename);
        createParent(path);
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (errorMessage) {
                *errorMessage = tr("无法写入缓存文件: %1").arg(filename);
            }
            return false;
        }
        ofs << entries.dump(2);
        return true;
    }
    catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = tr("写入缓存失败: %1\n%2").arg(filename).arg(QString::fromStdString(e.what()));
        }
        return false;
    }
}

