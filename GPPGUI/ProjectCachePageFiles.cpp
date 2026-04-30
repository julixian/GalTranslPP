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

void ProjectCachePage::_loadCacheFiles(bool discardDirty)
{
    // 首次进入页面会保守保留脏文件内存副本；用户主动刷新时 discardDirty=true，
    // 表示放弃未保存编辑并以磁盘内容为准，文件名前的 * 也会随之清掉。
    _cacheFiles.clear();
    _cacheFilesLoaded = true;
    _problemsLoaded = false;
    const QString previousFile = _currentFile;
    if (discardDirty) {
        _dirtyFiles.clear();
        _loadedEntriesByFile.clear();
    }
    const bool keepPreviousFromMemory = !discardDirty && !previousFile.isEmpty() && _dirtyFiles.contains(previousFile);
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
            if (discardDirty || !_dirtyFiles.contains(info.relativeName)) {
                _loadedEntriesByFile.remove(info.relativeName);
            }
            std::error_code ec;
            info.size = entry.file_size(ec);
            QFileInfo fileInfo(QString(entry.path().wstring()));
            info.modified = fileInfo.lastModified();

            json data;
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
    // 使用 Windows 资源管理器式自然排序，例如 1、2、10，而不是 1、10、2。
    std::ranges::sort(_cacheFiles, [&collator](const CacheFileInfo& a, const CacheFileInfo& b)
        {
            return collator.compare(a.relativeName, b.relativeName) < 0;
        });

    _cacheDirLabel->setText(QString(cacheDir.wstring()));
    _renderFileList();
    const bool previousStillExists = std::ranges::any_of(_cacheFiles, [&](const CacheFileInfo& file)
        {
            return file.relativeName == previousFile && file.parseOk;
        });
    if (!previousFile.isEmpty()) {
        if (previousStillExists) {
            _loadCacheFile(previousFile, !keepPreviousFromMemory);
        }
        else {
            _currentFile.clear();
            _entries = json::array();
            _selectedEntryRows.clear();
            _renderEntries();
        }
    }
    // 如果用户当前正在问题页或搜索页，刷新磁盘文件后同步刷新可见结果。
    if (_sidebarStack && _sidebarStack->currentIndex() == 2) {
        _loadProblems();
    }
    if (_globalSearchEdit && !_globalSearchEdit->text().trimmed().isEmpty()) {
        _runGlobalSearch();
    }
    _updateActionStates();
}

void ProjectCachePage::_renderFileList()
{
    // 文件列表只保存相对路径到 Qt::UserRole，点击时再加载对应 JSON。
    // 这样列表重建不会复制完整缓存内容。
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
    // 先用内存副本，保证切换文件时不丢尚未保存的改动；
    // forceReload 用于刷新后重新读取磁盘上的干净文件。
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

fs::path ProjectCachePage::_cacheDir() const
{
    return _projectDir / transCacheDirName;
}

fs::path ProjectCachePage::_cachePathForRelativeName(const QString& filename) const
{
    return _cacheDir() / fs::path(filename.toStdWString());
}

bool ProjectCachePage::_readCacheFile(const QString& filename, json& entries, QString* errorMessage) const
{
    try {
        // 缓存文件是翻译核心生成的 UTF-8 JSON 数组。
        std::ifstream ifs(_cachePathForRelativeName(filename), std::ios::binary);
        if (!ifs.is_open()) {
            if (errorMessage) {
                *errorMessage = tr("无法打开缓存文件: ") + filename;
            }
            return false;
        }
        entries = json::parse(ifs);
        return true;
    }
    catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = tr("解析缓存失败: ") + filename + "\n" + QString::fromStdString(e.what());
        }
        return false;
    }
}

bool ProjectCachePage::_writeCacheFile(const QString& filename, const json& entries, QString* errorMessage) const
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

