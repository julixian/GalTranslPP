#include "CommonGptDictPage.h"

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

CommonGptDictPage::CommonGptDictPage(toml::table& globalConfig, QWidget* parent) :
	BasePage(parent), _globalConfig(globalConfig), _mainWindow(parent)
{
	setWindowTitle("默认GPT字典设置");
	setTitleVisible(false);

	_setupUI();
}

CommonGptDictPage::~CommonGptDictPage()
{

}

void CommonGptDictPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

QList<DictionaryEntry> CommonGptDictPage::readGptDicts(const fs::path& dictPath)
{
	QList<DictionaryEntry> result;
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
	auto dictArr = tbl["gptDict"].as_array();
	if (!dictArr) {
		return result;
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
	return result;
}

QString CommonGptDictPage::readGptDictsStr(const fs::path& dictPath)
{
	std::string result = "gptDict = [\n";
	auto readDict = [&]() -> bool
		{
			std::ifstream ifs(dictPath);
			toml::table tbl;
			try {
				tbl = toml::parse(ifs);
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败",
					QString(dictPath.filename().wstring()) + " 不符合规范", 3000);
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
			return true;
		};

	bool gptOk = readDict();
	if (gptOk) {
		result += "]\n";
	}
	else {
		std::ifstream ifs(dictPath);
		result = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		ifs.close();
	}
	return QString::fromStdString(result);
}

void CommonGptDictPage::_setupUI()
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
	auto commonGptDicts = _globalConfig["commonGptDicts"]["dictNames"].as_array();
	if (commonGptDicts) {
		toml::array newGptDicts;
		for (const auto& elem : *commonGptDicts) {
			auto dictNameOpt = elem.value<std::string>();
			if (!dictNameOpt.has_value()) {
				continue;
			}
			fs::path dictPath = L"BaseConfig/Dict/gpt/" + ascii2Wide(*dictNameOpt) + L".toml";
			if (!fs::exists(dictPath)) {
				continue;
			}
			try {
				GptTabEntry gptTabEntry;

				QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
				ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
				QFont plainTextFont = plainTextEdit->font();
				plainTextFont.setPixelSize(15);
				plainTextEdit->setFont(plainTextFont);
				plainTextEdit->setPlainText(readGptDictsStr(dictPath));
				stackedWidget->addWidget(plainTextEdit);
				ElaTableView* tableView = new ElaTableView(stackedWidget);
				QFont tableHeaderFont = tableView->horizontalHeader()->font();
				tableHeaderFont.setPixelSize(16);
				tableView->horizontalHeader()->setFont(tableHeaderFont);
				tableView->verticalHeader()->setHidden(true);
				tableView->setAlternatingRowColors(true);
				tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
				DictionaryModel* model = new DictionaryModel(tableView);
				QList<DictionaryEntry> gptData = readGptDicts(dictPath);
				model->loadData(gptData);
				tableView->setModel(model);
				stackedWidget->addWidget(tableView);
				stackedWidget->setCurrentIndex(_globalConfig["commonGptDicts"]["spec"][*dictNameOpt]["openMode"].value_or(1));
				tableView->setColumnWidth(0, _globalConfig["commonGptDicts"]["spec"][*dictNameOpt]["columnWidth"]["0"].value_or(175));
				tableView->setColumnWidth(1, _globalConfig["commonGptDicts"]["spec"][*dictNameOpt]["columnWidth"]["1"].value_or(175));
				tableView->setColumnWidth(2, _globalConfig["commonGptDicts"]["spec"][*dictNameOpt]["columnWidth"]["2"].value_or(425));

				gptTabEntry.stackedWidget = stackedWidget;
				gptTabEntry.plainTextEdit = plainTextEdit;
				gptTabEntry.tableView = tableView;
				gptTabEntry.normalDictModel = model;
				gptTabEntry.dictPath = dictPath;
				_gptTabEntries.push_back(gptTabEntry);
				newGptDicts.push_back(*dictNameOpt);
				tabWidget->addTab(stackedWidget, QString(dictPath.stem().wstring()));
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "默认译前字典 " +
					QString::fromStdString(*dictNameOpt) + " 不符合规范", 3000);
				continue;
			}
		}
		insertToml(_globalConfig, "commonGptDicts.dictNames", newGptDicts);
	}

	tabWidget->setCurrentIndex(0);
	int curIdx = tabWidget->currentIndex();
	if (curIdx >= 0) {
		QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
		auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
			{
				return entry.stackedWidget == stackedWidget;
			});
		if (stackedWidget && it != _gptTabEntries.end()) {
			std::string dictName = wide2Ascii(it->dictPath.stem().wstring());
			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			defaultOnButton->setEnabled(true);
			defaultOnButton->setIsToggled(_globalConfig["commonGptDicts"]["spec"][dictName]["defaultOn"].value_or(true));
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
			auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});

			if (!stackedWidget || it == _gptTabEntries.end()) {
				return;
			}

			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			defaultOnButton->setEnabled(true);
			defaultOnButton->setIsToggled(
				_globalConfig["commonGptDicts"]["spec"][wide2Ascii(it->dictPath.stem().wstring())]["defaultOn"].value_or(true));
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
			auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _gptTabEntries.end()) {
				return;
			}
			insertToml(_globalConfig, "commonGptDicts.spec." + wide2Ascii(it->dictPath.stem().wstring())
				+ ".defaultOn", checked);
		});

	connect(saveAllButton, &ElaPushButton::clicked, this, &CommonGptDictPage::apply2Config);
	connect(saveButton, &ElaPushButton::clicked, this, [=]()
		{
			int index = tabWidget->currentIndex();
			if (index < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _gptTabEntries.end()) {
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
				QList<DictionaryEntry> newDictEntries = readGptDicts(it->dictPath);
				it->normalDictModel->loadData(newDictEntries);
			}
			else if (stackedWidget->currentIndex() == 1) {
				QList<DictionaryEntry> dictEntries = it->normalDictModel->getEntries();
				toml::array dictArr;
				for (const auto& entry : dictEntries) {
					toml::table dictTable;
					dictTable.insert("searchStr", entry.original.toStdString());
					dictTable.insert("replaceStr", entry.translation.toStdString());
					dictTable.insert("note", entry.description.toStdString());
					dictArr.push_back(dictTable);
				}
				ofs << toml::table{ {"gptDict", dictArr} };
				ofs.close();
				it->plainTextEdit->setPlainText(readGptDictsStr(it->dictPath));
			}

			auto newDictNames = _globalConfig["commonGptDicts"]["dictNames"].as_array();
			if (!newDictNames) {
				insertToml(_globalConfig, "commonGptDicts.dictNames", toml::array{ dictName });
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

			fs::path newDictPath = L"BaseConfig/Dict/gpt/" + dictName.toStdWString() + L".toml";
			if (fs::exists(newDictPath) || dictName == "项目GPT字典") {
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

			GptTabEntry gptTabEntry;

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
			DictionaryModel* model = new DictionaryModel(tableView);
			tableView->setModel(model);
			stackedWidget->addWidget(tableView);
			stackedWidget->setCurrentIndex(1);

			gptTabEntry.stackedWidget = stackedWidget;
			gptTabEntry.plainTextEdit = plainTextEdit;
			gptTabEntry.tableView = tableView;
			gptTabEntry.normalDictModel = model;
			gptTabEntry.dictPath = newDictPath;
			_gptTabEntries.push_back(gptTabEntry);
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
			auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});

			if (!stackedWidget || it == _gptTabEntries.end()) {
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
					_gptTabEntries.erase(it);
					auto dictNames = _globalConfig["commonGptDicts"]["dictNames"].as_array();
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

	connect(addDictButton, &ElaPushButton::clicked, this, [=]()
		{
			int currentIndex = tabWidget->currentIndex();
			if (currentIndex < 0) {
				return;
			}
			QStackedWidget* stackedWidget = qobject_cast<QStackedWidget*>(tabWidget->currentWidget());
			auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _gptTabEntries.end()) {
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
			auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
				{
					return entry.stackedWidget == stackedWidget;
				});
			if (!stackedWidget || it == _gptTabEntries.end()) {
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
				auto it = std::find_if(_gptTabEntries.begin(), _gptTabEntries.end(), [=](const GptTabEntry& entry)
					{
						return entry.stackedWidget == stackedWidget;
					});
				if (!stackedWidget || it == _gptTabEntries.end()) {
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
					QList<DictionaryEntry> newDictEntries = readGptDicts(it->dictPath);
					it->normalDictModel->loadData(newDictEntries);
				}
				else if (stackedWidget->currentIndex() == 1) {
					QList<DictionaryEntry> dictEntries = it->normalDictModel->getEntries();
					toml::array dictArr;
					for (const auto& entry : dictEntries) {
						toml::table dictTable;
						dictTable.insert("searchStr", entry.original.toStdString());
						dictTable.insert("replaceStr", entry.translation.toStdString());
						dictTable.insert("note", entry.description.toStdString());
						dictArr.push_back(dictTable);
					}
					ofs << toml::table{ {"gptDict", dictArr} };
					ofs.close();
					it->plainTextEdit->setPlainText(readGptDictsStr(it->dictPath));
				}

				insertToml(_globalConfig, "commonGptDicts.spec." + dictName + ".openMode", stackedWidget->currentIndex());
				insertToml(_globalConfig, "commonGptDicts.spec." + dictName + ".columnWidth.0", it->tableView->columnWidth(0));
				insertToml(_globalConfig, "commonGptDicts.spec." + dictName + ".columnWidth.1", it->tableView->columnWidth(1));
				insertToml(_globalConfig, "commonGptDicts.spec." + dictName + ".columnWidth.2", it->tableView->columnWidth(2));
			}
			insertToml(_globalConfig, "commonGptDicts.dictNames", dictNamesArr);

			auto spec = _globalConfig["commonGptDicts"]["spec"].as_table();
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
