#include "NameTableSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"

import Tool;

NameTableSettingsPage::NameTableSettingsPage(fs::path& projectDir, toml::table& globalConfig, toml::table& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir)
{
	setWindowTitle("人名表");
	setTitleVisible(false);

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
		if (auto valueArr = value.as_array()) {
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
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	QWidget* buttonWidget = new QWidget(mainWidget);
	QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
	ElaPushButton* plainTextModeButtom = new ElaPushButton("切换至纯文本模式", buttonWidget);
	ElaPushButton* TableModeButtom = new ElaPushButton("切换至表模式", buttonWidget);
	ElaPushButton* refreshButtom = new ElaPushButton("刷新", buttonWidget);
	ElaPushButton* addNameButtom = new ElaPushButton("添加人名", buttonWidget);
	ElaPushButton* delNameButtom = new ElaPushButton("删除人名", buttonWidget);
	buttonLayout->addWidget(plainTextModeButtom);
	buttonLayout->addWidget(TableModeButtom);
	buttonLayout->addStretch();
	buttonLayout->addWidget(refreshButtom);
	buttonLayout->addWidget(addNameButtom);
	buttonLayout->addWidget(delNameButtom);

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
	insertToml(_projectConfig, "GUIConfig.nameTableMode", stackedWidget->currentIndex());

	plainTextModeButtom->setEnabled(stackedWidget->currentIndex() != 0);
	TableModeButtom->setEnabled(stackedWidget->currentIndex() != 1);
	addNameButtom->setEnabled(stackedWidget->currentIndex() == 1);
	delNameButtom->setEnabled(stackedWidget->currentIndex() == 1);

	connect(plainTextModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			stackedWidget->setCurrentIndex(0);
			plainTextModeButtom->setEnabled(false);
			TableModeButtom->setEnabled(true);
			addNameButtom->setEnabled(false);
			delNameButtom->setEnabled(false);
			insertToml(_projectConfig, "GUIConfig.nameTableOpenMode", 0);
		});
	connect(TableModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			stackedWidget->setCurrentIndex(1);
			plainTextModeButtom->setEnabled(true);
			TableModeButtom->setEnabled(false);
			addNameButtom->setEnabled(true);
			delNameButtom->setEnabled(true);
			insertToml(_projectConfig, "GUIConfig.nameTableOpenMode", 1);
		});
	auto refreshFunc = [=]()
		{
			plainTextEdit->setPlainText(readNameTableStr());
			nameTableModel->loadData(readNameTable());
			ElaMessageBar::success(ElaMessageBarType::TopLeft, "刷新成功", "重新载入了人名表", 3000);
		};
	connect(refreshButtom, &ElaPushButton::clicked, this, refreshFunc);
	connect(addNameButtom, &ElaPushButton::clicked, this, [=]()
		{
			nameTableModel->insertRow(nameTableModel->rowCount());
		});
	connect(delNameButtom, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList indexList = nameTableView->selectionModel()->selectedRows();
			if (!indexList.isEmpty()) {
				std::ranges::sort(indexList, [](const QModelIndex& a, const QModelIndex& b)
					{
						return a.row() > b.row();
					});
				for (const QModelIndex& index : indexList) {
					nameTableModel->removeRow(index.row());
				}
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
					toml::table tbl;
					tbl.insert(entry.original.toStdString(), toml::array{ entry.translation.toStdString(), entry.count });
					ofs << tbl << "\n";
				}
				ofs.close();
				plainTextEdit->setPlainText(readNameTableStr());
			}
			insertToml(_projectConfig, "GUIConfig.nameTableColumnWidth.0", nameTableView->columnWidth(0));
			insertToml(_projectConfig, "GUIConfig.nameTableColumnWidth.1", nameTableView->columnWidth(1));
			insertToml(_projectConfig, "GUIConfig.nameTableColumnWidth.2", nameTableView->columnWidth(2));
		};

	_refreshFunc = refreshFunc;

	addCentralWidget(mainWidget, true, true, 0);
}
