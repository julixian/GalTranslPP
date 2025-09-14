#include "CommonNormalDictPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QHeaderView>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaToggleButton.h"
#include "ElaPlainTextEdit.h"
#include "ElaTabWidget.h"
#include "ElaInputDialog.h"

import Tool;
namespace fs = std::filesystem;

CommonNormalDictPage::CommonNormalDictPage(const std::string& mode, toml::table& globalConfig, QWidget* parent) :
	BasePage(parent), _globalConfig(globalConfig), _mainWindow(parent)
{
	setWindowTitle("默认译前字典设置");
	setTitleVisible(false);
	setContentsMargins(5, 5, 5, 5);

	if (mode == "pre") {
		_modeConfig = "commonPreDicts";
		_modePath = "pre";
	}
	else if (mode == "post") {
		_modeConfig = "commonPostDicts";
		_modePath = "post";
	}
	else {
		QMessageBox::critical(parent, "错误", "未知通用字典模式", QMessageBox::Ok);
		exit(1);
	}

	_setupUI();
}

CommonNormalDictPage::~CommonNormalDictPage()
{

}

void CommonNormalDictPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

QList<NormalDictEntry> CommonNormalDictPage::readNormalDicts(const fs::path& dictPath)
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

QString CommonNormalDictPage::readNormalDictsStr(const fs::path& dictPath)
{
	if (!fs::exists(dictPath)) {
		return {};
	}
	std::ifstream ifs(dictPath);
	std::string result((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	return QString::fromStdString(result);
}

void CommonNormalDictPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	QWidget* mainButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* mainButtonLayout = new QHBoxLayout(mainButtonWidget);
	ElaText* dictNameLabel = new ElaText(mainButtonWidget);
	dictNameLabel->setTextPixelSize(18);
	QString dictNameText;
	if (_modePath == "pre") {
		dictNameText = "通用译前字典";
	}
	else if (_modePath == "post") {
		dictNameText = "通用译后字典";
	}
	dictNameLabel->setText(dictNameText);
	ElaPushButton* addNewTabButton = new ElaPushButton(mainButtonWidget);
	addNewTabButton->setText("添加新字典页");
	ElaPushButton* removeTabButton = new ElaPushButton(mainButtonWidget);
	removeTabButton->setText("移除当前页");
	mainButtonLayout->addSpacing(10);
	mainButtonLayout->addWidget(dictNameLabel);
	mainButtonLayout->addStretch();
	mainButtonLayout->addWidget(addNewTabButton);
	mainButtonLayout->addWidget(removeTabButton);
	mainLayout->addWidget(mainButtonWidget, 0, Qt::AlignTop);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);

	auto createNormalDictPage = [=](const fs::path& dictPath) -> QWidget*
		{
			std::string dictName = wide2Ascii(dictPath.stem().wstring());
			NormalTabEntry normalTabEntry;

			QWidget* pageMainWidget = new QWidget(tabWidget);
			QVBoxLayout* pageMainLayout = new QVBoxLayout(pageMainWidget);

			QWidget* pageButtonWidget = new QWidget(pageMainWidget);
			QHBoxLayout* pageButtonLayout = new QHBoxLayout(pageButtonWidget);
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
			ElaPushButton* withdrawButton = new ElaPushButton(mainButtonWidget);
			withdrawButton->setText("撤回");
			withdrawButton->setEnabled(false);
			ElaPushButton* addDictButton = new ElaPushButton(mainButtonWidget);
			addDictButton->setText("添加词条");
			ElaPushButton* removeDictButton = new ElaPushButton(mainButtonWidget);
			removeDictButton->setText("删除词条");
			pageButtonLayout->addWidget(plainTextModeButton);
			pageButtonLayout->addWidget(tableModeButton);
			pageButtonLayout->addWidget(defaultOnButton);
			pageButtonLayout->addStretch();
			pageButtonLayout->addWidget(saveAllButton);
			pageButtonLayout->addWidget(saveButton);
			pageButtonLayout->addWidget(withdrawButton);
			pageButtonLayout->addWidget(addDictButton);
			pageButtonLayout->addWidget(removeDictButton);
			pageMainLayout->addWidget(pageButtonWidget, 0, Qt::AlignTop);

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
			stackedWidget->setCurrentIndex(_globalConfig[_modeConfig]["spec"][dictName]["openMode"].value_or(1));
			tableView->setColumnWidth(0, _globalConfig[_modeConfig]["spec"][dictName]["columnWidth"]["0"].value_or(200));
			tableView->setColumnWidth(1, _globalConfig[_modeConfig]["spec"][dictName]["columnWidth"]["1"].value_or(150));
			tableView->setColumnWidth(2, _globalConfig[_modeConfig]["spec"][dictName]["columnWidth"]["2"].value_or(100));
			tableView->setColumnWidth(3, _globalConfig[_modeConfig]["spec"][dictName]["columnWidth"]["3"].value_or(172));
			tableView->setColumnWidth(4, _globalConfig[_modeConfig]["spec"][dictName]["columnWidth"]["4"].value_or(75));
			tableView->setColumnWidth(5, _globalConfig[_modeConfig]["spec"][dictName]["columnWidth"]["5"].value_or(60));
			pageMainLayout->addWidget(stackedWidget, 1);

			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			defaultOnButton->setIsToggled(_globalConfig[_modeConfig]["spec"][dictName]["defaultOn"].value_or(true));
			insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".defaultOn", defaultOnButton->getIsToggled());

			connect(plainTextModeButton, &ElaPushButton::clicked, this, [=]()
				{
					stackedWidget->setCurrentIndex(0);
					plainTextModeButton->setEnabled(false);
					tableModeButton->setEnabled(true);
					addDictButton->setEnabled(false);
					removeDictButton->setEnabled(false);
					withdrawButton->setEnabled(false);
				});

			connect(tableModeButton, &ElaPushButton::clicked, this, [=]()
				{
					stackedWidget->setCurrentIndex(1);
					plainTextModeButton->setEnabled(true);
					tableModeButton->setEnabled(false);
					addDictButton->setEnabled(true);
					removeDictButton->setEnabled(true);
					withdrawButton->setEnabled(!normalTabEntry.withdrawList->empty());
				});

			connect(defaultOnButton, &ElaToggleButton::toggled, this, [=](bool checked)
				{
					insertToml(_globalConfig, _modeConfig + ".spec." + dictName
						+ ".defaultOn", checked);
				});

			connect(saveAllButton, &ElaPushButton::clicked, this, &CommonNormalDictPage::apply2Config);
			connect(saveButton, &ElaPushButton::clicked, this, [=]()
				{
					std::ofstream ofs(dictPath);
					if (!ofs.is_open()) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, "保存失败", "无法打开字典: " +
							QString(dictPath.wstring()), 3000);
						return;
					}

					if (stackedWidget->currentIndex() == 0) {
						ofs << plainTextEdit->toPlainText().toStdString();
						ofs.close();
						QList<NormalDictEntry> newDictEntries = readNormalDicts(dictPath);
						model->loadData(newDictEntries);
					}
					else if (stackedWidget->currentIndex() == 1) {
						QList<NormalDictEntry> dictEntries = model->getEntries();
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
						plainTextEdit->setPlainText(readNormalDictsStr(dictPath));
					}

					auto newDictNames = _globalConfig[_modeConfig]["dictNames"].as_array();
					if (!newDictNames) {
						insertToml(_globalConfig, _modeConfig + ".dictNames", toml::array{ dictName });
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
						QString::fromStdString(dictName) + " 已保存", 3000);
				});

			connect(addDictButton, &ElaPushButton::clicked, this, [=]()
				{
					QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
					if (selectedRows.isEmpty()) {
						model->insertRow(model->rowCount());
					}
					else {
						model->insertRow(selectedRows.first().row());
					}
				});

			connect(removeDictButton, &ElaPushButton::clicked, this, [=]()
				{
					QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
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
					NormalDictEntry entry = normalTabEntry.withdrawList->front();
					normalTabEntry.withdrawList->pop_front();
					model->insertRow(0, entry);
					if (normalTabEntry.withdrawList->empty()) {
						withdrawButton->setEnabled(false);
					}
				});

			normalTabEntry.pageMainWidget = pageMainWidget;
			normalTabEntry.stackedWidget = stackedWidget;
			normalTabEntry.plainTextEdit = plainTextEdit;
			normalTabEntry.tableView = tableView;
			normalTabEntry.dictModel = model;
			normalTabEntry.dictPath = dictPath;
			_normalTabEntries.push_back(normalTabEntry);
			return pageMainWidget;
		};

	auto commonNormalDicts = _globalConfig[_modeConfig]["dictNames"].as_array();
	if (commonNormalDicts) {
		toml::array newDictNames;
		for (const auto& elem : *commonNormalDicts) {
			auto dictNameOpt = elem.value<std::string>();
			if (!dictNameOpt.has_value()) {
				continue;
			}
			fs::path dictPath = L"BaseConfig/Dict/" + ascii2Wide(_modePath) + L"/" + ascii2Wide(*dictNameOpt) + L".toml";
			if (!fs::exists(dictPath)) {
				continue;
			}
			try {
				QWidget* pageMainWidget = createNormalDictPage(dictPath);
				newDictNames.push_back(*dictNameOpt);
				tabWidget->addTab(pageMainWidget, QString::fromStdString(*dictNameOpt));
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "默认译前字典 " +
					QString::fromStdString(*dictNameOpt) + " 不符合规范", 3000);
				continue;
			}
		}
		insertToml(_globalConfig, _modeConfig + ".dictNames", newDictNames);
	}

	tabWidget->setCurrentIndex(0);
	int curIdx = tabWidget->currentIndex();
	if (curIdx >= 0) {
		removeTabButton->setEnabled(true);
	}
	else {
		removeTabButton->setEnabled(false);
	}

	connect(tabWidget, &ElaTabWidget::currentChanged, this, [=](int index)
		{
			if (index < 0) {
				removeTabButton->setEnabled(false);
			}
			else {
				removeTabButton->setEnabled(true);
			}
		});


	connect(addNewTabButton, &ElaPushButton::clicked, this, [=]()
		{
			QString dictName;
			bool ok;
			ElaInputDialog inputDialog(_mainWindow, "请输入字典表名称", "新建字典", dictName, &ok);
			inputDialog.exec();

			if (!ok) {
				return;
			}
			if (dictName.isEmpty() || dictName.contains('/') || dictName.contains('\\') || dictName.contains('.')) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft,
					"新建失败", "字典名称不能为空，且不能包含点号、斜杠或反斜杠！", 3000);
				return;
			}

			fs::path newDictPath = L"BaseConfig/Dict/" + ascii2Wide(_modePath) + L"/" + dictName.toStdWString() + L".toml";
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

			QWidget* pageMainWidget = createNormalDictPage(newDictPath);
			tabWidget->addTab(pageMainWidget, dictName);
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
		});

	connect(removeTabButton, &ElaPushButton::clicked, this, [=]()
		{
			int index = tabWidget->currentIndex();
			if (index < 0) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "移除失败", "请先选择一个字典页！", 3000);
				return;
			}
			QWidget* pageMainWidget = tabWidget->currentWidget();
			auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
				{
					return entry.pageMainWidget == pageMainWidget;
				});

			if (!pageMainWidget || it == _normalTabEntries.end()) {
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
					pageMainWidget->deleteLater();
					tabWidget->removeTab(index);
					fs::remove(it->dictPath);
					_normalTabEntries.erase(it);
					auto dictNames = _globalConfig[_modeConfig]["dictNames"].as_array();
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
						+ QString::fromStdString(dictName) + " 已从字典管理和磁盘中移除！", 3000);
				});
			helpDialog.exec();
		});


	_applyFunc = [=]()
		{
			int tabCount = tabWidget->count();
			toml::array dictNamesArr;
			for (int i = 0; i < tabCount; i++) {
				QWidget* pageMainWidget = tabWidget->widget(i);
				auto it = std::find_if(_normalTabEntries.begin(), _normalTabEntries.end(), [=](const NormalTabEntry& entry)
					{
						return entry.pageMainWidget == pageMainWidget;
					});
				if (!pageMainWidget || it == _normalTabEntries.end()) {
					continue;
				}

				std::ofstream ofs(it->dictPath);
				if (!ofs.is_open()) {
					ElaMessageBar::error(ElaMessageBarType::TopLeft, "保存失败", "无法打开字典: " +
						QString(it->dictPath.wstring()) + " ，将跳过该字典的保存", 3000);
					continue;
				}

				std::string dictName = wide2Ascii(it->dictPath.stem().wstring());
				dictNamesArr.push_back(dictName);

				if (it->stackedWidget->currentIndex() == 0) {
					ofs << it->plainTextEdit->toPlainText().toStdString();
					ofs.close();
					QList<NormalDictEntry> newDictEntries = readNormalDicts(it->dictPath);
					it->dictModel->loadData(newDictEntries);
				}
				else if (it->stackedWidget->currentIndex() == 1) {
					QList<NormalDictEntry> dictEntries = it->dictModel->getEntries();
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

				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".openMode", it->stackedWidget->currentIndex());
				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".columnWidth.0", it->tableView->columnWidth(0));
				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".columnWidth.1", it->tableView->columnWidth(1));
				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".columnWidth.2", it->tableView->columnWidth(2));
				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".columnWidth.3", it->tableView->columnWidth(3));
				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".columnWidth.4", it->tableView->columnWidth(4));
				insertToml(_globalConfig, _modeConfig + ".spec." + dictName + ".columnWidth.5", it->tableView->columnWidth(5));
			}
			insertToml(_globalConfig, _modeConfig + ".dictNames", dictNamesArr);

			auto spec = _globalConfig[_modeConfig]["spec"].as_table();
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
