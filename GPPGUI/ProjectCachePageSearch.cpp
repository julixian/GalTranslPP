#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <QAction>
#include <QClipboard>
#include <QFont>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "ElaContentDialog.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPushButton.h"
#include "ElaSpinBox.h"
#include "ElaText.h"
#include "ElaToolButton.h"

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

int ProjectCachePage::countOccurrences(const QString& text, const QString& query,
    const QRegularExpression* regex)
{
    if (regex) {
        int count = 0;
        QRegularExpressionMatchIterator matches = regex->globalMatch(text);
        while (matches.hasNext()) {
            matches.next();
            ++count;
        }
        return count;
    }
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

int ProjectCachePage::replaceInString(QString& text, const QString& query,
    const QString& replacement, const QRegularExpression* regex)
{
    const int count = countOccurrences(text, query, regex);
    if (count > 0) {
        if (regex) {
            text.replace(*regex, replacement);
        }
        else {
            text.replace(query, replacement, Qt::CaseSensitive);
        }
    }
    return count;
}

QList<ProjectCachePage::ReplaceDetail> ProjectCachePage::collectReplaceDetails(
    const QString& query, const QString& replacement, const QString& field,
    const QRegularExpression* regex,
    int* totalMatches) const
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
        ReplaceDetail detail;
        detail.filename = file.relativeName;
        int fileMatches = 0;
        for (int row = 0; row < (int)entries.size(); ++row) {
            const auto& item = entries[row];
            if (!item.is_object()) {
                continue;
            }
            // 替换字段和翻译缓存实际字段保持一致：
            // src -> pre_processed_text，dst -> translated_raw_text。
            const auto collectField = [&](const QString& fieldName, const QString& before)
                {
                    QString after = before;
                    const int matches = replaceInString(after, query, replacement, regex);
                    fileMatches += matches;
                    if (matches <= 0) {
                        return;
                    }
                    detail.previews.push_back({
                        file.relativeName,
                        row,
                        sentenceIndexOf(item, row),
                        fieldName,
                        before,
                        after,
                    });
                };
            if (field == "src" || field == "all") {
                collectField("src", entrySource(item));
            }
            if (field == "dst" || field == "all") {
                collectField("dst", entryDst(item));
            }
        }
        if (fileMatches > 0) {
            detail.matches = fileMatches;
            details.push_back(detail);
            total += fileMatches;
        }
    }
    if (totalMatches) {
        *totalMatches = total;
    }
    return details;
}

int ProjectCachePage::applyReplaceToEntries(json& entries, const QString& query,
    const QString& replacement, const QString& field, const QRegularExpression* regex) const
{
    int total = 0;
    for (auto& item : entries) {
        if (!item.is_object()) {
            continue;
        }
        if (field == "src" || field == "all") {
            QString source = entrySource(item);
            const int matches = replaceInString(source, query, replacement, regex);
            if (matches > 0) {
                item["pre_processed_text"] = source.toStdString();
                total += matches;
            }
        }
        if (field == "dst" || field == "all") {
            QString dst = entryDst(item);
            const int matches = replaceInString(dst, query, replacement, regex);
            if (matches > 0) {
                item["translated_raw_text"] = dst.toStdString();
                total += matches;
            }
        }
    }
    return total;
}

void ProjectCachePage::setRegexError(ElaLineEdit* edit, QAction* errorAction,
    const QString& message) const
{
    if (!edit || !errorAction) {
        return;
    }
    errorAction->setToolTip(message);
    errorAction->setVisible(!message.isEmpty());
    edit->setToolTip(message);
}

bool ProjectCachePage::prepareRegex(const QString& query, bool enabled, bool caseInsensitive,
    QRegularExpression& regex, ElaLineEdit* edit, QAction* errorAction) const
{
    if (!enabled || query.isEmpty()) {
        setRegexError(edit, errorAction, {});
        return true;
    }

    QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
    if (caseInsensitive) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    regex = QRegularExpression(query, options);
    if (regex.isValid()) {
        setRegexError(edit, errorAction, {});
        return true;
    }

    setRegexError(edit, errorAction, tr("正则表达式无效: %1（位置 %2）")
        .arg(regex.errorString())
        .arg(regex.patternErrorOffset()));
    return false;
}

