#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QStandardItem>

#include "ElaDialog.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToolButton.h"

using namespace ProjectCachePagePrivate;

void ProjectCachePage::renderEntries()
{
    // 右侧概览列表是当前文件 JSON 的“视图层”：本地搜索和只看问题句
    // 只影响 QStandardItemModel，不改变 m_entries 的真实行号和顺序。
    if (!m_entryModel) {
        return;
    }
    m_renderingEntries = true;
    m_entryModel->clear();
    m_selectedEntryRows.clear();

    const QString query = m_localSearchEdit ? m_localSearchEdit->text() : QString();
    const bool regexEnabled = m_localRegexButton && m_localRegexButton->isChecked();
    QRegularExpression regex;
    if (!prepareRegex(query, regexEnabled, true, regex,
        m_localSearchEdit, m_localRegexErrorAction)) {
        m_renderingEntries = false;
        updateCurrentSummary();
        updateActionStates();
        return;
    }

    if (m_currentFile.isEmpty()) {
        m_currentFileLabel->setText(tr("未选择缓存文件"));
        m_renderingEntries = false;
        updateCurrentSummary();
        updateActionStates();
        return;
    }

    const bool onlyProblems = m_filterProblemsCheck && m_filterProblemsCheck->isChecked();
    const auto matches = [&](const QString& text)
        {
            return regexEnabled
                ? regex.match(text).hasMatch()
                : text.contains(query, Qt::CaseInsensitive);
        };

    for (int i = 0; i < (int)m_entries.size(); ++i) {
        const auto& item = m_entries[i];
        if (!item.is_object()) {
            continue;
        }
        const QString source = entrySource(item);
        const QString dst = entryDst(item);
        const QString problems = problemString(item, " | ");
        if (onlyProblems && problems.isEmpty()) {
            continue;
        }
        if (!query.isEmpty() && !matches(source) && !matches(dst) && !matches(problems)) {
            continue;
        }
        QStandardItem* itemRow = new QStandardItem(entryListText(item, i));
        itemRow->setData(i, JsonRowRole);
        itemRow->setData(sentenceIndexOf(item, i), EntryIndexRole);
        itemRow->setData(speakerString(item), EntrySpeakerRole);
        itemRow->setData(problems, EntryProblemRole);
        itemRow->setData(entryTransby(item), EntryEngineRole);
        itemRow->setData(source, EntrySourceRole);
        itemRow->setData(dst, EntryDstRole);
        itemRow->setEditable(false);
        setModelItemFont(itemRow, BodyFontPx);
        m_entryModel->appendRow(itemRow);
    }

    m_renderingEntries = false;
    if (m_currentFile.isEmpty()) {
        m_currentFileLabel->setText(tr("未选择缓存文件"));
    }
    else {
        m_currentFileLabel->setText((m_dirtyFiles.contains(m_currentFile) ? "*" : "") + m_currentFile);
    }
    updateCurrentSummary();
    updateActionStates();
}

void ProjectCachePage::syncSelectedEntryRows()
{
    // UI 里选中的是可见模型行，真正执行删除/编辑时必须先还原成 JSON 行号。
    m_selectedEntryRows.clear();
    if (!m_entryList || !m_entryList->selectionModel()) {
        return;
    }
    const QModelIndexList selected = m_entryList->selectionModel()->selectedRows();
    for (const QModelIndex& index : selected) {
        const int row = index.data(JsonRowRole).toInt();
        if (row >= 0) {
            m_selectedEntryRows.insert(row);
        }
    }
}

void ProjectCachePage::updateEntryListItem(int row)
{
    if (!m_entryModel) {
        return;
    }
    for (int modelRow = 0; modelRow < m_entryModel->rowCount(); ++modelRow) {
        QStandardItem* item = m_entryModel->item(modelRow);
        if (item && item->data(JsonRowRole).toInt() == row) {
            item->setText(entryListText(m_entries[row], row));
            item->setData(sentenceIndexOf(m_entries[row], row), EntryIndexRole);
            item->setData(speakerString(m_entries[row]), EntrySpeakerRole);
            item->setData(problemString(m_entries[row], " | "), EntryProblemRole);
            item->setData(entryTransby(m_entries[row]), EntryEngineRole);
            item->setData(entrySource(m_entries[row]), EntrySourceRole);
            item->setData(entryDst(m_entries[row]), EntryDstRole);
            return;
        }
    }
}

void ProjectCachePage::updateEntryField(int row, const char* key, const QString& value)
{
    if (isProjectRunning() || m_currentFile.isEmpty()) {
        return;
    }
    if (row < 0 || row >= (int)m_entries.size() || !m_entries[row].is_object()) {
        return;
    }
    if (jsonString(m_entries[row], key) == value) {
        return;
    }
    // 编辑窗口只允许写回 pre_processed_text / translated_raw_text；
    // original_text 在这个页面始终当作元信息。
    m_entries[row][key] = value.toStdString();
    markDirty(m_currentFile);
    updateEntryListItem(row);
    updateCurrentSummary();
}

