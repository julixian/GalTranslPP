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
#include "ElaToggleButton.h"
#include "ElaToggleSwitch.h"
#include "ElaPlainTextEdit.h"
#include "ElaTabWidget.h"
#include "ElaInputDialog.h"

import Tool;
namespace fs = std::filesystem;

CommonPreDictPage::CommonPreDictPage(toml::table& globalConfig, QWidget* parent) :
	BasePage(parent), _globalConfig(globalConfig), _mainWindow(parent)
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
		ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败",
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
	ElaToggleButton* defaultOnButton = new ElaToggleButton(mainButtonWidget);
	defaultOnButton->setText("默认启用");
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
	removeDictButton->setText("删除词条");
	mainButtonLayout->addWidget(plainTextModeButton);
	mainButtonLayout->addWidget(tableModeButton);
	mainButtonLayout->addWidget(defaultOnButton);
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
	tabWidget->setIsTabTransparent(true);
	auto commonPreDicts = _globalConfig["commonPreDicts"]["dictNames"].as_array();
	if (commonPreDicts) {
		toml::array newPreDicts;
		for (const auto& elem : *commonPreDicts) {
			auto dictNameOpt = elem.value<std::string>();
			if (!dictNameOpt.has_value()) {
				continue;
			}
			fs::path dictPath = L"BaseConfig/Dict/pre/" + ascii2Wide(*dictNameOpt) + L".toml";
			if (!fs::exists(dictPath)) {
				continue;
			}
			try {
				NormalTabEntry normalTabEntry;

				QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
				ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
				QFont plainTextFont = plainTextEdit->font();
				plainTextFont.setPixelSize(15);
				plainTextEdit->setFont(plainTextFont);
				plainTextEdit->setPlainText(readNormalDictsStr(dictPath));
				stackedWidget->addWidget(plainTextEdit);
				ElaTableView* tableView = new ElaTableView(stackedWidget);
				QFont tableHeaderFont = tableView->horizontalHeader()->font();
				tableHeaderFont.setPixelSize(16);
				tableView->horizontalHeader()->setFont(tableHeaderFont);
				tableView->verticalHeader()->setHidden(true);
				tableView->setAlternatingRowColors(true);
				tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
				NormalDictModel* model = new NormalDictModel(tableView);
				QList<NormalDictEntry> normalData = readNormalDicts(dictPath);
				model->loadData(normalData);
				tableView->setModel(model);
				stackedWidget->addWidget(tableView);
				stackedWidget->setCurrentIndex(_globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["openMode"].value_or(1));
				tableView->setColumnWidth(0, _globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["columnWidth"]["0"].value_or(200));
				tableView->setColumnWidth(1, _globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["columnWidth"]["1"].value_or(150));
				tableView->setColumnWidth(2, _globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["columnWidth"]["2"].value_or(100));
				tableView->setColumnWidth(3, _globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["columnWidth"]["3"].value_or(172));
				tableView->setColumnWidth(4, _globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["columnWidth"]["4"].value_or(75));
				tableView->setColumnWidth(5, _globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["columnWidth"]["5"].value_or(60));

				normalTabEntry.stackedWidget = stackedWidget;
				normalTabEntry.plainTextEdit = plainTextEdit;
				normalTabEntry.tableView = tableView;
				normalTabEntry.normalDictModel = model;
				normalTabEntry.dictPath = dictPath;
				_normalTabEntries.push_back(normalTabEntry);
				newPreDicts.push_back(*dictNameOpt);
				tabWidget->addTab(stackedWidget, QString(dictPath.stem().wstring()));
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "默认译前字典 " +
					QString::fromStdString(*dictNameOpt) + " 不符合规范", 3000);
				continue;
			}
		}
		insertToml(_globalConfig, "commonPreDicts.dictNames", newPreDicts);
	}

	tabWidget->setCurrentIndex(0);
	int curIdx = tabWidget->currentIndex();
	if (curIdx >= 0) {
		QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
		auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
			{
				return entry.stackedWidget == stackedWidget;
			});
		if (stackedWidget && it != _normalTabEntries.end()) {
			std::string dictName = wide2Ascii(it->dictPath.stem().wstring());
			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			defaultOnButton->setEnabled(true);
			defaultOnButton->setIsToggled(_globalConfig["commonPreDicts"]["spec"][dictName]["defaultOn"].value_or(true));
			addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeTabButton->setEnabled(true);
			saveAllButton->setEnabled(true);
			saveButton->setEnabled(true);
		}
	}
	else {
		plainTextModeButton->setEnabled(false);
		tableModeButton->setEnabled(false);
		defaultOnButton->setIsToggled(false);
		defaultOnButton->setEnabled(false);
		addDictButton->setEnabled(false);
		removeDictButton->setEnabled(false);
		removeTabButton->setEnabled(false);
		saveAllButton->setEnabled(false);
		saveButton->setEnabled(false);
	}

	connect(tabWidget, &ElaTabWidget::currentChanged, this, [=](int index)
		{
			if (index < 0) {
				plainTextModeButton->setEnabled(false);
				tableModeButton->setEnabled(false);
				defaultOnButton->setIsToggled(false);
				defaultOnButton->setEnabled(false);
				addDictButton->setEnabled(false);
				removeDictButton->setEnabled(false);
				removeTabButton->setEnabled(false);
				saveAllButton->setEnabled(false);
				saveButton->setEnabled(false);
				return;
			}

			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});

			if (!stackedWidget || it == _normalTabEntries.end()) {
				return;
			}

			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			defaultOnButton->setEnabled(true);
			defaultOnButton->setIsToggled(
				_globalConfig["commonPreDicts"]["spec"][wide2Ascii(it->dictPath.stem().wstring())]["defaultOn"].value_or(true));
			addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeTabButton->setEnabled(true);
			saveAllButton->setEnabled(true);
			saveButton->setEnabled(true);
		});

	connect(plainTextModeButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			if (stackedWidget) {
				stackedWidget->setCurrentIndex(0);
				plainTextModeButton->setEnabled(false);
				tableModeButton->setEnabled(true);
				addDictButton->setEnabled(false);
				removeDictButton->setEnabled(false);
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
				plainTextModeButton->setEnabled(true);
				tableModeButton->setEnabled(false);
				addDictButton->setEnabled(true);
				removeDictButton->setEnabled(true);
			}
		});

	connect(defaultOnButton, &ElaToggleButton::toggled, this, [=](bool checked)
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _normalTabEntries.end()) {
				return;
			}
			insertToml(_globalConfig, "commonPreDicts.spec." + wide2Ascii(it->dictPath.stem().wstring())
				+ ".defaultOn", checked);
		});

	connect(saveAllButton, &ElaPushButton::clicked, this, &CommonPreDictPage::apply2Config);
	connect(saveButton, &ElaPushButton::clicked, this, [=]()
		{
			int index = tabWidget->currentIndex();
			if (index < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _normalTabEntries.end()) {
				return;
			}

			std::ofstream ofs(it->dictPath);
			if (!ofs.is_open()) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "保存失败", "无法打开 " +
					QString(it->dictPath.wstring()) + " 字典", 3000);
				return;
			}

			std::string dictName = wide2Ascii(it->dictPath.stem().wstring());

			if (stackedWidget->currentIndex() == 0) {
				ofs << it->plainTextEdit->toPlainText().toStdString();
				ofs.close();
				QList<NormalDictEntry> newDictEntries = readNormalDicts(it->dictPath);
				it->normalDictModel->loadData(newDictEntries);
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
				ofs.close();
				it->plainTextEdit->setPlainText(readNormalDictsStr(it->dictPath));
			}

			auto newDictNames = _globalConfig["commonPreDicts"]["dictNames"].as_array();
			if (!newDictNames) {
				insertToml(_globalConfig, "commonPreDicts.dictNames", toml::array{ dictName });
			}
			else {
				auto it = std::find_if(newDictNames->begin(), newDictNames->end(), [=](const auto& elem)
					{
						return elem.value_or(std::string{}) == dictName;
					});
				if (it == newDictNames->end()) {
					newDictNames->push_back(dictName);
				}
			}
			Q_EMIT commonDictsChanged();
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "保存成功", "字典 " +
				QString(it->dictPath.stem().wstring()) + " 已保存", 3000);
		});

	connect(addNewTabButton, &ElaPushButton::clicked, this, [=]()
		{
			QString dictName;
			bool ok;
			ElaInputDialog inputDialog(this, "请输入字典表名称", "新建字典", dictName, &ok);
			inputDialog.exec();

			if (!ok) {
				return;
			}
			if (dictName.isEmpty() || dictName.contains('/') || dictName.contains('\\') || dictName.contains('.')) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, 
					"新建失败", "字典名称不能为空，且不能包含点号、斜杠或反斜杠！", 3000);
				return;
			}

			fs::path newDictPath = L"BaseConfig/Dict/pre/" + dictName.toStdWString() + L".toml";
			if (fs::exists(newDictPath) || dictName == "项目字典_译后") {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "新建失败", "字典 " +
					QString(newDictPath.filename().wstring()) + " 已存在", 3000);
				return;
			}

			std::ofstream ofs(newDictPath);
			if (!ofs.is_open()) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "新建失败", "无法创建 " +
					QString(newDictPath.wstring()) + " 字典", 3000);
				return;
			}
			ofs.close();

			NormalTabEntry normalTabEntry;

			QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
			ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
			QFont plainTextFont = plainTextEdit->font();
			plainTextFont.setPixelSize(15);
			plainTextEdit->setFont(plainTextFont);
			stackedWidget->addWidget(plainTextEdit);
			ElaTableView* tableView = new ElaTableView(stackedWidget);
			QFont tableHeaderFont = tableView->horizontalHeader()->font();
			tableHeaderFont.setPixelSize(16);
			tableView->horizontalHeader()->setFont(tableHeaderFont);
			tableView->verticalHeader()->setHidden(true);
			tableView->setAlternatingRowColors(true);
			tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
			NormalDictModel* model = new NormalDictModel(tableView);
			tableView->setModel(model);
			stackedWidget->addWidget(tableView);
			stackedWidget->setCurrentIndex(1);

			normalTabEntry.stackedWidget = stackedWidget;
			normalTabEntry.plainTextEdit = plainTextEdit;
			normalTabEntry.tableView = tableView;
			normalTabEntry.normalDictModel = model;
			normalTabEntry.dictPath = newDictPath;
			_normalTabEntries.push_back(normalTabEntry);
			tabWidget->addTab(stackedWidget, dictName);
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
		});

	connect(removeTabButton, &ElaPushButton::clicked, this, [=]()
		{
			int index = tabWidget->currentIndex();
			if (index < 0) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "移除失败", "请先选择一个字典页！", 3000);
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});

			if (!stackedWidget || it == _normalTabEntries.end()) {
				return;
			}

			std::string dictName = wide2Ascii(it->dictPath.stem().wstring());

			// 删除提示框
			ElaContentDialog helpDialog(_mainWindow);

			helpDialog.setRightButtonText("是");
			helpDialog.setMiddleButtonText("思考人生");
			helpDialog.setLeftButtonText("否");

			QWidget* widget = new QWidget(&helpDialog);
			widget->setFixedHeight(110);
			QVBoxLayout* layout = new QVBoxLayout(widget);
			layout->addWidget(new ElaText("你确定要删除当前字典吗？", widget));
			ElaText* tip = new ElaText("将永久删除该字典文件，如有需要请先备份！", widget);
			tip->setTextPixelSize(14);
			layout->addWidget(tip);
			helpDialog.setCentralWidget(widget);

			connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
				{
					stackedWidget->deleteLater();
					tabWidget->removeTab(index);
					fs::remove(it->dictPath);
					_normalTabEntries.erase(it);
					auto dictNames = _globalConfig["commonPreDicts"]["dictNames"].as_array();
					if (dictNames) {
						auto it = std::find_if(dictNames->begin(), dictNames->end(), [=](const auto& elem)
							{
								return elem.value_or(std::string{}) == dictName;
							});
						if (it != dictNames->end()) {
							dictNames->erase(it);
						}
					}
					Q_EMIT commonDictsChanged();
					ElaMessageBar::success(ElaMessageBarType::TopLeft, "删除成功", "字典 "
						+ QString(dictName.c_str()) + " 已从字典管理和磁盘中移除！", 3000);
				});
			helpDialog.exec();
		});

	connect(addDictButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _normalTabEntries.end()) {
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
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _normalTabEntries.end()) {
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
			toml::array dictNamesArr;
			for (int i = 0; i < tabCount; i++) {
				QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->widget(i));
				auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
					{
						return entry.stackedWidget == stackedWidget;
					});
				if (!stackedWidget || it == _normalTabEntries.end()) {
					continue;
				}
				
				std::ofstream ofs(it->dictPath);
				if (!ofs.is_open()) {
					ElaMessageBar::error(ElaMessageBarType::TopLeft, "保存失败", "无法打开 " +
						QString(it->dictPath.wstring()) + " 字典，将跳过该字典的保存", 3000);
					continue;
				}

				std::string dictName = wide2Ascii(it->dictPath.stem().wstring());
				dictNamesArr.push_back(dictName);

				if (stackedWidget->currentIndex() == 0) {
					ofs << it->plainTextEdit->toPlainText().toStdString();
					ofs.close();
					QList<NormalDictEntry> newDictEntries = readNormalDicts(it->dictPath);
					it->normalDictModel->loadData(newDictEntries);
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
					ofs.close();
					it->plainTextEdit->setPlainText(readNormalDictsStr(it->dictPath));
				}

				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".openMode", stackedWidget->currentIndex());
				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".columnWidth.0", it->tableView->columnWidth(0));
				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".columnWidth.1", it->tableView->columnWidth(1));
				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".columnWidth.2", it->tableView->columnWidth(2));
				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".columnWidth.3", it->tableView->columnWidth(3));
				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".columnWidth.4", it->tableView->columnWidth(4));
				insertToml(_globalConfig, "commonPreDicts.spec." + dictName + ".columnWidth.5", it->tableView->columnWidth(5));
			}
			insertToml(_globalConfig, "commonPreDicts.dictNames", dictNamesArr);

			auto spec = _globalConfig["commonPreDicts"]["spec"].as_table();
			if (spec) {
				std::vector<std::string_view> keysToRemove;
				for (const auto& [key, value] : *spec) {
					if (std::find_if(dictNamesArr.begin(), dictNamesArr.end(), [=](const auto& elem)
						{
							return elem.value_or(std::string{}) == key.str();
						}) == dictNamesArr.end())
					{
						keysToRemove.push_back(key.str());
					}
				}
				for (const auto& key : keysToRemove) {
					spec->erase(key);
				}
			}
			Q_EMIT commonDictsChanged();
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "保存成功", "所有默认字典配置均已保存", 3000);
		};


	mainLayout->addWidget(tabWidget, 1);
	addCentralWidget(mainWidget, true, true, 0);
}
