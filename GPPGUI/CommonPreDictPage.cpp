#include "CommonPreDictPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QHeaderView>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaToggleSwitch.h"
#include "ElaPlainTextEdit.h"
#include "ElaTabWidget.h"
#include "ElaInputDialog.h"

import Tool;
namespace fs = std::filesystem;

CommonPreDictPage::CommonPreDictPage(toml::table& globalConfig, QWidget* parent) :
	BasePage(parent), _globalConfig(globalConfig)
{
	setWindowTitle("默认译前字典设置");
	setTitleVisible(false);

	_setupUI();
}

CommonPreDictPage::~CommonPreDictPage()
{

}

void CommonPreDictPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

QList<NormalDictEntry> CommonPreDictPage::readNormalDicts(const fs::path& dictPath)
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
			QString(dictPath.filename().wstring()) + " 不符合规范", 3000);
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

QString CommonPreDictPage::readNormalDictsStr(const fs::path& dictPath)
{
	if (!fs::exists(dictPath)) {
		return {};
	}
	std::ifstream ifs(dictPath);
	std::string result((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	return QString::fromStdString(result);
}

void CommonPreDictPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	QWidget* mainButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* mainButtonLayout = new QHBoxLayout(mainButtonWidget);
	ElaPushButton* plainTextModeButton = new ElaPushButton(mainButtonWidget);
	plainTextModeButton->setText("切换至纯文本模式");
	ElaPushButton* tableModeButton = new ElaPushButton(mainButtonWidget);
	tableModeButton->setText("切换至表模式");
	ElaPushButton* saveAllButton = new ElaPushButton(mainButtonWidget);
	saveAllButton->setText("保存全部");
	ElaPushButton* saveButton = new ElaPushButton(mainButtonWidget);
	saveButton->setText("保存");
	ElaPushButton* addNewTabButton = new ElaPushButton(mainButtonWidget);
	addNewTabButton->setText("添加新字典页");
	ElaPushButton* removeTabButton = new ElaPushButton(mainButtonWidget);
	removeTabButton->setText("移除当前页");
	ElaPushButton* addDictButton = new ElaPushButton(mainButtonWidget);
	addDictButton->setText("添加词条");
	ElaPushButton* removeDictButton = new ElaPushButton(mainButtonWidget);
	removeDictButton->setText("删除字典");
	mainButtonLayout->addWidget(plainTextModeButton);
	mainButtonLayout->addWidget(tableModeButton);
	mainButtonLayout->addStretch();
	mainButtonLayout->addWidget(saveAllButton);
	mainButtonLayout->addWidget(saveButton);
	mainButtonLayout->addWidget(addNewTabButton);
	mainButtonLayout->addWidget(removeTabButton);
	mainButtonLayout->addWidget(addDictButton);
	mainButtonLayout->addWidget(removeDictButton);
	mainLayout->addWidget(mainButtonWidget, 0, Qt::AlignTop);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainButtonWidget);
	tabWidget->setTabsClosable(false);
	auto commonPreDicts = _globalConfig["commonPreDicts"].as_array();
	if (commonPreDicts) {
		for (const auto& elem : *commonPreDicts) {
			if (auto dictPathOpt = elem.value<std::string>()) {
				fs::path dictPath = L"BaseConfig/Dict/pre/" + ascii2Wide(*dictPathOpt);
				if (!fs::exists(dictPath)) {
					continue;
				}
				try {
					PreTabEntry preTabEntry;

					QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
					ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
					QFont plainTextFont = plainTextEdit->font();
					plainTextFont.setPixelSize(15);
					plainTextEdit->setFont(plainTextFont);
					plainTextEdit->setPlainText(readNormalDictsStr(dictPath));
					stackedWidget->addWidget(plainTextEdit);
					ElaTableView* tableView = new ElaTableView(stackedWidget);
					NormalDictModel* model = new NormalDictModel(tableView);
					QList<NormalDictEntry> preData = readNormalDicts(dictPath);
					model->loadData(preData);
					tableView->setModel(model);
					stackedWidget->addWidget(tableView);

					preTabEntry.stackedWidget = stackedWidget;
					preTabEntry.plainTextEdit = plainTextEdit;
					preTabEntry.tableView = tableView;
					preTabEntry.normalDictModel = model;
					preTabEntry.dictPath = dictPath;
					_preTabEntries.push_back(preTabEntry);
					tabWidget->addTab(stackedWidget, QString(dictPath.filename().wstring()));
				}
				catch (...) {
					ElaMessageBar::error(ElaMessageBarType::TopRight, "解析失败", "默认译前字典 " +
						QString::fromStdString(*dictPathOpt) + " 不符合规范", 3000);
					continue;
				}
			}
		}
	}


	connect(plainTextModeButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			if (stackedWidget) {
				stackedWidget->setCurrentIndex(0);
			}
		});

	connect(tableModeButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			if (stackedWidget) {
				stackedWidget->setCurrentIndex(1);
			}
		});

	connect(saveAllButton, &ElaPushButton::clicked, this, &CommonPreDictPage::apply2Config);
	connect(saveButton, &ElaPushButton::clicked, this, [=]()
		{
			int index = tabWidget->currentIndex();
			if (index < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_preTabEntries.begin(), _preTabEntries.end(), [=](const PreTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _preTabEntries.end()) {
				return;
			}

			std::ofstream ofs(it->dictPath);
			if (!ofs.is_open()) {
				ElaMessageBar::error(ElaMessageBarType::TopRight, "保存失败", "无法打开 " +
					QString(it->dictPath.wstring()) + " 字典", 3000);
				return;
			}

			if (stackedWidget->currentIndex() == 0) {
				ofs << it->plainTextEdit->toPlainText().toStdString();
			}
			else if (stackedWidget->currentIndex() == 1) {
				QList<NormalDictEntry> dictEntries = it->normalDictModel->getEntries();
				toml::array dictArr;
				for (const auto& entry : dictEntries) {
					toml::table dictTable;
					dictTable.insert("searchStr", entry.original.toStdString());
					dictTable.insert("replaceStr", entry.translation.toStdString());
					dictTable.insert("conditionTarget", entry.conditionTar.toStdString());
					dictTable.insert("conditionReg", entry.conditionReg.toStdString());
					dictTable.insert("isReg", entry.isReg);
					dictTable.insert("priority", entry.priority);
					dictArr.push_back(dictTable);
				}
				ofs << toml::table{ {"normalDict", dictArr} };
			}
			ofs.close();
			QList<NormalDictEntry> newDictEntries = readNormalDicts(it->dictPath);
			it->normalDictModel->loadData(newDictEntries);
			it->plainTextEdit->setPlainText(readNormalDictsStr(it->dictPath));
			ElaMessageBar::success(ElaMessageBarType::TopRight, "保存成功", "字典 " +
				QString(it->dictPath.filename().wstring()) + " 已保存", 3000);
		});

	connect(addNewTabButton, &ElaPushButton::clicked, this, [=]()
		{
			QString dictName;
			bool ok;
			ElaInputDialog inputDialog(this, "请输入字典表名称", "新建字典", dictName, &ok);
			if (!ok) {
				return;
			}
			if (dictName.isEmpty() || dictName.contains('/') || dictName.contains('\\')) {
				ElaMessageBar::error(ElaMessageBarType::TopRight, "新建失败", "字典名称不能为空，且不能包含斜杠或反斜杠！", 3000);
				return;
			}

			fs::path newDictPath = L"BaseConfig/Dict/pre/" + dictName.toStdWString();
			if (fs::exists(newDictPath)) {
				ElaMessageBar::error(ElaMessageBarType::TopRight, "新建失败", "字典 " +
					QString(newDictPath.filename().wstring()) + " 已存在", 3000);
				return;
			}

			std::ofstream ofs(newDictPath);
			if (!ofs.is_open()) {
				ElaMessageBar::error(ElaMessageBarType::TopRight, "新建失败", "无法创建 " +
					QString(newDictPath.wstring()) + " 字典", 3000);
				return;
			}
			ofs.close();

			PreTabEntry preTabEntry;

			QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
			ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
			QFont plainTextFont = plainTextEdit->font();
			plainTextFont.setPixelSize(15);
			plainTextEdit->setFont(plainTextFont);
			stackedWidget->addWidget(plainTextEdit);
			ElaTableView* tableView = new ElaTableView(stackedWidget);
			NormalDictModel* model = new NormalDictModel(tableView);
			stackedWidget->addWidget(tableView);

			preTabEntry.stackedWidget = stackedWidget;
			preTabEntry.plainTextEdit = plainTextEdit;
			preTabEntry.tableView = tableView;
			preTabEntry.normalDictModel = model;
			preTabEntry.dictPath = newDictPath;
			_preTabEntries.push_back(preTabEntry);
			tabWidget->addTab(stackedWidget, dictName);
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
		});

	connect(removeTabButton, &ElaPushButton::clicked, this, [=]()
		{
			int index = tabWidget->currentIndex();
			if (index < 0) {
				ElaMessageBar::error(ElaMessageBarType::TopRight, "移除失败", "请先选择一个字典页！", 3000);
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_preTabEntries.begin(), _preTabEntries.end(), [=](const PreTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _preTabEntries.end()) {
				return;
			}
			stackedWidget->deleteLater();
			tabWidget->removeTab(index);
			_preTabEntries.erase(it);
		});

	connect(addDictButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_preTabEntries.begin(), _preTabEntries.end(), [=](const PreTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _preTabEntries.end()) {
				return;
			}
			it->normalDictModel->insertRow(it->normalDictModel->rowCount());
		});

	connect(removeDictButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_preTabEntries.begin(), _preTabEntries.end(), [=](const PreTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _preTabEntries.end()) {
				return;
			}
			QModelIndexList selectedRows = it->tableView->selectionModel()->selectedRows();
			std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
				{
					return a.row() > b.row();
				});
			for (const auto& index : selectedRows) {
				it->normalDictModel->removeRow(index.row());
			}
		});



	_applyFunc = [=]()
		{
			int tabCount = tabWidget->count();
			for (int i = 0; i < tabCount; i++) {
				QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->widget(i));
				auto it = std::find_if(_preTabEntries.begin(), _preTabEntries.end(), [=](const PreTabEntry& entry)
					{
						return entry.stackedWidget == stackedWidget;
					});
				if (!stackedWidget || it == _preTabEntries.end()) {
					continue;
				}
				
				std::ofstream ofs(it->dictPath);
				if (!ofs.is_open()) {
					ElaMessageBar::error(ElaMessageBarType::TopRight, "保存失败", "无法打开 " +
						QString(it->dictPath.wstring()) + " 字典，将跳过该字典的保存", 3000);
					continue;
				}

				if (stackedWidget->currentIndex() == 0) {
					ofs << it->plainTextEdit->toPlainText().toStdString();
				}
				else if (stackedWidget->currentIndex() == 1) {
					QList<NormalDictEntry> dictEntries = it->normalDictModel->getEntries();
					toml::array dictArr;
					for (const auto& entry : dictEntries) {
						toml::table dictTable;
						dictTable.insert("searchStr", entry.original.toStdString());
						dictTable.insert("replaceStr", entry.translation.toStdString());
						dictTable.insert("conditionTarget", entry.conditionTar.toStdString());
						dictTable.insert("conditionReg", entry.conditionReg.toStdString());
						dictTable.insert("isReg", entry.isReg);
						dictTable.insert("priority", entry.priority);
						dictArr.push_back(dictTable);
					}
					ofs << toml::table{ {"normalDict", dictArr} };
				}
				ofs.close();
				QList<NormalDictEntry> newDictEntries = readNormalDicts(it->dictPath);
				it->normalDictModel->loadData(newDictEntries);
				it->plainTextEdit->setPlainText(readNormalDictsStr(it->dictPath));
			}
		};


	mainLayout->addWidget(tabWidget, 1);
	addCentralWidget(mainWidget, true, true, 0);
}
