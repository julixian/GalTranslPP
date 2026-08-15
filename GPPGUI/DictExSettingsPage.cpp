#include "DictExSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaMultiSelectComboBox.h"
#include "ElaToggleSwitch.h"

import Tool;

DictExSettingsPage::DictExSettingsPage(toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) :
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
		    [=](const QString& tipText, const QString& projectDictShowName, const std::string& projectDictStdFileName,
			    const std::string& globalConfigKey, const std::string& projectConfigKey) -> ElaMultiSelectComboBox*
		{
			ElaScrollPageArea* dictSelectArea = new ElaScrollPageArea(mainWidget);
			QHBoxLayout* dictSelectLayout = new QHBoxLayout(dictSelectArea);
			ElaText* dictSelectText = new ElaText(dictSelectArea);
			dictSelectText->setText(tipText);
			dictSelectText->setWordWrap(false);
			dictSelectText->setTextPixelSize(16);
			dictSelectLayout->addWidget(dictSelectText);
			dictSelectLayout->addStretch();
			ElaMultiSelectComboBox* dictNamesComboBox = new ElaMultiSelectComboBox(dictSelectArea);
			dictNamesComboBox->setFixedWidth(500);
			const auto globalConfigDictNames = toml::find_or_default<toml::array>(m_globalConfig, globalConfigKey, "dictNames");
				for (const auto& globalConfigDictName : globalConfigDictNames) {
					if (globalConfigDictName.is_string()) {
						dictNamesComboBox->addItem(QString::fromStdString(globalConfigDictName.as_string()));
					}
				}
			dictNamesComboBox->addItem(projectDictShowName);
			const auto projectConfigDictFileNames = toml::find_or_default<toml::array>(m_projectConfig, "dictionary", projectConfigKey);
			QList<int> indexesToSelect;
			for (const auto& projectConfigDictFileName : projectConfigDictFileNames) {
				if (!projectConfigDictFileName.is_string()) {
					continue;
				}
				const QString dictShowName = projectDictStdFileName == projectConfigDictFileName.as_string()
					? projectDictShowName
					: QString::fromStdWString(fs::path(ascii2Wide(projectConfigDictFileName.as_string())).stem().wstring());
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
	ElaMultiSelectComboBox* preDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的译前字典"), projectPreDictShowName,
		projectPreDictStdFileName, "commonPreDicts", "preDicts");
	ElaMultiSelectComboBox* gptDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的GPT字典"), projectGptDictShowName,
		projectGptDictStdFileName, "commonGptDicts", "gptDicts");
	ElaMultiSelectComboBox* postDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的译后字典"), projectPostDictShowName,
		projectPostDictStdFileName, "commonPostDicts", "postDicts");


	bool usePreDictInName = toml::find_or(m_projectConfig, "dictionary", "usePreDictInName", false);
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

	bool usePostDictInName = toml::find_or(m_projectConfig, "dictionary", "usePostDictInName", false);
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

	bool usePreDictInMsg = toml::find_or(m_projectConfig, "dictionary", "usePreDictInMsg", true);
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

	bool usePostDictInMsg = toml::find_or(m_projectConfig, "dictionary", "usePostDictInMsg", true);
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

	bool useGPTDictToReplaceName = toml::find_or(m_projectConfig, "dictionary", "useGPTDictToReplaceName", false);
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

	m_refreshCommonDictsListFunc = [=]()
		{
			auto refreshCommonDictsListFunc =
				    [=](const QString& projectDictShowName, const std::string& globalConfigKey, ElaMultiSelectComboBox* dictNamesComboBox)
				{
					const toml::array commonDictNamesArr = toml::find_or_default<toml::array>(m_globalConfig, globalConfigKey, "dictNames");
					QList<int> dictIndexesToRemove;
					for (int i = 0; i < dictNamesComboBox->count(); ++i) {
						if (
							const QString dictNameInComboBox = dictNamesComboBox->itemText(i);
							dictNameInComboBox != projectDictShowName &&
							!std::ranges::any_of(commonDictNamesArr, [&](const toml::value& commonDictName)
								{
									return commonDictName.is_string() && commonDictName.as_string() == dictNameInComboBox.toStdString();
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
						if (!commonDictName.is_string()) {
							continue;
						}
						const int index = dictNamesComboBox->findText(QString::fromStdString(commonDictName.as_string()));
						if (index >= 0) {
							continue;
						}
						dictNamesComboBox->insertItem(0, QString::fromStdString(commonDictName.as_string()));
						if (toml::find_or(m_globalConfig, globalConfigKey, "spec", commonDictName.as_string(), "defaultOn", true)) {
							selectedDictNames.append(QString::fromStdString(commonDictName.as_string()));
						}
					}
					dictNamesComboBox->setCurrentSelection(selectedDictNames);
				};

			refreshCommonDictsListFunc(projectPreDictShowName, "commonPreDicts", preDictNamesComboBox);
			refreshCommonDictsListFunc(projectGptDictShowName, "commonGptDicts", gptDictNamesComboBox);
			refreshCommonDictsListFunc(projectPostDictShowName, "commonPostDicts", postDictNamesComboBox);

		};


	m_applyFunc = [=]()
		{
			auto appendDictNamesFunc = [](toml::array& dictNamesArr, const ElaMultiSelectComboBox* dictNamesComboBox,
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
			toml::array preDictNamesArr, gptDictNamesArr, postDictNamesArr;
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
