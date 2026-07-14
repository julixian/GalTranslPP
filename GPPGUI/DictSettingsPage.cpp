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

void DictSettingsPage::refreshDicts()
{
	if (m_refreshFunc) {
		m_refreshFunc();
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
		[=]<typename EntryType>(const std::function<QString()>& readPlainTextFunc, const std::function<QList<EntryType>()>& readEntriesFunc,
			QList<EntryType>& withdrawList, const std::string& configKey, const QString& tabName, const fs::path& dictPath)
		-> std::pair<std::function<void()>, std::function<void(bool)>>
	{
		using ModelType = std::conditional_t<std::is_same_v<EntryType, GptDictEntry>, GptDictModel, NormalDictModel>;
		QWidget* dictWidget = new QWidget(mainWidget);
		QVBoxLayout* dictLayout = new QVBoxLayout(dictWidget);
		dictLayout->setContentsMargins(0, 0, 0, 0);

		QHBoxLayout* buttonLayout = new QHBoxLayout(dictWidget);
		ElaToolButton* plainTextModeButtom = new ElaToolButton(dictWidget);
		plainTextModeButtom->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		plainTextModeButtom->setElaIcon(ElaIconType::Text);
		plainTextModeButtom->setText(tr("纯文本"));
		ElaToolButton* tableModeButtom = new ElaToolButton(dictWidget);
		tableModeButtom->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		tableModeButtom->setElaIcon(ElaIconType::Table);
		tableModeButtom->setText(tr("表模式"));

		ElaIconButton* saveDictButton = new ElaIconButton(ElaIconType::Check, dictWidget);
		saveDictButton->setFixedWidth(30);
		ElaToolTip* saveDictButtonToolTip = new ElaToolTip(saveDictButton);
		saveDictButtonToolTip->setToolTip(tr("保存当前页"));
		ElaIconButton* importDictButton = new ElaIconButton(ElaIconType::ArrowDownFromLine, dictWidget);
		importDictButton->setFixedWidth(30);
		ElaToolTip* importDictButtonToolTip = new ElaToolTip(importDictButton);
		importDictButtonToolTip->setToolTip(tr("导入字典页"));
		ElaIconButton* withdrawDictButton = new ElaIconButton(ElaIconType::ArrowLeft, dictWidget);
		withdrawDictButton->setFixedWidth(30);
		ElaToolTip* withdrawDictButtonToolTip = new ElaToolTip(withdrawDictButton);
		withdrawDictButtonToolTip->setToolTip(tr("撤回删除行"));
		withdrawDictButton->setEnabled(false);
		ElaIconButton* refreshDictButton = new ElaIconButton(ElaIconType::ArrowRotateRight, dictWidget);
		refreshDictButton->setFixedWidth(30);
		ElaToolTip* refreshDictButtonToolTip = new ElaToolTip(refreshDictButton);
		refreshDictButtonToolTip->setToolTip(tr("刷新当前页"));
		ElaIconButton* addDictButton = new ElaIconButton(ElaIconType::Plus, dictWidget);
		addDictButton->setFixedWidth(30);
		ElaToolTip* addDictButtonToolTip = new ElaToolTip(addDictButton);
		addDictButtonToolTip->setToolTip(tr("添加词条"));
		ElaIconButton* delDictButton = new ElaIconButton(ElaIconType::Minus, dictWidget);
		delDictButton->setFixedWidth(30);
		ElaToolTip* delDictButtonToolTip = new ElaToolTip(delDictButton);
		delDictButtonToolTip->setToolTip(tr("删除词条"));
		ElaIconButton* editEntryButton = new ElaIconButton(ElaIconType::PenToSquare, dictWidget);
		editEntryButton->setFixedWidth(30);
		ElaToolTip* editEntryButtonToolTip = new ElaToolTip(editEntryButton);
		editEntryButtonToolTip->setToolTip(tr("编辑词条"));
		buttonLayout->addWidget(plainTextModeButtom);
		buttonLayout->addWidget(tableModeButtom);
		buttonLayout->addStretch();
		buttonLayout->addWidget(saveDictButton);
		buttonLayout->addWidget(importDictButton);
		buttonLayout->addWidget(withdrawDictButton);
		buttonLayout->addWidget(refreshDictButton);
		buttonLayout->addWidget(addDictButton);
		buttonLayout->addWidget(editEntryButton);
		buttonLayout->addWidget(delDictButton);
		dictLayout->addLayout(buttonLayout);


		// 每个字典里又有一个StackedWidget区分表和纯文本
		QStackedWidget* stackedWidget = new QStackedWidget(dictWidget);
		// 纯文本模式
		ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
		QFont plainTextFont = plainTextEdit->font();
		plainTextFont.setPixelSize(15);
		plainTextEdit->setFont(plainTextFont);
		installTreeSitterHighlighter(plainTextEdit->document(), SyntaxLanguage::Toml);
		plainTextEdit->setPlainText(readPlainTextFunc());
		stackedWidget->addWidget(plainTextEdit);

		// 表格模式
		ReorderableTableView* dictTableView = new ReorderableTableView(stackedWidget);
		ModelType* dictModel = new ModelType(dictTableView);
		QList<EntryType> dictData = readEntriesFunc();
		dictModel->loadData(dictData);
		dictTableView->setModel(dictModel);
		QFont tableHeaderFont = dictTableView->horizontalHeader()->font();
		tableHeaderFont.setPixelSize(16);
		dictTableView->horizontalHeader()->setFont(tableHeaderFont);
		dictTableView->verticalHeader()->setHidden(true);
		dictTableView->setAlternatingRowColors(true);
		dictTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
		dictTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

		if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
			dictTableView->setColumnWidth(GptDictModel::Original, toml::find_or(m_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "0", 346));
			dictTableView->setColumnWidth(GptDictModel::Translation, toml::find_or(m_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "1", 199));
			dictTableView->setColumnWidth(GptDictModel::Description, toml::find_or(m_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "2", 559));
		}
		else {
			dictTableView->setColumnWidth(NormalDictModel::Original, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "0", 285));
			dictTableView->setColumnWidth(NormalDictModel::Translation, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "1", 195));
			dictTableView->setColumnWidth(NormalDictModel::Conditions, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "2", 350));
			dictTableView->setColumnWidth(NormalDictModel::IsReg, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "3", 82));
			dictTableView->setColumnWidth(NormalDictModel::Priority, toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "4", 65));
		}

		stackedWidget->addWidget(dictTableView);
		stackedWidget->setCurrentIndex(toml::find_or(m_projectConfig, "GUIConfig", configKey + "DictTableOpenMode",
			toml::find_or(m_globalConfig, "defaultDictOpenMode", 1)));
		DictionarySearchBar* searchBar = new DictionarySearchBar(dictTableView,
			std::is_same_v<EntryType, GptDictEntry> ? tr("备注") : tr("条件"), dictWidget);
		searchBar->setVisible(stackedWidget->currentIndex() == 1);
		buttonLayout->insertWidget(2, searchBar);
		plainTextModeButtom->setEnabled(stackedWidget->currentIndex() != 0);
		tableModeButtom->setEnabled(stackedWidget->currentIndex() != 1);
		addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
		delDictButton->setEnabled(stackedWidget->currentIndex() == 1
			&& dictTableView->selectionModel()->hasSelection());
		editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
			&& dictTableView->selectionModel()->hasSelection() && dictTableView->currentIndex().isValid());

		dictLayout->addWidget(stackedWidget);
		auto refreshDictFunc = [=]()
			{
				plainTextEdit->setPlainText(readPlainTextFunc());
				dictModel->loadData(readEntriesFunc());
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"),
					tr("重新载入了 %1").arg(tabName), 3000);
			};
		auto saveDictFunc = [=](bool forceSaveInTableModeToInit)
			{
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					std::ofstream ofs;
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
						atomicOutputFile(ofs, dictPath, plainTextEdit->toPlainText().toStdString());
						dictModel->loadData(DictionaryReader::readGptDict(dictPath));
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						toml::ordered_value dictArr = toml::array{};
						const QList<EntryType>& entries = dictModel->getEntriesRef();
						for (const auto& entry : entries) {
							toml::ordered_table dictTbl;
							dictTbl.insert({ "org", entry.original.toStdString() });
							dictTbl.insert({ "rep", entry.translation.toStdString() });
							dictTbl.insert({ "note", entry.description.toStdString() });
							dictArr.push_back(dictTbl);
						}
						dictArr.as_array_fmt().fmt = toml::array_format::multiline;
						atomicOutputFile(ofs, dictPath, toml::format(toml::ordered_value{ toml::ordered_table{{"gptDict", dictArr}} }));
						plainTextEdit->setPlainText(DictionaryReader::readDictStr(dictPath));
					}
					insertToml(m_projectConfig, "GUIConfig.gptDictTableColumnWidth.0", dictTableView->columnWidth(GptDictModel::Original));
					insertToml(m_projectConfig, "GUIConfig.gptDictTableColumnWidth.1", dictTableView->columnWidth(GptDictModel::Translation));
					insertToml(m_projectConfig, "GUIConfig.gptDictTableColumnWidth.2", dictTableView->columnWidth(GptDictModel::Description));
					insertToml(m_projectConfig, "GUIConfig.gptDictTableOpenMode", stackedWidget->currentIndex());
				}
				else {
					std::ofstream ofs;
					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						atomicOutputFile(ofs, dictPath, plainTextEdit->toPlainText().toStdString());
						dictModel->loadData(readEntriesFunc());
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						toml::ordered_value dictArr = toml::array{};
						for (const NormalDictEntry& entry : dictModel->getEntriesRef()) {
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
						atomicOutputFile(ofs, dictPath,
							toml::format(toml::ordered_value{ toml::ordered_table{{ "normalDict", std::move(dictArr) }} }));
						plainTextEdit->setPlainText(readPlainTextFunc());
					}
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.0", dictTableView->columnWidth(NormalDictModel::Original));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.1", dictTableView->columnWidth(NormalDictModel::Translation));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.2", dictTableView->columnWidth(NormalDictModel::Conditions));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.3", dictTableView->columnWidth(NormalDictModel::IsReg));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.4", dictTableView->columnWidth(NormalDictModel::Priority));
					insertToml(m_projectConfig, "GUIConfig." + configKey + "DictTableOpenMode", stackedWidget->currentIndex());
				}
			};
		connect(importDictButton, &ElaIconButton::clicked, this, [=]()
			{
				QString filter;
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					filter = "TOML files (*.toml);;JSON files (*.json);;TSV files (*.tsv *.txt)";
				}
				else {
					filter = "TOML files (*.toml);;JSON files (*.json)";
				}
				QString dictPathStr = QFileDialog::getOpenFileName(window(), tr("选择字典文件"),
					QString::fromStdString(toml::find_or(m_globalConfig, "lastProjectDictPath", wide2Ascii(m_projectDir))), filter);
				if (dictPathStr.isEmpty()) {
					return;
				}
				insertToml(m_globalConfig, "lastProjectDictPath", dictPathStr.toStdString());
				fs::path importDictPath = dictPathStr.toStdWString();
				QList<EntryType> entries;
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					entries = DictionaryReader::readGptDict(importDictPath);
				}
				else {
					entries = DictionaryReader::readNormalDict(importDictPath);
				}
				if (entries.isEmpty()) {
					ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("导入失败"), tr("字典文件中没有词条"), 3000);
					return;
				}
				dictModel->loadData(entries);
				saveDictFunc(true);
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("导入成功"),
					tr("从文件 %1 中导入了 %2 个词条")
					.arg(QString::fromStdWString(importDictPath.filename().wstring()))
					.arg(QString::number(entries.size())), 3000);
			});
		connect(plainTextModeButtom, &ElaToolButton::clicked, this, [=]()
			{
				stackedWidget->setCurrentIndex(0);
				addDictButton->setEnabled(false);
				delDictButton->setEnabled(false);
				editEntryButton->setEnabled(false);
				plainTextModeButtom->setEnabled(false);
				tableModeButtom->setEnabled(true);
				withdrawDictButton->setEnabled(false);
				searchBar->hide();
			});
		connect(tableModeButtom, &ElaToolButton::clicked, this, [=, &withdrawList]()
			{
				stackedWidget->setCurrentIndex(1);
				addDictButton->setEnabled(true);
				delDictButton->setEnabled(dictTableView->selectionModel()->hasSelection());
				editEntryButton->setEnabled(dictTableView->selectionModel()->hasSelection()
					&& dictTableView->currentIndex().isValid());
				plainTextModeButtom->setEnabled(true);
				tableModeButtom->setEnabled(false);
				withdrawDictButton->setEnabled(!withdrawList.empty());
				searchBar->show();
			});
		connect(refreshDictButton, &ElaPushButton::clicked, this, refreshDictFunc);
		connect(saveDictButton, &ElaPushButton::clicked, this, [=]()
			{
				saveDictFunc(false);
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
				const QList<EntryType>& entries = dictModel->getEntriesRef();
				if (row < 0 || row >= entries.size()) {
					return;
				}
				EntryType editedEntry;
				if (openEntryDialog(entries.at(row), editedEntry)) {
					dictModel->setEntry(row, editedEntry);
				}
			};

		connect(addDictButton, &ElaPushButton::clicked, this, [=]()
			{
				EntryType newEntry;
				if (!openEntryDialog(EntryType{}, newEntry)) {
					return;
				}
				const QModelIndexList selectedRows = dictTableView->selectionModel()->selectedRows();
				const int insertRow = selectedRows.isEmpty() ? dictModel->rowCount() : selectedRows.first().row();
				dictModel->insertRow(insertRow, newEntry);
				dictTableView->setCurrentIndex(dictModel->index(insertRow, 0));
				dictTableView->selectRow(insertRow);
			});
		connect(editEntryButton, &ElaIconButton::clicked, this, [=]()
			{
				editEntry(dictTableView->currentIndex().row());
			});
		connect(dictTableView, &QAbstractItemView::doubleClicked, this, [=](const QModelIndex& index)
			{
				editEntry(index.row());
			});
		connect(dictTableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
			[=](const QModelIndex& current)
			{
				editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
					&& dictTableView->selectionModel()->hasSelection() && current.isValid());
			});
		connect(dictTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
			[=]()
			{
				delDictButton->setEnabled(stackedWidget->currentIndex() == 1
					&& dictTableView->selectionModel()->hasSelection());
				editEntryButton->setEnabled(stackedWidget->currentIndex() == 1
					&& dictTableView->selectionModel()->hasSelection() && dictTableView->currentIndex().isValid());
			});
		connect(delDictButton, &ElaPushButton::clicked, this, [=, &withdrawList]()
			{
				QModelIndexList selectedRows = dictTableView->selectionModel()->selectedRows();
				if (selectedRows.isEmpty()) {
					return;
				}

				DictionaryEntryDeleteDialog confirmDialog(selectedRows.size(), window());
				if (confirmDialog.exec() != QDialog::Accepted) {
					return;
				}

				const QList<EntryType>& entries = dictModel->getEntriesRef();
				std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
					{
						return a.row() > b.row();
					});
				for (const QModelIndex& index : selectedRows) {
					if (withdrawList.size() > 100) {
						withdrawList.pop_front();
					}
					withdrawList.push_back(entries[index.row()]);
					dictModel->removeRow(index.row());
				}
				if (!withdrawList.empty()) {
					withdrawDictButton->setEnabled(true);
				}
			});
		connect(withdrawDictButton, &ElaPushButton::clicked, this, [=, &withdrawList]()
			{
				if (withdrawList.empty()) {
					return;
				}
				EntryType entry = withdrawList.back();
				withdrawList.pop_back();
				dictModel->insertRow(0, entry);
				if (withdrawList.empty()) {
					withdrawDictButton->setEnabled(false);
				}
			});

		tabWidget->addTab(dictWidget, tabName);
		return { refreshDictFunc, saveDictFunc };
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


	fs::path preDictPath = m_projectDir / L"ProjPreDict.toml";
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


	fs::path postDictPath = m_projectDir / L"ProjPostDict.toml";
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


	m_refreshFunc = [=]()
		{
			refreshAndSaveGptDictFunc.first();
			//refreshAndSavePreDictFunc.first();
			//refreshAndSavePostDictFunc.first();
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