void ProjectCachePage::runGlobalSearch()
{
    if (!m_globalSearchEdit || !m_searchResultList) {
        return;
    }
    const QString query = m_globalSearchEdit->text();
    const QString field = m_globalSearchFieldKey;
    m_searchHits.clear();
    m_searchModel->clear();
    const bool regexEnabled = m_globalRegexButton && m_globalRegexButton->isChecked();
    QRegularExpression regex;
    if (!prepareRegex(query, regexEnabled, true, regex,
        m_globalSearchEdit, m_globalRegexErrorAction)) {
        m_searchStatusLabel->clear();
        if (m_searchNavButton) {
            m_searchNavButton->setText(tr("搜索"));
        }
        return;
    }
    if (query.isEmpty()) {
        m_searchStatusLabel->clear();
        if (m_searchNavButton) {
            m_searchNavButton->setText(tr("搜索"));
        }
        return;
    }

    // 搜索框 textChanged 会频繁触发，这里限制结果数，避免大缓存目录里边输入边卡死。
    constexpr int maxResults = 2000;
    const auto matches = [&](const QString& text)
        {
            return regexEnabled
                ? regex.match(text).hasMatch()
                : text.contains(query, Qt::CaseInsensitive);
        };
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
            const bool matchSrc = matches(src);
            const bool matchDst = matches(dst);
            const bool matchProblem = matches(problem);
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
    const QString replacement = m_replaceWithEdit->text();
    const bool regexEnabled = m_replaceRegexButton && m_replaceRegexButton->isChecked();
    QRegularExpression regex;
    if (!prepareRegex(query, regexEnabled, false, regex,
        m_replaceQueryEdit, m_replaceRegexErrorAction)) {
        return;
    }
    if (query.isEmpty()) {
        setInfo(tr("请输入查找内容"));
        return;
    }

    int total = 0;
    const QList<ReplaceDetail> details = collectReplaceDetails(
        query, replacement, m_replaceFieldKey,
        regexEnabled ? &regex : nullptr, &total);
    if (total <= 0) {
        setInfo(tr("无匹配内容"));
        return;
    }

    QList<const ReplacePreviewEntry*> previewEntries;
    previewEntries.reserve(total);
    QString clipboardText;
    for (const ReplaceDetail& detail : details) {
        for (const ReplacePreviewEntry& preview : detail.previews) {
            previewEntries.push_back(&preview);
        }
    }
    const QString summary = tr("共 %1 处匹配，涉及 %2 个文件").arg(total).arg(details.size());

    const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
    const QString textColor = dark ? "#f0f0f0" : "#202020";
    const QString borderColor = dark ? "#505050" : "#d8d8d8";
    const QString cardColor = dark ? "#292929" : "#fafafa";
    const QString headerColor = dark ? "#32465f" : "#e5effa";
    const QString headerTextColor = dark ? "#e8f2ff" : "#204f80";
    const QString beforeColor = dark ? "#ffb4b4" : "#9f2727";
    const QString beforeFill = dark ? "#442b2d" : "#fff0f0";
    const QString afterColor = dark ? "#a9e8bd" : "#256d3e";
    const QString afterFill = dark ? "#263c2d" : "#edf8f0";

    const auto htmlText = [](const QString& text)
        {
            return text.toHtmlEscaped().replace("\r\n", "<br>").replace('\n', "<br>").replace('\r', "<br>");
        };

    const QString htmlPrefix = QString(
        "<html><head><style>"
        "body{margin:0;color:%1;font-size:13px;}"
        "</style></head><body>")
        .arg(textColor);

    const auto cardHtml = [&](const ReplacePreviewEntry& preview)
        {
            const QString fieldLabel = preview.field == "src" ? tr("原文") : tr("译文");
            return QString(
                "<table width='100%' border='1' cellspacing='0' cellpadding='8' "
                "bgcolor='%8' style='border-color:%9;'>"
                "<tr><td bgcolor='%10'><font color='%11'><b>%1</b>&nbsp;&nbsp;#%2 | %3</font></td></tr>"
                "<tr><td bgcolor='%12'><font color='%13'><b>%4</b>&nbsp;%5</font></td></tr>"
                "<tr><td bgcolor='%14'><font color='%15'><b>%6</b>&nbsp;%7</font></td></tr>"
                "</table><br>")
                .arg(htmlText(preview.filename))
                .arg(preview.sentenceIndex)
                .arg(fieldLabel)
                .arg(tr("替换前"), htmlText(preview.before), tr("替换后"), htmlText(preview.after))
                .arg(cardColor, borderColor, headerColor, headerTextColor,
                    beforeFill, beforeColor, afterFill, afterColor);
        };

    clipboardText = summary + "\n";
    for (const ReplacePreviewEntry* preview : previewEntries) {
        const QString fieldLabel = preview->field == "src" ? tr("原文") : tr("译文");
        clipboardText += QString("\n%1  #%2  |  %3\n%4: %5\n%6: %7\n")
            .arg(preview->filename)
            .arg(preview->sentenceIndex)
            .arg(fieldLabel)
            .arg(tr("替换前"), preview->before, tr("替换后"), preview->after);
    }

    ElaContentDialog dialog(window());
    dialog.setLeftButtonText(tr("关闭"));
    dialog.setMiddleButtonText(tr("复制列表"));
    dialog.setRightButtonText(tr("执行替换"));
    dialog.setMinimumSize(640, 480);
    const QSize parentSize = window()->size();
    dialog.resize(qBound(640, parentSize.width() - 80, 920),
        qBound(480, parentSize.height() - 80, 680));

    QWidget* content = new QWidget(&dialog);
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(18, 18, 18, 12);
    layout->setSpacing(8);

    ElaText* title = new ElaText(tr("批量替换预览"), content);
    title->setTextStyle(ElaTextType::Title);
    layout->addWidget(title);
    ElaText* summaryText = new ElaText(summary, BodyFontPx, content);
    summaryText->setStyleSheet(auxiliaryTextStyle());
    layout->addWidget(summaryText);

    constexpr int pageSize = 500;
    const int pageCount = (int)qMax(1, (previewEntries.size() + pageSize - 1) / pageSize);
    QHBoxLayout* paginationLayout = new QHBoxLayout();
    paginationLayout->setContentsMargins(0, 0, 0, 0);
    paginationLayout->setSpacing(6);
    ElaSpinBox* pageSpinBox = new ElaSpinBox(content);
    pageSpinBox->setRange(1, pageCount);
    pageSpinBox->setFixedWidth(120);
    pageSpinBox->setValue(1);
    pageSpinBox->setToolTip(tr("跳转页码"));
    paginationLayout->addWidget(pageSpinBox);
    ElaText* pageLabel = new ElaText(tr("第 %1 / %2 页").arg(1).arg(pageCount), BodyFontPx, content);
    pageLabel->setStyleSheet(auxiliaryTextStyle());
    paginationLayout->addWidget(pageLabel);
    paginationLayout->addStretch();
    layout->addLayout(paginationLayout);

    QTextBrowser* previewBrowser = new QTextBrowser(content);
    previewBrowser->setOpenLinks(false);
    previewBrowser->setStyleSheet("QTextBrowser{background:transparent;border:none;}");
    QFont previewFont = previewBrowser->font();
    previewFont.setPixelSize(BodyFontPx);
    previewBrowser->setFont(previewFont);
    layout->addWidget(previewBrowser, 1);

    const auto renderPage = [=](int page)
        {
            const int first = (page - 1) * pageSize;
            const int last = (int)qMin(first + pageSize, previewEntries.size());
            QString pageHtml = htmlPrefix;
            for (int i = first; i < last; ++i) {
                pageHtml += cardHtml(*previewEntries.at(i));
            }
            pageHtml += "</body></html>";
            previewBrowser->setHtml(pageHtml);
            previewBrowser->verticalScrollBar()->setValue(0);
            pageLabel->setText(tr("第 %1 / %2 页").arg(page).arg(pageCount));
        };
    connect(pageSpinBox, QOverload<int>::of(&ElaSpinBox::valueChanged), this,
        renderPage);
    renderPage(1);

    connect(&dialog, &ElaContentDialog::middleButtonClicked, this,
        [clipboardText]() { QGuiApplication::clipboard()->setText(clipboardText); });
    dialog.setCentralWidget(content);
    if (dialog.exec() == QDialog::Accepted) {
        executeReplace();
    }
}

void ProjectCachePage::executeReplace()
{
    if (!ensureWritableAction(tr("批量替换"))) {
        return;
    }
    const QString query = m_replaceQueryEdit->text();
    const QString replacement = m_replaceWithEdit->text();
    const QString field = m_replaceFieldKey;
    const bool regexEnabled = m_replaceRegexButton && m_replaceRegexButton->isChecked();
    QRegularExpression regex;
    if (!prepareRegex(query, regexEnabled, false, regex,
        m_replaceQueryEdit, m_replaceRegexErrorAction)) {
        return;
    }
    if (query.isEmpty()) {
        setInfo(tr("请输入查找内容"));
        return;
    }

    int total = 0;
    const QList<ReplaceDetail> details = collectReplaceDetails(
        query, replacement, field, regexEnabled ? &regex : nullptr, &total);
    if (total <= 0) {
        setInfo(tr("无匹配内容"));
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
        const int matches = applyReplaceToEntries(
            entries, query, replacement, field, regexEnabled ? &regex : nullptr);
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
    setInfo(tr("已替换 %1 处，涉及 %2 个文件；保存后落盘。")
        .arg(changedMatches).arg(changedFiles));
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
