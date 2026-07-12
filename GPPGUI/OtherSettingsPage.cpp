#include "OtherSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QButtonGroup>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaContentDialog.h"
#include "ElaInputDialog.h"
#include "ElaDoubleText.h"
#include "ElaRadioButton.h"

import Tool;
import NormalJsonTranslatorHelperTool;

OtherSettingsPage::OtherSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), m_projectDir(projectDir), m_globalConfig(globalConfig), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("其它设置"));
	setTitleVisible(false);

	setupUi();
}

void OtherSettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(20, 15, 15, 0);

	// 项目路径
	ElaScrollPageArea* pathArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* pathLayout = new QHBoxLayout(pathArea);
	ElaText* pathLabel = new ElaText(pathArea);
	pathLabel->setText(tr("项目路径"));
	pathLabel->setTextPixelSize(16);
	pathLayout->addWidget(pathLabel);
	pathLayout->addStretch();
	ElaLineEdit* pathEdit = new ElaLineEdit(pathArea);
	pathEdit->setReadOnly(true);
	pathEdit->setText(QString::fromStdWString(m_projectDir.wstring()));
	pathEdit->setFixedWidth(650);
	pathLayout->addWidget(pathEdit);
	ElaPushButton* openButton = new ElaPushButton(pathArea);
	openButton->setText(tr("打开文件夹"));
	connect(openButton, &ElaPushButton::clicked, this, [=]()
		{
			QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(m_projectDir.wstring()));
			QDesktopServices::openUrl(dirUrl);
		});
	pathLayout->addWidget(openButton);
	mainLayout->addWidget(pathArea);

	// 问题概览输出格式
	const std::string problemOverviewFormat = toml::find_or(m_projectConfig, "common", "problemOverviewFormat", "json");
	ElaScrollPageArea* overviewFormatArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* overviewFormatLayout = new QHBoxLayout(overviewFormatArea);
	ElaDoubleText* overviewFormatLabel = new ElaDoubleText(tr("问题概览输出格式"), 16,
		tr("翻译完成后输出 ProblemOverview 的文件格式"), 10, "", overviewFormatArea);
	overviewFormatLayout->addWidget(overviewFormatLabel);
	overviewFormatLayout->addStretch();
	ElaRadioButton* overviewTomlRadio = new ElaRadioButton("toml", overviewFormatArea);
	ElaRadioButton* overviewJsonRadio = new ElaRadioButton("json", overviewFormatArea);
	QButtonGroup* overviewFormatGroup = new QButtonGroup(overviewFormatArea);
	overviewFormatGroup->addButton(overviewTomlRadio);
	overviewFormatGroup->addButton(overviewJsonRadio);
	overviewTomlRadio->setChecked(problemOverviewFormat != "json");
	overviewJsonRadio->setChecked(problemOverviewFormat == "json");
	overviewFormatLayout->addWidget(overviewTomlRadio);
	overviewFormatLayout->addWidget(overviewJsonRadio);
	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "common.problemOverviewFormat", overviewFormatGroup->checkedButton()->text().toStdString());
		};
	mainLayout->addWidget(overviewFormatArea);


	// 项目移动/更名
	ElaScrollPageArea* moveRenameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* moveRenameLayout = new QHBoxLayout(moveRenameArea);
	ElaText* moveRenameLabel = new ElaText(moveRenameArea);
	moveRenameLabel->setWordWrap(false);
	moveRenameLabel->setText(tr("项目移动/更名"));
	moveRenameLabel->setTextPixelSize(16);
	moveRenameLayout->addWidget(moveRenameLabel);
	moveRenameLayout->addStretch();
	ElaPushButton* moveButton = new ElaPushButton(pathArea);
	moveButton->setText(tr("移动项目"));
	connect(moveButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(m_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移动失败"), tr("项目仍在运行中，无法移动"), 3000);
				return;
			}

			const QString newProjectParentPath = QFileDialog::getExistingDirectory(window(), tr("请选择要移动到的文件夹"),
				QString::fromStdString(toml::find_or(m_globalConfig, "lastProjectPath", "./Projects")));
			if (newProjectParentPath.isEmpty()) {
				return;
			}
			insertToml(m_globalConfig, "lastProjectPath", newProjectParentPath.toStdString());
			const fs::path newProjectPath = fs::path(newProjectParentPath.toStdWString()) / m_projectDir.filename();
			if (fs::exists(newProjectPath)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移动失败"), tr("目录下已有同名文件或文件夹"), 3000);
				return;
			}

			try {
				fs::rename(m_projectDir, newProjectPath);
			}
			catch (const fs::filesystem_error& e) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移动失败"), QString(e.what()), 3000);
				return;
			}
			m_projectDir = newProjectPath;
			pathEdit->setText(QString::fromStdWString(m_projectDir.wstring()));
			ElaMessageBar::success(ElaMessageBarType::TopRight, tr("移动成功"),
				tr("%1 项目已移动到新文件夹").arg(QString::fromStdWString(m_projectDir.filename().wstring())), 3000);
			Q_EMIT refreshProjectConfigSignal();
		});
	moveRenameLayout->addWidget(moveButton);
	ElaPushButton* renameButton = new ElaPushButton(moveRenameArea);
	renameButton->setText(tr("项目更名"));
	connect(renameButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(m_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), tr("项目仍在运行中，无法更名"), 3000);
				return;
			}

			QString newProjectName;
			ElaInputDialog inputDialog(tr("请输入新的项目名称"), tr("新的项目名"), newProjectName, window());
			if (inputDialog.exec() != QDialog::Accepted) {
				return;
			}

			if(newProjectName.isEmpty() || newProjectName.contains("\\") || newProjectName.contains("/")){
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), tr("项目名不能为空且不能包含斜杠"), 3000);
				return;
			}

			const fs::path newProjectPath = m_projectDir.parent_path() / newProjectName.toStdWString();
			if (fs::exists(newProjectPath)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), tr("目录下已有同名文件或文件夹"), 3000);
				return;
			}

			try {
				fs::rename(m_projectDir, newProjectPath);
			}
			catch (const fs::filesystem_error& e) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), QString(e.what()), 3000);
				return;
			}
			m_projectDir = newProjectPath;
			pathEdit->setText(QString::fromStdWString(m_projectDir.wstring()));
			Q_EMIT changeProjectNameSignal(newProjectName);
			ElaMessageBar::success(ElaMessageBarType::TopRight, tr("更名成功"),
				tr("项目已更名为 %1").arg(newProjectName), 3000);
			Q_EMIT refreshProjectConfigSignal();
		});
	moveRenameLayout->addWidget(renameButton);
	mainLayout->addWidget(moveRenameArea);


	// 导入翻译问题概览至翻译缓存
	ElaScrollPageArea* importArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* importLayout = new QHBoxLayout(importArea);
	ElaDoubleText* importLabel = new ElaDoubleText(tr("导入翻译问题概览至翻译缓存"), 16,
		tr("使用 ProblemOverview.json/.toml 中的 Sentence 替换 trans_cache 中的 Sentence"), 10, "", importArea);
	importLayout->addWidget(importLabel);
	importLayout->addStretch();
	ElaPushButton* importButton = new ElaPushButton(importArea);
	importButton->setText(tr("导入"));
	connect(importButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(m_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("导入失败"), tr("项目仍在运行中，无法导入"), 3000);
				return;
			}
			const QString importOverviewPathQStr = QFileDialog::getOpenFileName(window(), tr("选择翻译问题概览文件"), QString::fromStdWString(m_projectDir.wstring()),
				"JSON files (*.json);;TOML files (*.toml)");
			if (importOverviewPathQStr.isEmpty()) {
				return;
			}
			try {
				std::unordered_map<std::string, std::vector<json>> overviewFileMap;
				std::vector<std::string> problems;
				std::ifstream ifs;
				std::ofstream ofs;
				size_t importCount = 0;

				{
					json overviewData;
					if (importOverviewPathQStr.endsWith(".json", Qt::CaseInsensitive)) {
						overviewData = parseJson(fs::path(importOverviewPathQStr.toStdWString()));
					}
					else if (importOverviewPathQStr.endsWith(".toml", Qt::CaseInsensitive)) {
						const auto tomlData = toml::uparse(fs::path(importOverviewPathQStr.toStdWString()));
						overviewData = toml2Json(tomlData.at("problemOverview"));
					}
					else {
						throw std::runtime_error("未知的文件类型");
					}
					for (const auto& overviewItem : overviewData) {
						overviewFileMap[overviewItem["file_name"].get<std::string>()].push_back(overviewItem);
					}
				}

				for (const auto& [cacheFileName, overviewItems] : overviewFileMap) {
					const fs::path cachePath = m_projectDir / transCacheDirName / ascii2Wide(cacheFileName);
					if (!fs::exists(cachePath)) {
						problems.push_back(tr("[文件 %1] 未在 cache 中找到，跳过导入")
							.arg(QString::fromStdString(cacheFileName)).toStdString());
						continue;
					}

					json cacheData;
					std::unordered_map<int, std::reference_wrapper<json>> cacheIndexMap;

					try {
						cacheData = parseJson(cachePath, ifs);
						for (auto& cacheItem : cacheData) {
							cacheIndexMap.insert({ cacheItem["index"].get<int>(), cacheItem });
						}
					}
					catch (...) {
						problems.push_back(tr("[文件 %1] 无法解析，跳过导入")
							.arg(QString::fromStdString(cacheFileName)).toStdString());
						continue;
					}
					size_t fileImportCount = 0;
					for (const auto& overviewItem : overviewItems) {
						int overviewItemIndex = overviewItem["index"].get<int>();
						auto it = cacheIndexMap.find(overviewItemIndex);
						if (it == cacheIndexMap.end()) {
							problems.push_back(tr("[文件 %1] 句子(index %2) 未在 cache 中找到，跳过导入")
								.arg(QString::fromStdString(cacheFileName))
								.arg(overviewItemIndex).toStdString());
							continue;
						}
						auto& cacheItem = it->second.get();
						const std::string overviewItemOrigText = overviewItem["original_text"].get<std::string>();
						const std::string cacheItemOrigText = cacheItem["original_text"].get<std::string>();
						if (overviewItemOrigText != cacheItemOrigText) {
							problems.push_back(tr("[文件 %1] 句子(index %2) 与 cache 中原文不匹配，可能产生意外结果，\n概览原文: %3\n缓存原文: %4")
								.arg(QString::fromStdString(cacheFileName))
								.arg(overviewItemIndex)
								.arg(QString::fromStdString(overviewItemOrigText))
								.arg(QString::fromStdString(cacheItemOrigText)).toStdString());
						}
						cacheItem = overviewItem;
						cacheItem.erase("file_name");
						++fileImportCount;
					}
					if (fileImportCount != 0) {
						try {
							atomicOutputFile(ofs, cachePath, cacheData.dump(2));
						}
						catch (...) {
							problems.push_back(tr("[文件 %1] 无法写入，跳过导入")
								.arg(QString::fromStdString(cacheFileName)).toStdString());
							continue;
						}
						importCount += fileImportCount;
					}
				}

				const QString completeQStr = tr("成功导入 %1 个句子至 trans_cache").arg(QString::number(importCount));
				if (!problems.empty()) {
					const fs::path problemPath = m_projectDir / L"import_problems.log";
					std::string problemLog;
					for (const auto& problem : problems) {
						problemLog += problem + "\n";
					}
					problemLog += completeQStr.toStdString() + "\n";
					atomicOutputFile(ofs, problemPath, problemLog);
					ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("导入完毕"), tr("导入中出现的问题记录在 import_problems.log 中"), 3000);
				}
				else {
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("导入完毕"), completeQStr, 3000);
				}
			}
			catch (const std::exception& e) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("导入失败"), QString(e.what()), 3000);
				return;
			}
		});
	importLayout->addWidget(importButton);
	mainLayout->addWidget(importArea);


	// 保存配置
	ElaScrollPageArea* saveArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* saveLayout = new QHBoxLayout(saveArea);
	ElaDoubleText* saveLabel = new ElaDoubleText(tr("保存项目配置"), 16,
		tr("开始翻译或关闭程序时会自动保存所有项目的配置，一般无需手动保存"), 10, "", saveArea);
	saveLayout->addWidget(saveLabel);
	saveLayout->addStretch();
	ElaPushButton* saveButton = new ElaPushButton(saveArea);
	saveButton->setText(tr("保存"));
	connect(saveButton, &ElaPushButton::clicked, this, [=]()
		{
			Q_EMIT saveConfigSignal();
			ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"),
				tr("项目 %1 的配置信息已保存").arg(QString::fromStdWString(m_projectDir.filename().wstring())), 3000);
		});
	saveLayout->addWidget(saveButton);
	mainLayout->addWidget(saveArea);


	// 刷新项目配置
	ElaScrollPageArea* refreshArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* refreshLayout = new QHBoxLayout(refreshArea);
	ElaDoubleText* refreshLabel = new ElaDoubleText(tr("刷新项目配置"), 16,
		tr("刷新现有配置和字典，谨慎使用"), 10, "", refreshArea);
	refreshLayout->addWidget(refreshLabel);
	refreshLayout->addStretch();
	ElaPushButton* refreshButton = new ElaPushButton(refreshArea);
	refreshButton->setText(tr("刷新"));
	connect(refreshButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(m_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("刷新失败"), tr("项目仍在运行中，无法刷新"), 3000);
				return;
			}

			ElaContentDialog helpDialog(window());
			helpDialog.setLeftButtonText(tr("否"));
			helpDialog.setMiddleButtonText(tr("思考人生"));
			helpDialog.setRightButtonText(tr("是"));

			QWidget* widget = new QWidget(&helpDialog);
			QVBoxLayout* layout = new QVBoxLayout(widget);
			layout->setContentsMargins(15, 25, 15, 10);
			ElaText* confirmText = new ElaText(tr("你确定要刷新项目配置吗？"), widget);
			confirmText->setTextStyle(ElaTextType::Title);
			confirmText->setWordWrap(false);
			layout->addWidget(confirmText);
			layout->addSpacing(2);
			ElaText* subTitle = new ElaText(tr("GUI中未保存的数据将会被覆盖！"), widget);
			subTitle->setTextStyle(ElaTextType::Body);
			layout->addWidget(subTitle);
			layout->addStretch();
			helpDialog.setCentralWidget(widget);

			if (helpDialog.exec() == QDialog::Accepted) {
				Q_EMIT refreshProjectConfigSignal();
			}
		});
	refreshLayout->addWidget(refreshButton);
	mainLayout->addWidget(refreshArea);


	// 删除翻译缓存
	ElaScrollPageArea* cacheArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* cacheLayout = new QHBoxLayout(cacheArea);
	ElaDoubleText* cacheLabel = new ElaDoubleText(tr("删除翻译缓存"), 16,
		tr("删除项目的翻译缓存，下次翻译将会重新从头开始"), 10, "", cacheArea);
	cacheLayout->addWidget(cacheLabel);
	cacheLayout->addStretch();
	ElaPushButton* cacheButton = new ElaPushButton(cacheArea);
	cacheButton->setText(tr("删除"));
	connect(cacheButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(m_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), tr("项目仍在运行中，无法删除缓存"), 3000);
				return;
			}

			ElaContentDialog helpDialog(window());

			helpDialog.setLeftButtonText(tr("否"));
			helpDialog.setMiddleButtonText(tr("思考人生"));
			helpDialog.setRightButtonText(tr("是"));

			QWidget* widget = new QWidget(&helpDialog);
			QVBoxLayout* layout = new QVBoxLayout(widget);
			layout->setContentsMargins(15, 25, 15, 10);
			ElaText* confirmText = new ElaText(tr("你确定要删除项目翻译缓存吗？"), widget);
			confirmText->setTextStyle(ElaTextType::Title);
			confirmText->setWordWrap(false);
			layout->addWidget(confirmText);
			layout->addSpacing(2);
			ElaText* subTitle = new ElaText(tr("再次翻译将会重新从头开始！"), widget);
			subTitle->setTextStyle(ElaTextType::Body);
			layout->addWidget(subTitle);
			layout->addStretch();
			helpDialog.setCentralWidget(widget);

			if (helpDialog.exec() == QDialog::Accepted) {
				try {
					fs::remove_all(m_projectDir / transCacheDirName);
				}
				catch (const fs::filesystem_error& e) {
					ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), QString(e.what()), 3000);
					return;
				}
				ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"),
					tr("项目 %1 的翻译缓存已删除").arg(QString::fromStdWString(m_projectDir.filename().wstring())), 3000);
			}
		});
	cacheLayout->addWidget(cacheButton);
	mainLayout->addWidget(cacheArea);

	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, false, 0);
}
