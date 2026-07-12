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
		[=](const QString& text, const QString& defaultItem, const std::vector<std::string>& projectDictFileNames,
			const std::string& globalConfigKey, const std::string& projectConfigKey) -> ElaMultiSelectComboBox*
		{
			ElaScrollPageArea* dictNamesArea = new ElaScrollPageArea(mainWidget);
			QHBoxLayout* dictNamesLayout = new QHBoxLayout(dictNamesArea);
			ElaText* dictNamesText = new ElaText(dictNamesArea);
			dictNamesText->setText(text);
			dictNamesText->setWordWrap(false);
			dictNamesText->setTextPixelSize(16);
			dictNamesLayout->addWidget(dictNamesText);
			dictNamesLayout->addStretch();
			ElaMultiSelectComboBox* dictNamesComboBox = new ElaMultiSelectComboBox(dictNamesArea);
			dictNamesComboBox->setFixedWidth(500);
			const auto& globalConfigDictNames = toml::find_or_default<toml::array>(m_globalConfig, globalConfigKey, "dictNames");
				for (const auto& dictName : globalConfigDictNames) {
					if (dictName.is_string()) {
						dictNamesComboBox->addItem(QString::fromStdString(dictName.as_string()));
					}
				}
			dictNamesComboBox->addItem(defaultItem);
			const auto& projectConfigDictNames = toml::find_or_default<toml::array>(m_projectConfig, "dictionary", projectConfigKey);
			QList<int> indexesToSelect;
			for (const auto& dictName : projectConfigDictNames) {
				if (dictName.is_string()) {
					QString dictNameStr;
					if (std::ranges::contains(projectDictFileNames, dictName.as_string())) {
						dictNameStr = defaultItem;
					}
					else {
						dictNameStr = QString::fromStdWString(fs::path(ascii2Wide(dictName.as_string())).stem().wstring());
					}
					int index = dictNamesComboBox->findText(dictNameStr);
					if (index < 0 || indexesToSelect.contains(index)) {
						continue;
					}
					indexesToSelect.append(index);
				}
			}
			dictNamesComboBox->setCurrentSelection(indexesToSelect);

			dictNamesLayout->addWidget(dictNamesComboBox);
			mainLayout->addWidget(dictNamesArea);
			return dictNamesComboBox;
		};

	const QString projectPreDictItem = tr("项目译前字典");
	const QString projectGptDictItem = tr("项目GPT字典");
	const QString projectPostDictItem = tr("项目译后字典");
	const std::vector<std::string> projectPreDictFiles = { "ProjPreDict.toml" };
	const std::vector<std::string> projectGptDictFiles = { "ProjGptDict.toml" };
	const std::vector<std::string> projectPostDictFiles = { "ProjPostDict.toml" };
	ElaMultiSelectComboBox* comboBox = createDictSelectAreaFunc(tr("选择要启用的译前字典"), projectPreDictItem, projectPreDictFiles, "commonPreDicts", "preDicts");
	ElaMultiSelectComboBox* gptDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的GPT字典"), projectGptDictItem, projectGptDictFiles, "commonGptDicts", "gptDicts");
	ElaMultiSelectComboBox* postDictNamesComboBox = createDictSelectAreaFunc(tr("选择要启用的译后字典"), projectPostDictItem, projectPostDictFiles, "commonPostDicts", "postDicts");


	bool usePreDictInName = toml::find_or(m_projectConfig, "dictionary", "usePreDictInName", false);
	ElaScrollPageArea* usePreDictInNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePreDictInNameLayout = new QHBoxLayout(usePreDictInNameArea);
	ElaText* usePreDictInNameText = new ElaText(usePreDictInNameArea);
	usePreDictInNameText->setText(tr("将译前字典用在name字段"));
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
	usePostDictInNameText->setText(tr("将译后字典用在name字段"));
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
	usePreDictInMsgText->setText(tr("将译前字典用在msg字段"));
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
	usePostDictInMsgText->setText(tr("将译后字典用在msg字段"));
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
	useGPTDictToReplaceNameText->setText(tr("启用GPT字典替换name字段"));
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
				[=](const QString& excludeName, const std::string& globalConfigKey, ElaMultiSelectComboBox* comboBox_)
				{
					const auto& commonDictsArr = toml::find_or_default<toml::array>(m_globalConfig, globalConfigKey, "dictNames");
					QList<int> dictIndexesToRemove;
					for (int i = 0; i < comboBox_->count(); i++) {
						if (
							comboBox_->itemText(i) != excludeName &&
							!std::ranges::any_of(commonDictsArr, [=](const auto& elem)
								{
									return elem.is_string() && elem.as_string() == comboBox_->itemText(i).toStdString();
								})
							)
						{
							dictIndexesToRemove.append(i);
						}
					}
					std::ranges::sort(dictIndexesToRemove, [](int a, int b) { return a > b; });
					for (int index : dictIndexesToRemove) {
						comboBox_->removeItem(index);
					}
					QStringList commonDictsChosen = comboBox_->getCurrentSelection();
					for (const auto& dictName : commonDictsArr) {
						if (dictName.is_string()) {
							int index = comboBox_->findText(QString::fromStdString(dictName.as_string()));
							if (index >= 0) {
								continue;
							}
							comboBox_->insertItem(0, QString::fromStdString(dictName.as_string()));
							if (toml::find_or(m_globalConfig, globalConfigKey, "spec", dictName.as_string(), "defaultOn", true)) {
								commonDictsChosen.append(QString::fromStdString(dictName.as_string()));
							}
						}
					}
					comboBox_->setCurrentSelection(commonDictsChosen);
				};

			refreshCommonDictsListFunc(projectPreDictItem, "commonPreDicts", comboBox);
			refreshCommonDictsListFunc(projectGptDictItem, "commonGptDicts", gptDictNamesComboBox);
			refreshCommonDictsListFunc(projectPostDictItem, "commonPostDicts", postDictNamesComboBox);

		};


	m_applyFunc = [=]()
		{
			auto appendDictNames = [](toml::array& dictNamesArr, const QStringList& selectedNames,
				const QString& projectDictItem, const std::vector<std::string>& projectDictFileNames)
				{
					for (const auto& name : selectedNames) {
						if (name == projectDictItem) {
							for (const std::string& fileName : projectDictFileNames) {
								dictNamesArr.push_back(fileName);
							}
						}
						else {
							dictNamesArr.push_back(name.toStdString() + ".toml");
						}
					}
				};
			toml::array preDictNamesArr, gptDictNamesArr, postDictNamesArr;
			QStringList preDictNamesStr = comboBox->getCurrentSelection(),
				gptDictNames = gptDictNamesComboBox->getCurrentSelection(),
				postDictNames = postDictNamesComboBox->getCurrentSelection();
			appendDictNames(preDictNamesArr, preDictNamesStr, projectPreDictItem, projectPreDictFiles);
			appendDictNames(gptDictNamesArr, gptDictNames, projectGptDictItem, projectGptDictFiles);
			appendDictNames(postDictNamesArr, postDictNames, projectPostDictItem, projectPostDictFiles);
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
