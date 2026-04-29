#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <algorithm>

#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QStandardItem>

#include "ElaCheckBox.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPushButton.h"
#include "ElaText.h"

using namespace ProjectCachePagePrivate;

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
    // 替换预览和实际替换共用同一套收集逻辑，避免“预览命中”和“实际修改”
    // 因字段范围不同步而产生偏差。
    int total = 0;
    QList<ReplaceDetail> details;
    for (const CacheFileInfo& file : _cacheFiles) {
        json entries;
        const auto loaded = _dirtyFiles.contains(file.relativeName) ? _loadedEntriesByFile.find(file.relativeName) : _loadedEntriesByFile.end();
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
            // 替换字段和翻译缓存实际字段保持一致：
            // src -> pre_processed_text，dst -> pre_translated_text。
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

int ProjectCachePage::_applyReplaceToEntries(json& entries, const QString& query, const QString& replacement, const QString& field) const
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

    // 搜索框 textChanged 会频繁触发，这里限制结果数，避免大缓存目录里边输入边卡死。
    constexpr int maxResults = 2000;
    for (const CacheFileInfo& file : _cacheFiles) {
        json entries;
        const auto loaded = _dirtyFiles.contains(file.relativeName) ? _loadedEntriesByFile.find(file.relativeName) : _loadedEntriesByFile.end();
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
        QStandardItem* item = new QStandardItem(hit.filename);
        item->setData(i, HitIndexRole);
        item->setData(hit.filename, HitFileRole);
        item->setData(hit.sentenceIndex, HitSentenceRole);
        item->setData(badges, HitBadgesRole);
        item->setData(hit.sourcePreview, HitSourceRole);
        item->setData(hit.dstPreview, HitDstRole);
        item->setData(hit.problemPreview, HitProblemRole);
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
    if (!_confirmAction(tr("确认替换"), tr("确定要替换 %1 处内容吗？").arg(total))) {
        return;
    }

    int changedFiles = 0;
    int changedMatches = 0;
    for (const ReplaceDetail& detail : details) {
        json entries;
        const auto loaded = _dirtyFiles.contains(detail.filename) ? _loadedEntriesByFile.find(detail.filename) : _loadedEntriesByFile.end();
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

    // 替换只改内存副本并标脏；刷新三个视图让用户立刻看到待保存状态。
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
    _problemsLoaded = true;
    // 问题页只聚合 GalTranslPP 的 problems 数组；这里不兼容 GalTransl 旧 problem 字段。
    QMap<QString, int> counts;
    for (const CacheFileInfo& file : _cacheFiles) {
        json entries;
        const auto loaded = _dirtyFiles.contains(file.relativeName) ? _loadedEntriesByFile.find(file.relativeName) : _loadedEntriesByFile.end();
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
    std::ranges::sort(items, [](const auto& a, const auto& b)
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
    // 命中搜索结果后清掉右侧局部过滤，再重新渲染，保证目标行一定可见。
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
