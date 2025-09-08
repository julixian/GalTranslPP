#include "DictSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaPivot.h"
#include "ElaToggleSwitch.h"
#include "ElaPlainTextEdit.h"

import Tool;

DictSettingsPage::DictSettingsPage(fs::path& projectDir, toml::table& globalConfig, toml::table& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir)
{
	setWindowTitle("项目字典设置");
	setTitleVisible(false);

	_setupUI();
}

DictSettingsPage::~DictSettingsPage()
{

}

void DictSettingsPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
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
				ElaMessageBar::error(ElaMessageBarType::TopRight, "解析失败",
					QString(dictPath.filename().wstring()) + "不符合规范", 3000);
				return;
			}
			ifs.close();
			auto dictArr = tbl["gptDict"].as_array();
			if (!dictArr) {
				return;
			}
			for (const auto& dict : *dictArr) {
				DictionaryEntry entry;
				entry.original = dict.as_table()->contains("org") ? QString::fromStdString((*dict.as_table())["org"].value_or("")) : 
					QString::fromStdString((*dict.as_table())["searchStr"].value_or(""));
				entry.translation = dict.as_table()->contains("rep") ? QString::fromStdString((*dict.as_table())["rep"].value_or("")) : 
					QString::fromStdString((*dict.as_table())["replaceStr"].value_or(""));
				entry.description = dict.as_table()->contains("note") ? QString::fromStdString((*dict.as_table())["note"].value_or("")) : "";
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
				ElaMessageBar::error(ElaMessageBarType::TopRight, "解析失败",
					QString(dictPath.filename().wstring()) + "不符合规范", 3000);
				return false;
			}
			ifs.close();
			auto dictArr = tbl["gptDict"].as_array();
			if (!dictArr) {
				return false;
			}
			for (const auto& dict : *dictArr) {
				std::string original = dict.as_table()->contains("org") ? (*dict.as_table())["org"].value_or("") :
					(*dict.as_table())["searchStr"].value_or("");
				std::string translation = dict.as_table()->contains("rep") ? (*dict.as_table())["rep"].value_or("") :
					(*dict.as_table())["replaceStr"].value_or("");
				std::string description = dict.as_table()->contains("note") ? (*dict.as_table())["note"].value_or("") : "";
				toml::table tbl{ {"org", original }, { "rep", translation }, { "note", description } };
				result += std::format("    {{ org = {}, rep = {}, note = {} }},",
					stream2String(tbl["org"]), stream2String(tbl["rep"]), stream2String(tbl["note"])) + "\n";
			}
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
		ElaMessageBar::error(ElaMessageBarType::TopRight, "解析失败",
			QString(dictPath.filename().wstring()) + "不符合规范", 3000);
		return result;
	}
	ifs.close();
	auto dictArr = tbl["normalDict"].as_array();
	if (!dictArr) {
		return result;
	}
	for (const auto& dict : *dictArr) {
		NormalDictEntry entry;
		entry.original = dict.as_table()->contains("org") ? QString::fromStdString((*dict.as_table())["org"].value_or("")) : 
			QString::fromStdString((*dict.as_table())["searchStr"].value_or(""));
		entry.translation = dict.as_table()->contains("rep") ? QString::fromStdString((*dict.as_table())["rep"].value_or("")) : 
			QString::fromStdString((*dict.as_table())["replaceStr"].value_or(""));
		entry.conditionTar = (*dict.as_table())["conditionTarget"].value_or("");
		entry.conditionReg = (*dict.as_table())["conditionReg"].value_or("");
		entry.isReg = (*dict.as_table())["isReg"].value_or(false);
		entry.priority = (*dict.as_table())["priority"].value_or(0);
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
	mainLayout->setContentsMargins(0, 0, 0, 0);

	// 创建 Pivot 导航和 StackedWidget 内容区
	_pivot = new ElaPivot(mainWidget);
	_stackedWidget = new QStackedWidget(mainWidget);

	// 建立 Pivot 和 StackedWidget 的连接
	connect(_pivot, &ElaPivot::pivotClicked, _stackedWidget, &QStackedWidget::setCurrentIndex);

	// 把pivot包到scrollarea中
	ElaScrollPageArea* pivotScrollArea = new ElaScrollPageArea(mainWidget);
	QVBoxLayout* pivotLayout = new QVBoxLayout(pivotScrollArea);
	pivotLayout->addWidget(_pivot);

	//QStringList dictNames = { "项目GPT字典", "项目译前字典", "项目译后字典" };
	QWidget* gptDictWidget = new QWidget(mainWidget);
	QVBoxLayout* gptDictLayout = new QVBoxLayout(gptDictWidget);
	QWidget* gptButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* gptButtonLayout = new QHBoxLayout(gptButtonWidget);
	ElaPushButton* gptPlainTextModeButtom = new ElaPushButton(gptButtonWidget);
	gptPlainTextModeButtom->setText("切换至纯文本模式");
	ElaPushButton* gptTableModeButtom = new ElaPushButton(gptButtonWidget);
	gptTableModeButtom->setText("切换至表模式");
	ElaPushButton* refreshGptDictButton = new ElaPushButton(gptButtonWidget);
	refreshGptDictButton->setText("刷新");
	ElaPushButton* addGptDictButton = new ElaPushButton(gptButtonWidget);
	addGptDictButton->setText("添加词条");
	ElaPushButton* delGptDictButton = new ElaPushButton(gptButtonWidget);
	delGptDictButton->setText("删除词条");
	gptButtonLayout->addWidget(gptPlainTextModeButtom);
	gptButtonLayout->addWidget(gptTableModeButtom);
	gptButtonLayout->addStretch();
	gptButtonLayout->addWidget(refreshGptDictButton);
	gptButtonLayout->addWidget(addGptDictButton);
	gptButtonLayout->addWidget(delGptDictButton);
	gptDictLayout->addWidget(gptButtonWidget, 0, Qt::AlignTop);


	// 每个字典StackedWidget里又有一个StackedWidget区分表和纯文本

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
	insertToml(_projectConfig, "GUIConfig.gptDictTableOpenMode", gptStackedWidget->currentIndex());

	addGptDictButton->setEnabled(gptStackedWidget->currentIndex() == 1);
	delGptDictButton->setEnabled(gptStackedWidget->currentIndex() == 1);

	gptDictLayout->addWidget(gptStackedWidget, 1);
	auto refreshGptDictFunc = [=]()
		{
			gptPlainTextEdit->setPlainText(readGptDictsStr());
			gptDictModel->loadData(readGptDicts());
		};
	connect(gptPlainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			gptStackedWidget->setCurrentIndex(0);
			addGptDictButton->setEnabled(false);
			delGptDictButton->setEnabled(false);
			insertToml(_projectConfig, "GUIConfig.gptDictTableOpenMode", 0);
		});
	connect(gptTableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			gptStackedWidget->setCurrentIndex(1);
			addGptDictButton->setEnabled(true);
			delGptDictButton->setEnabled(true);
			insertToml(_projectConfig, "GUIConfig.gptDictTableOpenMode", 1);
		});
	connect(refreshGptDictButton, &ElaPushButton::clicked, this, refreshGptDictFunc);
	connect(addGptDictButton, &ElaPushButton::clicked, this, [=]()
		{
			gptDictModel->insertRow(gptDictModel->rowCount());
		});
	connect(delGptDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList selectedRows = gptDictTableView->selectionModel()->selectedRows();
			if (!selectedRows.isEmpty()) {
				std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
					{
						return a.row() > b.row();
					});
				for (const QModelIndex& index : selectedRows) {
					gptDictModel->removeRow(index.row());
				}
			}
		});
	_pivot->appendPivot("项目GPT字典");
	_stackedWidget->addWidget(gptDictWidget);

	QWidget* preDictWidget = new QWidget(mainWidget);
	QVBoxLayout* preDictLayout = new QVBoxLayout(preDictWidget);
	QWidget* preButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* preButtonLayout = new QHBoxLayout(preButtonWidget);
	ElaPushButton* prePlainTextModeButtom = new ElaPushButton(preButtonWidget);
	prePlainTextModeButtom->setText("切换至纯文本模式");
	ElaPushButton* preTableModeButtom = new ElaPushButton(preButtonWidget);
	preTableModeButtom->setText("切换至表模式");
	ElaPushButton* refreshPreDictButton = new ElaPushButton(preButtonWidget);
	refreshPreDictButton->setText("刷新");
	ElaPushButton* addPreDictButton = new ElaPushButton(preButtonWidget);
	addPreDictButton->setText("添加词条");
	ElaPushButton* delPreDictButton = new ElaPushButton(preButtonWidget);
	delPreDictButton->setText("删除词条");
	preButtonLayout->addWidget(prePlainTextModeButtom);
	preButtonLayout->addWidget(preTableModeButtom);
	preButtonLayout->addStretch();
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
	insertToml(_projectConfig, "GUIConfig.preDictTableOpenMode", preStackedWidget->currentIndex());

	addPreDictButton->setEnabled(preStackedWidget->currentIndex() == 1);
	delPreDictButton->setEnabled(preStackedWidget->currentIndex() == 1);

	preDictLayout->addWidget(preStackedWidget, 1);
	auto refreshPreDictFunc = [=]()
		{
			prePlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译前.toml"));
			preDictModel->loadData(readNormalDicts(_projectDir / L"项目字典_译前.toml"));
		};
	connect(prePlainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			preStackedWidget->setCurrentIndex(0);
			addPreDictButton->setEnabled(false);
			delPreDictButton->setEnabled(false);
			insertToml(_projectConfig, "GUIConfig.preDictTableOpenMode", 0);
		});
	connect(preTableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			preStackedWidget->setCurrentIndex(1);
			addPreDictButton->setEnabled(true);
			delPreDictButton->setEnabled(true);
			insertToml(_projectConfig, "GUIConfig.preDictTableOpenMode", 1);
		});
	connect(refreshPreDictButton, &ElaPushButton::clicked, this, refreshPreDictFunc);
	connect(addPreDictButton, &ElaPushButton::clicked, this, [=]()
		{
			preDictModel->insertRow(preDictModel->rowCount());
		});
	connect(delPreDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList selectedRows = preDictTableView->selectionModel()->selectedRows();
			std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
				{
					return a.row() > b.row();
				});
			if (!selectedRows.isEmpty()) {
				for (const QModelIndex& index : selectedRows) {
					preDictModel->removeRow(index.row());
				}
			}
		});
	_pivot->appendPivot("项目译前字典");
	_stackedWidget->addWidget(preDictWidget);

	QWidget* postDictWidget = new QWidget(mainWidget);
	QVBoxLayout* postDictLayout = new QVBoxLayout(postDictWidget);
	QWidget* postButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* postButtonLayout = new QHBoxLayout(postButtonWidget);
	ElaPushButton* postPlainTextModeButtom = new ElaPushButton(postButtonWidget);
	postPlainTextModeButtom->setText("切换至纯文本模式");
	ElaPushButton* postTableModeButtom = new ElaPushButton(postButtonWidget);
	postTableModeButtom->setText("切换至表模式");
	ElaPushButton* refreshPostDictButton = new ElaPushButton(postButtonWidget);
	refreshPostDictButton->setText("刷新");
	ElaPushButton* addPostDictButton = new ElaPushButton(postButtonWidget);
	addPostDictButton->setText("添加词条");
	ElaPushButton* delPostDictButton = new ElaPushButton(postButtonWidget);
	delPostDictButton->setText("删除词条");
	postButtonLayout->addWidget(postPlainTextModeButtom);
	postButtonLayout->addWidget(postTableModeButtom);
	postButtonLayout->addStretch();
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
	insertToml(_projectConfig, "GUIConfig.postDictTableOpenMode", postStackedWidget->currentIndex());

	addPostDictButton->setEnabled(postStackedWidget->currentIndex() == 1);
	delPostDictButton->setEnabled(postStackedWidget->currentIndex() == 1);

	postDictLayout->addWidget(postStackedWidget, 1);
	auto refreshPostDictFunc = [=]()
		{
			postPlainTextEdit->setPlainText(readNormalDictsStr(_projectDir / L"项目字典_译后.toml"));
			postDictModel->loadData(readNormalDicts(_projectDir / L"项目字典_译后.toml"));
		};
	connect(postPlainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			postStackedWidget->setCurrentIndex(0);
			addPostDictButton->setEnabled(false);
			delPostDictButton->setEnabled(false);
			insertToml(_projectConfig, "GUIConfig.postDictTableOpenMode", 0);
		});
	connect(postTableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			postStackedWidget->setCurrentIndex(1);
			addPostDictButton->setEnabled(true);
			delPostDictButton->setEnabled(true);
			insertToml(_projectConfig, "GUIConfig.postDictTableOpenMode", 1);
		});
	connect(refreshPostDictButton, &ElaPushButton::clicked, this, refreshPostDictFunc);
	connect(addPostDictButton, &ElaPushButton::clicked, this, [=]()
		{
			postDictModel->insertRow(postDictModel->rowCount());
		});
	connect(delPostDictButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList selectedRows = postDictTableView->selectionModel()->selectedRows();
			std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
				{
					return a.row() > b.row();
				});
			if (!selectedRows.isEmpty()) {
				for (const QModelIndex& index : selectedRows) {
					postDictModel->removeRow(index.row());
				}
			}
		});
	_pivot->appendPivot("项目译后字典");
	_stackedWidget->addWidget(postDictWidget);

	_refreshFunc = [=]()
		{
			refreshGptDictFunc();
			refreshPreDictFunc();
			refreshPostDictFunc();
		};

	_applyFunc = [=]()
		{
			std::ofstream ofs;

			fs::remove(_projectDir / L"项目GPT字典-生成.toml");

			if (gptStackedWidget->currentIndex() == 0) {
				ofs.open(_projectDir / L"项目GPT字典.toml");
				ofs << gptPlainTextEdit->toPlainText().toStdString();
				ofs.close();
			}
			else if (gptStackedWidget->currentIndex() == 1) {
				toml::array gptDictArr;
				QList<DictionaryEntry> gptEntries = gptDictModel->getEntries();
				for (const auto& entry : gptEntries) {
					if (entry.original.isEmpty() || entry.translation.isEmpty()) {
						continue;
					}
					toml::table gptDictTbl;
					gptDictTbl.insert("searchStr", entry.original.toStdString());
					gptDictTbl.insert("replaceStr", entry.translation.toStdString());
					gptDictTbl.insert("note", entry.description.toStdString());
					gptDictArr.push_back(gptDictTbl);
				}
				ofs.open(_projectDir / L"项目GPT字典.toml");
				ofs << toml::table{ {"gptDict", gptDictArr} };
				ofs.close();
			}
			
			if (preStackedWidget->currentIndex() == 0) {
				ofs.open(_projectDir / L"项目字典_译前.toml");
				ofs << prePlainTextEdit->toPlainText().toStdString();
				ofs.close();
			}
			else if (preStackedWidget->currentIndex() == 1) {
				toml::array preDictArr;
				QList<NormalDictEntry> preEntries = preDictModel->getEntries();
				for (const auto& entry : preEntries) {
					if (entry.original.isEmpty()) {
						continue;
					}
					toml::table preDictTbl;
					preDictTbl.insert("searchStr", entry.original.toStdString());
					preDictTbl.insert("replaceStr", entry.translation.toStdString());
					preDictTbl.insert("conditionTarget", entry.conditionTar.toStdString());
					preDictTbl.insert("conditionReg", entry.conditionReg.toStdString());
					preDictTbl.insert("isReg", entry.isReg);
					preDictTbl.insert("priority", entry.priority);
					preDictArr.push_back(preDictTbl);
				}
				ofs.open(_projectDir / L"项目字典_译前.toml");
				ofs << toml::table{ {"normalDict", preDictArr} };
				ofs.close();
			}
			
			if (postStackedWidget->currentIndex() == 0) {
				ofs.open(_projectDir / L"项目字典_译后.toml");
				ofs << postPlainTextEdit->toPlainText().toStdString();
				ofs.close();
			}
			else if (postStackedWidget->currentIndex() == 1) {
				toml::array postDictArr;
				QList<NormalDictEntry> postEntries = postDictModel->getEntries();
				for (const auto& entry : postEntries) {
					if (entry.original.isEmpty()) {
						continue;
					}
					toml::table postDictTbl;
					postDictTbl.insert("searchStr", entry.original.toStdString());
					postDictTbl.insert("replaceStr", entry.translation.toStdString());
					postDictTbl.insert("conditionTarget", entry.conditionTar.toStdString());
					postDictTbl.insert("conditionReg", entry.conditionReg.toStdString());
					postDictTbl.insert("isReg", entry.isReg);
					postDictTbl.insert("priority", entry.priority);
					postDictArr.push_back(postDictTbl);
				}
				ofs.open(_projectDir / L"项目字典_译后.toml");
				ofs << toml::table{ {"normalDict", postDictArr} };
				ofs.close();
			}

			insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.0", gptDictTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.1", gptDictTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.gptDictTableColumnWidth.2", gptDictTableView->columnWidth(2));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.0", preDictTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.1", preDictTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.2", preDictTableView->columnWidth(2));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.3", preDictTableView->columnWidth(3));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.4", preDictTableView->columnWidth(4));
			insertToml(_projectConfig, "GUIConfig.preDictTableColumnWidth.5", preDictTableView->columnWidth(5));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.0", postDictTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.1", postDictTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.2", postDictTableView->columnWidth(2));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.3", postDictTableView->columnWidth(3));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.4", postDictTableView->columnWidth(4));
			insertToml(_projectConfig, "GUIConfig.postDictTableColumnWidth.5", postDictTableView->columnWidth(5));
			insertToml(_projectConfig, "dictionary.gptDict", toml::array{ "项目GPT字典.toml" });
		};



	// 设置页
	QWidget* settingWidget = new QWidget(mainWidget);
	QVBoxLayout* settingLayout = new QVBoxLayout(settingWidget);

	bool usePreDictInName = _projectConfig["dictionary"]["usePreDictInName"].value_or(false);
	ElaScrollPageArea* usePreDictInNameArea = new ElaScrollPageArea(settingWidget);
	QHBoxLayout* usePreDictInNameLayout = new QHBoxLayout(usePreDictInNameArea);
	ElaText* usePreDictInNameText = new ElaText(usePreDictInNameArea);
	usePreDictInNameText->setText("将译前字典用在name字段");
	usePreDictInNameText->setWordWrap(false);
	usePreDictInNameText->setTextPixelSize(16);
	usePreDictInNameLayout->addWidget(usePreDictInNameText);
	usePreDictInNameLayout->addStretch();
	ElaToggleSwitch* usePreDictInNameSwitch = new ElaToggleSwitch(usePreDictInNameArea);
	usePreDictInNameSwitch->setIsToggled(usePreDictInName);
	usePreDictInNameLayout->addWidget(usePreDictInNameSwitch);
	connect(usePreDictInNameSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "dictionary.usePreDictInName", checked);
		});
	settingLayout->addWidget(usePreDictInNameArea);

	bool usePostDictInName = _projectConfig["dictionary"]["usePostDictInName"].value_or(false);
	ElaScrollPageArea* usePostDictInNameArea = new ElaScrollPageArea(settingWidget);
	QHBoxLayout* usePostDictInNameLayout = new QHBoxLayout(usePostDictInNameArea);
	ElaText* usePostDictInNameText = new ElaText(usePostDictInNameArea);
	usePostDictInNameText->setText("将译后字典用在name字段");
	usePostDictInNameText->setWordWrap(false);
	usePostDictInNameText->setTextPixelSize(16);
	usePostDictInNameLayout->addWidget(usePostDictInNameText);
	usePostDictInNameLayout->addStretch();
	ElaToggleSwitch* usePostDictInNameSwitch = new ElaToggleSwitch(usePostDictInNameArea);
	usePostDictInNameSwitch->setIsToggled(usePostDictInName);
	usePostDictInNameLayout->addWidget(usePostDictInNameSwitch);
	connect(usePostDictInNameSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "dictionary.usePostDictInName", checked);
		});
	settingLayout->addWidget(usePostDictInNameArea);

	bool usePreDictInMsg = _projectConfig["dictionary"]["usePreDictInMsg"].value_or(true);
	ElaScrollPageArea* usePreDictInMsgArea = new ElaScrollPageArea(settingWidget);
	QHBoxLayout* usePreDictInMsgLayout = new QHBoxLayout(usePreDictInMsgArea);
	ElaText* usePreDictInMsgText = new ElaText(usePreDictInMsgArea);
	usePreDictInMsgText->setText("将译前字典用在msg字段");
	usePreDictInMsgText->setWordWrap(false);
	usePreDictInMsgText->setTextPixelSize(16);
	usePreDictInMsgLayout->addWidget(usePreDictInMsgText);
	usePreDictInMsgLayout->addStretch();
	ElaToggleSwitch* usePreDictInMsgSwitch = new ElaToggleSwitch(usePreDictInMsgArea);
	usePreDictInMsgSwitch->setIsToggled(usePreDictInMsg);
	usePreDictInMsgLayout->addWidget(usePreDictInMsgSwitch);
	connect(usePreDictInMsgSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "dictionary.usePreDictInMsg", checked);
		});
	settingLayout->addWidget(usePreDictInMsgArea);

	bool usePostDictInMsg = _projectConfig["dictionary"]["usePostDictInMsg"].value_or(true);
	ElaScrollPageArea* usePostDictInMsgArea = new ElaScrollPageArea(settingWidget);
	QHBoxLayout* usePostDictInMsgLayout = new QHBoxLayout(usePostDictInMsgArea);
	ElaText* usePostDictInMsgText = new ElaText(usePostDictInMsgArea);
	usePostDictInMsgText->setText("将译后字典用在msg字段");
	usePostDictInMsgText->setWordWrap(false);
	usePostDictInMsgText->setTextPixelSize(16);
	usePostDictInMsgLayout->addWidget(usePostDictInMsgText);
	usePostDictInMsgLayout->addStretch();
	ElaToggleSwitch* usePostDictInMsgSwitch = new ElaToggleSwitch(usePostDictInMsgArea);
	usePostDictInMsgSwitch->setIsToggled(usePostDictInMsg);
	usePostDictInMsgLayout->addWidget(usePostDictInMsgSwitch);
	connect(usePostDictInMsgSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "dictionary.usePostDictInMsg", checked);
		});
	settingLayout->addWidget(usePostDictInMsgArea);

	bool useGPTDictToReplaceName = _projectConfig["dictionary"]["useGPTDictToReplaceName"].value_or(false);
	ElaScrollPageArea* useGPTDictToReplaceNameArea = new ElaScrollPageArea(settingWidget);
	QHBoxLayout* useGPTDictToReplaceNameLayout = new QHBoxLayout(useGPTDictToReplaceNameArea);
	ElaText* useGPTDictToReplaceNameText = new ElaText(useGPTDictToReplaceNameArea);
	useGPTDictToReplaceNameText->setText("启用GPT字典替换name字段");
	useGPTDictToReplaceNameText->setWordWrap(false);
	useGPTDictToReplaceNameText->setTextPixelSize(16);
	useGPTDictToReplaceNameLayout->addWidget(useGPTDictToReplaceNameText);
	useGPTDictToReplaceNameLayout->addStretch();
	ElaToggleSwitch* useGPTDictToReplaceNameSwitch = new ElaToggleSwitch(useGPTDictToReplaceNameArea);
	useGPTDictToReplaceNameSwitch->setIsToggled(useGPTDictToReplaceName);
	useGPTDictToReplaceNameLayout->addWidget(useGPTDictToReplaceNameSwitch);
	connect(useGPTDictToReplaceNameSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "dictionary.useGPTDictToReplaceName", checked);
		});
	settingLayout->addWidget(useGPTDictToReplaceNameArea);

	settingLayout->addStretch();
	_pivot->appendPivot("项目字典设置");
	_stackedWidget->addWidget(settingWidget);

	// 说明页
	ElaScrollPageArea* descScrollArea = new ElaScrollPageArea(mainWidget);
	QVBoxLayout* descLayout = new QVBoxLayout(descScrollArea);
	descScrollArea->setFixedHeight(500);
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
将 项目GPT字典 纯文本模式 中的文本原样保存到 项目文件夹的 项目GPT字典.toml 中，
然后再执行翻译。所以如果在纯文本模式下没有按 toml 格式来编辑，翻译时肯定会报错。

按刷新将会重新从项目文件夹中的 toml 文件读取数据，如果你在GUI中还有修改了没有保存的数据，
请务必先确认备份情况再刷新。
)");
	descLayout->addWidget(descText);
	descLayout->addStretch();
	_pivot->appendPivot("说明");
	_stackedWidget->addWidget(descScrollArea);


	mainLayout->addWidget(pivotScrollArea, 0, Qt::AlignTop);
	mainLayout->addWidget(_stackedWidget, 1);
	_pivot->setCurrentIndex(0);

	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, true, 0);
}
