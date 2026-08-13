#include "DictSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>
#include <QFileDialog>
#include <QItemSelectionModel>

#include "ElaIconButton.h"
#include "ElaTabWidget.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "DictionaryEntryDialog.h"
#include "DictionaryReader.h"
#include "DictionarySearchBar.h"
#include "ReorderableTableView.h"
#include "TreeSitterHighlighter.h"

import Tool;

DictSettingsPage::DictSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), m_projectDir(projectDir), m_globalConfig(globalConfig), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("项目字典设置"));
	setTitleVisible(false);

	setupUi();
}

void DictSettingsPage::refreshGptDict()
{
	if (m_refreshGptDictFunc) {
		m_refreshGptDictFunc();
	}
}


void DictSettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);


	auto createDictTabFunc =
		[=]<typename EntryType>
	    (const std::function<QString()>& readPlainTextFunc, const std::function<QList<EntryType>()>& readEntriesFunc,
	     QList<EntryType>& withdrawList, const std::string& configKey, const QString& tabName, const fs::path& dictPath)
	     -> std::pair<std::function<void()>, std::function<void(bool)>>
	{
		using ModelType = std::conditional_t<std::is_same_v<EntryType, GptDictEntry>, GptDictModel, NormalDictModel>;
		QWidget* pageMainWidget = new QWidget(mainWidget);
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

		ElaIconButton* saveButton = new ElaIconButton(ElaIconType::Check, pageMainWidget);
		saveButton->setFixedWidth(30);
		ElaToolTip* saveButtonToolTip = new ElaToolTip(saveButton);
		saveButtonToolTip->setToolTip(tr("保存当前页"));
		ElaIconButton* importButton = new ElaIconButton(ElaIconType::ArrowDownFromLine, pageMainWidget);
		importButton->setFixedWidth(30);
		ElaToolTip* importButtonToolTip = new ElaToolTip(importButton);
		importButtonToolTip->setToolTip(tr("导入字典页"));
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
		pageButtonLayout->addStretch();
		pageButtonLayout->addWidget(saveButton);
		pageButtonLayout->addWidget(importButton);
		pageButtonLayout->addWidget(withdrawButton);
		pageButtonLayout->addWidget(refreshButton);
		pageButtonLayout->addWidget(addDictButton);
		pageButtonLayout->addWidget(editEntryButton);
		pageButtonLayout->addWidget(removeDictButton);
		pageMainLayout->addLayout(pageButtonLayout);


		// 每个字典里又有一个StackedWidget区分表和纯文本
		QStackedWidget* stackedWidget = new QStackedWidget(pageMainWidget);
		// 纯文本模式
		ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
		QFont plainTextFont = plainTextEdit->font();
		plainTextFont.setPixelSize(15);
		plainTextEdit->setFont(plainTextFont);
		installTreeSitterHighlighter(plainTextEdit->document(), SyntaxLanguage::Toml);
		plainTextEdit->setPlainText(readPlainTextFunc());
		stackedWidget->addWidget(plainTextEdit);

		// 表格模式
		ReorderableTableView* tableView = new ReorderableTableView(stackedWidget);
		ModelType* model = new ModelType(tableView);
			const QList<EntryType> dictData = readEntriesFunc();
		model->loadData(dictData);
		tableView->setModel(model);
		QFont tableHeaderFont = tableView->horizontalHeader()->font();
		tableHeaderFont.setPixelSize(16);
		tableView->horizontalHeader()->setFont(tableHeaderFont);
		tableView->verticalHeader()->setHidden(true);
		tableView->setAlternatingRowColors(true);
		tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
		tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

		if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
			tableView->setColumnWidth(GptDictModel::Original, toml::find_or(m_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "0", 346));
			tableView->setColumnWidth(GptDictModel::Translation, toml::find_or(m_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "1", 199));
			tableView->setColumnWidth(GptDictModel::Description, toml::find_or(m_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "2", 559));
		}
		else {
			tableView->setColumnWidth(NormalDictModel::Original, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "0", 285));
			tableView->setColumnWidth(NormalDictModel::Translation, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "1", 195));
			tableView->setColumnWidth(NormalDictModel::Conditions, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "2", 350));
			tableView->setColumnWidth(NormalDictModel::IsReg, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "3", 82));
			tableView->setColumnWidth(NormalDictModel::Priority, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "4", 65));
		}

		stackedWidget->addWidget(tableView);
		stackedWidget->setCurrentIndex(toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableOpenMode",
			toml::find_or(m_globalConfig, "defaultDictOpenMode", 1)));
		DictionarySearchBar* searchBar = new DictionarySearchBar(tableView,
			std::is_same_v<EntryType, GptDictEntry> ? tr("备注") : tr("条件"), pageMainWidget);
		searchBar->setVisible(stackedWidget->currentIndex() == 1);
		pageButtonLayout->insertWidget(2, searchBar);
		plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
		tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
		addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
		removeDictButton->setEnabled(stackedWidget->currentIndex() == 1
			&& tableView->selectionModel()->hasSelection());
		editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
			&& tableView->selectionModel()->hasSelection() && tableView->currentIndex().isValid());

		pageMainLayout->addWidget(stackedWidget);
		auto refreshFunc = [=]()
			{
				plainTextEdit->setPlainText(readPlainTextFunc());
				model->loadData(readEntriesFunc());
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"),
					tr("重新载入了 %1").arg(tabName), 3000);
			};
		auto saveFunc = [=](bool forceSaveInTableModeToInit)
			{
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					const fs::path generatedDictPath = m_projectDir / L"ProjGptDict-Gen.toml";
					if (fs::exists(generatedDictPath)) {
						try {
							fs::remove(generatedDictPath);
						}
						catch (const fs::filesystem_error& e) {
							ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("生成字典删除失败"), e.what(), 3000);
						}
					}
					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						atomicOutputFile(dictPath, plainTextEdit->toPlainText().toStdString());
						model->loadData(DictionaryReader::readGptDict(dictPath));
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						toml::ordered_value dictsArr = toml::array{};
						const QList<EntryType>& dictEntries = model->getEntriesRef();
						for (const auto& dictEntry : dictEntries) {
							toml::ordered_table dictTable;
							dictTable.insert({ "org", dictEntry.original.toStdString() });
							dictTable.insert({ "rep", dictEntry.translation.toStdString() });
							dictTable.insert({ "note", dictEntry.description.toStdString() });
							dictsArr.push_back(std::move(dictTable));
						}
						dictsArr.as_array_fmt().fmt = toml::array_format::multiline;
						atomicOutputFile(dictPath, toml::format(toml::ordered_value{ toml::ordered_table{{"gptDict", dictsArr}} }));
						plainTextEdit->setPlainText(DictionaryReader::readDictStr(dictPath));
					}
					insertToml(m_projectConfig, "GUIConfig.gptDictTableColumnWidth.0", tableView->columnWidth(GptDictModel::Original));
					insertToml(m_projectConfig, "GUIConfig.gptDictTableColumnWidth.1", tableView->columnWidth(GptDictModel::Translation));
					insertToml(m_projectConfig, "GUIConfig.gptDictTableColumnWidth.2", tableView->columnWidth(GptDictModel::Description));
					insertToml(m_projectConfig, "GUIConfig.gptDictTableOpenMode", stackedWidget->currentIndex());
				}
				else {
					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						atomicOutputFile(dictPath, plainTextEdit->toPlainText().toStdString());
						model->loadData(readEntriesFunc());
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						toml::ordered_value dictsArr = toml::array{};
						for (const NormalDictEntry& dictEntry : model->getEntriesRef()) {
							toml::ordered_table dictTable;
							dictTable.insert({ "org", dictEntry.original.toStdString() });
							dictTable.insert({ "rep", dictEntry.translation.toStdString() });
							if (!dictEntry.conditions.isEmpty()) {
								toml::ordered_value conditions = toml::array{};
								for (const NormalCondition& condition : dictEntry.conditions) {
									toml::ordered_table conditionTable;
									conditionTable.insert({ "conditionReg", condition.pattern.toStdString() });
									conditionTable.insert({ "conditionTarget", serializeNormalConditionTarget(condition).toStdString() });
									conditions.push_back(std::move(conditionTable));
								}
								dictTable.insert({ "conditions", std::move(conditions) });
							}
							dictTable.insert({ "isReg", dictEntry.isReg });
							dictTable.insert({ "priority", dictEntry.priority });
							dictsArr.push_back(std::move(dictTable));
						}
						atomicOutputFile(dictPath,
							toml::format(toml::ordered_value{ toml::ordered_table{{ "normalDict", std::move(dictsArr) }} }));
						plainTextEdit->setPlainText(readPlainTextFunc());
					}
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.0", tableView->columnWidth(NormalDictModel::Original));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.1", tableView->columnWidth(NormalDictModel::Translation));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.2", tableView->columnWidth(NormalDictModel::Conditions));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.3", tableView->columnWidth(NormalDictModel::IsReg));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.4", tableView->columnWidth(NormalDictModel::Priority));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableOpenMode", stackedWidget->currentIndex());
				}
			};
		connect(importButton, &ElaIconButton::clicked, this, [=]()
			{
				const QString filter = std::is_same_v<EntryType, GptDictEntry>
					? "TOML files (*.toml);;JSON files (*.json);;TSV files (*.tsv *.txt)"
					: "TOML files (*.toml);;JSON files (*.json)";
				const QString importDictPathQStr = QFileDialog::getOpenFileName(window(), tr("选择字典文件"),
					QString::fromStdString(toml::find_or(m_globalConfig, "lastProjectDictPath", wide2Ascii(m_projectDir))), filter);
				if (importDictPathQStr.isEmpty()) {
					return;
				}
				insertToml(m_globalConfig, "lastProjectDictPath", importDictPathQStr.toStdString());
				const fs::path importDictPath = importDictPathQStr.toStdWString();
				QList<EntryType> dictEntries;
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					dictEntries = DictionaryReader::readGptDict(importDictPath);
				}
				else {
					dictEntries = DictionaryReader::readNormalDict(importDictPath);
				}
				if (dictEntries.isEmpty()) {
					ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("导入失败"), tr("字典文件中没有词条"), 3000);
					return;
				}
				model->loadData(dictEntries);
				saveFunc(true);
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("导入成功"),
					tr("从文件 %1 中导入了 %2 个词条")
					.arg(QString::fromStdWString(importDictPath.filename().wstring()))
					.arg(QString::number(dictEntries.size())), 3000);
			});
		connect(plainTextModeButton, &ElaToolButton::clicked, this, [=]()
			{
				stackedWidget->setCurrentIndex(0);
				addDictButton->setEnabled(false);
				removeDictButton->setEnabled(false);
				editEntryButton->setEnabled(false);
				plainTextModeButton->setEnabled(false);
				tableModeButton->setEnabled(true);
				withdrawButton->setEnabled(false);
				searchBar->hide();
			});
		connect(tableModeButton, &ElaToolButton::clicked, this, [=, &withdrawList]()
			{
				stackedWidget->setCurrentIndex(1);
				addDictButton->setEnabled(true);
				removeDictButton->setEnabled(tableView->selectionModel()->hasSelection());
				editEntryButton->setEnabled(tableView->selectionModel()->hasSelection()
					&& tableView->currentIndex().isValid());
				plainTextModeButton->setEnabled(true);
				tableModeButton->setEnabled(false);
				withdrawButton->setEnabled(!withdrawList.empty());
				searchBar->show();
			});
		connect(refreshButton, &ElaPushButton::clicked, this, refreshFunc);
		connect(saveButton, &ElaPushButton::clicked, this, [=]()
			{
				saveFunc(false);
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"),
					tr("%1 已保存").arg(tabName), 3000);
			});
		auto openEntryDialog = [=](const EntryType& entry, EntryType& result) -> bool
			{
				DictionaryEntryDialog dialog(entry, window());
				if (dialog.exec() != QDialog::Accepted) {
					return false;
				}
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					result = dialog.getGptEntry();
				}
				else {
					result = dialog.getNormalEntry();
				}
				return true;
			};

		auto editEntry = [=](int row)
			{
				const QList<EntryType>& entries = model->getEntriesRef();
				if (row < 0 || row >= entries.size()) {
					return;
				}
				EntryType editedEntry;
				if (openEntryDialog(entries.at(row), editedEntry)) {
					model->setEntry(row, editedEntry);
				}
			};

		connect(addDictButton, &ElaPushButton::clicked, this, [=]()
			{
				EntryType newEntry;
				if (!openEntryDialog(EntryType{}, newEntry)) {
					return;
				}
				const QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
				const int insertRow = selectedRows.isEmpty() ? model->rowCount() : selectedRows.first().row();
				model->insertRow(insertRow, newEntry);
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
		connect(removeDictButton, &ElaPushButton::clicked, this, [=, &withdrawList]()
			{
				QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
				if (selectedRows.isEmpty()) {
					return;
				}

				DictionaryEntryDeleteDialog confirmDialog(selectedRows.size(), window());
				if (confirmDialog.exec() != QDialog::Accepted) {
					return;
				}

				const QList<EntryType>& entries = model->getEntriesRef();
				std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
					{
						return a.row() > b.row();
					});
				for (const QModelIndex& index : selectedRows) {
					if (withdrawList.size() > 100) {
						withdrawList.pop_front();
					}
					withdrawList.push_back(entries[index.row()]);
					model->removeRow(index.row());
				}
				if (!withdrawList.empty()) {
					withdrawButton->setEnabled(true);
				}
			});
		connect(withdrawButton, &ElaPushButton::clicked, this, [=, &withdrawList]()
			{
				if (withdrawList.empty()) {
					return;
				}
				EntryType entry = withdrawList.back();
				withdrawList.pop_back();
				model->insertRow(0, entry);
				if (withdrawList.empty()) {
					withdrawButton->setEnabled(false);
				}
			});

		tabWidget->addTab(pageMainWidget, tabName);
		return { refreshFunc, saveFunc };
	};


	const fs::path gptDictPath = m_projectDir / L"ProjGptDict.toml";
	const fs::path generatedGptDictPath = m_projectDir / L"ProjGptDict-Gen.toml";
	const std::vector<fs::path> gptDictPaths = { gptDictPath, generatedGptDictPath };
	std::function<QString()> gptReadPlainTextFunc = [=]() -> QString
		{
			return DictionaryReader::readGptDictsStr(gptDictPaths);
		};
	std::function<QList<GptDictEntry>()> gptReadEntriesFunc = [=]() -> QList<GptDictEntry>
		{
			return DictionaryReader::readGptDicts(gptDictPaths);
		};
	auto refreshAndSaveGptDictFunc =
		createDictTabFunc(gptReadPlainTextFunc, gptReadEntriesFunc, m_withdrawGptList,
			"gpt", tr("项目GPT字典"), gptDictPath);


	const fs::path preDictPath = m_projectDir / L"ProjPreDict.toml";
	std::function<QString()> preReadPlainTextFunc = [=]() -> QString
		{
			return DictionaryReader::readDictStr(preDictPath);
		};
	std::function<QList<NormalDictEntry>()> preReadEntriesFunc = [=]() -> QList<NormalDictEntry>
		{
			return DictionaryReader::readNormalDict(preDictPath);
		};
	auto refreshAndSavePreDictFunc =
		createDictTabFunc(preReadPlainTextFunc, preReadEntriesFunc, m_withdrawPreList,
			"pre", tr("项目译前字典"), preDictPath);


	const fs::path postDictPath = m_projectDir / L"ProjPostDict.toml";
	std::function<QString()> postReadPlainTextFunc = [=]() -> QString
		{
			return DictionaryReader::readDictStr(postDictPath);
		};
	std::function<QList<NormalDictEntry>()> postReadEntriesFunc = [=]() -> QList<NormalDictEntry>
		{
			return DictionaryReader::readNormalDict(postDictPath);
		};
	auto refreshAndSavePostDictFunc =
		createDictTabFunc(postReadPlainTextFunc, postReadEntriesFunc, m_withdrawPostList,
			"post", tr("项目译后字典"), postDictPath);


	m_refreshGptDictFunc = [=]()
		{
			refreshAndSaveGptDictFunc.first();
		};


	m_applyFunc = [=]()
		{
			refreshAndSaveGptDictFunc.second(false);
			refreshAndSavePreDictFunc.second(false);
			refreshAndSavePostDictFunc.second(false);
		};

	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
