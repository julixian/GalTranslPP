#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QStandardItem>

#include "ElaCheckBox.h"
#include "ElaNoWheelComboBox.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPushButton.h"
#include "ElaText.h"

using namespace ProjectCachePagePrivate;

QStringList ProjectCachePage::problemsFromEditorText(const QString& text)
{
    QStringList result;
    for (const QString& line : text.split('\n')) {
        if (!line.isEmpty()) {
            result.push_back(line);
        }
    }
    return result;
}

int ProjectCachePage::countOccurrences(const QString& text, const QString& query)
{
    if (query.isEmpty()) {
        return 0;
    }
    int count = 0;
    qsizetype pos = 0;
    while ((pos = text.indexOf(query, pos, Qt::CaseSensitive)) >= 0) {
        ++count;
        pos += query.size();
    }
    return count;
}

int ProjectCachePage::replaceInString(QString& text, const QString& query, const QString& replacement)
{
    const int count = countOccurrences(text, query);
    if (count > 0) {
        text.replace(query, replacement, Qt::CaseSensitive);
    }
    return count;
}

QList<ProjectCachePage::ReplaceDetail> ProjectCachePage::collectReplaceDetails(const QString& query, const QString& field, int* totalMatches) const
{
    // 替换预览和实际替换共用同一套收集逻辑，避免“预览命中”和“实际修改”
    // 因字段范围不同步而产生偏差。
    int total = 0;
    QList<ReplaceDetail> details;
    for (const CacheFileInfo& file : m_cacheFiles) {
        json entries;
        const auto loaded = m_dirtyFiles.contains(file.relativeName) ? m_loadedEntriesByFile.find(file.relativeName) : m_loadedEntriesByFile.end();
        if (loaded != m_loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!readCacheFile(file.relativeName, entries, nullptr)) {
            continue;
        }
        int fileMatches = 0;
        for (const auto& item : entries) {
            if (!item.is_object()) {
                continue;
            }
            // 替换字段和翻译缓存实际字段保持一致：
            // src -> pre_processed_text，dst -> translated_raw_text。
            if (field == "src" || field == "all") {
                fileMatches += countOccurrences(entrySource(item), query);
            }
            if (field == "dst" || field == "all") {
                fileMatches += countOccurrences(entryDst(item), query);
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

int ProjectCachePage::applyReplaceToEntries(json& entries, const QString& query, const QString& replacement, const QString& field) const
{
    int total = 0;
    for (auto& item : entries) {
        if (!item.is_object()) {
            continue;
        }
        if (field == "src" || field == "all") {
            QString source = entrySource(item);
            const int matches = replaceInString(source, query, replacement);
            if (matches > 0) {
                item["pre_processed_text"] = source.toStdString();
                total += matches;
            }
        }
        if (field == "dst" || field == "all") {
            QString dst = entryDst(item);
            const int matches = replaceInString(dst, query, replacement);
            if (matches > 0) {
                item["translated_raw_text"] = dst.toStdString();
                total += matches;
            }
        }
    }
    return total;
}

void ProjectCachePage::runGlobalSearch()
{
    if (!m_globalSearchEdit || !m_searchResultList) {
        return;
    }
    const QString query = m_globalSearchEdit->text();
    const QString field = m_globalSearchField->currentData().toString();
    m_searchHits.clear();
    m_searchModel->clear();
    if (query.isEmpty()) {
        m_searchStatusLabel->clear();
        if (m_searchNavButton) {
            m_searchNavButton->setText(tr("搜索"));
        }
        return;
    }

    // 搜索框 textChanged 会频繁触发，这里限制结果数，避免大缓存目录里边输入边卡死。
    constexpr int maxResults = 2000;
    for (const CacheFileInfo& file : m_cacheFiles) {
        json entries;
        const auto loaded = m_dirtyFiles.contains(file.relativeName) ? m_loadedEntriesByFile.find(file.relativeName) : m_loadedEntriesByFile.end();
        if (loaded != m_loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!readCacheFile(file.relativeName, entries, nullptr)) {
            continue;
        }
        for (int i = 0; i < (int)entries.size(); ++i) {
            const auto& item = entries[i];
            if (!item.is_object()) {
                continue;
            }
            const QString src = entrySource(item);
            const QString dst = entryDst(item);
            const QString problem = problemString(item, " | ");
            const bool matchSrc = src.contains(query, Qt::CaseInsensitive);
            const bool matchDst = dst.contains(query, Qt::CaseInsensitive);
            const bool matchProblem = problem.contains(query, Qt::CaseInsensitive);
            if ((field == "src" && !matchSrc)
                || (field == "dst" && !matchDst)
                || (field == "problems" && !matchProblem)
                || (field == "all" && !matchSrc && !matchDst && !matchProblem)) {
                continue;
            }
            m_searchHits.push_back({
                file.relativeName,
                i,
                sentenceIndexOf(item, i),
                matchSrc,
                matchDst,
                matchProblem,
                truncateForList(src),
                truncateForList(dst),
                truncateForList(problem),
            });
            if (m_searchHits.size() >= maxResults) {
                break;
            }
        }
        if (m_searchHits.size() >= maxResults) {
            break;
        }
    }

    for (int i = 0; i < m_searchHits.size(); ++i) {
        const SearchHit& hit = m_searchHits[i];
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
        m_searchModel->appendRow(item);
    }
    m_searchStatusLabel->setText(tr("%1 条结果").arg(m_searchHits.size()));
    if (m_searchNavButton) {
        m_searchNavButton->setText(tr("搜索 (%1)").arg(m_searchHits.size()));
    }
}

void ProjectCachePage::previewReplace()
{
    const QString query = m_replaceQueryEdit->text();
    if (query.isEmpty()) {
        m_replacePreviewLabel->setText(tr("请输入查找内容"));
        return;
    }
    int total = 0;
    const QList<ReplaceDetail> details = collectReplaceDetails(query, m_replaceField->currentData().toString(), &total);
    QString text = tr("共 %1 处匹配，涉及 %2 个文件").arg(total).arg(details.size());
    const int limit = std::min((int)details.size(), 8);
    for (int i = 0; i < limit; ++i) {
        text += QString("\n%1: %2").arg(details[i].filename).arg(details[i].matches);
    }
    if (details.size() > limit) {
        text += tr("\n...");
    }
    m_replacePreviewLabel->setText(text);
}

void ProjectCachePage::executeReplace()
{
    if (!ensureWritableAction(tr("批量替换"))) {
        return;
    }
    const QString query = m_replaceQueryEdit->text();
    const QString replacement = m_replaceWithEdit->text();
    const QString field = m_replaceField->currentData().toString();
    if (query.isEmpty()) {
        m_replacePreviewLabel->setText(tr("请输入查找内容"));
        return;
    }

    int total = 0;
    const QList<ReplaceDetail> details = collectReplaceDetails(query, field, &total);
    if (total <= 0) {
        m_replacePreviewLabel->setText(tr("无匹配内容"));
        return;
    }
    if (!confirmAction(tr("确认替换"), tr("确定要替换 %1 处内容吗？").arg(total))) {
        return;
    }

    int changedFiles = 0;
    int changedMatches = 0;
    for (const ReplaceDetail& detail : details) {
        json entries;
        const auto loaded = m_dirtyFiles.contains(detail.filename) ? m_loadedEntriesByFile.find(detail.filename) : m_loadedEntriesByFile.end();
        if (loaded != m_loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!readCacheFile(detail.filename, entries, nullptr)) {
            continue;
        }
        const int matches = applyReplaceToEntries(entries, query, replacement, field);
        if (matches > 0) {
            m_loadedEntriesByFile[detail.filename] = entries;
            m_dirtyFiles.insert(detail.filename);
            if (m_currentFile == detail.filename) {
                m_entries = entries;
            }
            changedMatches += matches;
            ++changedFiles;
        }
    }

    // 替换只改内存副本并标脏；刷新三个视图让用户立刻看到待保存状态。
    renderFileList();
    renderEntries();
    runGlobalSearch();
    m_replacePreviewLabel->setText(tr("已替换 %1 处，涉及 %2 个文件；保存后落盘。").arg(changedMatches).arg(changedFiles));
    setInfo(tr("批量替换完成，记得保存修改"));
    updateActionStates();
}

void ProjectCachePage::loadProblems()
{
    if (!m_problemList) {
        return;
    }
    m_problemsLoaded = true;
    // 问题页只聚合 GalTranslPP 的 problems 数组；这里不兼容 GalTransl 旧 problem 字段。
    QMap<QString, int> counts;
    for (const CacheFileInfo& file : m_cacheFiles) {
        json entries;
        const auto loaded = m_dirtyFiles.contains(file.relativeName) ? m_loadedEntriesByFile.find(file.relativeName) : m_loadedEntriesByFile.end();
        if (loaded != m_loadedEntriesByFile.end()) {
            entries = loaded.value();
        }
        else if (!readCacheFile(file.relativeName, entries, nullptr)) {
            continue;
        }
        for (const auto& item : entries) {
            if (!item.is_object()) {
                continue;
            }
            for (const QString& problemText : problemsFromEditorText(problemString(item))) {
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

    m_problemModel->clear();
    for (const auto& itemPair : items) {
        const QString& problem = itemPair.first;
        const int count = itemPair.second;
        QStandardItem* item = new QStandardItem(QString("%1  (%2)").arg(problem).arg(count));
        item->setData(problem, ProblemTextRole);
        item->setToolTip(tr("点击搜索此问题"));
        item->setEditable(false);
        setModelItemFont(item, BodyFontPx);
        m_problemModel->appendRow(item);
    }
    if (m_problemsNavButton) {
        m_problemsNavButton->setText(items.empty() ? tr("问题") : tr("问题 (%1)").arg(items.size()));
    }
}

void ProjectCachePage::jumpToHit(int hitIndex)
{
    if (hitIndex < 0 || hitIndex >= m_searchHits.size()) {
        return;
    }
    const SearchHit& hit = m_searchHits[hitIndex];
    loadCacheFile(hit.filename);
    selectEntryByRow(hit.row);
}

void ProjectCachePage::selectEntryByRow(int row)
{
    if (row < 0 || !m_entryList || !m_entryModel) {
        return;
    }
    if (m_localSearchEdit && !m_localSearchEdit->text().isEmpty()) {
        QSignalBlocker blocker(m_localSearchEdit);
        m_localSearchEdit->clear();
    }
    if (m_filterProblemsCheck && m_filterProblemsCheck->isChecked()) {
        QSignalBlocker blocker(m_filterProblemsCheck);
        m_filterProblemsCheck->setChecked(false);
    }
    // 命中搜索结果后清掉右侧局部过滤，再重新渲染，保证目标行一定可见。
    renderEntries();
    for (int modelRow = 0; modelRow < m_entryModel->rowCount(); ++modelRow) {
        const QModelIndex index = m_entryModel->index(modelRow, 0);
        if (index.data(JsonRowRole).toInt() == row) {
            m_entryList->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_entryList->setCurrentIndex(index);
            m_entryList->scrollTo(index, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

int ProjectCachePage::currentJsonRow() const
{
    if (!m_entryList || !m_entryList->selectionModel()) {
        return -1;
    }
    const QModelIndexList selected = m_entryList->selectionModel()->selectedRows();
    if (selected.empty()) {
        return -1;
    }
    return selected.first().data(JsonRowRole).toInt();
}
