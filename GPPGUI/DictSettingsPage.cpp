#include "DictSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaIconButton.h"
#include "ElaTabWidget.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMultiSelectComboBox.h"
#include "ElaMessageBar.h"
#include "ElaToggleSwitch.h"
#include "ElaPlainTextEdit.h"

import Tool;

DictSettingsPage::DictSettingsPage(fs::path& projectDir, toml::table& globalConfig, toml::table& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir)
{
	setWindowTitle("项目字典设置");
	setTitleVisible(false);
	setContentsMargins(0, 0, 0, 0);

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

QList<DictionaryEntry> DictSettingsPage::readGptDicts()
{
	QList<DictionaryEntry> result;
	fs::path gptDictPath = _projectDir / L"项目GPT字典.toml";
	fs::path genDictPath = _projectDir / L"项目GPT字典-生成.toml";
	auto readDict = [&](const fs::path& dictPath)
		{
			std::ifstream ifs(dictPath);
			toml::table tbl;
			try {
				tbl = toml::parse(ifs);
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败",
					QString(dictPath.filename().wstring()) + "不符合规范", 3000);
				return;
			}
			ifs.close();
			auto dictArr = tbl["gptDict"].as_array();
			if (!dictArr) {
				return;
			}
			for (const auto& elem : *dictArr) {
				auto dict = elem.as_table();
				if (!dict) {
					continue;
				}
				DictionaryEntry entry;
				entry.original = dict->contains("org") ? (*dict)["org"].value_or("") :
					(*dict)["searchStr"].value_or("");
				entry.translation = dict->contains("rep") ? QString::fromStdString((*dict)["rep"].value_or("")) :
					(*dict)["replaceStr"].value_or("");
				entry.description = dict->contains("note") ? (*dict)["note"].value_or("") : QString{};
				result.push_back(entry);
			}
		};
	if (fs::exists(gptDictPath)) {
		readDict(gptDictPath);
	}
	if (fs::exists(genDictPath)) {
		readDict(genDictPath);
	}
	return result;
}
QString DictSettingsPage::readGptDictsStr()
{
	std::string result= "gptDict = [\n";
	bool gptOk = true;
	fs::path gptDictPath = _projectDir / L"项目GPT字典.toml";
	fs::path genDictPath = _projectDir / L"项目GPT字典-生成.toml";
	auto readDict = [&](const fs::path& dictPath) -> bool
		{
			std::ifstream ifs(dictPath);
			toml::table tbl;
			try {
				tbl = toml::parse(ifs);
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败",
					QString(dictPath.filename().wstring()) + "不符合规范", 3000);
				return false;
			}
			ifs.close();
			auto dictArr = tbl["gptDict"].as_array();
			if (!dictArr) {
				return false;
			}
			for (const auto& elem : *dictArr) {
				auto dict = elem.as_table();
				if (!dict) {
					continue;
				}
				std::string original = dict->contains("org") ? (*dict)["org"].value_or("") :
					(*dict)["searchStr"].value_or("");
				std::string translation = dict->contains("rep") ? (*dict)["rep"].value_or("") :
					(*dict)["replaceStr"].value_or("");
				std::string description = dict->contains("note") ? (*dict)["note"].value_or("") : std::string{};
				toml::table tbl{ {"org", original }, { "rep", translation }, { "note", description } };
				result += std::format("\t{{ org = {}, rep = {}, note = {} }},\n",
					stream2String(tbl["org"]), stream2String(tbl["rep"]), stream2String(tbl["note"]));
			}
			return true;
		};
	if (fs::exists(gptDictPath)) {
		gptOk = readDict(gptDictPath);
	}
	if (fs::exists(genDictPath)) {
		readDict(genDictPath);
	}
	if (gptOk) {
		result += "]\n";
	}
	else {
		std::ifstream ifs(gptDictPath);
		result = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		ifs.close();
	}
	return QString::fromStdString(result);
}

QList<NormalDictEntry> DictSettingsPage::readNormalDicts(const fs::path& dictPath)
{
	QList<NormalDictEntry> result;
	if (!fs::exists(dictPath)) {
		return result;
	}
	std::ifstream ifs(dictPath);
	toml::table tbl;
	try {
		tbl = toml::parse(ifs);
	}
	catch (...) {
		ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败",
			QString(dictPath.filename().wstring()) + " 不符合规范", 3000);
		return result;
	}
	ifs.close();
	auto dictArr = tbl["normalDict"].as_array();
	if (!dictArr) {
		return result;
	}
	for (const auto& elem : *dictArr) {
		auto dict = elem.as_table();
		if (!dict) {
			continue;
		}
		NormalDictEntry entry;
		entry.original = dict->contains("org") ? (*dict)["org"].value_or("") :
			(*dict)["searchStr"].value_or("");
		entry.translation = dict->contains("rep") ? (*dict)["rep"].value_or("") :
			(*dict)["replaceStr"].value_or("");
		entry.conditionTar = (*dict)["conditionTarget"].value_or("");
		entry.conditionReg = (*dict)["conditionReg"].value_or("");
		entry.isReg = (*dict)["isReg"].value_or(false);
		entry.priority = (*dict)["priority"].value_or(0);
		result.push_back(entry);
	}
	return result;
}