void ProjectCachePage::openEntryEditor(int row)
{
    if (m_currentFile.isEmpty() || row < 0 || row >= (int)m_entries.size() || !m_entries[row].is_object()) {
        return;
    }

    // 弹窗内使用当前条目的快照展示；保存时通过 row 写回 m_entries。
    const auto item = m_entries[row];
    const bool writable = !isProjectRunning();

    ElaDialog dialog(this);
    const QString speaker = speakerString(item);
    dialog.setWindowTitle(speaker.isEmpty()
        ? QString("#%1").arg(sentenceIndexOf(item, row))
        : QString("#%1  %2").arg(sentenceIndexOf(item, row)).arg(speaker));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    dialog.resize(860, 540);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(16, 0, 16, 14);

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

    ElaPlainTextEdit* originalEdit = addEditor(tr("original_text（元信息，只读）"), entryOriginal(item), true, 60);
    Q_UNUSED(originalEdit);
    ElaPlainTextEdit* sourceEdit = addEditor(tr("pre_processed_text（原文，可编辑）"), entrySource(item), !writable, 80);
    ElaPlainTextEdit* dstEdit = addEditor(tr("translated_raw_text（译文，可编辑）"), entryDst(item), !writable, 80);
    ElaPlainTextEdit* problemsEdit = addEditor(tr("problems（只读）"), problemString(item), true, 60);
    Q_UNUSED(problemsEdit);
    ElaPlainTextEdit* previewEdit = addEditor(tr("translated_view_text（只读）"), entryPreview(item), true, 60);
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
            updateEntryField(row, "pre_processed_text", sourceEdit->toPlainText());
            updateEntryField(row, "translated_raw_text", dstEdit->toPlainText());
            dialog.accept();
        });
    buttonLayout->addWidget(saveButton);
    mainLayout->addLayout(buttonLayout);

    dialog.move(window()->frameGeometry().center() - dialog.rect().center());
    dialog.exec();
}

void ProjectCachePage::deleteEntryRows(QList<int> rows)
{
    if (m_currentFile.isEmpty() || rows.isEmpty()) {
        return;
    }
    std::ranges::sort(rows);
    const auto duplicatedRows = std::ranges::unique(rows);
    rows.erase(duplicatedRows.begin(), duplicatedRows.end());
    std::ranges::sort(rows, std::greater<>());

    int deleted = 0;
    // 从后往前删，避免前面的 erase 改变后续行号。
    for (int row : rows) {
        if (row >= 0 && row < (int)m_entries.size()) {
            m_entries.erase(m_entries.begin() + row);
            ++deleted;
        }
    }
    if (deleted <= 0) {
        return;
    }
    m_selectedEntryRows.clear();
    markDirty(m_currentFile);
    renderEntries();
    runGlobalSearch();
    if (m_problemsLoaded) {
        loadProblems();
    }
    setInfo(tr("已删除 %1 个条目，保存后生效").arg(QString::number(deleted)));
}

QString ProjectCachePage::jsonString(const json& object, const char* key)
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

QString ProjectCachePage::speakerString(const json& object)
{
    if (object.contains("name_translated")) {
        const QString preview = jsonString(object, "name_translated");
        if (!preview.isEmpty()) {
            return preview;
        }
    }
    if (object.contains("name")) {
        return jsonString(object, "name");
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

QString ProjectCachePage::problemString(const json& object, const QString& separator)
{
    if (!object.contains("problems") || !object["problems"].is_array()) {
        return {};
    }
    QStringList problems;
    for (const auto& problem : object["problems"]) {
        if (problem.is_string()) {
            const QString text = QString::fromStdString(problem.get<std::string>());
            if (!text.isEmpty() && !problems.contains(text)) {
                problems.push_back(text);
            }
        }
    }
    return problems.join(separator);
}

QString ProjectCachePage::entrySource(const json& object)
{
    return jsonString(object, "pre_processed_text");
}

QString ProjectCachePage::entryDst(const json& object)
{
    return jsonString(object, "translated_raw_text");
}

QString ProjectCachePage::entryOriginal(const json& object)
{
    return jsonString(object, "original_text");
}

QString ProjectCachePage::entryPreview(const json& object)
{
    return jsonString(object, "translated_view_text");
}

QString ProjectCachePage::entryTransby(const json& object)
{
    return jsonString(object, "translated_by");
}

QString ProjectCachePage::truncateForList(const QString& text, int maxChars)
{
    return compactPreview(text, maxChars);
}

QString ProjectCachePage::entryListText(const json& object, int row) const
{
    QStringList header;
    header << QString("#%1").arg(sentenceIndexOf(object, row));
    const QString speaker = speakerString(object);
    if (!speaker.isEmpty()) {
        header << speaker;
    }
    const QString problems = problemString(object, " | ");
    if (!problems.isEmpty()) {
        header << tr("问题: %1").arg(compactPreview(problems, 120));
    }
    const QString engine = entryTransby(object);
    if (!engine.isEmpty()) {
        header << engine;
    }
    QStringList lines;
    lines << header.join("  ·  ");
    lines << tr("原文: %1").arg(compactPreview(entrySource(object), 220));
    lines << tr("译文: %1").arg(compactPreview(entryDst(object), 220));
    return lines.join("\n");
}

