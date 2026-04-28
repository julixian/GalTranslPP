#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialog>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ElaContentDialog.h"
#include "ElaIconButton.h"
#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

using namespace ProjectCachePagePrivate;

void ProjectCachePage::_markDirty(const QString& filename)
{
    if (filename.isEmpty()) {
        return;
    }
    const bool wasClean = !_dirtyFiles.contains(filename);
    // Dirty files are tracked per relative cache path. Saving can flush just the
    // current file or all modified files without re-reading disk state.
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
    if (!message.isEmpty()) {
        ElaMessageBar::success(ElaMessageBarType::TopRight, tr("完成"), message, 2500);
    }
}

void ProjectCachePage::_setError(const QString& message)
{
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
    if (index == 2 && !_problemsLoaded) {
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
    if (_mainSplitter) {
        _mainSplitter->setStyleSheet(splitterStyle());
    }
    if (_sidebarStack && _sidebarButtonGroup) {
        _setSidebarPage(_sidebarStack->currentIndex());
    }
}

void ProjectCachePage::_setReplacePanelVisible(bool visible)
{
    if (_replacePanel) {
        _replacePanel->setVisible(visible);
    }
    if (_replaceToggleButton) {
        if (_replaceToggleButton->isChecked() != visible) {
            QSignalBlocker blocker(_replaceToggleButton);
            _replaceToggleButton->setChecked(visible);
        }
        _replaceToggleButton->setText(visible ? tr("收起批量替换") : tr("展开批量替换"));
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

bool ProjectCachePage::_confirmAction(const QString& title, const QString& message)
{
    // Keep confirmations on ElaContentDialog so the cache page matches the rest
    // of the Ela-themed settings UI.
    ElaContentDialog dialog(this);
    dialog.setLeftButtonText(tr("否"));
    dialog.setMiddleButtonText(tr("思考人生"));
    dialog.setRightButtonText(tr("是"));

    QWidget* widget = new QWidget(&dialog);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(15, 25, 15, 10);

    ElaText* titleText = new ElaText(title, widget);
    titleText->setTextStyle(ElaTextType::Title);
    titleText->setWordWrap(false);
    layout->addWidget(titleText);
    layout->addSpacing(2);

    ElaText* messageText = new ElaText(message, 16, widget);
    messageText->setTextStyle(ElaTextType::Body);
    messageText->setWordWrap(true);
    layout->addWidget(messageText);
    layout->addStretch();

    dialog.setCentralWidget(widget);
    connect(&dialog, &ElaContentDialog::middleButtonClicked, &dialog, &ElaContentDialog::close);
    return dialog.exec() == QDialog::Accepted;
}

