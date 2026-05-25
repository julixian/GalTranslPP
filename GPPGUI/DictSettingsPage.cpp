#include "DictSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>
#include <QFileDialog>

#include "ElaIconButton.h"
#include "ElaTabWidget.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "ReadDicts.h"

import Tool;

DictSettingsPage::DictSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir)
{
	setWindowTitle(tr("项目字典设置"));
	setTitleVisible(false);

	_setupUI();
}

DictSettingsPage::~DictSettingsPage()
{

}

void DictSettingsPage::refreshDicts()
{
	if (_refreshFunc) {
		_refreshFunc();
	}
}


void DictSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);


	auto createDictTabFunc =
		[=]<typename EntryType>(const std::function<QString()>& readPlainTextFunc, const std::function<QList<EntryType>()>& readEntriesFunc,
			QList<EntryType>& withdrawList, const std::string& configKey, const QString& tabName) 
		-> std::pair<std::function<void()>, std::function<void(bool)>>
	{
		using ModelType = typename std::conditional_t<std::is_same_v<EntryType, GptDictEntry>, GptDictModel, NormalDictModel>;
		QWidget* dictWidget = new QWidget(mainWidget);
		QVBoxLayout* dictLayout = new QVBoxLayout(dictWidget);
		dictLayout->setContentsMargins(0, 0, 0, 0);
		
		QHBoxLayout* buttonLayout = new QHBoxLayout(dictWidget);
		ElaPushButton* plainTextModeButtom = new ElaPushButton(dictWidget);
		plainTextModeButtom->setText(tr("纯文本模式"));
		ElaPushButton* tableModeButtom = new ElaPushButton(dictWidget);
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
		buttonLayout->addWidget(plainTextModeButtom);
		buttonLayout->addWidget(tableModeButtom);
		buttonLayout->addStretch();
		buttonLayout->addWidget(saveDictButton);
		buttonLayout->addWidget(importDictButton);
		buttonLayout->addWidget(withdrawDictButton);
		buttonLayout->addWidget(refreshDictButton);
		buttonLayout->addWidget(addDictButton);
		buttonLayout->addWidget(delDictButton);
		dictLayout->addLayout(buttonLayout);


		// 每个字典里又有一个StackedWidget区分表和纯文本
		QStackedWidget* stackedWidget = new QStackedWidget(dictWidget);
		// 纯文本模式
		ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
		QFont plainTextFont = plainTextEdit->font();
		plainTextFont.setPixelSize(15);
		plainTextEdit->setFont(plainTextFont);
		plainTextEdit->setPlainText(readPlainTextFunc());
		stackedWidget->addWidget(plainTextEdit);

		// 表格模式
		ElaTableView* dictTableView = new ElaTableView(stackedWidget);
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

		if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
			dictTableView->setColumnWidth(0, toml::find_or(_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "0", 360));
			dictTableView->setColumnWidth(1, toml::find_or(_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "1", 215));
			dictTableView->setColumnWidth(2, toml::find_or(_projectConfig, "GUIConfig", "gptDictTableColumnWidth", "2", 425));
		}
		else {
			dictTableView->setColumnWidth(0, toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "0", 285));
			dictTableView->setColumnWidth(1, toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "1", 195));
			dictTableView->setColumnWidth(2, toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "2", 135));
			dictTableView->setColumnWidth(3, toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "3", 250));
			dictTableView->setColumnWidth(4, toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "4", 75));
			dictTableView->setColumnWidth(5, toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableColumnWidth", "5", 60));
		}

		stackedWidget->addWidget(dictTableView);
		stackedWidget->setCurrentIndex(toml::find_or(_projectConfig, "GUIConfig", configKey + "DictTableOpenMode", 
			toml::find_or(_globalConfig, "defaultDictOpenMode", 0)));
		plainTextModeButtom->setEnabled(stackedWidget->currentIndex() != 0);
		tableModeButtom->setEnabled(stackedWidget->currentIndex() != 1);
		addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
		delDictButton->setEnabled(stackedWidget->currentIndex() == 1);

		dictLayout->addWidget(stackedWidget);
		auto refreshDictFunc = [=]()
			{
				plainTextEdit->setPlainText(readPlainTextFunc());
				dictModel->loadData(readEntriesFunc());
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"), tr("重新载入了 ") + tabName, 3000);
			};
		auto saveDictFunc = [=](bool forceSaveInTableModeToInit)
			{
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					std::ofstream ofs(_projectDir / (tabName.toStdWString() + L".toml"), std::ios::binary);
					if (fs::exists(_projectDir / L"项目GPT字典-生成.toml")) {
						try {
							fs::remove(_projectDir / L"项目GPT字典-生成.toml");
						}
						catch (const fs::filesystem_error& e) {
							ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("生成字典删除失败"), e.what(), 3000);
						}
					}
					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						ofs << plainTextEdit->toPlainText().toStdString();
						ofs.close();
						dictModel->loadData(ReadDicts::readGptDicts(_projectDir / (tabName.toStdWString() + L".toml")));
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
						ofs << toml::ordered_value{ toml::ordered_table{{"gptDict", dictArr}} };
						ofs.close();
						plainTextEdit->setPlainText(ReadDicts::readDictsStr(_projectDir / (tabName.toStdWString() + L".toml")));
					}
					insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.0", dictTableView->columnWidth(0));
					insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.1", dictTableView->columnWidth(1));
					insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.2", dictTableView->columnWidth(2));
					insertToml(_projectConfig, "GUIConfig.gptDictTableOpenMode", stackedWidget->currentIndex());
				}
				else {
					std::ofstream ofs(_projectDir / (tabName.toStdWString() + L".toml"), std::ios::binary);
					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						ofs << plainTextEdit->toPlainText().toStdString();
						ofs.close();
						dictModel->loadData(readEntriesFunc());
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						toml::ordered_value dictArr = toml::array{};
						const QList<EntryType>& entries = dictModel->getEntriesRef();
						for (const auto& entry : entries) {
							toml::ordered_table dictTbl;
							dictTbl.insert({ "org", entry.original.toStdString() });
							dictTbl.insert({ "rep", entry.translation.toStdString() });
							dictTbl.insert({ "conditionTarget", entry.conditionTar.toStdString() });
							dictTbl.insert({ "conditionReg", entry.conditionReg.toStdString() });
							dictTbl.insert({ "isReg", entry.isReg });
							dictTbl.insert({ "priority", entry.priority });
							dictTbl["org"].as_string_fmt().fmt = toml::string_format::literal;
							dictTbl["rep"].as_string_fmt().fmt = toml::string_format::literal;
							dictTbl["conditionTarget"].as_string_fmt().fmt = toml::string_format::literal;
							dictTbl["conditionReg"].as_string_fmt().fmt = toml::string_format::literal;
							dictArr.push_back(dictTbl);
						}
						ofs << toml::ordered_value{ toml::ordered_table{{"normalDict", dictArr}} };
						ofs.close();
						plainTextEdit->setPlainText(readPlainTextFunc());
					}
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.0", dictTableView->columnWidth(0));
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.1", dictTableView->columnWidth(1));
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.2", dictTableView->columnWidth(2));
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.3", dictTableView->columnWidth(3));
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.4", dictTableView->columnWidth(4));
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableColumnWidth.5", dictTableView->columnWidth(5));
					insertToml(_projectConfig, "GUIConfig." + configKey + "DictTableOpenMode", stackedWidget->currentIndex());
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
				QString dictPathStr = QFileDialog::getOpenFileName(this, tr("选择字典文件"), 
					QString::fromStdString(toml::find_or(_globalConfig, "lastProjectDictPath", wide2Ascii(_projectDir))), filter);
				if (dictPathStr.isEmpty()) {
					return;
				}
				insertToml(_globalConfig, "lastProjectDictPath", dictPathStr.toStdString());
				fs::path importDictPath = dictPathStr.toStdWString();
				QList<EntryType> entries;
				if constexpr (std::is_same_v<EntryType, GptDictEntry>) {
					entries = ReadDicts::readGptDicts(importDictPath);
				}
				else {
					entries = ReadDicts::readNormalDicts(importDictPath);
				}
				if (entries.isEmpty()) {
					ElaMessageBar::warning(ElaMessageBarType::TopLeft, tr("导入失败"), tr("字典文件中没有词条"), 3000);
					return;
				}
				dictModel->loadData(entries);
				saveDictFunc(true);
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("导入成功"), tr("从文件 ") +
					QString::fromStdWString(importDictPath.filename().wstring()) + tr(" 中导入了 ") + QString::number(entries.size()) + tr(" 个词条"), 3000);
			});
		connect(plainTextModeButtom, &ElaPushButton::clicked, this, [=]()
			{
				stackedWidget->setCurrentIndex(0);
				addDictButton->setEnabled(false);
				delDictButton->setEnabled(false);
				plainTextModeButtom->setEnabled(false);
				tableModeButtom->setEnabled(true);
				withdrawDictButton->setEnabled(false);
			});
		connect(tableModeButtom, &ElaPushButton::clicked, this, [=, &withdrawList]()
			{
				stackedWidget->setCurrentIndex(1);
				addDictButton->setEnabled(true);
				delDictButton->setEnabled(true);
				plainTextModeButtom->setEnabled(true);
				tableModeButtom->setEnabled(false);
				withdrawDictButton->setEnabled(!withdrawList.empty());
			});
		connect(refreshDictButton, &ElaPushButton::clicked, this, refreshDictFunc);
		connect(saveDictButton, &ElaPushButton::clicked, this, [=]()
			{
				saveDictFunc(false);
				ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"), tabName + tr(" 已保存"), 3000);
			});
		connect(addDictButton, &ElaPushButton::clicked, this, [=]()
			{
				QModelIndexList index = dictTableView->selectionModel()->selectedIndexes();
				if (index.isEmpty()) {
					dictModel->insertRow(dictModel->rowCount());
				}
				else {
					dictModel->insertRow(index.first().row());
				}
			});
		connect(delDictButton, &ElaPushButton::clicked, this, [=, &withdrawList]()
			{
				QModelIndexList selectedRows = dictTableView->selectionModel()->selectedRows();
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


	const std::vector<fs::path> gptDictPaths = { (_projectDir / tr("项目GPT字典.toml").toStdWString()), (_projectDir / L"项目GPT字典-生成.toml") };
	std::function<QString()> gptReadPlainTextFunc = [=]() -> QString
		{
			return ReadDicts::readGptDictsStr(gptDictPaths);
		};
	std::function<QList<GptDictEntry>()> gptReadEntriesFunc = [=]() -> QList<GptDictEntry>
		{
			return ReadDicts::readGptDicts(gptDictPaths);
		};
	auto refreshAndSaveGptDictFunc = 
		createDictTabFunc(gptReadPlainTextFunc, gptReadEntriesFunc, _withdrawGptList, "gpt", QString::fromStdWString(gptDictPaths.front().stem().wstring()));


	fs::path preDictPath = _projectDir / tr("项目译前字典.toml").toStdWString();
	std::function<QString()> preReadPlainTextFunc = [=]() -> QString
		{
			return ReadDicts::readDictsStr(preDictPath);
		};
	std::function<QList<NormalDictEntry>()> preReadEntriesFunc = [=]() -> QList<NormalDictEntry>
		{
			return ReadDicts::readNormalDicts(preDictPath);
		};
	auto refreshAndSavePreDictFunc = 
		createDictTabFunc(preReadPlainTextFunc, preReadEntriesFunc, _withdrawPreList, "pre", QString::fromStdWString(preDictPath.stem().wstring()));
	

	fs::path postDictPath = _projectDir / tr("项目译后字典.toml").toStdWString();
	std::function<QString()> postReadPlainTextFunc = [=]() -> QString
		{
			return ReadDicts::readDictsStr(postDictPath);
		};
	std::function<QList<NormalDictEntry>()> postReadEntriesFunc = [=]() -> QList<NormalDictEntry>
		{
			return ReadDicts::readNormalDicts(postDictPath);
		};
	auto refreshAndSavePostDictFunc = 
		createDictTabFunc(postReadPlainTextFunc, postReadEntriesFunc, _withdrawPostList, "post", QString::fromStdWString(postDictPath.stem().wstring()));


	_refreshFunc = [=]()
		{
			refreshAndSaveGptDictFunc.first();
			//refreshAndSavePreDictFunc.first();
			//refreshAndSavePostDictFunc.first();
		};


	_applyFunc = [=]()
		{
			refreshAndSaveGptDictFunc.second(false);
			refreshAndSavePreDictFunc.second(false);
			refreshAndSavePostDictFunc.second(false);
		};

	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
