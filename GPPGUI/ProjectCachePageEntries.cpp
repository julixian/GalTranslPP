#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QVBoxLayout>

#include "ElaCheckBox.h"
#include "ElaDialog.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"

using namespace ProjectCachePagePrivate;

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

    // Local filtering only affects the visible model. The backing JSON array
    // remains in original order so selected rows can still map back to cache rows.
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
    // Editors are intentionally limited to pre_processed_text and
    // pre_translated_text; original_text stays metadata-only.
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

