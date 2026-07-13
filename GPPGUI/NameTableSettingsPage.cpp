#include "NameTableSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaIconButton.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ElaTabWidget.h"
#include "ElaPlainTextEdit.h"
#include "DictionaryReader.h"

import Tool;

NameTableSettingsPage::NameTableSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), m_projectDir(projectDir), m_globalConfig(globalConfig), m_projectConfig(projectConfig)
{
	setWindowTitle("NameTable");
	setTitleVisible(false);

	setupUi();
}

void NameTableSettingsPage::refreshTable()
{
	if (m_refreshFunc) {
		m_refreshFunc();
	}
}

QList<NameTableEntry> NameTableSettingsPage::readNameTable()
{
	QList<NameTableEntry> result;
	fs::path nameTablePath = m_projectDir / L"NameTable.toml";
	if (!fs::exists(nameTablePath)) {
		return result;
	}

	try {
		const toml::ordered_value tbl = toml::uoparse(nameTablePath);
		for (const auto& [key, value] : tbl.as_table()) {
			if (!value.is_array() || value.size() < 2 || !value[0].is_string() || !value[1].is_integer()) {
				continue;
			}
			NameTableEntry entry;
			entry.original = QString::fromStdString(key);
			entry.translation = QString::fromStdString(value[0].as_string());
			entry.count = value[1].as_integer();
			result.push_back(entry);
		}
		/*std::ranges::sort(result, [](const NameTableEntry& a, const NameTableEntry& b)
			{
				return a.count > b.count;
			});*/
	}
	catch (...) {
		ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("NameTable.toml 不符合 toml 规范"), 3000);
		return result;
	}

	return result;
}

QString NameTableSettingsPage::readNameTableStr()
{
	return DictionaryReader::readDictStr(m_projectDir / L"NameTable.toml");
}

void NameTableSettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setIsTabTransparent(true);
	tabWidget->setTabsClosable(false);

	QWidget* nameTableWidget = new QWidget(mainWidget);
	QVBoxLayout* nameTableLayout = new QVBoxLayout(nameTableWidget);
	nameTableLayout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout* buttonLayout = new QHBoxLayout(nameTableWidget);
	ElaToolButton* plainTextModeButton = new ElaToolButton(nameTableWidget);
	plainTextModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	plainTextModeButton->setElaIcon(ElaIconType::Text);
	plainTextModeButton->setText(tr("纯文本"));
	ElaToolButton* tableModeButton = new ElaToolButton(nameTableWidget);
	tableModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	tableModeButton->setElaIcon(ElaIconType::Table);
	tableModeButton->setText(tr("表模式"));

	ElaIconButton* saveDictButton = new ElaIconButton(ElaIconType::Check, nameTableWidget);
	saveDictButton->setFixedWidth(30);
	ElaToolTip* saveDictButtonToolTip = new ElaToolTip(saveDictButton);
	saveDictButtonToolTip->setToolTip(tr("保存当前页"));
	ElaIconButton* withdrawButton = new ElaIconButton(ElaIconType::ArrowLeft, nameTableWidget);
	withdrawButton->setFixedWidth(30);
	ElaToolTip* withdrawButtonToolTip = new ElaToolTip(withdrawButton);
	withdrawButtonToolTip->setToolTip(tr("撤回删除行"));
	withdrawButton->setEnabled(false);
	ElaIconButton* refreshButton = new ElaIconButton(ElaIconType::ArrowRotateRight, nameTableWidget);
	refreshButton->setFixedWidth(30);
	ElaToolTip* refreshButtonToolTip = new ElaToolTip(refreshButton);
	refreshButtonToolTip->setToolTip(tr("刷新当前页"));
	ElaIconButton* addNameButton = new ElaIconButton(ElaIconType::Plus, nameTableWidget);
	addNameButton->setFixedWidth(30);
	ElaToolTip* addNameButtonToolTip = new ElaToolTip(addNameButton);
	addNameButtonToolTip->setToolTip(tr("添加词条"));
	ElaIconButton* delNameButton = new ElaIconButton(ElaIconType::Minus, nameTableWidget);
	delNameButton->setFixedWidth(30);
	ElaToolTip* delNameButtonToolTip = new ElaToolTip(delNameButton);
	delNameButtonToolTip->setToolTip(tr("删除词条"));

	buttonLayout->addWidget(plainTextModeButton);
	buttonLayout->addWidget(tableModeButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(saveDictButton);
	buttonLayout->addWidget(withdrawButton);
	buttonLayout->addWidget(refreshButton);
	buttonLayout->addWidget(addNameButton);
	buttonLayout->addWidget(delNameButton);

	QStackedWidget* stackedWidget = new QStackedWidget(nameTableWidget);
	// 纯文本模式
	ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
	QFont plainTextFont = plainTextEdit->font();
	plainTextFont.setPixelSize(15);
	plainTextEdit->setFont(plainTextFont);
	plainTextEdit->setPlainText(readNameTableStr());
	stackedWidget->addWidget(plainTextEdit);

	// 表模式
	ElaTableView* nameTableView = new ElaTableView(stackedWidget);
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
	nameTableView->setColumnWidth(0, toml::find_or(m_projectConfig, "GUIConfig", "nameTableColumnWidth", "0", 258));
	nameTableView->setColumnWidth(1, toml::find_or(m_projectConfig, "GUIConfig", "nameTableColumnWidth", "1", 258));
	nameTableView->setColumnWidth(2, toml::find_or(m_projectConfig, "GUIConfig", "nameTableColumnWidth", "2", 258));
	stackedWidget->addWidget(nameTableView);
	stackedWidget->setCurrentIndex(toml::find_or(m_globalConfig, "GUIConfig", "nameTableOpenMode", toml::find_or(m_globalConfig, "defaultNameTableOpenMode", 1)));

	plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
	tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
	addNameButton->setEnabled(stackedWidget->currentIndex() == 1);
	delNameButton->setEnabled(stackedWidget->currentIndex() == 1);

	connect(plainTextModeButton, &ElaToolButton::clicked, this, [=]()
		{
			stackedWidget->setCurrentIndex(0);
			plainTextModeButton->setEnabled(false);
			tableModeButton->setEnabled(true);
			addNameButton->setEnabled(false);
			delNameButton->setEnabled(false);
			withdrawButton->setEnabled(false);
		});
	connect(tableModeButton, &ElaToolButton::clicked, this, [=]()
		{
			stackedWidget->setCurrentIndex(1);
			plainTextModeButton->setEnabled(true);
			tableModeButton->setEnabled(false);
			addNameButton->setEnabled(true);
			delNameButton->setEnabled(true);
			withdrawButton->setEnabled(!m_withdrawList.isEmpty());
		});
	auto refreshFunc = [=]()
		{
			plainTextEdit->setPlainText(readNameTableStr());
			nameTableModel->loadData(readNameTable());
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"), tr("重新载入了 NameTable.toml"), 3000);
		};
	connect(refreshButton, &ElaPushButton::clicked, this, refreshFunc);
	connect(addNameButton, &ElaPushButton::clicked, this, [=]()
		{
			QModelIndexList index = nameTableView->selectionModel()->selectedIndexes();
			if (index.isEmpty()) {
				nameTableModel->insertRow(nameTableModel->rowCount());
			}
			else {
				nameTableModel->insertRow(index.first().row());
			}
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
					if (m_withdrawList.size() > 100) {
						m_withdrawList.pop_front();
					}
					m_withdrawList.push_back(entries[index.row()]);
					nameTableModel->removeRow(index.row());
				}
				if (!m_withdrawList.isEmpty()) {
					withdrawButton->setEnabled(true);
				}
			}
		});
	connect(saveDictButton, &ElaPushButton::clicked, this, [=]()
		{
			// 保存的提示和按钮绑定，因为保存项目配置时用的自己的提示
			// 但刷新可以直接把提示丢进refreshFunc里，因为一般用到这个函数的时候都要提示(单独保存字典时的重载是静默的)
			if (m_applyFunc) {
				m_applyFunc();
			}
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"), tr("已保存 NameTable.toml"), 3000);
		});
	connect(withdrawButton, &ElaPushButton::clicked, this, [=]()
		{
			if (m_withdrawList.isEmpty()) {
				return;
			}
			NameTableEntry entry = m_withdrawList.front();
			m_withdrawList.pop_front();
			nameTableModel->insertRow(0, entry);
			if (m_withdrawList.isEmpty()) {
				withdrawButton->setEnabled(false);
			}
		});


	nameTableLayout->addLayout(buttonLayout);
	nameTableLayout->addWidget(stackedWidget);

	m_applyFunc = [=]()
		{
			const fs::path nameTablePath = m_projectDir / L"NameTable.toml";
			std::ofstream ofs;
			int index = stackedWidget->currentIndex();
			if (index == 0) {
				atomicOutputFile(ofs, nameTablePath, plainTextEdit->toPlainText().toStdString());
				nameTableModel->loadData(readNameTable());
			}
			else if (index == 1) {
				toml::ordered_value tbl = toml::ordered_table{};
				tbl.comments().push_back("'原名' = [ '译名', 出现次数 ]");
				const QList<NameTableEntry>& entries = nameTableModel->getEntriesRef();
				for (const NameTableEntry& entry : entries) {
					if (entry.original.isEmpty()) {
						continue;
					}
					tbl[entry.original.toStdString()] = toml::array{ entry.translation.toStdString(), entry.count };
				}
				atomicOutputFile(ofs, nameTablePath, toml::format(tbl));
				plainTextEdit->setPlainText(readNameTableStr());
			}
			insertToml(m_projectConfig, "GUIConfig.nameTableOpenMode", stackedWidget->currentIndex());
			insertToml(m_projectConfig, "GUIConfig.nameTableColumnWidth.0", nameTableView->columnWidth(0));
			insertToml(m_projectConfig, "GUIConfig.nameTableColumnWidth.1", nameTableView->columnWidth(1));
			insertToml(m_projectConfig, "GUIConfig.nameTableColumnWidth.2", nameTableView->columnWidth(2));
		};

	m_refreshFunc = refreshFunc;

	tabWidget->addTab(nameTableWidget, tr("人名替换表"));
	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