QString DictSettingsPage::readNormalDictsStr(const fs::path& dictPath)
{
	if (!fs::exists(dictPath)) {
		return {};
	}
	std::ifstream ifs(dictPath);
	std::string result((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	return QString::fromStdString(result);
}



void DictSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);


	QWidget* gptDictWidget = new QWidget(mainWidget);
	QVBoxLayout* gptDictLayout = new QVBoxLayout(gptDictWidget);
	QWidget* gptButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* gptButtonLayout = new QHBoxLayout(gptButtonWidget);
	ElaPushButton* gptPlainTextModeButtom = new ElaPushButton(gptButtonWidget);
	gptPlainTextModeButtom->setText("纯文本模式");
	ElaPushButton* gptTableModeButtom = new ElaPushButton(gptButtonWidget);
	gptTableModeButtom->setText("表模式");

	ElaIconButton* saveGptDictButton = new ElaIconButton(ElaIconType::Check, gptButtonWidget);
	saveGptDictButton->setFixedWidth(30);
	ElaToolTip* saveGptDictButtonToolTip = new ElaToolTip(saveGptDictButton);
	saveGptDictButtonToolTip->setToolTip("保存当前页");
	ElaIconButton* withdrawGptDictButton = new ElaIconButton(ElaIconType::ArrowLeft, gptButtonWidget);
	withdrawGptDictButton->setFixedWidth(30);
	ElaToolTip* withdrawGptDictButtonToolTip = new ElaToolTip(withdrawGptDictButton);
	withdrawGptDictButtonToolTip->setToolTip("撤回删除行");
	withdrawGptDictButton->setEnabled(false);
	ElaIconButton* refreshGptDictButton = new ElaIconButton(ElaIconType::ArrowRotateRight, gptButtonWidget);
	refreshGptDictButton->setFixedWidth(30);
	ElaToolTip* refreshGptDictButtonToolTip = new ElaToolTip(refreshGptDictButton);
	refreshGptDictButtonToolTip->setToolTip("刷新当前页");
	ElaIconButton* addGptDictButton = new ElaIconButton(ElaIconType::Plus, gptButtonWidget);
	addGptDictButton->setFixedWidth(30);
	ElaToolTip* addGptDictButtonToolTip = new ElaToolTip(addGptDictButton);
	addGptDictButtonToolTip->setToolTip("添加词条");
	ElaIconButton* delGptDictButton = new ElaIconButton(ElaIconType::Minus, gptButtonWidget);
	delGptDictButton->setFixedWidth(30);
	ElaToolTip* delGptDictButtonToolTip = new ElaToolTip(delGptDictButton);
	delGptDictButtonToolTip->setToolTip("删除词条");
	gptButtonLayout->addWidget(gptPlainTextModeButtom);
	gptButtonLayout->addWidget(gptTableModeButtom);
	gptButtonLayout->addStretch();
	gptButtonLayout->addWidget(saveGptDictButton);
	gptButtonLayout->addWidget(withdrawGptDictButton);
	gptButtonLayout->addWidget(refreshGptDictButton);
	gptButtonLayout->addWidget(addGptDictButton);
	gptButtonLayout->addWidget(delGptDictButton);
	gptDictLayout->addWidget(gptButtonWidget, 0, Qt::AlignTop);


	// 每个字典里又有一个StackedWidget区分表和纯文本

	QStackedWidget* gptStackedWidget = new QStackedWidget(gptDictWidget);
	// 项目GPT字典的纯文本模式
	ElaPlainTextEdit* gptPlainTextEdit = new ElaPlainTextEdit(gptStackedWidget);
	QFont plainTextFont = gptPlainTextEdit->font();
	plainTextFont.setPixelSize(15);
	gptPlainTextEdit->setFont(plainTextFont);
	gptPlainTextEdit->setPlainText(readGptDictsStr());
	gptStackedWidget->addWidget(gptPlainTextEdit);

	// 项目GPT字典的表格模式
	ElaTableView* gptDictTableView = new ElaTableView(gptStackedWidget);
	DictionaryModel* gptDictModel = new DictionaryModel(gptDictTableView);
	QList<DictionaryEntry> gptData = readGptDicts();
	gptDictModel->loadData(gptData);
	gptDictTableView->setModel(gptDictModel);
	QFont tableHeaderFont = gptDictTableView->horizontalHeader()->font();
	tableHeaderFont.setPixelSize(16);
	gptDictTableView->horizontalHeader()->setFont(tableHeaderFont);
	gptDictTableView->verticalHeader()->setHidden(true);
	gptDictTableView->setAlternatingRowColors(true);
	gptDictTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	gptDictTableView->setColumnWidth(0, _projectConfig["GUIConfig"]["gptDictTableColumnWidth"]["0"].value_or(175));
	gptDictTableView->setColumnWidth(1, _projectConfig["GUIConfig"]["gptDictTableColumnWidth"]["1"].value_or(175));
	gptDictTableView->setColumnWidth(2, _projectConfig["GUIConfig"]["gptDictTableColumnWidth"]["2"].value_or(425));
	gptStackedWidget->addWidget(gptDictTableView);
	gptStackedWidget->setCurrentIndex(_projectConfig["GUIConfig"]["gptDictTableOpenMode"].value_or(_globalConfig["defaultDictOpenMode"].value_or(0)));

	gptPlainTextModeButtom->setEnabled(gptStackedWidget->currentIndex() != 0);
	gptTableModeButtom->setEnabled(gptStackedWidget->currentIndex() != 1);
	addGptDictButton->setEnabled(gptStackedWidget->currentIndex() == 1);
	delGptDictButton->setEnabled(gptStackedWidget->currentIndex() == 1);

	gptDictLayout->addWidget(gptStackedWidget, 1);
	auto refreshGptDictFunc = [=]()
		{
			gptPlainTextEdit->setPlainText(readGptDictsStr());
			gptDictModel->loadData(readGptDicts());
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "刷新成功", "重新载入了项目GPT字典", 3000);
		};
	auto saveGptDictFunc = [=]()
		{
			std::ofstream ofs(_projectDir / L"项目GPT字典.toml");

			if (fs::exists(_projectDir / L"项目GPT字典-生成.toml")) {
				fs::remove(_projectDir / L"项目GPT字典-生成.toml");
			}

			if (gptStackedWidget->currentIndex() == 0) {
				ofs << gptPlainTextEdit->toPlainText().toStdString();
				ofs.close();
				gptDictModel->loadData(readGptDicts());
			}
			else if (gptStackedWidget->currentIndex() == 1) {
				toml::array gptDictArr;
				QList<DictionaryEntry> gptEntries = gptDictModel->getEntries();
				for (const auto& entry : gptEntries) {
					toml::table gptDictTbl;
					gptDictTbl.insert("searchStr", entry.original.toStdString());
					gptDictTbl.insert("replaceStr", entry.translation.toStdString());
					gptDictTbl.insert("note", entry.description.toStdString());
					gptDictArr.push_back(gptDictTbl);
				}
				ofs << toml::table{ {"gptDict", gptDictArr} };
				ofs.close();
				gptPlainTextEdit->setPlainText(readGptDictsStr());
			}

			insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.0", gptDictTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.1", gptDictTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.2", gptDictTableView->columnWidth(2));
		};
	connect(gptPlainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			gptStackedWidget->setCurrentIndex(0);
			addGptDictButton->setEnabled(false);
			delGptDictButton->setEnabled(false);
			gptPlainTextModeButtom->setEnabled(false);
			gptTableModeButtom->setEnabled(true);
			withdrawGptDictButton->setEnabled(false);
		});
	connect(gptTableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			gptStackedWidget->setCurrentIndex(1);
			addGptDictButton->setEnabled(true);
			delGptDictButton->setEnabled(true);
			gptPlainTextModeButtom->setEnabled(true);
			gptTableModeButtom->setEnabled(false);
			withdrawGptDictButton->setEnabled(!_withdrawGptList.empty());
		});
	connect(refreshGptDictButton, &ElaPushButton::clicked, this, refreshGptDictFunc);
	connect(saveGptDictButton, &ElaPushButton::clicked, this, [=]()
		{
			saveGptDictFunc();
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "保存成功", "已保存项目GPT字典", 3000);
		});
	connect(addGptDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList index = gptDictTableView->selectionModel()->selectedIndexes();
			if (index.isEmpty()) {
				gptDictModel->insertRow(gptDictModel->rowCount());
			}
			else {
				gptDictModel->insertRow(index.first().row());
			}
		});
	connect(delGptDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList selectedRows = gptDictTableView->selectionModel()->selectedRows();
			const QList<DictionaryEntry>& entries = gptDictModel->getEntriesRef();
			std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
				{
					return a.row() > b.row();
				});
			for (const QModelIndex& index : selectedRows) {
				if (_withdrawGptList.size() > 100) {
					_withdrawGptList.pop_front();
				}
				_withdrawGptList.push_back(entries[index.row()]);
				gptDictModel->removeRow(index.row());
			}
			if (!_withdrawGptList.empty()) {
				withdrawGptDictButton->setEnabled(true);
			}
		});
	connect(withdrawGptDictButton, &ElaPushButton::clicked, this, [=]()
		{
			if (_withdrawGptList.empty()) {
				return;
			}
			DictionaryEntry entry = _withdrawGptList.front();
			_withdrawGptList.pop_front();
			gptDictModel->insertRow(0, entry);
			if (_withdrawGptList.empty()) {
				withdrawGptDictButton->setEnabled(false);
			}
		});
	tabWidget->addTab(gptDictWidget, "项目GPT字典");

	QWidget* preDictWidget = new QWidget(mainWidget);
	QVBoxLayout* preDictLayout = new QVBoxLayout(preDictWidget);
	QWidget* preButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* preButtonLayout = new QHBoxLayout(preButtonWidget);
	ElaPushButton* prePlainTextModeButtom = new ElaPushButton(preButtonWidget);
	prePlainTextModeButtom->setText("纯文本模式");
	ElaPushButton* preTableModeButtom = new ElaPushButton(preButtonWidget);
	preTableModeButtom->setText("表模式");
	ElaIconButton* savePreDictButton = new ElaIconButton(ElaIconType::Check, preButtonWidget);
	savePreDictButton->setFixedWidth(30);
	ElaToolTip* savePreDictButtonToolTip = new ElaToolTip(savePreDictButton);
	savePreDictButtonToolTip->setToolTip("保存当前页");
	ElaIconButton* withdrawPreDictButton = new ElaIconButton(ElaIconType::ArrowLeft, preButtonWidget);
	withdrawPreDictButton->setFixedWidth(30);
	ElaToolTip* withdrawPreDictButtonToolTip = new ElaToolTip(withdrawPreDictButton);
	withdrawPreDictButtonToolTip->setToolTip("撤回删除行");
	withdrawPreDictButton->setEnabled(false);
	ElaIconButton* refreshPreDictButton = new ElaIconButton(ElaIconType::ArrowRotateRight, preButtonWidget);
	refreshPreDictButton->setFixedWidth(30);
	ElaToolTip* refreshPreDictButtonToolTip = new ElaToolTip(refreshPreDictButton);
	refreshPreDictButtonToolTip->setToolTip("刷新当前页");
	ElaIconButton* addPreDictButton = new ElaIconButton(ElaIconType::Plus, preButtonWidget);
	addPreDictButton->setFixedWidth(30);
	ElaToolTip* addPreDictButtonToolTip = new ElaToolTip(addPreDictButton);
	addPreDictButtonToolTip->setToolTip("添加词条");
	ElaIconButton* delPreDictButton = new ElaIconButton(ElaIconType::Minus, preButtonWidget);
	delPreDictButton->setFixedWidth(30);
	ElaToolTip* delPreDictButtonToolTip = new ElaToolTip(delPreDictButton);
	delPreDictButtonToolTip->setToolTip("删除词条");
	preButtonLayout->addWidget(prePlainTextModeButtom);
	preButtonLayout->addWidget(preTableModeButtom);
	preButtonLayout->addStretch();
	preButtonLayout->addWidget(savePreDictButton);
	preButtonLayout->addWidget(withdrawPreDictButton);
	preButtonLayout->addWidget(refreshPreDictButton);
	preButtonLayout->addWidget(addPreDictButton);
	preButtonLayout->addWidget(delPreDictButton);
	preDictLayout->addWidget(preButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* preStackedWidget = new QStackedWidget(preDictWidget);
	// 项目译前字典的纯文本模式
	ElaPlainTextEdit* prePlainTextEdit = new ElaPlainTextEdit(preStackedWidget);
	prePlainTextEdit->setFont(plainTextFont);
	prePlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译前.toml"));
	preStackedWidget->addWidget(prePlainTextEdit);

	// 项目译前字典的表格模式
	ElaTableView* preDictTableView = new ElaTableView(mainWidget);
	NormalDictModel* preDictModel = new NormalDictModel(preDictTableView);
	QList<NormalDictEntry> preData = readNormalDicts(_projectDir / L"项目字典_译前.toml");
	preDictModel->loadData(preData);
	preDictTableView->setModel(preDictModel);
	preDictTableView->horizontalHeader()->setFont(tableHeaderFont);
	preDictTableView->verticalHeader()->setHidden(true);
	preDictTableView->setAlternatingRowColors(true);
	preDictTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	preDictTableView->setColumnWidth(0, _projectConfig["GUIConfig"]["preDictTableColumnWidth"]["0"].value_or(200));
	preDictTableView->setColumnWidth(1, _projectConfig["GUIConfig"]["preDictTableColumnWidth"]["1"].value_or(150));
	preDictTableView->setColumnWidth(2, _projectConfig["GUIConfig"]["preDictTableColumnWidth"]["2"].value_or(100));
	preDictTableView->setColumnWidth(3, _projectConfig["GUIConfig"]["preDictTableColumnWidth"]["3"].value_or(172));
	preDictTableView->setColumnWidth(4, _projectConfig["GUIConfig"]["preDictTableColumnWidth"]["4"].value_or(75));
	preDictTableView->setColumnWidth(5, _projectConfig["GUIConfig"]["preDictTableColumnWidth"]["5"].value_or(60));
	preStackedWidget->addWidget(preDictTableView);
	preStackedWidget->setCurrentIndex(_projectConfig["GUIConfig"]["preDictTableOpenMode"].value_or(_globalConfig["defaultDictOpenMode"].value_or(0)));

	prePlainTextModeButtom->setEnabled(preStackedWidget->currentIndex() != 0);
	preTableModeButtom->setEnabled(preStackedWidget->currentIndex() != 1);
	addPreDictButton->setEnabled(preStackedWidget->currentIndex() == 1);
	delPreDictButton->setEnabled(preStackedWidget->currentIndex() == 1);

	preDictLayout->addWidget(preStackedWidget, 1);
	auto refreshPreDictFunc = [=]()
		{
			prePlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译前.toml"));
			preDictModel->loadData(readNormalDicts(_projectDir / L"项目字典_译前.toml"));
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "刷新成功", "重新载入了项目译前字典", 3000);
		};
	auto savePreDictFunc = [=]()
		{
			std::ofstream ofs(_projectDir / L"项目字典_译前.toml");
			if (preStackedWidget->currentIndex() == 0) {
				ofs << prePlainTextEdit->toPlainText().toStdString();
				ofs.close();
				preDictModel->loadData(readNormalDicts(_projectDir / L"项目字典_译前.toml"));
			}
			else if (preStackedWidget->currentIndex() == 1) {
				toml::array preDictArr;
				QList<NormalDictEntry> preEntries = preDictModel->getEntries();
				for (const auto& entry : preEntries) {
					toml::table preDictTbl;
					preDictTbl.insert("searchStr", entry.original.toStdString());
					preDictTbl.insert("replaceStr", entry.translation.toStdString());
					preDictTbl.insert("conditionTarget", entry.conditionTar.toStdString());
					preDictTbl.insert("conditionReg", entry.conditionReg.toStdString());
					preDictTbl.insert("isReg", entry.isReg);
					preDictTbl.insert("priority", entry.priority);
					preDictArr.push_back(preDictTbl);
				}
				ofs << toml::table{ {"normalDict", preDictArr} };
				ofs.close();
				prePlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译前.toml"));
			}
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.0", preDictTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.1", preDictTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.2", preDictTableView->columnWidth(2));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.3", preDictTableView->columnWidth(3));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.4", preDictTableView->columnWidth(4));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.5", preDictTableView->columnWidth(5));
		};
	connect(prePlainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			preStackedWidget->setCurrentIndex(0);
			addPreDictButton->setEnabled(false);
			delPreDictButton->setEnabled(false);
			prePlainTextModeButtom->setEnabled(false);
			preTableModeButtom->setEnabled(true);
			withdrawPreDictButton->setEnabled(false);
		});
	connect(preTableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			preStackedWidget->setCurrentIndex(1);
			addPreDictButton->setEnabled(true);
			delPreDictButton->setEnabled(true);
			prePlainTextModeButtom->setEnabled(true);
			preTableModeButtom->setEnabled(false);
			withdrawPreDictButton->setEnabled(!_withdrawPreList.empty());
		});
	connect(refreshPreDictButton, &ElaPushButton::clicked, this, refreshPreDictFunc);
	connect(savePreDictButton, &ElaPushButton::clicked, this, [=]()
		{
			savePreDictFunc();
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "保存成功", "已保存项目译前字典", 3000);
		});
	connect(addPreDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList index = preDictTableView->selectionModel()->selectedIndexes();
			if (index.isEmpty()) {
				preDictModel->insertRow(preDictModel->rowCount());
			}
			else {
				preDictModel->insertRow(index.first().row());
			}
		});
	connect(delPreDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList selectedRows = preDictTableView->selectionModel()->selectedRows();
			const QList<NormalDictEntry>& entries = preDictModel->getEntriesRef();
			std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
				{
					return a.row() > b.row();
				});
			for (const QModelIndex& index : selectedRows) {
				if (_withdrawPreList.size() > 100) {
					_withdrawPreList.pop_front();
				}
				_withdrawPreList.push_back(entries[index.row()]);
				preDictModel->removeRow(index.row());
			}
			if (!_withdrawPreList.empty()) {
				withdrawPreDictButton->setEnabled(true);
			}
		});
	connect(withdrawPreDictButton, &ElaPushButton::clicked, this, [=]()
		{
			if (_withdrawPreList.empty()) {
				return;
			}
			NormalDictEntry entry = _withdrawPreList.front();
			_withdrawPreList.pop_front();
			preDictModel->insertRow(0, entry);
			if (_withdrawPreList.empty()) {
				withdrawPreDictButton->setEnabled(false);
			}
		});
	tabWidget->addTab(preDictWidget, "项目译前字典");

	QWidget* postDictWidget = new QWidget(mainWidget);
	QVBoxLayout* postDictLayout = new QVBoxLayout(postDictWidget);
	QWidget* postButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* postButtonLayout = new QHBoxLayout(postButtonWidget);
	ElaPushButton* postPlainTextModeButtom = new ElaPushButton(postButtonWidget);
	postPlainTextModeButtom->setText("纯文本模式");
	ElaPushButton* postTableModeButtom = new ElaPushButton(postButtonWidget);
	postTableModeButtom->setText("表模式");
	ElaIconButton* savePostDictButton = new ElaIconButton(ElaIconType::Check, postButtonWidget);
	savePostDictButton->setFixedWidth(30);
	ElaToolTip* savePostDictButtonToolTip = new ElaToolTip(savePostDictButton);
	savePostDictButtonToolTip->setToolTip("保存当前页");
	ElaIconButton* withdrawPostDictButton = new ElaIconButton(ElaIconType::ArrowLeft, postButtonWidget);
	withdrawPostDictButton->setFixedWidth(30);
	ElaToolTip* withdrawPostDictButtonToolTip = new ElaToolTip(withdrawPostDictButton);
	withdrawPostDictButtonToolTip->setToolTip("撤回删除行");
	withdrawPostDictButton->setEnabled(false);
	ElaIconButton* refreshPostDictButton = new ElaIconButton(ElaIconType::ArrowRotateRight, postButtonWidget);
	refreshPostDictButton->setFixedWidth(30);
	ElaToolTip* refreshPostDictButtonToolTip = new ElaToolTip(refreshPostDictButton);
	refreshPostDictButtonToolTip->setToolTip("刷新当前页");
	ElaIconButton* addPostDictButton = new ElaIconButton(ElaIconType::Plus, postButtonWidget);
	addPostDictButton->setFixedWidth(30);
	ElaToolTip* addPostDictButtonToolTip = new ElaToolTip(addPostDictButton);
	addPostDictButtonToolTip->setToolTip("添加词条");
	ElaIconButton* delPostDictButton = new ElaIconButton(ElaIconType::Minus, postButtonWidget);
	delPostDictButton->setFixedWidth(30);
	ElaToolTip* delPostDictButtonToolTip = new ElaToolTip(delPostDictButton);
	delPostDictButtonToolTip->setToolTip("删除词条");
	postButtonLayout->addWidget(postPlainTextModeButtom);
	postButtonLayout->addWidget(postTableModeButtom);
	postButtonLayout->addStretch();
	postButtonLayout->addWidget(savePostDictButton);
	postButtonLayout->addWidget(withdrawPostDictButton);
	postButtonLayout->addWidget(refreshPostDictButton);
	postButtonLayout->addWidget(addPostDictButton);
	postButtonLayout->addWidget(delPostDictButton);
	postDictLayout->addWidget(postButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* postStackedWidget = new QStackedWidget(postDictWidget);
	// 项目译后字典的纯文本模式
	ElaPlainTextEdit* postPlainTextEdit = new ElaPlainTextEdit(postStackedWidget);
	postPlainTextEdit->setFont(plainTextFont);
	postPlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译后.toml"));
	postStackedWidget->addWidget(postPlainTextEdit);

	// 项目译后字典的表格模式
	ElaTableView* postDictTableView = new ElaTableView(mainWidget);
	NormalDictModel* postDictModel = new NormalDictModel(postDictTableView);
	QList<NormalDictEntry> postData = readNormalDicts(_projectDir / L"项目字典_译后.toml");
	postDictModel->loadData(postData);
	postDictTableView->setModel(postDictModel);
	postDictTableView->horizontalHeader()->setFont(tableHeaderFont);
	postDictTableView->verticalHeader()->setHidden(true);
	postDictTableView->setAlternatingRowColors(true);
	postDictTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	postDictTableView->setColumnWidth(0, _projectConfig["GUIConfig"]["postDictTableColumnWidth"]["0"].value_or(200));
	postDictTableView->setColumnWidth(1, _projectConfig["GUIConfig"]["postDictTableColumnWidth"]["1"].value_or(150));
	postDictTableView->setColumnWidth(2, _projectConfig["GUIConfig"]["postDictTableColumnWidth"]["2"].value_or(100));
	postDictTableView->setColumnWidth(3, _projectConfig["GUIConfig"]["postDictTableColumnWidth"]["3"].value_or(172));
	postDictTableView->setColumnWidth(4, _projectConfig["GUIConfig"]["postDictTableColumnWidth"]["4"].value_or(75));
	postDictTableView->setColumnWidth(5, _projectConfig["GUIConfig"]["postDictTableColumnWidth"]["5"].value_or(60));
	postStackedWidget->addWidget(postDictTableView);
	postStackedWidget->setCurrentIndex(_projectConfig["GUIConfig"]["postDictTableOpenMode"].value_or(_globalConfig["defaultDictOpenMode"].value_or(0)));

	postPlainTextModeButtom->setEnabled(postStackedWidget->currentIndex() != 0);
	postTableModeButtom->setEnabled(postStackedWidget->currentIndex() != 1);
	addPostDictButton->setEnabled(postStackedWidget->currentIndex() == 1);
	delPostDictButton->setEnabled(postStackedWidget->currentIndex() == 1);

	postDictLayout->addWidget(postStackedWidget, 1);
	auto refreshPostDictFunc = [=]()
		{
			postPlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译后.toml"));
			postDictModel->loadData(readNormalDicts(_projectDir / L"项目字典_译后.toml"));
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "刷新成功", "重新载入了项目译后字典", 3000);
		};
	auto savePostDictFunc = [=]()
		{
			std::ofstream ofs(_projectDir / L"项目字典_译后.toml");
			if (postStackedWidget->currentIndex() == 0) {
				ofs << postPlainTextEdit->toPlainText().toStdString();
				ofs.close();
				postDictModel->loadData(readNormalDicts(_projectDir / L"项目字典_译后.toml"));
			}
			else if (postStackedWidget->currentIndex() == 1) {
				toml::array postDictArr;
				QList<NormalDictEntry> postEntries = postDictModel->getEntries();
				for (const auto& entry : postEntries) {
					toml::table postDictTbl;
					postDictTbl.insert("searchStr", entry.original.toStdString());
					postDictTbl.insert("replaceStr", entry.translation.toStdString());
					postDictTbl.insert("conditionTarget", entry.conditionTar.toStdString());
					postDictTbl.insert("conditionReg", entry.conditionReg.toStdString());
					postDictTbl.insert("isReg", entry.isReg);
					postDictTbl.insert("priority", entry.priority);
					postDictArr.push_back(postDictTbl);
				}
				ofs << toml::table{ {"normalDict", postDictArr} };
				ofs.close();
				postPlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译后.toml"));
			}
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.0", postDictTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.1", postDictTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.2", postDictTableView->columnWidth(2));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.3", postDictTableView->columnWidth(3));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.4", postDictTableView->columnWidth(4));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.5", postDictTableView->columnWidth(5));
		};
	connect(postPlainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			postStackedWidget->setCurrentIndex(0);
			addPostDictButton->setEnabled(false);
			delPostDictButton->setEnabled(false);
			postPlainTextModeButtom->setEnabled(false);
			postTableModeButtom->setEnabled(true);
			withdrawPostDictButton->setEnabled(false);
		});
	connect(postTableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			postStackedWidget->setCurrentIndex(1);
			addPostDictButton->setEnabled(true);
			delPostDictButton->setEnabled(true);
			postPlainTextModeButtom->setEnabled(true);
			postTableModeButtom->setEnabled(false);
			withdrawPostDictButton->setEnabled(!_withdrawPostList.empty());
		});
	connect(refreshPostDictButton, &ElaPushButton::clicked, this, refreshPostDictFunc);
	connect(savePostDictButton, &ElaPushButton::clicked, this, [=]()
		{
			savePostDictFunc();
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "保存成功", "已保存项目译后字典", 3000);
		});
	connect(addPostDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList index = postDictTableView->selectionModel()->selectedIndexes();
			if (index.isEmpty()) {
				postDictModel->insertRow(postDictModel->rowCount());
			}
			else {
				postDictModel->insertRow(index.first().row());
			}
		});
	connect(delPostDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList selectedRows = postDictTableView->selectionModel()->selectedRows();
			const QList<NormalDictEntry>& entries = postDictModel->getEntriesRef();
			std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
				{
					return a.row() > b.row();
				});
			if (!selectedRows.isEmpty()) {
				for (const QModelIndex& index : selectedRows) {
					if (_withdrawPostList.size() > 100) {
						_withdrawPostList.pop_front();
					}
					_withdrawPostList.push_back(entries[index.row()]);
					postDictModel->removeRow(index.row());
				}
			}
			if (!_withdrawPostList.empty()) {
				withdrawPostDictButton->setEnabled(true);
			}
		});
	connect(withdrawPostDictButton, &ElaPushButton::clicked, this, [=]()
		{
			if (_withdrawPostList.empty()) {
				return;
			}
			NormalDictEntry entry = _withdrawPostList.front();
			_withdrawPostList.pop_front();
			postDictModel->insertRow(0, entry);
			if (_withdrawPostList.empty()) {
				withdrawPostDictButton->setEnabled(false);
			}
		});
	tabWidget->addTab(postDictWidget, "项目译后字典");

	_refreshFunc = [=]()
		{
			refreshGptDictFunc();
			//refreshPreDictFunc();
			//refreshPostDictFunc();
		};


	// 说明页
	ElaScrollPageArea* descScrollArea = new ElaScrollPageArea(mainWidget);
	QVBoxLayout* descLayout = new QVBoxLayout(descScrollArea);
	descScrollArea->setMaximumHeight(700);
	ElaText* descText = new ElaText(descScrollArea);
	descText->setTextPixelSize(14);
	descText->setText(R"(译前字典会搜索并替换 original_text 以输出 pre_processed_text 提供给AI，
译后字典会搜索并替换 pre_translated_text 以供 translated_preview 最终输出。

条件对象是指条件正则要作用于的文本，
可以是 name, orig_text, preproc_text, pretrans_text 中的任意一个。

当 启用正则 为 true 时，原文和译文将被视为正则表达式进行替换，优先级越高的字典越先执行。

name 字段在翻译后的处理顺序是 执行GPT字典替换 -> 执行人名表替换 -> 执行译后字典替换
msg 字段则是 执行插件处理 -> 执行译后字典替换 -> 问题分析

人名表和字典用什么模式会在你按开始翻译按钮时决定。
比如说，你在按开始翻译按钮时，人名表是以表模式显示的，项目GPT字典是纯文本模式显示的，
则会先将 人名表 表模式 中的数据保存到 项目文件夹的 人名替换表.toml 中，
将 项目GPT字典 纯文本模式 中的文本原样保存到 项目文件夹的 项目GPT字典.toml 中(会顺带删除 项目GPT字典-生成.toml)，
然后再执行翻译。所以如果在纯文本模式下没有按 toml 格式来编辑，翻译时肯定会报错。

按刷新将会重新从项目文件夹中的 toml 文件读取数据
(人名表读取 人名替换表.toml，项目GPT字典读取 项目GPT字典.toml 和 项目GPT字典-生成.toml)，
如果你在GUI中还有修改了没有保存的数据，请务必先确认备份情况再刷新。
需要特别注意的是，DumpName/GenDict任务会分别重新生成 人名替换表.toml/ 项目GPT字典-生成.toml，
默认也会对应刷新 人名表/项目GPT字典，请务必注意不要被其覆盖掉有用的信息

另，GalTransl++中能够自定义编辑的所有正则均是以字符为单位的，
比如即使emoji表情'😪' 在UTF-8中占用4个字节，在正则中依然是可以以一个.来匹配的。
)");
	descLayout->addWidget(descText);
	descLayout->addStretch();
	tabWidget->addTab(descScrollArea, "说明");


	_applyFunc = [=]()
		{
			saveGptDictFunc();
			savePreDictFunc();
			savePostDictFunc();
			insertToml(_projectConfig, "GUIConfig.gptDictTableOpenMode", gptStackedWidget->currentIndex());
			insertToml(_projectConfig, "GUIConfig.preDictTableOpenMode", preStackedWidget->currentIndex());
			insertToml(_projectConfig, "GUIConfig.postDictTableOpenMode", postStackedWidget->currentIndex());
		};

	mainLayout->addWidget(tabWidget, 1);
	addCentralWidget(mainWidget, true, true, 0);
}
