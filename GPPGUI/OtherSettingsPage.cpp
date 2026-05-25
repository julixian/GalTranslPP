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
#include "ElaToolTip.h"
#include "ElaContentDialog.h"
#include "ElaInputDialog.h"
#include "ElaDoubleText.h"

import Tool;
import NormalJsonTranslatorHelperTool;

OtherSettingsPage::OtherSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir), _mainWindow(mainWindow)
{
	setWindowTitle(tr("其它设置"));
	setTitleVisible(false);

	_setupUI();
}

OtherSettingsPage::~OtherSettingsPage()
{

}

void OtherSettingsPage::_setupUI()
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
	pathEdit->setText(QString::fromStdWString(_projectDir.wstring()));
	pathEdit->setFixedWidth(650);
	pathLayout->addWidget(pathEdit);
	ElaPushButton* openButton = new ElaPushButton(pathArea);
	openButton->setText(tr("打开文件夹"));
	connect(openButton, &ElaPushButton::clicked, this, [=]()
		{
			QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(_projectDir.wstring()));
			QDesktopServices::openUrl(dirUrl);
		});
	pathLayout->addWidget(openButton);
	mainLayout->addWidget(pathArea);


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
			if (toml::find_or(_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移动失败"), tr("项目仍在运行中，无法移动"), 3000);
				return;
			}

			QString newProjectParentPath = QFileDialog::getExistingDirectory(this, tr("请选择要移动到的文件夹"), QString::fromStdString(toml::find_or(_globalConfig, "lastProjectPath", "./Projects")));
			if (newProjectParentPath.isEmpty()) {
				return;
			}
			insertToml(_globalConfig, "lastProjectPath", newProjectParentPath.toStdString());
			fs::path newProjectPath = fs::path(newProjectParentPath.toStdWString()) / _projectDir.filename();
			if (fs::exists(newProjectPath)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移动失败"), tr("目录下已有同名文件或文件夹"), 3000);
				return;
			}

			try {
				fs::rename(_projectDir, newProjectPath);
			}
			catch (const fs::filesystem_error& e) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("移动失败"), QString(e.what()), 3000);
				return;
			}
			_projectDir = newProjectPath;
			pathEdit->setText(QString::fromStdWString(_projectDir.wstring()));
			ElaMessageBar::success(ElaMessageBarType::TopRight, tr("移动成功"), QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 项目已移动到新文件夹"), 3000);
			Q_EMIT refreshProjectConfigSignal();
		});
	moveRenameLayout->addWidget(moveButton);
	ElaPushButton* renameButton = new ElaPushButton(moveRenameArea);
	renameButton->setText(tr("项目更名"));
	connect(renameButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), tr("项目仍在运行中，无法更名"), 3000);
				return;
			}

			bool ok;
			QString newProjectName;
			ElaInputDialog inputDialog(_mainWindow, tr("请输入新的项目名称"), tr("新的项目名"), newProjectName, &ok);
			inputDialog.exec();
			if (!ok) {
				return;
			}
			if(newProjectName.isEmpty() || newProjectName.contains("\\") || newProjectName.contains("/")){
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), tr("项目名不能为空且不能包含斜杠"), 3000);
				return;
			}

			fs::path newProjectPath = _projectDir.parent_path() / newProjectName.toStdWString();
			if (fs::exists(newProjectPath)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), tr("目录下已有同名文件或文件夹"), 3000);
				return;
			}

			try {
				fs::rename(_projectDir, newProjectPath);
			}
			catch (const fs::filesystem_error& e) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("更名失败"), QString(e.what()), 3000);
				return;
			}
			_projectDir = newProjectPath;
			pathEdit->setText(QString::fromStdWString(_projectDir.wstring()));
			Q_EMIT changeProjectNameSignal(newProjectName);
			ElaMessageBar::success(ElaMessageBarType::TopRight, tr("更名成功"), tr("项目已更名为 ") + newProjectName, 3000);
			Q_EMIT refreshProjectConfigSignal();
		});
	moveRenameLayout->addWidget(renameButton);
	mainLayout->addWidget(moveRenameArea);


	// 导入翻译问题概览至翻译缓存
	ElaScrollPageArea* importArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* importLayout = new QHBoxLayout(importArea);
	ElaDoubleText* importLabel = new ElaDoubleText(importArea,
		tr("导入翻译问题概览至翻译缓存"), 16, tr("使用 翻译问题概览.json/.toml 中的 Sentence 替换 trans_cache 中的 Sentence"), 10, "");
	importLayout->addWidget(importLabel);
	importLayout->addStretch();
	ElaPushButton* importButton = new ElaPushButton(importArea);
	importButton->setText(tr("导入"));
	connect(importButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("导入失败"), tr("项目仍在运行中，无法导入"), 3000);
				return;
			}
			QString importOverviewPathStr = QFileDialog::getOpenFileName(this, tr("选择翻译问题概览文件"), QString::fromStdWString(_projectDir.wstring()),
				"JSON files (*.json);;TOML files (*.toml)");
			if (importOverviewPathStr.isEmpty()) {
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
					if (importOverviewPathStr.endsWith(".json", Qt::CaseInsensitive)) {
						ifs.open(importOverviewPathStr.toStdWString(), std::ios::binary);
						overviewData = json::parse(ifs);
						ifs.close();
					}
					else if (importOverviewPathStr.endsWith(".toml", Qt::CaseInsensitive)) {
						const auto tomlData = toml::uparse(fs::path(importOverviewPathStr.toStdWString()));
						overviewData = toml2Json(tomlData.at("problemOverview"));
					}
					else {
						throw std::runtime_error("未知的文件类型");
					}
					for (const auto& overviewItem : overviewData) {
						overviewFileMap[overviewItem["filename"].get<std::string>()].push_back(overviewItem);
					}
				}

				for (const auto& [cacheFileName, overviewItems] : overviewFileMap) {
					const fs::path cachePath = _projectDir / transCacheDirName / ascii2Wide(cacheFileName);
					if (!fs::exists(cachePath)) {
						problems.push_back(std::format("[文件 {}] 未在 cache 中找到，跳过导入", cacheFileName));
						continue;
					}

					json cacheData;
					std::unordered_map<int, std::reference_wrapper<json>> cacheIndexMap;

					try {
						ifs.open(cachePath, std::ios::binary);
						cacheData = json::parse(ifs);
						for (auto& cacheItem : cacheData) {
							cacheIndexMap.insert({ cacheItem["index"].get<int>(), cacheItem });
						}
					}
					catch (...) {
						ifs.close();
						problems.push_back(std::format("[文件 {}] 无法解析，跳过导入", cacheFileName));
						continue;
					}
					ifs.close();
					size_t fileImportCount = 0;
					for (const auto& overviewItem : overviewItems) {
						int overviewItemIndex = overviewItem["index"].get<int>();
						auto it = cacheIndexMap.find(overviewItemIndex);
						if (it == cacheIndexMap.end()) {
							problems.push_back(std::format("[文件 {}] 句子(index {}) 未在 cache 中找到，跳过导入", cacheFileName, overviewItemIndex));
							continue;
						}
						auto& cacheItem = it->second.get();
						std::string overviewItemOrigText = overviewItem["original_text"].get<std::string>();
						std::string cacheItemOrigText = cacheItem["original_text"].get<std::string>();
						if (overviewItemOrigText != cacheItemOrigText) {
							problems.push_back(std::format("[文件 {}] 句子(index {}) 与 cache 中原文不匹配，可能产生意外结果，\n概览原文: {}\n缓存原文: {}",
								cacheFileName, overviewItemIndex, overviewItemOrigText, cacheItemOrigText));
						}
						cacheItem = overviewItem;
						cacheItem.erase("filename");
						++fileImportCount;
					}
					if (fileImportCount != 0) {
						ofs.open(cachePath, std::ios::binary);
						if (!ofs.is_open()) {
							problems.push_back(std::format("[文件 {}] 无法写入，跳过导入", cacheFileName));
							continue;
						}
						ofs << cacheData.dump(2);
						ofs.close();
						importCount += fileImportCount;
					}
				}

				QString completeStr = tr("成功导入 ") + QString::number(importCount) + tr(" 个句子至 trans_cache");
				if (!problems.empty()) {
					const fs::path problemPath = _projectDir / L"import_problems.log";
					std::ofstream ofs(problemPath);
					for (const auto& problem : problems) {
						ofs << problem << "\n";
					}
					ofs << completeStr.toStdString() << "\n";
					ofs.close();
					ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("导入完毕"), tr("导入中出现的问题记录在 import_problems.log 中"), 3000);
				}
				else {
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("导入完毕"), completeStr, 3000);
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
	ElaDoubleText* saveLabel = new ElaDoubleText(saveArea,
		tr("保存项目配置"), 16, tr("开始翻译或关闭程序时会自动保存所有项目的配置，一般无需手动保存"), 10, "");
	saveLayout->addWidget(saveLabel);
	saveLayout->addStretch();
	ElaPushButton* saveButton = new ElaPushButton(saveArea);
	saveButton->setText(tr("保存"));
	connect(saveButton, &ElaPushButton::clicked, this, [=]()
		{
			Q_EMIT saveConfigSignal();
			ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"), tr("项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的配置信息已保存"), 3000);
		});
	saveLayout->addWidget(saveButton);
	mainLayout->addWidget(saveArea);


	// 刷新项目配置
	ElaScrollPageArea* refreshArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* refreshLayout = new QHBoxLayout(refreshArea);
	ElaDoubleText* refreshLabel = new ElaDoubleText(refreshArea,
		tr("刷新项目配置"), 16, tr("刷新现有配置和字典，谨慎使用"), 10, "");
	refreshLayout->addWidget(refreshLabel);
	refreshLayout->addStretch();
	ElaPushButton* refreshButton = new ElaPushButton(refreshArea);
	refreshButton->setText(tr("刷新"));
	connect(refreshButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("刷新失败"), tr("项目仍在运行中，无法刷新"), 3000);
				return;
			}

			ElaContentDialog helpDialog(_mainWindow);
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

			connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
				{
					Q_EMIT refreshProjectConfigSignal();
				});
			helpDialog.exec();
		});
	refreshLayout->addWidget(refreshButton);
	mainLayout->addWidget(refreshArea);


	// 删除翻译缓存
	ElaScrollPageArea* cacheArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* cacheLayout = new QHBoxLayout(cacheArea);
	ElaDoubleText* cacheLabel = new ElaDoubleText(cacheArea,
		tr("删除翻译缓存"), 16, tr("删除项目的翻译缓存，下次翻译将会重新从头开始"), 10, "");
	cacheLayout->addWidget(cacheLabel);
	cacheLayout->addStretch();
	ElaPushButton* cacheButton = new ElaPushButton(cacheArea);
	cacheButton->setText(tr("删除"));
	connect(cacheButton, &ElaPushButton::clicked, this, [=]()
		{
			if (toml::find_or(_projectConfig, "GUIConfig", "isRunning", true)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), tr("项目仍在运行中，无法删除缓存"), 3000);
				return;
			}

			ElaContentDialog helpDialog(_mainWindow);

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

			connect(&helpDialog, &ElaContentDialog::rightButtonClicked, this, [=]()
				{
					try {
						fs::remove_all(_projectDir / transCacheDirName);
					}
					catch (const fs::filesystem_error& e) {
						ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("删除失败"), QString(e.what()), 3000);
						return;
					}
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("删除成功"), tr("项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的翻译缓存已删除"), 3000);

				});
			helpDialog.exec();
		});
	cacheLayout->addWidget(cacheButton);
	mainLayout->addWidget(cacheArea);
	
	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, false, 0);
}