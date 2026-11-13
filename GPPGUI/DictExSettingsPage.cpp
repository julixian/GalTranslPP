#include "DictExSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "GppMultiSelectComboBox.h"
#include "ElaToggleSwitch.h"

import Tool;

using namespace gpp;

DictExSettingsPage::DictExSettingsPage(tomlkit::Item& globalConfig, tomlkit::Item& projectConfig, QWidget* parent) :
	BasePage(parent), m_projectConfig(projectConfig), m_globalConfig(globalConfig)
{
	setWindowTitle(tr("项目字典设置"));
	setTitleVisible(false);

	setupUi();
}

void DictExSettingsPage::refreshCommonDictsList()
{
	if (m_refreshCommonDictsListFunc) {
		m_refreshCommonDictsListFunc();
	}
}

void DictExSettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(20, 15, 15, 0);

	auto createDictSelectAreaFunc =
		    [=, this](const QString& tipText, const QString& projectDictShowName, const std::string& projectDictStdFileName,
			    const std::string& globalConfigKey, const std::string& projectConfigKey) -> GppMultiSelectComboBox*
		{
			ElaScrollPageArea* dictSelectArea = new ElaScrollPageArea(mainWidget);
			QHBoxLayout* dictSelectLayout = new QHBoxLayout(dictSelectArea);
			ElaText* dictSelectText = new ElaText(dictSelectArea);
			dictSelectText->setText(tipText);
			dictSelectText->setWordWrap(false);
			dictSelectText->setTextPixelSize(16);
			dictSelectLayout->addWidget(dictSelectText);
			dictSelectLayout->addStretch();
			GppMultiSelectComboBox* dictNamesComboBox = new GppMultiSelectComboBox(dictSelectArea);
			dictNamesComboBox->setFixedWidth(500);
			const auto globalConfigDictNames = tomlkit::findArrayOrEmpty(m_globalConfig, globalConfigKey, "dictNames");
				for (const auto& globalConfigDictName : globalConfigDictNames) {
					if (globalConfigDictName.isString()) {
						dictNamesComboBox->addItem(QString::fromStdString(globalConfigDictName.asString()));
					}
				}
			dictNamesComboBox->addItem(projectDictShowName);
			const auto projectConfigDictFileNames = tomlkit::findArrayOrEmpty(m_projectConfig, "dictionary", projectConfigKey);
			QList<int> indexesToSelect;
			for (const auto& projectConfigDictFileName : projectConfigDictFileNames) {
				if (!projectConfigDictFileName.isString()) {
					continue;
				}
				const QString dictShowName = projectDictStdFileName == projectConfigDictFileName.asString()
					? projectDictShowName
					: QString::fromStdWString(fs::path(ascii2Wide(projectConfigDictFileName.asString())).stem().wstring());
				const int index = dictNamesComboBox->findText(dictShowName);
				if (index < 0) {
					continue;
				}
				indexesToSelect.append(index);
			}
			dictNamesComboBox->setCurrentSelection(indexesToSelect);

			dictSelectLayout->addWidget(dictNamesComboBox);
			mainLayout->addWidget(dictSelectArea);
			return dictNamesComboBox;
		};

	const QString projectPreDictShowName = tr("项目译前字典");
	const QString projectGptDictShowName = tr("项目GPT字典");
	const QString projectPostDictShowName = tr("项目译后字典");
	const std::string projectPreDictStdFileName = "ProjPreDict.toml";
	const std::string projectGptDictStdFileName = "ProjGptDict.toml";
	const std::string projectPostDictStdFileName = "ProjPostDict.toml";
	GppMultiSelectComboBox* preDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的译前字典"), projectPreDictShowName,
		projectPreDictStdFileName, "commonPreDicts", "preDicts");
	GppMultiSelectComboBox* gptDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的GPT字典"), projectGptDictShowName,
		projectGptDictStdFileName, "commonGptDicts", "gptDicts");
	GppMultiSelectComboBox* postDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的译后字典"), projectPostDictShowName,
		projectPostDictStdFileName, "commonPostDicts", "postDicts");


	bool usePreDictInName = tomlkit::findOr(m_projectConfig, "dictionary", "usePreDictInName", false);
	ElaScrollPageArea* usePreDictInNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePreDictInNameLayout = new QHBoxLayout(usePreDictInNameArea);
	ElaText* usePreDictInNameText = new ElaText(usePreDictInNameArea);
	usePreDictInNameText->setText(tr("将译前字典用在 name 字段"));
	usePreDictInNameText->setWordWrap(false);
	usePreDictInNameText->setTextPixelSize(16);
	usePreDictInNameLayout->addWidget(usePreDictInNameText);
	usePreDictInNameLayout->addStretch();
	ElaToggleSwitch* usePreDictInNameSwitch = new ElaToggleSwitch(usePreDictInNameArea);
	usePreDictInNameSwitch->setIsToggled(usePreDictInName);
	usePreDictInNameLayout->addWidget(usePreDictInNameSwitch);
	mainLayout->addWidget(usePreDictInNameArea);

	bool usePostDictInName = tomlkit::findOr(m_projectConfig, "dictionary", "usePostDictInName", false);
	ElaScrollPageArea* usePostDictInNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePostDictInNameLayout = new QHBoxLayout(usePostDictInNameArea);
	ElaText* usePostDictInNameText = new ElaText(usePostDictInNameArea);
	usePostDictInNameText->setText(tr("将译后字典用在 name 字段"));
	usePostDictInNameText->setWordWrap(false);
	usePostDictInNameText->setTextPixelSize(16);
	usePostDictInNameLayout->addWidget(usePostDictInNameText);
	usePostDictInNameLayout->addStretch();
	ElaToggleSwitch* usePostDictInNameSwitch = new ElaToggleSwitch(usePostDictInNameArea);
	usePostDictInNameSwitch->setIsToggled(usePostDictInName);
	usePostDictInNameLayout->addWidget(usePostDictInNameSwitch);
	mainLayout->addWidget(usePostDictInNameArea);

	bool usePreDictInMsg = tomlkit::findOr(m_projectConfig, "dictionary", "usePreDictInMsg", true);
	ElaScrollPageArea* usePreDictInMsgArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePreDictInMsgLayout = new QHBoxLayout(usePreDictInMsgArea);
	ElaText* usePreDictInMsgText = new ElaText(usePreDictInMsgArea);
	usePreDictInMsgText->setText(tr("将译前字典用在 message 字段"));
	usePreDictInMsgText->setWordWrap(false);
	usePreDictInMsgText->setTextPixelSize(16);
	usePreDictInMsgLayout->addWidget(usePreDictInMsgText);
	usePreDictInMsgLayout->addStretch();
	ElaToggleSwitch* usePreDictInMsgSwitch = new ElaToggleSwitch(usePreDictInMsgArea);
	usePreDictInMsgSwitch->setIsToggled(usePreDictInMsg);
	usePreDictInMsgLayout->addWidget(usePreDictInMsgSwitch);
	mainLayout->addWidget(usePreDictInMsgArea);

	bool usePostDictInMsg = tomlkit::findOr(m_projectConfig, "dictionary", "usePostDictInMsg", true);
	ElaScrollPageArea* usePostDictInMsgArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePostDictInMsgLayout = new QHBoxLayout(usePostDictInMsgArea);
	ElaText* usePostDictInMsgText = new ElaText(usePostDictInMsgArea);
	usePostDictInMsgText->setText(tr("将译后字典用在 message 字段"));
	usePostDictInMsgText->setWordWrap(false);
	usePostDictInMsgText->setTextPixelSize(16);
	usePostDictInMsgLayout->addWidget(usePostDictInMsgText);
	usePostDictInMsgLayout->addStretch();
	ElaToggleSwitch* usePostDictInMsgSwitch = new ElaToggleSwitch(usePostDictInMsgArea);
	usePostDictInMsgSwitch->setIsToggled(usePostDictInMsg);
	usePostDictInMsgLayout->addWidget(usePostDictInMsgSwitch);
	mainLayout->addWidget(usePostDictInMsgArea);

	bool useGPTDictToReplaceName = tomlkit::findOr(m_projectConfig, "dictionary", "useGPTDictToReplaceName", false);
	ElaScrollPageArea* useGPTDictToReplaceNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* useGPTDictToReplaceNameLayout = new QHBoxLayout(useGPTDictToReplaceNameArea);
	ElaText* useGPTDictToReplaceNameText = new ElaText(useGPTDictToReplaceNameArea);
	useGPTDictToReplaceNameText->setText(tr("启用GPT字典替换 name 字段"));
	useGPTDictToReplaceNameText->setWordWrap(false);
	useGPTDictToReplaceNameText->setTextPixelSize(16);
	useGPTDictToReplaceNameLayout->addWidget(useGPTDictToReplaceNameText);
	useGPTDictToReplaceNameLayout->addStretch();
	ElaToggleSwitch* useGPTDictToReplaceNameSwitch = new ElaToggleSwitch(useGPTDictToReplaceNameArea);
	useGPTDictToReplaceNameSwitch->setIsToggled(useGPTDictToReplaceName);
	useGPTDictToReplaceNameLayout->addWidget(useGPTDictToReplaceNameSwitch);
	mainLayout->addWidget(useGPTDictToReplaceNameArea);
	mainLayout->addStretch();

	m_refreshCommonDictsListFunc = [=, this]()
		{
			auto refreshCommonDictsListFunc =
				    [=, this](const QString& projectDictShowName, const std::string& globalConfigKey, GppMultiSelectComboBox* dictNamesComboBox)
				{
					const tomlkit::Item commonDictNamesArr = tomlkit::findArrayOrEmpty(m_globalConfig, globalConfigKey, "dictNames");
					QList<int> dictIndexesToRemove;
					for (int i = 0; i < dictNamesComboBox->count(); ++i) {
						if (
							const QString dictNameInComboBox = dictNamesComboBox->itemText(i);
							dictNameInComboBox != projectDictShowName &&
							!std::ranges::any_of(commonDictNamesArr, [&](const tomlkit::Item& commonDictName)
								{
									return commonDictName.isString() && commonDictName.asString() == dictNameInComboBox.toStdString();
								})
							)
						{
							dictIndexesToRemove.append(i);
						}
					}
					std::ranges::sort(dictIndexesToRemove, [](int a, int b) { return a > b; });
					for (const int index : dictIndexesToRemove) {
						dictNamesComboBox->removeItem(index);
					}
					QStringList selectedDictNames = dictNamesComboBox->getCurrentSelection();
					for (const auto& commonDictName : commonDictNamesArr) {
						if (!commonDictName.isString()) {
							continue;
						}
						const int index = dictNamesComboBox->findText(QString::fromStdString(commonDictName.asString()));
						if (index >= 0) {
							continue;
						}
						dictNamesComboBox->insertItem(0, QString::fromStdString(commonDictName.asString()));
						if (tomlkit::findOr(m_globalConfig, globalConfigKey, "spec", commonDictName.asString(), "defaultOn", true)) {
							selectedDictNames.append(QString::fromStdString(commonDictName.asString()));
						}
					}
					dictNamesComboBox->setCurrentSelection(selectedDictNames);
				};

			refreshCommonDictsListFunc(projectPreDictShowName, "commonPreDicts", preDictNamesComboBox);
			refreshCommonDictsListFunc(projectGptDictShowName, "commonGptDicts", gptDictNamesComboBox);
			refreshCommonDictsListFunc(projectPostDictShowName, "commonPostDicts", postDictNamesComboBox);

		};


	m_applyFunc = [=, this]()
		{
			auto appendDictNamesFunc = [](tomlkit::Item& dictNamesArr, const GppMultiSelectComboBox* dictNamesComboBox,
				const QString& projectDictShowName, const std::string& projectDictStdFileName)
				{
					for (const auto& selectedDictName : dictNamesComboBox->getCurrentSelection()) {
						if (selectedDictName == projectDictShowName) {
							dictNamesArr.push_back(projectDictStdFileName);
						}
						else {
							dictNamesArr.push_back(selectedDictName.toStdString() + ".toml");
						}
					}
				};
			tomlkit::Item preDictNamesArr = tomlkit::Item::array();
			tomlkit::Item gptDictNamesArr = tomlkit::Item::array();
			tomlkit::Item postDictNamesArr = tomlkit::Item::array();
			appendDictNamesFunc(preDictNamesArr, preDictNamesComboBox, projectPreDictShowName, projectPreDictStdFileName);
			appendDictNamesFunc(gptDictNamesArr, gptDictNamesComboBox, projectGptDictShowName, projectGptDictStdFileName);
			appendDictNamesFunc(postDictNamesArr, postDictNamesComboBox, projectPostDictShowName, projectPostDictStdFileName);
			insertToml(m_projectConfig, "dictionary.preDicts", preDictNamesArr);
			insertToml(m_projectConfig, "dictionary.gptDicts", gptDictNamesArr);
			insertToml(m_projectConfig, "dictionary.postDicts", postDictNamesArr);

			insertToml(m_projectConfig, "dictionary.usePreDictInName", usePreDictInNameSwitch->getIsToggled());
			insertToml(m_projectConfig, "dictionary.usePostDictInName", usePostDictInNameSwitch->getIsToggled());
			insertToml(m_projectConfig, "dictionary.usePreDictInMsg", usePreDictInMsgSwitch->getIsToggled());
			insertToml(m_projectConfig, "dictionary.usePostDictInMsg", usePostDictInMsgSwitch->getIsToggled());
			insertToml(m_projectConfig, "dictionary.useGPTDictToReplaceName", useGPTDictToReplaceNameSwitch->getIsToggled());
		};

	addCentralWidget(mainWidget, true, false, 0);
}
