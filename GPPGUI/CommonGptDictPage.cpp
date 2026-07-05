#include "CommonGptDictPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaIconButton.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaToggleButton.h"
#include "ElaPlainTextEdit.h"
#include "ElaTabWidget.h"
#include "ElaInputDialog.h"
#include "ReadDicts.h"

import Tool;
namespace fs = std::filesystem;

CommonGptDictPage::CommonGptDictPage(toml::ordered_value& globalConfig, QWidget* parent) :
	BasePage(parent), m_globalConfig(globalConfig)
{
	setWindowTitle(tr("默认GPT字典设置"));
	setTitleVisible(false);

	setupUi();
}

CommonGptDictPage::~CommonGptDictPage()
{

}

void CommonGptDictPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	QHBoxLayout* mainButtonLayout = new QHBoxLayout(mainWidget);
	ElaText* dictNameLabel = new ElaText(tr("通用GPT字典"), mainWidget);
	dictNameLabel->setTextPixelSize(18);
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

	auto createGptTab = [=](const fs::path& orgDictPath) -> QWidget*
		{
			const fs::path dictPath = defaultGptDictPath / fs::path(orgDictPath.filename()).replace_extension(".toml");
			const std::string dictName = wide2Ascii(orgDictPath.stem().wstring());
			GptTabEntry gptTabEntry;

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
			GptDictModel* model = new GptDictModel(tableView);
			const QList<GptDictEntry> gptData = ReadDicts::readGptDicts(orgDictPath);
			model->loadData(gptData);
			tableView->setModel(model);
			stackedWidget->addWidget(tableView);
			stackedWidget->setCurrentIndex(toml::find_or(m_globalConfig, "commonGptDicts", "spec", dictName, "openMode", 1));
			tableView->setColumnWidth(0, toml::find_or(m_globalConfig, "commonGptDicts", "spec", dictName, "columnWidth", "0", 360));
			tableView->setColumnWidth(1, toml::find_or(m_globalConfig, "commonGptDicts", "spec", dictName, "columnWidth", "1", 215));
			tableView->setColumnWidth(2, toml::find_or(m_globalConfig, "commonGptDicts", "spec", dictName, "columnWidth", "2", 425));
			pageMainLayout->addWidget(stackedWidget);

			plainTextModeButton->setEnabled(stackedWidget->currentIndex() != 0);
			tableModeButton->setEnabled(stackedWidget->currentIndex() != 1);
			addDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			removeDictButton->setEnabled(stackedWidget->currentIndex() == 1);
			defaultOnButton->setIsToggled(toml::find_or(m_globalConfig, "commonGptDicts", "spec", dictName, "defaultOn", true));
			insertToml(m_globalConfig, "commonGptDicts.spec." + dictName + ".defaultOn", defaultOnButton->getIsToggled());

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
					withdrawButton->setEnabled(!gptTabEntry.withdrawList->empty());
				});

			connect(defaultOnButton, &ElaToggleButton::toggled, this, [=](bool checked)
				{
					insertToml(m_globalConfig, "commonGptDicts.spec." + dictName
						+ ".defaultOn", checked);
				});

			connect(saveAllButton, &ElaPushButton::clicked, this, [=]()
				{
					this->apply2Config();
					ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"), tr("所有默认字典配置均已保存"), 3000);
				});

			auto saveFunc = [=](bool forceSaveInTableModeToInit) -> bool
				{
					const auto it = std::ranges::find_if(m_gptTabEntries, [=](const GptTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_gptTabEntries.end()) {
						return false;
					}
					std::ofstream ofs(it->dictPath, std::ios::binary);
					if (!ofs.is_open()) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("保存失败"),
							tr("无法打开文件: %1").arg(QString::fromStdWString(it->dictPath.wstring())), 3000);
						return false;
					}

					const std::string tmpDictName = wide2Ascii(it->dictPath.stem().wstring());

					if (stackedWidget->currentIndex() == 0 && !forceSaveInTableModeToInit) {
						ofs << plainTextEdit->toPlainText().toStdString();
						ofs.close();
						const QList<GptDictEntry> newDictEntries = ReadDicts::readGptDicts(it->dictPath);
						model->loadData(newDictEntries);
					}
					else if (stackedWidget->currentIndex() == 1 || forceSaveInTableModeToInit) {
						const QList<GptDictEntry> dictEntries = model->getEntries();
						toml::ordered_value dictArr = toml::array{};
						for (const auto& entry : dictEntries) {
							toml::ordered_table dictTable;
							dictTable.insert({ "org", entry.original.toStdString() });
							dictTable.insert({ "rep", entry.translation.toStdString() });
							dictTable.insert({ "note", entry.description.toStdString() });
							dictArr.push_back(dictTable);
						}
						dictArr.as_array_fmt().fmt = toml::array_format::multiline;
						ofs << toml::ordered_value{ toml::ordered_table{{"gptDict", dictArr}} };
						ofs.close();
						plainTextEdit->setPlainText(ReadDicts::readDictsStr(it->dictPath));
					}

					auto& dictNamesArr = m_globalConfig["commonGptDicts"]["dictNames"];
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

					insertToml(m_globalConfig, "commonGptDicts.spec." + tmpDictName + ".openMode", stackedWidget->currentIndex());
					insertToml(m_globalConfig, "commonGptDicts.spec." + tmpDictName + ".columnWidth.0", tableView->columnWidth(0));
					insertToml(m_globalConfig, "commonGptDicts.spec." + tmpDictName + ".columnWidth.1", tableView->columnWidth(1));
					insertToml(m_globalConfig, "commonGptDicts.spec." + tmpDictName + ".columnWidth.2", tableView->columnWidth(2));
					return true;
				};
			gptTabEntry.saveFunc = saveFunc;

			connect(saveButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto it = std::ranges::find_if(m_gptTabEntries, [=](const GptTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_gptTabEntries.end()) {
						return;
					}
					if (it->saveFunc(false)) {
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("保存成功"),
							tr("字典 %1 已保存").arg(QString::fromStdWString(it->dictPath.stem().wstring())), 3000);
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
					const QList<GptDictEntry>& entries = model->getEntriesRef();
					std::ranges::sort(selectedRows, [](const QModelIndex& a, const QModelIndex& b)
						{
							return a.row() > b.row();
						});
					for (const auto& index : selectedRows) {
						if (gptTabEntry.withdrawList->size() > 100) {
							gptTabEntry.withdrawList->pop_front();
						}
						gptTabEntry.withdrawList->push_back(entries[index.row()]);
						model->removeRow(index.row());
					}
					if (!gptTabEntry.withdrawList->empty()) {
						withdrawButton->setEnabled(true);
					}
				});
			connect(withdrawButton, &ElaPushButton::clicked, this, [=]()
				{
					if (gptTabEntry.withdrawList->empty()) {
						return;
					}
					const GptDictEntry entry = gptTabEntry.withdrawList->back();
					gptTabEntry.withdrawList->pop_back();
					model->insertRow(0, entry);
					if (gptTabEntry.withdrawList->empty()) {
						withdrawButton->setEnabled(false);
					}
				});
			connect(refreshButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto it = std::ranges::find_if(m_gptTabEntries, [=](const GptTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (it == m_gptTabEntries.end()) {
						return;
					}
					plainTextEdit->setPlainText(ReadDicts::readDictsStr(it->dictPath));
					model->loadData(ReadDicts::readGptDicts(it->dictPath));
					ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("刷新成功"),
						tr("字典 %1 已刷新").arg(QString::fromStdWString(it->dictPath.filename().wstring())), 3000);
				});
			connect(renameTabButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto gptTabEntryIt = std::ranges::find_if(m_gptTabEntries, [=](const GptTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (gptTabEntryIt == m_gptTabEntries.end()) {
						return;
					}

					QString newDictName;
					ElaInputDialog inputDialog(tr("请输入新名称"), tr("重命名字典"), newDictName, window());
					if (inputDialog.exec() != QDialog::Accepted) {
						return;
					}

					if (newDictName.isEmpty() || newDictName.contains('/') || newDictName.contains('\\') || newDictName.contains('.')) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft,
							tr("重命名失败"), tr("字典名称不能为空，且不能包含点号、斜杠或反斜杠！"), 3000);
						return;
					}

					const bool hasSameNameTab = std::ranges::any_of(m_gptTabEntries, [=](const GptTabEntry& entry)
						{
							return entry.pageMainWidget != pageMainWidget && entry.dictPath.stem().wstring() == newDictName.toStdWString();
						});
					if (hasSameNameTab || newDictName == "项目GPT字典") {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"),
							tr("字典 %1 已存在").arg(newDictName), 3000);
						return;
					}

					const fs::path oldDictPath = gptTabEntryIt->dictPath;
					const std::string oldDictName = wide2Ascii(oldDictPath.stem().wstring());
					const fs::path newDictPath = defaultGptDictPath / (newDictName.toStdWString() + L".toml");
					try {
						if (fs::exists(oldDictPath)) {
							try {
								fs::rename(oldDictPath, newDictPath);
							}
							catch (...) { }
						}
						gptTabEntryIt->dictPath = newDictPath;
						auto& dictNames = m_globalConfig["commonGptDicts"]["dictNames"];
						if (dictNames.is_array()) {
							const auto dicrNameIt = std::ranges::find_if(dictNames.as_array(), [=](const auto& elem)
								{
									return elem.is_string() && elem.as_string() == oldDictName;
								});
							if (dicrNameIt != dictNames.as_array().end()) {
								*dicrNameIt = newDictName.toStdString();
							}
						}
						else {
							dictNames = toml::array{};
						}
						tabWidget->setTabText(tabWidget->indexOf(pageMainWidget), newDictName);
						Q_EMIT commonDictsChangedSignal();
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("重命名成功"),
							tr("字典 %1 已重命名为 %2").arg(QString::fromStdWString(oldDictPath.stem().wstring()), newDictName), 3000);
					}
					catch (...) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("重命名失败"),
							tr("字典 %1 重命名失败").arg(QString::fromStdWString(oldDictPath.stem().wstring())), 3000);
						return;
					}
				});
			connect(removeTabButton, &ElaPushButton::clicked, this, [=]()
				{
					const auto gptTabEntryIt = std::ranges::find_if(m_gptTabEntries, [=](const GptTabEntry& entry)
						{
							return entry.pageMainWidget == pageMainWidget;
						});
					if (gptTabEntryIt == m_gptTabEntries.end()) {
						return;
					}

					const std::string tmpDictName = wide2Ascii(gptTabEntryIt->dictPath.stem().wstring());

					// 删除提示框
					ElaContentDialog helpDialog(window());

					helpDialog.setRightButtonText(tr("是"));
					helpDialog.setMiddleButtonText(tr("思考人生"));
					helpDialog.setLeftButtonText(tr("否"));

					QWidget* widget = new QWidget(&helpDialog);
					QVBoxLayout* layout = new QVBoxLayout(widget);
					layout->setContentsMargins(15, 25, 15, 10);
					ElaText* confirmText = new ElaText(tr("你确定要删除 %1 吗？").arg(QString::fromStdString(tmpDictName)), widget);
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
							fs::remove(gptTabEntryIt->dictPath);
						}
						catch (...) {}
						m_gptTabEntries.erase(gptTabEntryIt);
						auto& dictNames = m_globalConfig["commonGptDicts"]["dictNames"];
						if (dictNames.is_array()) {
							const auto dictNameIt = std::ranges::find_if(dictNames.as_array(), [=](const auto& elem)
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
						ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("删除成功"),
							tr("字典 %1 已从字典管理和磁盘中移除！").arg(QString::fromStdString(tmpDictName)), 3000);
					}
				});

			gptTabEntry.pageMainWidget = pageMainWidget;
			gptTabEntry.stackedWidget = stackedWidget;
			gptTabEntry.plainTextEdit = plainTextEdit;
			gptTabEntry.tableView = tableView;
			gptTabEntry.dictModel = model;
			gptTabEntry.dictPath = dictPath;
			m_gptTabEntries.push_back(gptTabEntry);

			if (!fs::exists(dictPath)) {
				saveFunc(true);
			}
			return pageMainWidget;
		};

	auto& commonGptDicts = m_globalConfig["commonGptDicts"]["dictNames"];
	if (commonGptDicts.is_array()) {
		auto it = commonGptDicts.as_array().begin();
		while (it != commonGptDicts.as_array().end()) {
			if (!it->is_string()) {
				it = commonGptDicts.as_array().erase(it);
				continue;
			}
			fs::path dictPath = defaultGptDictPath / (ascii2Wide(it->as_string()) + L".toml");
			if (!fs::exists(dictPath)) {
				it = commonGptDicts.as_array().erase(it);
				continue;
			}
			QWidget* pageMainWidget = createGptTab(dictPath);
			tabWidget->addTab(pageMainWidget, QString::fromStdWString(dictPath.stem().wstring()));
			++it;
		}
	}
	else {
		commonGptDicts = toml::array{};
	}

	tabWidget->setCurrentIndex(0);

	connect(importButton, &ElaPushButton::clicked, this, [=]()
		{
			QString importDictPathStr = QFileDialog::getOpenFileName(window(), tr("选择字典文件"),
				QString::fromStdString(toml::find_or(m_globalConfig, "lastCommonGptDictPath", "./")),
				"TOML files (*.toml);;JSON files (*.json);;TSV files (*.tsv *.txt)");
			if (importDictPathStr.isEmpty()) {
				return;
			}
			insertToml(m_globalConfig, "lastCommonGptDictPath", importDictPathStr.toStdString());
			fs::path importDictPath = importDictPathStr.toStdWString();
			fs::path newDictPath = defaultGptDictPath / importDictPath.filename().replace_extension(".toml");
			if (fs::exists(newDictPath) && !fs::equivalent(importDictPath, newDictPath)) {
				try {
					fs::remove(newDictPath);
				}
				catch (...) {
					ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("导入失败"), tr("原文件删除失败"), 3000);
					return;
				}
			}
			const bool hasSameNameTab = std::ranges::any_of(m_gptTabEntries, [=](const GptTabEntry& entry)
				{
					return entry.dictPath.stem().wstring() == importDictPath.stem().wstring();
				});
			if (hasSameNameTab) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("导入失败"),
					tr("字典 %1 已存在").arg(QString::fromStdWString(importDictPath.stem().wstring())), 3000);
				return;
			}
			QWidget* pageMainWidget = createGptTab(importDictPath);
			tabWidget->addTab(pageMainWidget, QString::fromStdWString(importDictPath.stem().wstring()));
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("创建成功"),
				tr("字典页 %1 已创建").arg(QString::fromStdWString(importDictPath.stem().wstring())), 3000);
		});

	connect(addNewTabButton, &ElaPushButton::clicked, this, [=]()
		{
			QString dictName;
			ElaInputDialog inputDialog(tr("请输入字典表名称"), tr("新建字典"), dictName, window());
			if (inputDialog.exec() != QDialog::Accepted) {
				return;
			}

			if (dictName.isEmpty() || dictName.contains('/') || dictName.contains('\\') || dictName.contains('.')) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft,
					tr("新建失败"), tr("字典名称不能为空，且不能包含点号、斜杠或反斜杠！"), 3000);
				return;
			}

			const bool hasSameNameTab = std::ranges::any_of(m_gptTabEntries, [=](const GptTabEntry& entry)
				{
					return entry.dictPath.stem().wstring() == dictName.toStdWString();
				});
			if (hasSameNameTab || dictName == "项目GPT字典") {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"),
					tr("字典 %1 已存在").arg(dictName), 3000);
				return;
			}

			const fs::path newDictPath = defaultGptDictPath / (dictName.toStdWString() + L".toml");
			std::ofstream ofs(newDictPath, std::ios::binary);
			if (!ofs.is_open()) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("新建失败"),
					tr("无法创建 %1 文件").arg(QString::fromStdWString(newDictPath.wstring())), 3000);
				return;
			}
			ofs.close();

			QWidget* pageMainWidget = createGptTab(newDictPath);
			tabWidget->addTab(pageMainWidget, dictName);
			tabWidget->setCurrentIndex(tabWidget->count() - 1);
			ElaMessageBar::success(ElaMessageBarType::TopLeft, tr("创建成功"),
				tr("字典页 %1 已创建").arg(QString::fromStdWString(newDictPath.stem().wstring())), 3000);
		});


	m_applyFunc = [=]()
		{
			toml::array dictNamesArr;
			std::vector<std::pair<std::string, QWidget*>> pageWidgets;
			for (const GptTabEntry& entry : m_gptTabEntries) {
				if (!entry.saveFunc(false)) {
					continue;
				}
				std::string dictName = wide2Ascii(entry.dictPath.stem().wstring());
				pageWidgets.push_back({ std::move(dictName), entry.pageMainWidget });
			}
			std::ranges::sort(pageWidgets, [=](const auto& a, const auto& b)
				{
					return tabWidget->indexOf(a.second) < tabWidget->indexOf(b.second);
				});
			for (const auto& dictName : pageWidgets | std::views::keys) {
				dictNamesArr.push_back(dictName);
			}

			insertToml(m_globalConfig, "commonGptDicts.dictNames", dictNamesArr);

			auto& spec = m_globalConfig["commonGptDicts"]["spec"];
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
			else{
				spec = toml::ordered_table{};
			}
			Q_EMIT commonDictsChangedSignal();
		};


	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
