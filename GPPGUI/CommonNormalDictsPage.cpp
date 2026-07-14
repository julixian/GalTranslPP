#include "CommonNormalDictsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QItemSelectionModel>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaIconButton.h"
#include "ElaToolButton.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaToggleSwitch.h"
#include "ElaPlainTextEdit.h"
#include "ElaTabWidget.h"
#include "ElaInputDialog.h"
#include "ElaContentDialog.h"
#include "DictionaryEntryDialog.h"
#include "DictionaryReader.h"
#include "DictionarySearchBar.h"
#include "ReorderableTableView.h"
#include "TreeSitterHighlighter.h"

import Tool;

CommonNormalDictsPage::CommonNormalDictsPage(const std::string& mode, toml::ordered_value& globalConfig, QWidget* parent) :
	BasePage(parent), m_globalConfig(globalConfig)
{
	setWindowTitle(tr("默认译前字典设置"));
	setTitleVisible(false);

	m_mode = mode;
	if (m_mode == "pre") {
		m_modeConfigKey = "commonPreDicts";
		m_modeDictDir = defaultPreDictPath;
	}
	else if (m_mode == "post") {
		m_modeConfigKey = "commonPostDicts";
		m_modeDictDir = defaultPostDictPath;
	}
	else {
		QMessageBox::critical(window(), tr("内部错误"), tr("未知通用字典模式"), QMessageBox::Ok);
		exit(1);
	}

	setupUi();
}

void CommonNormalDictsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	QHBoxLayout* mainButtonLayout = new QHBoxLayout(mainWidget);
	ElaText* dictNameLabel = new ElaText(mainWidget);
	dictNameLabel->setTextPixelSize(18);
	QString dictNameText;
	if (m_mode == "pre") {
		dictNameText = tr("通用译前字典");
	}
	else if (m_mode == "post") {
		dictNameText = tr("通用译后字典");
	}
	dictNameLabel->setText(dictNameText);
	ElaToolButton* importButton = new ElaToolButton(mainWidget);
	importButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	importButton->setElaIcon(ElaIconType::ArrowDownFromLine);
	importButton->setText(tr("导入字典页"));
	ElaToolButton* addNewTabButton = new ElaToolButton(mainWidget);
	addNewTabButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	addNewTabButton->setElaIcon(ElaIconType::Plus);
	addNewTabButton->setText(tr("添加新字典页"));
	ElaToolButton* saveAllButton = new ElaToolButton(mainWidget);
	saveAllButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	saveAllButton->setElaIcon(ElaIconType::CheckDouble);
	saveAllButton->setText(tr("保存所有页"));
	mainButtonLayout->addSpacing(10);
	mainButtonLayout->addWidget(dictNameLabel);
	mainButtonLayout->addStretch();
	mainButtonLayout->addWidget(importButton);
	mainButtonLayout->addWidget(addNewTabButton);
	mainButtonLayout->addWidget(saveAllButton);
	mainLayout->addLayout(mainButtonLayout);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);

	auto createNormalTab = [=](const fs::path& orgDictPath) -> QWidget*
		{
			const fs::path dictPath = m_modeDictDir / orgDictPath.filename().replace_extension(L".toml");
			const std::string dictName = wide2Ascii(orgDictPath.stem().wstring());
			NormalTabEntry normalTabEntry;

			QWidget* pageMainWidget = new QWidget(tabWidget);
			QVBoxLayout* pageMainLayout = new QVBoxLayout(pageMainWidget);
			pageMainLayout->setContentsMargins(0, 0, 0, 0);

			QHBoxLayout* pageButtonLayout = new QHBoxLayout(pageMainWidget);
			ElaToolButton* plainTextModeButton = new ElaToolButton(pageMainWidget);
			plainTextModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			plainTextModeButton->setElaIcon(ElaIconType::Text);
			plainTextModeButton->setText(tr("纯文本"));
			ElaToolButton* tableModeButton = new ElaToolButton(pageMainWidget);
			tableModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			tableModeButton->setElaIcon(ElaIconType::Table);
			tableModeButton->setText(tr("表模式"));
			QWidget* defaultOnWidget = new QWidget(pageMainWidget);
			QHBoxLayout* defaultOnLayout = new QHBoxLayout(defaultOnWidget);
			defaultOnLayout->setContentsMargins(4, 0, 4, 0);
			defaultOnLayout->setSpacing(6);
			ElaText* defaultOnLabel = new ElaText(tr("默认启用"), 14, defaultOnWidget);
			defaultOnLabel->setWordWrap(false);
			ElaToggleSwitch* defaultOnSwitch = new ElaToggleSwitch(defaultOnWidget);
			defaultOnLayout->addWidget(defaultOnLabel);
			defaultOnLayout->addWidget(defaultOnSwitch);
			ElaIconButton* saveButton = new ElaIconButton(ElaIconType::Check, pageMainWidget);
			saveButton->setFixedWidth(30);
			ElaToolTip* saveButtonToolTip = new ElaToolTip(saveButton);
			saveButtonToolTip->setToolTip(tr("保存当前页"));
			ElaIconButton* removeTabButton = new ElaIconButton(ElaIconType::Trash, pageMainWidget);
			removeTabButton->setFixedWidth(30);
			ElaToolTip* removeTabButtonToolTip = new ElaToolTip(removeTabButton);
			removeTabButtonToolTip->setToolTip(tr("删除当前页"));
			ElaIconButton* renameTabButton = new ElaIconButton(ElaIconType::ArrowsRetweet, pageMainWidget);
			renameTabButton->setFixedWidth(30);
			ElaToolTip* renameTabButtonToolTip = new ElaToolTip(renameTabButton);
			renameTabButtonToolTip->setToolTip(tr("重命名当前页"));
			ElaIconButton* withdrawButton = new ElaIconButton(ElaIconType::ArrowLeft, pageMainWidget);
			withdrawButton->setFixedWidth(30);
			ElaToolTip* withdrawButtonToolTip = new ElaToolTip(withdrawButton);
			withdrawButtonToolTip->setToolTip(tr("撤回删除行"));
			withdrawButton->setEnabled(false);
			ElaIconButton* refreshButton = new ElaIconButton(ElaIconType::ArrowRotateRight, pageMainWidget);
			refreshButton->setFixedWidth(30);
			ElaToolTip* refreshButtonToolTip = new ElaToolTip(refreshButton);
			refreshButtonToolTip->setToolTip(tr("刷新当前页"));
			ElaIconButton* addDictButton = new ElaIconButton(ElaIconType::Plus, pageMainWidget);
			addDictButton->setFixedWidth(30);
			ElaToolTip* addDictButtonToolTip = new ElaToolTip(addDictButton);
			addDictButtonToolTip->setToolTip(tr("添加词条"));
			ElaIconButton* removeDictButton = new ElaIconButton(ElaIconType::Minus, pageMainWidget);
			removeDictButton->setFixedWidth(30);
			ElaToolTip* removeDictButtonToolTip = new ElaToolTip(removeDictButton);
			removeDictButtonToolTip->setToolTip(tr("删除词条"));
			ElaIconButton* editEntryButton = new ElaIconButton(ElaIconType::PenToSquare, pageMainWidget);
			editEntryButton->setFixedWidth(30);
			ElaToolTip* editEntryButtonToolTip = new ElaToolTip(editEntryButton);
			editEntryButtonToolTip->setToolTip(tr("编辑词条"));
			pageButtonLayout->addWidget(plainTextModeButton);
			pageButtonLayout->addWidget(tableModeButton);
			pageButtonLayout->addWidget(defaultOnWidget);
			pageButtonLayout->addStretch();
			pageButtonLayout->addWidget(saveButton);
			pageButtonLayout->addWidget(removeTabButton);
			pageButtonLayout->addWidget(renameTabButton);
			pageButtonLayout->addWidget(withdrawButton);
			pageButtonLayout->addWidget(refreshButton);
			pageButtonLayout->addWidget(addDictButton);
			pageButtonLayout->addWidget(editEntryButton);
			pageButtonLayout->addWidget(removeDictButton);
			pageMainLayout->addLayout(pageButtonLayout);

			QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
			ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
			QFont plainTextFont = plainTextEdit->font();
			plainTextFont.setPixelSize(15);
			plainTextEdit->setFont(plainTextFont);
			installTreeSitterHighlighter(plainTextEdit->document(), SyntaxLanguage::Toml);
			plainTextEdit->setPlainText(DictionaryReader::readDictStr(orgDictPath));
			stackedWidget->addWidget(plainTextEdit);
			ReorderableTableView* tableView = new ReorderableTableView(stackedWidget);
			QFont tableHeaderFont = tableView->horizontalHeader()->font();
			tableHeaderFont.setPixelSize(16);
			tableView->horizontalHeader()->setFont(tableHeaderFont);
			tableView->verticalHeader()->setHidden(true);
			tableView->setAlternatingRowColors(true);
			tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
			tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
			NormalDictModel* model = new NormalDictModel(tableView);
			QList<NormalDictEntry> normalData = DictionaryReader::readNormalDict(orgDictPath);
			model->loadData(normalData);
			tableView->setModel(model);
			stackedWidget->addWidget(tableView);
			stackedWidget->setCurrentIndex(toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "openMode", 1));
			tableView->setColumnWidth(NormalDictModel::Original, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "0", 285));
			tableView->setColumnWidth(NormalDictModel::Translation, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "1", 195));
			tableView->setColumnWidth(NormalDictModel::Conditions, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "2", 350));
			tableView->setColumnWidth(NormalDictModel::IsReg, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "3", 82));
			tableView->setColumnWidth(NormalDictModel::Priority, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "4", 65));
			DictionarySearchBar* searchBar = new DictionarySearchBar(tableView, tr("条件"), pageMainWidget);
			searchBar->setVisible(stackedWidget->currentIndex() == 1);
			pageButtonLayout->insertWidget(3, searchBar);
			pageMainLayout->addWidget(stackedWidget, 1);

			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeDictButton->setEnabled(stackedWidget->currentIndex() == 1
				&& tableView->selectionModel()->hasSelection());
			editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
				&& tableView->selectionModel()->hasSelection() && tableView->currentIndex().isValid());
			defaultOnSwitch->setIsToggled(toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "defaultOn", true));
			insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".defaultOn", defaultOnSwitch->getIsToggled());

			connect(plainTextModeButton, &ElaToolButton::clicked, this, [=]()
				{
					stackedWidget->setCurrentIndex(0);
					plainTextModeButton->setEnabled(false);
					tableModeButton->setEnabled(true);
					addDictButton->setEnabled(false);
					removeDictButton->setEnabled(false);
					editEntryButton->setEnabled(false);
					withdrawButton->setEnabled(false);
					searchBar->hide();
				});

			connect(tableModeButton, &ElaToolButton::clicked, this, [=]()
				{
					stackedWidget->setCurrentIndex(1);
					plainTextModeButton->setEnabled(true);
					tableModeButton->setEnabled(false);
					addDictButton->setEnabled(true);
					removeDictButton->setEnabled(tableView->selectionModel()->hasSelection());
					editEntryButton->setEnabled(tableView->selectionModel()->hasSelection()
						&& tableView->currentIndex().isValid());
					withdrawButton->setEnabled(!normalTabEntry.withdrawList->empty());
					searchBar->show();
				});

			connect(defaultOnSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
				{
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName
						+ ".defaultOn", checked);
				});

			auto saveFunc = [=](bool forceSaveInTableModeToInit) -> bool // param 导入时先强制保存一下来给纯文本模式作初始化
				{
					const auto it = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_normalTabEntries.end()) {
						return false;
					}

					auto writeDictFile = [&]() -> bool
						{
							try {
								if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
									atomicOutputFile(it->dictPath, plainTextEdit->toPlainText().toStdString());
								}
								else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
									toml::ordered_value dictArr = toml::array{};
									for (const NormalDictEntry& entry : model->getEntriesRef()) {
										toml::ordered_table dictTbl;
										dictTbl.insert({ "org", entry.original.toStdString() });
										dictTbl.insert({ "rep", entry.translation.toStdString() });
										if (!entry.conditions.isEmpty()) {
											toml::ordered_value conditions = toml::array{};
											for (const NormalCondition& condition : entry.conditions) {
												toml::ordered_table conditionTbl;
											conditionTbl.insert({ "conditionReg", condition.pattern.toStdString() });
											conditionTbl.insert({ "conditionTarget", serializeNormalConditionTarget(condition).toStdString() });
											conditions.push_back(std::move(conditionTbl));
											}
											dictTbl.insert({ "conditions", std::move(conditions) });
										}
										dictTbl.insert({ "isReg", entry.isReg });
							dictTbl.insert({ "priority", entry.priority });
										dictArr.push_back(std::move(dictTbl));
									}
									atomicOutputFile(it->dictPath,
										toml::format(toml::ordered_value{ toml::ordered_table{{ "normalDict", std::move(dictArr) }} }));
								}
							}
							catch (...) {
								ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("保存失败"),
									tr("无法打开字典: %1").arg(QString::fromStdWString(dictPath.wstring())), 3000);
								return false;
							}
							return true;
						};
					if (!writeDictFile()) {
						return false;
					}

					const std::string tmpDictName = wide2Ascii(it->dictPath.stem().wstring());

					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						const QList<NormalDictEntry> newDictEntries = DictionaryReader::readNormalDict(it->dictPath);
						model->loadData(newDictEntries);
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						plainTextEdit->setPlainText(DictionaryReader::readDictStr(it->dictPath));
					}

					auto& dictNamesArr = m_globalConfig[m_modeConfigKey]["dictNames"];
					if (!dictNamesArr.is_array()) {
						dictNamesArr = toml::array{ tmpDictName };
					}
					else {
						if (
							!std::ranges::any_of(dictNamesArr.as_array(), [=](const toml::ordered_value& name)
							{
								return name.is_string() && name.as_string() == tmpDictName;
							})
							) {
							dictNamesArr.push_back(tmpDictName);
						}
					}

					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".openMode", stackedWidget->currentIndex());
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.0", tableView->columnWidth(NormalDictModel::Original));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.1", tableView->columnWidth(NormalDictModel::Translation));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.2", tableView->columnWidth(NormalDictModel::Conditions));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.3", tableView->columnWidth(NormalDictModel::IsReg));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.4", tableView->columnWidth(NormalDictModel::Priority));
					return true;
				};
			normalTabEntry.saveFunc = saveFunc;

			connect(saveButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto it = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_normalTabEntries.end()) {
						return;
					}
					if (it->saveFunc(false)) {
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"),
							tr("字典 %1 已保存").arg(QString::fromStdWString(it->dictPath.stem().wstring())), 3000);
					}
				});
			auto openEntryDialog = [=](const NormalDictEntry& entry, NormalDictEntry& result) -> bool
				{
					DictionaryEntryDialog dialog(entry, window());
					if (dialog.exec() != QDialog::Accepted) {
						return false;
					}
					result = dialog.getNormalEntry();
					return true;
				};

			auto editEntry = [=](int row)
				{
					const QList<NormalDictEntry>& entries = model->getEntriesRef();
					if (row < 0 || row >= entries.size()) {
						return;
					}
					NormalDictEntry editedEntry;
					if (openEntryDialog(entries.at(row), editedEntry)) {
						model->setEntry(row, std::move(editedEntry));
					}
				};

			connect(addDictButton, &ElaPushButton::clicked, this, [=]()
				{
					NormalDictEntry newEntry;
					if (!openEntryDialog(NormalDictEntry{}, newEntry)) {
						return;
					}
					const QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
					const int insertRow = selectedRows.isEmpty() ? model->rowCount() : selectedRows.first().row();
					model->insertRow(insertRow, std::move(newEntry));
					tableView->setCurrentIndex(model->index(insertRow, 0));
					tableView->selectRow(insertRow);
				});
			connect(editEntryButton, &ElaIconButton::clicked, this, [=]()
				{
					editEntry(tableView->currentIndex().row());
				});
			connect(tableView, &QAbstractItemView::doubleClicked, this, [=](const QModelIndex& index)
				{
					editEntry(index.row());
				});
			connect(tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
				[=](const QModelIndex& current)
				{
					editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
						&& tableView->selectionModel()->hasSelection() && current.isValid());
				});
			connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
				[=]()
				{
					removeDictButton->setEnabled(stackedWidget->currentIndex() == 1
						&& tableView->selectionModel()->hasSelection());
					editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
						&& tableView->selectionModel()->hasSelection() && tableView->currentIndex().isValid());
				});
			connect(removeDictButton, &ElaPushButton::clicked, this, [=]()
				{
					QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
					if (selectedRows.isEmpty()) {
						return;
					}

					DictionaryEntryDeleteDialog confirmDialog(selectedRows.size(), window());
					if (confirmDialog.exec() != QDialog::Accepted) {
						return;
					}

					const QList<NormalDictEntry>& entries = model->getEntries();
					std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
						{
							return a.row() > b.row();
						});
					for (const auto& index : selectedRows) {
						if (normalTabEntry.withdrawList->size() > 100) {
							normalTabEntry.withdrawList->pop_front();
						}
						normalTabEntry.withdrawList->push_back(entries[index.row()]);
						model->removeRow(index.row());
					}
					if (!selectedRows.isEmpty()) {
						withdrawButton->setEnabled(true);
					}
				});
			connect(withdrawButton, &ElaPushButton::clicked, this, [=]()
				{
					if (normalTabEntry.withdrawList->empty()) {
						return;
					}
					const NormalDictEntry entry = normalTabEntry.withdrawList->back();
					normalTabEntry.withdrawList->pop_back();
					model->insertRow(0, entry);
					if (normalTabEntry.withdrawList->empty()) {
						withdrawButton->setEnabled(false);
					}
				});
			connect(refreshButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto it = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_normalTabEntries.end()) {
						return;
					}
					plainTextEdit->setPlainText(DictionaryReader::readDictStr(it->dictPath));
					model->loadData(DictionaryReader::readNormalDict(it->dictPath));
					ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"),
						tr("字典 %1 已刷新").arg(QString::fromStdWString(it->dictPath.stem().wstring())), 3000);
				});
			connect(renameTabButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto normalTabEntryIt = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (normalTabEntryIt == m_normalTabEntries.end()) {
						return;
					}

					QString newDictName;
					ElaInputDialog inputDialog(tr("请输入新名称"), tr("重命名字典"), newDictName, window());
					if (inputDialog.exec() != QDialog::Accepted) {
						return;
					}

					if (newDictName.isEmpty() || newDictName.contains('/') || newDictName.contains('\\') || newDictName.contains('.')) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft,
							tr("重命名失败"), tr("字典名称不能为空，且不能包含点号、斜杠或反斜杠！"), 3000);
						return;
					}

					const bool hasSameNameTab = std::ranges::any_of(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget != pageMainWidget && entry.dictPath.stem().wstring() == newDictName.toStdWString();
					});
					if (hasSameNameTab || newDictName == "项目译前字典" || newDictName == "项目译后字典") {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"),
							tr("字典 %1 已存在").arg(newDictName), 3000);
						return;
					}

					const fs::path oldDictPath = normalTabEntryIt->dictPath;
					const std::string oldDictName = wide2Ascii(oldDictPath.stem().wstring());
					const fs::path newDictPath = m_modeDictDir / (newDictName.toStdWString() + L".toml");
					try {
						if (fs::exists(oldDictPath)) {
							try {
								fs::rename(oldDictPath, newDictPath);
							}
							catch(...) { }
						}
						normalTabEntryIt->dictPath = newDictPath;
						auto& dictNames = m_globalConfig[m_modeConfigKey]["dictNames"];
						if (dictNames.is_array()) {
							const auto dictNameIt = std::ranges::find_if(dictNames.as_array(), [=](const auto& elem)
								{
									return elem.is_string() && elem.as_string() == oldDictName;
								});
							if (dictNameIt != dictNames.as_array().end()) {
								*dictNameIt = newDictName.toStdString();
							}
						}
						else {
							dictNames = toml::array{};
						}
						tabWidget->setTabText(tabWidget->indexOf(pageMainWidget), newDictName);
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("重命名成功"),
							tr("字典 %1 已重命名为 %2")
							.arg(QString::fromStdWString(oldDictPath.stem().wstring()))
							.arg(newDictName), 3000);
					}
					catch (...) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("重命名失败"),
							tr("字典 %1 重命名失败").arg(QString::fromStdWString(oldDictPath.stem().wstring())), 3000);
						return;
					}
				});
			connect(removeTabButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto normalTabEntryIt = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (normalTabEntryIt == m_normalTabEntries.end()) {
						return;
					}

					std::string tmpDictName = wide2Ascii(normalTabEntryIt->dictPath.stem().wstring());

					// 删除提示框
					ElaContentDialog helpDialog(window());

					helpDialog.setRightButtonText(tr("是"));
					helpDialog.setMiddleButtonText(tr("思考人生"));
					helpDialog.setLeftButtonText(tr("否"));

					QWidget* widget = new QWidget(&helpDialog);
					QVBoxLayout* layout = new QVBoxLayout(widget);
					layout->setContentsMargins(15, 25, 15, 10);
					ElaText* confirmText = new ElaText(tr("你确定要删除 %1 吗？").arg(QString::fromStdString(tmpDictName)), widget);
					confirmText->setTextStyle(ElaTextType::Title);
					confirmText->setWordWrap(false);
					layout->addWidget(confirmText);
					layout->addSpacing(2);
					ElaText* subTitle = new ElaText(tr("将永久删除该字典文件，如有需要请先备份！"), 16, widget);
					subTitle->setTextStyle(ElaTextType::Body);
					layout->addWidget(subTitle);
					layout->addStretch();
					helpDialog.setCentralWidget(widget);

					if (helpDialog.exec() == QDialog::Accepted) {
						pageMainWidget->deleteLater();
						tabWidget->removeTab(tabWidget->indexOf(pageMainWidget));
						saveAllButton->setEnabled(tabWidget->count() > 0);
						try {
							fs::remove(normalTabEntryIt->dictPath);
						}
						catch (...) {}
						m_normalTabEntries.erase(normalTabEntryIt);
						auto& dictNames = m_globalConfig[m_modeConfigKey]["dictNames"];
						if (dictNames.is_array()) {
							auto dictNameIt = std::ranges::find_if(dictNames.as_array(), [=](const auto& elem)
								{
									return elem.is_string() && elem.as_string() == tmpDictName;
								});
							if (dictNameIt != dictNames.as_array().end()) {
								dictNames.as_array().erase(dictNameIt);
							}
						}
						else {
							dictNames = toml::array{};
						}
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("删除成功"),
							tr("字典 %1 已从字典管理和磁盘中移除！").arg(QString::fromStdString(tmpDictName)), 3000);
					}
				});

			normalTabEntry.pageMainWidget = pageMainWidget;
			normalTabEntry.stackedWidget = stackedWidget;
			normalTabEntry.plainTextEdit = plainTextEdit;
			normalTabEntry.tableView = tableView;
			normalTabEntry.dictModel = model;
			normalTabEntry.dictPath = dictPath;
			m_normalTabEntries.push_back(normalTabEntry);

			if (!fs::exists(dictPath)) {
				saveFunc(true);
			}
			return pageMainWidget;
		};

	auto& commonNormalDicts = m_globalConfig[m_modeConfigKey]["dictNames"];
	if (commonNormalDicts.is_array()) {
		auto it = commonNormalDicts.as_array().begin();
		while (it != commonNormalDicts.as_array().end()) {
			if (!it->is_string()) {
				it = commonNormalDicts.as_array().erase(it);
				continue;
			}
			fs::path dictPath = m_modeDictDir / (ascii2Wide(it->as_string()) + L".toml");
			if (!fs::exists(dictPath)) {
				it = commonNormalDicts.as_array().erase(it);
				continue;
			}
			QWidget* pageMainWidget = createNormalTab(dictPath);
			tabWidget->addTab(pageMainWidget, QString::fromStdString(it->as_string()));
			++it;
		}
	}
	else {
		commonNormalDicts = toml::array{};
	}

	tabWidget->setCurrentIndex(0);
	saveAllButton->setEnabled(tabWidget->count() > 0);
	connect(tabWidget, &QTabWidget::currentChanged, saveAllButton, [=](int)
		{
			saveAllButton->setEnabled(tabWidget->count() > 0);
		});

	connect(saveAllButton, &ElaToolButton::clicked, this, [=]()
		{
			this->apply2Config();
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"), tr("所有默认字典配置均已保存"), 3000);
		});

	connect(importButton, &ElaToolButton::clicked, this, [=]()
		{
			const QString importDictPathQStr = QFileDialog::getOpenFileName(window(), tr("选择字典文件"),
				QString::fromStdString(toml::find_or(m_globalConfig, "lastCommonNormalDictPath", "./")),
				"TOML files (*.toml);;JSON files (*.json)");
			if (importDictPathQStr.isEmpty()) {
				return;
			}
			insertToml(m_globalConfig, "lastCommonNormalDictPath", importDictPathQStr.toStdString());
			const fs::path importDictPath = importDictPathQStr.toStdWString();
			const fs::path newDictPath = m_modeDictDir / (importDictPath.stem().wstring() + L".toml");
			if (fs::exists(newDictPath) && !fs::equivalent(importDictPath, newDictPath)) {
				try {
					fs::remove(newDictPath);
				}
				catch (...) {
					ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("导入失败"), tr("原文件删除失败"), 3000);
					return;
				}
			}
			const bool hasSameNameTab = std::ranges::any_of(m_normalTabEntries, [=](const NormalTabEntry& entry)
				{
					return entry.dictPath.stem().wstring() == importDictPath.stem().wstring();
			});
			if (hasSameNameTab) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("导入失败"),
					tr("字典 %1 已存在").arg(QString::fromStdWString(importDictPath.stem().wstring())), 3000);
				return;
			}
			QWidget* pageMainWidget = createNormalTab(importDictPath);
			tabWidget->addTab(pageMainWidget, QString::fromStdWString(importDictPath.stem().wstring()));
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("创建成功"),
				tr("字典页 %1 已创建").arg(QString::fromStdWString(importDictPath.stem().wstring())), 3000);
		});

	connect(addNewTabButton, &ElaToolButton::clicked, this, [=]()
		{
			QString dictName;
			ElaInputDialog inputDialog(tr("请输入字典表名称"), tr("新建字典"), dictName, window());
			if (inputDialog.exec() != QDialog::Accepted) {
				return;
			}

			if (dictName.isEmpty() || dictName.contains('/') || dictName.contains('\\') || dictName.contains('.')) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft,
					tr("新建失败"), tr("字典名称不能为空，且不能包含点号、斜杠或反斜杠！"), 3000);
				return;
			}

			const fs::path newDictPath = m_modeDictDir / (dictName.toStdWString() + L".toml");
			const bool hasSameNameTab = std::ranges::any_of(m_normalTabEntries, [=](const NormalTabEntry& entry)
				{
					return entry.dictPath.stem().wstring() == dictName.toStdWString();
			});
			if (hasSameNameTab || dictName == "项目译前字典" || dictName == "项目译后字典") {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"),
					tr("字典 %1 已存在").arg(QString::fromStdWString(newDictPath.stem().wstring())), 3000);
				return;
			}

			try {
				atomicOutputFile(newDictPath, std::string{});
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"),
					tr("无法创建 %1 文件").arg(QString::fromStdWString(newDictPath.wstring())), 3000);
				return;
			}

			QWidget* pageMainWidget = createNormalTab(newDictPath);
			tabWidget->addTab(pageMainWidget, dictName);
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("新建成功"),
				tr("字典页 %1 已创建").arg(QString::fromStdWString(newDictPath.stem().wstring())), 3000);
		});


	m_applyFunc = [=]()
		{
			toml::array dictNamesArr;
			std::vector<std::pair<std::string, QWidget*>> pageWidgets;
			for (const NormalTabEntry& entry : m_normalTabEntries) {
				if (!entry.saveFunc(false)) {
					continue;
				}
				std::string dictName = wide2Ascii(entry.dictPath.stem().wstring());
				pageWidgets.push_back({ dictName, entry.pageMainWidget });
			}
			std::ranges::sort(pageWidgets, [=](const auto& a, const auto& b)
				{
					return tabWidget->indexOf(a.second) < tabWidget->indexOf(b.second);
				});
			for (const auto& dictName : pageWidgets | std::views::keys) {
				dictNamesArr.push_back(dictName);
			}

			insertToml(m_globalConfig, m_modeConfigKey + ".dictNames", dictNamesArr);

			auto& spec = m_globalConfig[m_modeConfigKey]["spec"];
			if (spec.is_table()) {
				for (const auto& key : spec.as_table() | std::views::keys) {
					if (
						!std::ranges::any_of(dictNamesArr, [&](const auto& elem)
							{
								return elem.is_string() && elem.as_string() == key;
							})
						)
					{
						spec.as_table().erase(key);
					}
				}
			}
			else {
				spec = toml::ordered_table{};
			}
			Q_EMIT commonDictsChangedSignal();
		};


	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
