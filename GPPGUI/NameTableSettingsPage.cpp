#include "NameTableSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaTabWidget.h"
#include "ElaPlainTextEdit.h"

import Tool;

NameTableSettingsPage::NameTableSettingsPage(fs::path& projectDir, toml::table& globalConfig, toml::table& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir)
{
	setWindowTitle("人名表");
	setTitleVisible(false);
	setContentsMargins(0, 0, 0, 0);

	_setupUI();
}

NameTableSettingsPage::~NameTableSettingsPage()
{

}

void NameTableSettingsPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

void NameTableSettingsPage::refreshTable()
{
	if (_refreshFunc) {
		_refreshFunc();
	}
}

QList<NameTableEntry> NameTableSettingsPage::readNameTable()
{
	QList<NameTableEntry> result;
	fs::path nameTablePath = _projectDir / L"人名替换表.toml";
	if (!fs::exists(nameTablePath)) {
		return result;
	}
	std::ifstream ifs(nameTablePath);
	toml::table tbl;
	try {
		tbl = toml::parse(ifs);
	}
	catch (...) {
		ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "人名替换表 不符合规范", 3000);
		return result;
	}
	ifs.close();
	for (const auto& [key, value] : tbl) {
		NameTableEntry entry;
		entry.original = QString::fromStdString(std::string(key.str()));
		if (auto valueArr = value.as_array(); valueArr->size() >= 2) {
			if (auto optTrans = valueArr->get(0)->value<std::string>()) {
				entry.translation = QString::fromStdString(*optTrans);
			}
			if (auto optCount = valueArr->get(1)->value<int>()) {
				entry.count = *optCount;
			}
		}
		result.push_back(entry);
	}
	std::ranges::sort(result, [](const NameTableEntry& a, const NameTableEntry& b)
		{
			return a.count > b.count;
		});
	return result;
}

QString NameTableSettingsPage::readNameTableStr()
{
	QString result;
	fs::path nameTablePath = _projectDir / L"人名替换表.toml";
	if (!fs::exists(nameTablePath)) {
		return result;
	}
	std::ifstream ifs(nameTablePath);
	std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	result = QString::fromStdString(str);
	return result;
}

