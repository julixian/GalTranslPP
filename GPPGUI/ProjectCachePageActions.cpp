#include "ProjectCachePage.h"
#include "ProjectCachePage_p.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialog>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "ElaContentDialog.h"
#include "ElaIconButton.h"
#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

using namespace ProjectCachePagePrivate;

void ProjectCachePage::markDirty(const QString& filename)
{
    if (filename.isEmpty()) {
        return;
    }
    const bool wasClean = !m_dirtyFiles.contains(filename);
    // 所有编辑都先进入内存缓存和脏文件集合；只有保存按钮才真正写回磁盘。
    m_dirtyFiles.insert(filename);
    m_loadedEntriesByFile[filename] = m_entries;
    m_currentFileLabel->setText("*" + filename);
    if (wasClean) {
        renderFileList();
    }
    updateActionStates();
}

void ProjectCachePage::setInfo(const QString& message)
{
    if (!message.isEmpty()) {
        ElaMessageBar::success(ElaMessageBarType::TopRight, tr("完成"), message, 2500);
    }
}

void ProjectCachePage::setError(const QString& message)
{
    if (!message.isEmpty()) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("失败"), message, 4000);
    }
}

void ProjectCachePage::updateCurrentSummary()
{
    if (m_currentFile.isEmpty()) {
        m_currentSummaryLabel->clear();
        return;
    }
    int translated = 0;
    int problemCount = 0;
    // 摘要按当前内存 JSON 计算，因此未保存的编辑也会立即反映到右上角。
    for (const auto& item : m_entries) {
        if (!item.is_object()) {
            continue;
        }
        if (!entryDst(item).isEmpty()) {
            ++translated;
        }
        if (!problemString(item).isEmpty()) {
            ++problemCount;
        }
    }
    m_currentSummaryLabel->setText(tr("%1 句 · %2 已翻译 · %3 有问题 · %4 已选择")
        .arg((int)m_entries.size()).arg(translated).arg(problemCount).arg(m_selectedEntryRows.size()));
}

void ProjectCachePage::updateActionStates()
{
    // 按“是否有当前文件 / 是否运行中 / 是否选中条目”集中刷新按钮状态，
    // 避免各个信号槽里散落一堆 enabled 判断。
    const bool hasFile = !m_currentFile.isEmpty();
    const bool writable = !isProjectRunning();
    if (m_saveButton) {
        m_saveButton->setEnabled(writable && hasFile && m_dirtyFiles.contains(m_currentFile));
    }
    if (m_saveAllButton) {
        m_saveAllButton->setEnabled(writable && !m_dirtyFiles.isEmpty());
    }
    if (m_deleteEntriesButton) {
        const int selectedCount = m_selectedEntryRows.size();
        m_deleteEntriesButton->setText(selectedCount > 0 ? tr("删除选中条目 (%1)").arg(selectedCount) : tr("删除选中条目"));
        m_deleteEntriesButton->setEnabled(writable && hasFile && selectedCount > 0);
    }
    if (m_editEntryButton) {
        m_editEntryButton->setEnabled(hasFile && currentJsonRow() >= 0);
    }
    if (m_deleteFilesButton) {
        m_deleteFilesButton->setEnabled(writable && m_fileList && m_fileList->selectionModel() && !m_fileList->selectionModel()->selectedRows().empty());
    }
    if (m_replaceExecuteButton) {
        m_replaceExecuteButton->setEnabled(writable);
    }
}

void ProjectCachePage::setSidebarPage(int index)
{
    if (!m_sidebarStack || !m_sidebarButtonGroup) {
        return;
    }
    m_sidebarStack->setCurrentIndex(index);
    if (QAbstractButton* button = m_sidebarButtonGroup->button(index)) {
        button->setChecked(true);
    }
    tuneNavButton(m_filesNavButton, index == 0);
    tuneNavButton(m_searchNavButton, index == 1);
    tuneNavButton(m_problemsNavButton, index == 2);
    // 问题聚合比较重，首次切到问题页时才做。
    if (index == 2 && !m_problemsLoaded) {
        loadProblems();
    }
}

void ProjectCachePage::refreshThemeStyles()
{
    if (m_cacheDirLabel) {
        m_cacheDirLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (m_replacePreviewLabel) {
        m_replacePreviewLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (m_searchStatusLabel) {
        m_searchStatusLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (m_currentSummaryLabel) {
        m_currentSummaryLabel->setStyleSheet(auxiliaryTextStyle());
    }
    if (m_mainSplitter) {
        m_mainSplitter->setStyleSheet(splitterStyle());
    }
    if (m_sidebarStack && m_sidebarButtonGroup) {
        setSidebarPage(m_sidebarStack->currentIndex());
    }
}

void ProjectCachePage::setReplacePanelVisible(bool visible)
{
    if (m_replacePanel) {
        m_replacePanel->setVisible(visible);
    }
    if (m_replaceToggleButton) {
        if (m_replaceToggleButton->isChecked() != visible) {
            QSignalBlocker blocker(m_replaceToggleButton);
            m_replaceToggleButton->setChecked(visible);
        }
        m_replaceToggleButton->setText(visible ? tr("收起批量替换") : tr("展开批量替换"));
    }
}

bool ProjectCachePage::isProjectRunning() const
{
    return toml::find_or(m_projectConfig, "GUIConfig", "isRunning", false);
}

bool ProjectCachePage::ensureWritableAction(const QString& actionName) const
{
    if (isProjectRunning()) {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, actionName, tr("项目正在运行中，只允许查看缓存。"), 3000);
        return false;
    }
    return true;
}

bool ProjectCachePage::confirmAction(const QString& title, const QString& message)
{
    QWidget* dialogParent = window();
    ElaContentDialog dialog(dialogParent ? dialogParent : this);
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

