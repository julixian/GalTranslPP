#include "CommonNormalDictPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaIconButton.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaToggleButton.h"
#include "ElaPlainTextEdit.h"
#include "ElaTabWidget.h"
#include "ElaInputDialog.h"
#include "ReadDicts.h"

import Tool;

CommonNormalDictPage::CommonNormalDictPage(const std::string& mode, toml::ordered_value& globalConfig, QWidget* parent) :
	BasePage(parent), m_mainWindow(parent), m_globalConfig(globalConfig)
{
	setWindowTitle(tr("默认译前字典设置"));
	setTitleVisible(false);

	m_mode = mode;
	if (m_mode == "pre") {
		m_modeConfigKey = "commonPreDicts";
		m_modeDictDir = defaultPreDictPath;
	}
	else if (m_mode == "post") {
		m_modeConfigKey = "commonPostDicts";
		m_modeDictDir = defaultPostDictPath;
	}
	else {
		QMessageBox::critical(parent, tr("内部错误"), tr("未知通用字典模式"), QMessageBox::Ok);
		exit(1);
	}

	setupUi();
}

CommonNormalDictPage::~CommonNormalDictPage()
{

}

void CommonNormalDictPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	QHBoxLayout* mainButtonLayout = new QHBoxLayout(mainWidget);
	ElaText* dictNameLabel = new ElaText(mainWidget);
	dictNameLabel->setTextPixelSize(18);
	QString dictNameText;
	if (m_mode == "pre") {
		dictNameText = tr("通用译前字典");
	}
	else if (m_mode == "post") {
		dictNameText = tr("通用译后字典");
	}
	dictNameLabel->setText(dictNameText);
	ElaPushButton* importButton = new ElaPushButton(mainWidget);
	importButton->setText(tr("导入字典页"));
	ElaPushButton* addNewTabButton = new ElaPushButton(mainWidget);
	addNewTabButton->setText(tr("添加新字典页"));
	mainButtonLayout->addSpacing(10);
	mainButtonLayout->addWidget(dictNameLabel);
	mainButtonLayout->addStretch();
	mainButtonLayout->addWidget(importButton);
	mainButtonLayout->addWidget(addNewTabButton);
	mainLayout->addLayout(mainButtonLayout);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);

	auto createNormalTab = [=](const fs::path& orgDictPath) -> QWidget*
		{
			const fs::path dictPath = m_modeDictDir / orgDictPath.filename().replace_extension(L".toml");
			const std::string dictName = wide2Ascii(orgDictPath.stem().wstring());
			NormalTabEntry normalTabEntry;

			QWidget* pageMainWidget = new QWidget(tabWidget);
			QVBoxLayout* pageMainLayout = new QVBoxLayout(pageMainWidget);
			pageMainLayout->setContentsMargins(0, 0, 0, 0);

			QHBoxLayout* pageButtonLayout = new QHBoxLayout(pageMainWidget);
			ElaPushButton* plainTextModeButton = new ElaPushButton(pageMainWidget);
			plainTextModeButton->setText(tr("纯文本模式"));
			ElaPushButton* tableModeButton = new ElaPushButton(pageMainWidget);
			tableModeButton->setText(tr("表模式"));
			ElaToggleButton* defaultOnButton = new ElaToggleButton(pageMainWidget);
			defaultOnButton->setText(tr("默认启用"));
			ElaIconButton* saveAllButton = new ElaIconButton(ElaIconType::CheckDouble, pageMainWidget);
			saveAllButton->setFixedWidth(30);
			ElaToolTip* saveAllButtonToolTip = new ElaToolTip(saveAllButton);
			saveAllButtonToolTip->setToolTip(tr("保存所有页"));
			ElaIconButton* saveButton = new ElaIconButton(ElaIconType::Check, pageMainWidget);
			saveButton->setFixedWidth(30);
			ElaToolTip* saveButtonToolTip = new ElaToolTip(saveButton);
			saveButtonToolTip->setToolTip(tr("保存当前页"));
			ElaIconButton* removeTabButton = new ElaIconButton(ElaIconType::Trash, pageMainWidget);
			removeTabButton->setFixedWidth(30);
			ElaToolTip* removeTabButtonToolTip = new ElaToolTip(removeTabButton);
			removeTabButtonToolTip->setToolTip(tr("删除当前页"));
			ElaIconButton* renameTabButton = new ElaIconButton(ElaIconType::ArrowsRetweet, pageMainWidget);
			renameTabButton->setFixedWidth(30);
			ElaToolTip* renameTabButtonToolTip = new ElaToolTip(renameTabButton);
			renameTabButtonToolTip->setToolTip(tr("重命名当前页"));
			ElaIconButton* withdrawButton = new ElaIconButton(ElaIconType::ArrowLeft, pageMainWidget);
			withdrawButton->setFixedWidth(30);
			ElaToolTip* withdrawButtonToolTip = new ElaToolTip(withdrawButton);
			withdrawButtonToolTip->setToolTip(tr("撤回删除行"));
			withdrawButton->setEnabled(false);
			ElaIconButton* refreshButton = new ElaIconButton(ElaIconType::ArrowRotateRight, pageMainWidget);
			refreshButton->setFixedWidth(30);
			ElaToolTip* refreshButtonToolTip = new ElaToolTip(refreshButton);
			refreshButtonToolTip->setToolTip(tr("刷新当前页"));
			ElaIconButton* addDictButton = new ElaIconButton(ElaIconType::Plus, pageMainWidget);
			addDictButton->setFixedWidth(30);
			ElaToolTip* addDictButtonToolTip = new ElaToolTip(addDictButton);
			addDictButtonToolTip->setToolTip(tr("添加词条"));
			ElaIconButton* removeDictButton = new ElaIconButton(ElaIconType::Minus, pageMainWidget);
			removeDictButton->setFixedWidth(30);
			ElaToolTip* removeDictButtonToolTip = new ElaToolTip(removeDictButton);
			removeDictButtonToolTip->setToolTip(tr("删除词条"));
			pageButtonLayout->addWidget(plainTextModeButton);
			pageButtonLayout->addWidget(tableModeButton);
			pageButtonLayout->addWidget(defaultOnButton);
			pageButtonLayout->addStretch();
			pageButtonLayout->addWidget(saveAllButton);
			pageButtonLayout->addWidget(saveButton);
			pageButtonLayout->addWidget(removeTabButton);
			pageButtonLayout->addWidget(renameTabButton);
			pageButtonLayout->addWidget(withdrawButton);
			pageButtonLayout->addWidget(refreshButton);
			pageButtonLayout->addWidget(addDictButton);
			pageButtonLayout->addWidget(removeDictButton);
			pageMainLayout->addLayout(pageButtonLayout);

			QStackedWidget* stackedWidget = new QStackedWidget(tabWidget);
			ElaPlainTextEdit* plainTextEdit = new ElaPlainTextEdit(stackedWidget);
			QFont plainTextFont = plainTextEdit->font();
			plainTextFont.setPixelSize(15);
			plainTextEdit->setFont(plainTextFont);
			plainTextEdit->setPlainText(ReadDicts::readDictsStr(orgDictPath));
			stackedWidget->addWidget(plainTextEdit);
			ElaTableView* tableView = new ElaTableView(stackedWidget);
			QFont tableHeaderFont = tableView->horizontalHeader()->font();
			tableHeaderFont.setPixelSize(16);
			tableView->horizontalHeader()->setFont(tableHeaderFont);
			tableView->verticalHeader()->setHidden(true);
			tableView->setAlternatingRowColors(true);
			tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
			NormalDictModel* model = new NormalDictModel(tableView);
			QList<NormalDictEntry> normalData = ReadDicts::readNormalDicts(orgDictPath);
			model->loadData(normalData);
			tableView->setModel(model);
			stackedWidget->addWidget(tableView);
			stackedWidget->setCurrentIndex(toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "openMode", 1));
			tableView->setColumnWidth(0, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "0", 285));
			tableView->setColumnWidth(1, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "1", 195));
			tableView->setColumnWidth(2, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "2", 135));
			tableView->setColumnWidth(3, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "3", 250));
			tableView->setColumnWidth(4, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "4", 75));
			tableView->setColumnWidth(5, toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "columnWidth", "5", 60));
			pageMainLayout->addWidget(stackedWidget, 1);

			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			defaultOnButton->setIsToggled(toml::find_or(m_globalConfig, m_modeConfigKey, "spec", dictName, "defaultOn", true));
			insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".defaultOn", defaultOnButton->getIsToggled());

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
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName
						+ ".defaultOn", checked);
				});

			connect(saveAllButton, &ElaPushButton::clicked, this, [=]()
				{
					this->apply2Config();
					ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"), tr("所有默认字典配置均已保存"), 3000);
				});

			auto saveFunc = [=](bool forceSaveInTableModeToInit) -> bool // param 导入时先强制保存一下来给纯文本模式作初始化
				{
					const auto it = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_normalTabEntries.end()) {
						return false;
					}

					const std::string tmpDictName = wide2Ascii(it->dictPath.stem().wstring());
					std::ofstream ofs(it->dictPath, std::ios::binary);
					if (!ofs.is_open()) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("保存失败"), tr("无法打开字典: ") +
							QString::fromStdWString(dictPath.wstring()), 3000);
						return false;
					}

					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						ofs << plainTextEdit->toPlainText().toStdString();
						ofs.close();
						const QList<NormalDictEntry> newDictEntries = ReadDicts::readNormalDicts(it->dictPath);
						model->loadData(newDictEntries);
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						const QList<NormalDictEntry> dictEntries = model->getEntries();
						toml::ordered_value dictArr= toml::array{};
						for (const auto& entry : dictEntries) {
							toml::ordered_table dictTbl;
							dictTbl.insert({ "org", entry.original.toStdString() });
							dictTbl.insert({ "rep", entry.translation.toStdString() });
							dictTbl.insert({ "conditionTarget", entry.conditionTar.toStdString() });
							dictTbl.insert({ "conditionReg", entry.conditionReg.toStdString() });
							dictTbl.insert({ "isReg", entry.isReg });
							dictTbl.insert({ "priority", entry.priority });
							dictTbl["org"].as_string_fmt().fmt = toml::string_format::literal;
							dictTbl["rep"].as_string_fmt().fmt = toml::string_format::literal;
							dictTbl["conditionTarget"].as_string_fmt().fmt = toml::string_format::literal;
							dictTbl["conditionReg"].as_string_fmt().fmt = toml::string_format::literal;
							dictArr.push_back(dictTbl);
						}
						ofs << toml::ordered_value{ toml::ordered_table{{"normalDict", dictArr}} };
						ofs.close();
						plainTextEdit->setPlainText(ReadDicts::readDictsStr(it->dictPath));
					}

					auto& dictNamesArr = m_globalConfig[m_modeConfigKey]["dictNames"];
					if (!dictNamesArr.is_array()) {
						dictNamesArr = toml::array{ tmpDictName };
					}
					else {
						if (
							!std::ranges::any_of(dictNamesArr.as_array(), [=](const toml::ordered_value& name)
							{
								return name.is_string() && name.as_string() == tmpDictName;
							})
							) {
							dictNamesArr.push_back(tmpDictName);
						}
					}

					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".openMode", stackedWidget->currentIndex());
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.0", tableView->columnWidth(0));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.1", tableView->columnWidth(1));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.2", tableView->columnWidth(2));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.3", tableView->columnWidth(3));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.4", tableView->columnWidth(4));
					insertToml(m_globalConfig, m_modeConfigKey + ".spec." + dictName + ".columnWidth.5", tableView->columnWidth(5));
					return true;
				};
			normalTabEntry.saveFunc = saveFunc;

			connect(saveButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto it = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_normalTabEntries.end()) {
						return;
					}
					if (it->saveFunc(false)) {
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"), tr("字典 ") +
							QString::fromStdWString(it->dictPath.stem().wstring()) + tr(" 已保存"), 3000);
					}
				});
			connect(addDictButton, &ElaPushButton::clicked, this, [=]()
				{
					const QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
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
					const NormalDictEntry entry = normalTabEntry.withdrawList->back();
					normalTabEntry.withdrawList->pop_back();
					model->insertRow(0, entry);
					if (normalTabEntry.withdrawList->empty()) {
						withdrawButton->setEnabled(false);
					}
				});
			connect(refreshButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto it = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_normalTabEntries.end()) {
						return;
					}
					plainTextEdit->setPlainText(ReadDicts::readDictsStr(it->dictPath));
					model->loadData(ReadDicts::readNormalDicts(it->dictPath));
					ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"), tr("字典 ") +
						QString::fromStdWString(it->dictPath.stem().wstring()) + tr(" 已刷新"), 3000);
				});
			connect(renameTabButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto normalTabEntryIt = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (normalTabEntryIt == m_normalTabEntries.end()) {
						return;
					}

					QString newDictName;
					ElaInputDialog inputDialog(m_mainWindow, tr("请输入新名称"), tr("重命名字典"), newDictName);
					if (inputDialog.exec() != QDialog::Accepted) {
						return;
					}

					if (newDictName.isEmpty() || newDictName.contains('/') || newDictName.contains('\\') || newDictName.contains('.')) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft,
							tr("重命名失败"), tr("字典名称不能为空，且不能包含点号、斜杠或反斜杠！"), 3000);
						return;
					}

					const bool hasSameNameTab = std::ranges::any_of(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget != pageMainWidget && entry.dictPath.stem().wstring() == newDictName.toStdWString();
						});
					if (hasSameNameTab || newDictName == "项目译前字典" || newDictName == "项目译后字典") {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"), tr("字典 ") +
							newDictName + tr(" 已存在"), 3000);
						return;
					}

					const fs::path oldDictPath = normalTabEntryIt->dictPath;
					const std::string oldDictName = wide2Ascii(oldDictPath.stem().wstring());
					const fs::path newDictPath = m_modeDictDir / (newDictName.toStdWString() + L".toml");
					try {
						if (fs::exists(oldDictPath)) {
							try {
								fs::rename(oldDictPath, newDictPath);
							}
							catch(...) { }
						}
						normalTabEntryIt->dictPath = newDictPath;
						auto& dictNames = m_globalConfig[m_modeConfigKey]["dictNames"];
						if (dictNames.is_array()) {
							const auto dictNameIt = std::ranges::find_if(dictNames.as_array(), [=](const auto& elem)
								{
									return elem.is_string() && elem.as_string() == oldDictName;
								});
							if (dictNameIt != dictNames.as_array().end()) {
								*dictNameIt = newDictName.toStdString();
							}
						}
						else {
							dictNames = toml::array{};
						}
						tabWidget->setTabText(tabWidget->indexOf(pageMainWidget), newDictName);
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("重命名成功"), tr("字典 ") +
							QString::fromStdWString(oldDictPath.stem().wstring()) + tr(" 已重命名为 ") + newDictName, 3000);
					}
					catch (...) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("重命名失败"), tr("字典 ") +
							QString::fromStdWString(oldDictPath.stem().wstring()) + tr(" 重命名失败"), 3000);
						return;
					}
				});
			connect(removeTabButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto normalTabEntryIt = std::ranges::find_if(m_normalTabEntries, [=](const NormalTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (normalTabEntryIt == m_normalTabEntries.end()) {
						return;
					}

					std::string tmpDictName = wide2Ascii(normalTabEntryIt->dictPath.stem().wstring());

					// 删除提示框
					ElaContentDialog helpDialog(m_mainWindow);

					helpDialog.setRightButtonText(tr("是"));
					helpDialog.setMiddleButtonText(tr("思考人生"));
					helpDialog.setLeftButtonText(tr("否"));

					QWidget* widget = new QWidget(&helpDialog);
					QVBoxLayout* layout = new QVBoxLayout(widget);
					layout->setContentsMargins(15, 25, 15, 10);
					ElaText* confirmText = new ElaText(tr("你确定要删除 ") + QString::fromStdString(tmpDictName) + tr(" 吗？"), widget);
					confirmText->setTextStyle(ElaTextType::Title);
					confirmText->setWordWrap(false);
					layout->addWidget(confirmText);
					layout->addSpacing(2);
					ElaText* subTitle = new ElaText(tr("将永久删除该字典文件，如有需要请先备份！"), 16, widget);
					subTitle->setTextStyle(ElaTextType::Body);
					layout->addWidget(subTitle);
					layout->addStretch();
					helpDialog.setCentralWidget(widget);

					if (helpDialog.exec() == QDialog::Accepted) {
						pageMainWidget->deleteLater();
						tabWidget->removeTab(tabWidget->indexOf(pageMainWidget));
						try {
							fs::remove(normalTabEntryIt->dictPath);
						}
						catch (...) {}
						m_normalTabEntries.erase(normalTabEntryIt);
						auto& dictNames = m_globalConfig[m_modeConfigKey]["dictNames"];
						if (dictNames.is_array()) {
							auto dictNameIt = std::ranges::find_if(dictNames.as_array(), [=](const auto& elem)
								{
									return elem.is_string() && elem.as_string() == tmpDictName;
								});
							if (dictNameIt != dictNames.as_array().end()) {
								dictNames.as_array().erase(dictNameIt);
							}
						}
						else {
							dictNames = toml::array{};
						}
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("删除成功"), tr("字典 ")
							+ QString::fromStdString(tmpDictName) + tr(" 已从字典管理和磁盘中移除！"), 3000);
					}
				});

			normalTabEntry.pageMainWidget = pageMainWidget;
			normalTabEntry.stackedWidget = stackedWidget;
			normalTabEntry.plainTextEdit = plainTextEdit;
			normalTabEntry.tableView = tableView;
			normalTabEntry.dictModel = model;
			normalTabEntry.dictPath = dictPath;
			m_normalTabEntries.push_back(normalTabEntry);

			if (!fs::exists(dictPath)) {
				saveFunc(true);
			}
			return pageMainWidget;
		};

	auto& commonNormalDicts = m_globalConfig[m_modeConfigKey]["dictNames"];
	if (commonNormalDicts.is_array()) {
		auto it = commonNormalDicts.as_array().begin();
		while (it != commonNormalDicts.as_array().end()) {
			if (!it->is_string()) {
				it = commonNormalDicts.as_array().erase(it);
				continue;
			}
			fs::path dictPath = m_modeDictDir / (ascii2Wide(it->as_string()) + L".toml");
			if (!fs::exists(dictPath)) {
				it = commonNormalDicts.as_array().erase(it);
				continue;
			}
			QWidget* pageMainWidget = createNormalTab(dictPath);
			tabWidget->addTab(pageMainWidget, QString::fromStdString(it->as_string()));
			++it;
		}
	}
	else {
		commonNormalDicts = toml::array{};
	}

	tabWidget->setCurrentIndex(0);

	connect(importButton, &ElaPushButton::clicked, this, [=]()
		{
			const QString importDictPathQStr = QFileDialog::getOpenFileName(this, tr("选择字典文件"),
				QString::fromStdString(toml::find_or(m_globalConfig, "lastCommonNormalDictPath", "./")),
				"TOML files (*.toml);;JSON files (*.json)");
			if (importDictPathQStr.isEmpty()) {
				return;
			}
			insertToml(m_globalConfig, "lastCommonNormalDictPath", importDictPathQStr.toStdString());
			const fs::path importDictPath = importDictPathQStr.toStdWString();
			const fs::path newDictPath = m_modeDictDir / (importDictPath.stem().wstring() + L".toml");
			if (fs::exists(newDictPath) && !fs::equivalent(importDictPath, newDictPath)) {
				try {
					fs::remove(newDictPath);
				}
				catch (...) {
					ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("导入失败"), tr("原文件删除失败"), 3000);
					return;
				}
			}
			const bool hasSameNameTab = std::ranges::any_of(m_normalTabEntries, [=](const NormalTabEntry& entry)
				{
					return entry.dictPath.stem().wstring() == importDictPath.stem().wstring();
				});
			if (hasSameNameTab) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("导入失败"), tr("字典 ") +
					QString::fromStdWString(importDictPath.stem().wstring()) + tr(" 已存在"), 3000);
				return;
			}
			QWidget* pageMainWidget = createNormalTab(importDictPath);
			tabWidget->addTab(pageMainWidget, QString::fromStdWString(importDictPath.stem().wstring()));
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("创建成功"), tr("字典页 ") +
				QString::fromStdWString(importDictPath.stem().wstring()) + tr(" 已创建"), 3000);
		});

	connect(addNewTabButton, &ElaPushButton::clicked, this, [=]()
		{
			QString dictName;
			ElaInputDialog inputDialog(m_mainWindow, tr("请输入字典表名称"), tr("新建字典"), dictName);
			if (inputDialog.exec() != QDialog::Accepted) {
				return;
			}

			if (dictName.isEmpty() || dictName.contains('/') || dictName.contains('\\') || dictName.contains('.')) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft,
					tr("新建失败"), tr("字典名称不能为空，且不能包含点号、斜杠或反斜杠！"), 3000);
				return;
			}

			const fs::path newDictPath = m_modeDictDir / (dictName.toStdWString() + L".toml");
			const bool hasSameNameTab = std::ranges::any_of(m_normalTabEntries, [=](const NormalTabEntry& entry)
				{
					return entry.dictPath.stem().wstring() == dictName.toStdWString();
				});
			if (hasSameNameTab || dictName == "项目译前字典" || dictName == "项目译后字典") {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"), tr("字典 ") +
					QString::fromStdWString(newDictPath.stem().wstring()) + tr(" 已存在"), 3000);
				return;
			}

			std::ofstream ofs(newDictPath, std::ios::binary);
			if (!ofs.is_open()) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"), tr("无法创建 ") +
					QString::fromStdWString(newDictPath.wstring()) + tr(" 文件"), 3000);
				return;
			}
			ofs.close();

			QWidget* pageMainWidget = createNormalTab(newDictPath);
			tabWidget->addTab(pageMainWidget, dictName);
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("新建成功"), tr("字典页 ") +
				QString::fromStdWString(newDictPath.stem().wstring()) + tr(" 已创建"), 3000);
		});


	m_applyFunc = [=]()
		{
			toml::array dictNamesArr;
			std::vector<std::pair<std::string, QWidget*>> pageWidgets;
			for (const NormalTabEntry& entry : m_normalTabEntries) {
				if (!entry.saveFunc(false)) {
					continue;
				}
				std::string dictName = wide2Ascii(entry.dictPath.stem().wstring());
				pageWidgets.push_back({ dictName, entry.pageMainWidget });
			}
			std::ranges::sort(pageWidgets, [=](const auto& a, const auto& b)
				{
					return tabWidget->indexOf(a.second) < tabWidget->indexOf(b.second);
				});
			for (const auto& dictName : pageWidgets | std::views::keys) {
				dictNamesArr.push_back(dictName);
			}

			insertToml(m_globalConfig, m_modeConfigKey + ".dictNames", dictNamesArr);

			auto& spec = m_globalConfig[m_modeConfigKey]["spec"];
			if (spec.is_table()) {
				for (const auto& key : spec.as_table() | std::views::keys) {
					if (
						!std::ranges::any_of(dictNamesArr, [&](const auto& elem)
							{
								return elem.is_string() && elem.as_string() == key;
							})
						)
					{
						spec.as_table().erase(key);
					}
				}
			}
			else {
				spec = toml::ordered_table{};
			}
			Q_EMIT commonDictsChangedSignal();
		};


	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