void NameTableSettingsPage::_setupUI()
{
	ElaTabWidget* tabWidget = new ElaTabWidget(this);
	tabWidget->setIsTabTransparent(true);
	tabWidget->setTabsClosable(false);

	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	QWidget* buttonWidget = new QWidget(mainWidget);
	QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
	ElaPushButton* plainTextModeButton = new ElaPushButton("切换至纯文本模式", buttonWidget);
	ElaPushButton* TableModeButton = new ElaPushButton("切换至表模式", buttonWidget);
	ElaPushButton* saveDictButton = new ElaPushButton("保存", buttonWidget);
	ElaPushButton* withdrawButton = new ElaPushButton("撤回", buttonWidget);
	withdrawButton->setEnabled(false);
	ElaPushButton* refreshButton = new ElaPushButton("刷新", buttonWidget);
	ElaPushButton* addNameButton = new ElaPushButton("添加人名", buttonWidget);
	ElaPushButton* delNameButton = new ElaPushButton("删除人名", buttonWidget);
	buttonLayout->addWidget(plainTextModeButton);
	buttonLayout->addWidget(TableModeButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(saveDictButton);
	buttonLayout->addWidget(withdrawButton);
	buttonLayout->addWidget(refreshButton);
	buttonLayout->addWidget(addNameButton);
	buttonLayout->addWidget(delNameButton);

	QStackedWidget* stackedWidget = new QStackedWidget(mainWidget);
	// 纯文本模式
	ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(mainWidget);
	QFont plainTextFont = plainTextEdit->font();
	plainTextFont.setPixelSize(15);
	plainTextEdit->setFont(plainTextFont);
	plainTextEdit->setPlainText(readNameTableStr());
	stackedWidget->addWidget(plainTextEdit);

	// 表模式
	ElaTableView* nameTableView = new ElaTableView(mainWidget);
	NameTableModel* nameTableModel = new NameTableModel(nameTableView);
	QList<NameTableEntry> nameList = readNameTable();
	nameTableModel->loadData(nameList);
	nameTableView->setModel(nameTableModel);
	QFont tableFont = nameTableView->horizontalHeader()->font();
	tableFont.setPixelSize(16);
	nameTableView->horizontalHeader()->setFont(tableFont);
	nameTableView->verticalHeader()->setHidden(true);
	nameTableView->setAlternatingRowColors(true);
	nameTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	nameTableView->setColumnWidth(0, _projectConfig["GUIConfig"]["nameTableColumnWidth"]["0"].value_or(258));
	nameTableView->setColumnWidth(1, _projectConfig["GUIConfig"]["nameTableColumnWidth"]["1"].value_or(258));
	nameTableView->setColumnWidth(2, _projectConfig["GUIConfig"]["nameTableColumnWidth"]["2"].value_or(258));
	stackedWidget->addWidget(nameTableView);
	stackedWidget->setCurrentIndex(_projectConfig["GUIConfig"]["nameTableOpenMode"].value_or(_globalConfig["defaultNameTableOpenMode"].value_or(0)));

	plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
	TableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
	addNameButton->setEnabled(stackedWidget->currentIndex() == 1);
	delNameButton->setEnabled(stackedWidget->currentIndex() == 1);

	connect(plainTextModeButton, &ElaPushButton::clicked, this, [=]()
		{
			stackedWidget->setCurrentIndex(0);
			plainTextModeButton->setEnabled(false);
			TableModeButton->setEnabled(true);
			addNameButton->setEnabled(false);
			delNameButton->setEnabled(false);
			withdrawButton->setEnabled(false);
		});
	connect(TableModeButton, &ElaPushButton::clicked, this, [=]()
		{
			stackedWidget->setCurrentIndex(1);
			plainTextModeButton->setEnabled(true);
			TableModeButton->setEnabled(false);
			addNameButton->setEnabled(true);
			delNameButton->setEnabled(true);
			withdrawButton->setEnabled(!_withdrawList.isEmpty());
		});
	auto refreshFunc = [=]()
		{
			plainTextEdit->setPlainText(readNameTableStr());
			nameTableModel->loadData(readNameTable());
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "刷新成功", "重新载入了人名表", 3000);
		};
	connect(refreshButton, &ElaPushButton::clicked, this, refreshFunc);
	connect(addNameButton, &ElaPushButton::clicked, this, [=]()
		{
			nameTableModel->insertRow(nameTableModel->rowCount());
		});
	connect(delNameButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList indexList = nameTableView->selectionModel()->selectedRows();
			const QList<NameTableEntry>& entries = nameTableModel->getEntriesRef();
			if (!indexList.isEmpty()) {
				std::ranges::sort(indexList, [](const QModelIndex& a, const QModelIndex& b)
					{
						return a.row() > b.row();
					});
				for (const QModelIndex& index : indexList) {
					if (_withdrawList.size() > 100) {
						_withdrawList.pop_front();
					}
					_withdrawList.push_back(entries[index.row()]);
					nameTableModel->removeRow(index.row());
				}
				if (!_withdrawList.isEmpty()) {
					withdrawButton->setEnabled(true);
				}
			}
		});
	connect(saveDictButton, &ElaPushButton::clicked, this, [=]()
		{
			// 保存的提示和按钮绑定，因为保存项目配置时用的自己的提示
			// 但刷新可以直接把提示丢进refreshFunc里，因为一般用到这个函数的时候都要提示(单独保存字典时的重载入是静默的)
			if (_applyFunc) {
				_applyFunc();
			}
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "保存成功", "已保存人名替换表", 3000);
		});
	connect(withdrawButton, &ElaPushButton::clicked, this, [=]()
		{
			if (_withdrawList.isEmpty()) {
				return;
			}
			NameTableEntry entry = _withdrawList.front();
			_withdrawList.pop_front();
			nameTableModel->insertRow(0, entry);
			if (_withdrawList.isEmpty()) {
				withdrawButton->setEnabled(false);
			}
		});


	mainLayout->addWidget(buttonWidget);
	mainLayout->addWidget(stackedWidget, 1);

	_applyFunc = [=]()
		{
			std::ofstream ofs(_projectDir / L"人名替换表.toml");
			int index = stackedWidget->currentIndex();
			if (index == 0) {
				std::string str = plainTextEdit->toPlainText().toStdString();
				ofs << str;
				ofs.close();
				nameTableModel->loadData(readNameTable());
			}
			else if (index == 1) {
				ofs << "# '原名' = [ '译名', 出现次数 ]\n";
				QList<NameTableEntry> entries = nameTableModel->getEntries();
				for (const NameTableEntry& entry : entries) {
					if (entry.original.isEmpty()) {
						continue;
					}
					toml::table tbl;
					tbl.insert(entry.original.toStdString(), toml::array{ entry.translation.toStdString(), entry.count });
					ofs << tbl << "\n";
				}
				ofs.close();
				plainTextEdit->setPlainText(readNameTableStr());
			}
			insertToml(_projectConfig, "GUIConfig.nameTableOpenMode", stackedWidget->currentIndex());
			insertToml(_projectConfig, "GUIConfig.nameTableColumnWidth.0", nameTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.nameTableColumnWidth.1", nameTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.nameTableColumnWidth.2", nameTableView->columnWidth(2));
		};

	_refreshFunc = refreshFunc;

	tabWidget->addTab(mainWidget, "人名表");
	addCentralWidget(tabWidget, true, true, 0);
}
