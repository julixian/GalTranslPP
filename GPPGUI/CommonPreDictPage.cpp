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
	mainButtonLayout->addWidget(plainTextModeButton);
	mainButtonLayout->addWidget(tableModeButton);
	mainButtonLayout->addStretch();
	mainButtonLayout->addWidget(saveAllButton);
	mainButtonLayout->addWidget(saveButton);
	mainButtonLayout->addWidget(addNewTabButton);
	mainButtonLayout->addWidget(removeTabButton);
	mainLayout->addWidget(mainButtonWidget, 0, Qt::AlignTop);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainButtonWidget);
	tabWidget->setTabsClosable(false);
	auto commonPreDicts = _globalConfig["commonPreDicts"].as_array();
	if (commonPreDicts) {
		for (const auto& elem : *commonPreDicts) {
			if (auto dictPathOpt = elem.value<std::string>()) {
				fs::path dictPath = ascii2Wide(*dictPathOpt);
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
					preTabEntry.normalDictModel = model;
					_preTabEntries.push_back(preTabEntry);
					tabWidget->addTab(stackedWidget, QString::fromStdString(dictPath.filename().string()));
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

	connect(saveAllButton, &ElaPushButton::clicked, this, CommonPreDictPage::apply2Config);
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
			if (stackedWidget->currentIndex() == 0) {
				
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
				if (stackedWidget->currentIndex() == 0) {
					QPlainTextEdit* plainTextEdit = qobject_cast<QPlainTextEdit*>(stackedWidget->currentWidget());
					if (!plainTextEdit) {
						continue;
					}

				}
				else if (stackedWidget->currentIndex() == 1) {

				}
			}
		};


	mainLayout->addWidget(tabWidget, 1);
	addCentralWidget(mainWidget, true, true, 0);
}
