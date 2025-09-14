#include "DictExSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaMultiSelectComboBox.h"
#include "ElaToggleSwitch.h"

import Tool;

DictExSettingsPage::DictExSettingsPage(toml::table& globalConfig, toml::table& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig)
{
	setWindowTitle("项目字典设置");
	setTitleVisible(false);

	_setupUI();
}

DictExSettingsPage::~DictExSettingsPage()
{

}

void DictExSettingsPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

void DictExSettingsPage::refreshCommonDictsList()
{
	if (_refreshCommonDictsListFunc) {
		_refreshCommonDictsListFunc();
	}
}

void DictExSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	ElaScrollPageArea* preDictNamesArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* preDictNamesLayout = new QHBoxLayout(preDictNamesArea);
	ElaText* preDictNamesText = new ElaText(preDictNamesArea);
	preDictNamesText->setText("选择要启用的译前字典");
	preDictNamesText->setWordWrap(false);
	preDictNamesText->setTextPixelSize(16);
	preDictNamesLayout->addWidget(preDictNamesText);
	preDictNamesLayout->addStretch();
	ElaMultiSelectComboBox* preDictNamesComboBox = new ElaMultiSelectComboBox(preDictNamesArea);
	preDictNamesComboBox->setFixedWidth(500);
	auto preDictNames = _globalConfig["commonPreDicts"]["dictNames"].as_array();
	if (preDictNames) {
		for (const auto& elem : *preDictNames) {
			if (auto dictNameOpt = elem.value<std::string>()) {
				preDictNamesComboBox->addItem(QString::fromStdString(*dictNameOpt));
			}
		}
	}
	preDictNamesComboBox->addItem("项目字典_译前");
	preDictNames = _projectConfig["dictionary"]["preDict"].as_array();
	if (preDictNames) {
		QList<int> indexesToSelect;
		for (const auto& elem : *preDictNames) {
			if (auto dictNameOpt = elem.value<std::string>()) {
				int index = preDictNamesComboBox->findText(QString(fs::path(ascii2Wide(*dictNameOpt)).stem().wstring()));
				if (index < 0) {
					continue;
				}
				indexesToSelect.append(index);
			}
		}
		preDictNamesComboBox->setCurrentSelection(indexesToSelect);
	}
	preDictNamesLayout->addWidget(preDictNamesComboBox);
	mainLayout->addWidget(preDictNamesArea);

	ElaScrollPageArea* gptDictNamesArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* gptDictNamesLayout = new QHBoxLayout(gptDictNamesArea);
	ElaText* gptDictNamesText = new ElaText(gptDictNamesArea);
	gptDictNamesText->setText("选择要启用的GPT字典");
	gptDictNamesText->setWordWrap(false);
	gptDictNamesText->setTextPixelSize(16);
	gptDictNamesLayout->addWidget(gptDictNamesText);
	gptDictNamesLayout->addStretch();
	ElaMultiSelectComboBox* gptDictNamesComboBox = new ElaMultiSelectComboBox(gptDictNamesArea);
	gptDictNamesComboBox->setFixedWidth(500);
	auto gptDictNames = _globalConfig["commonGptDicts"]["dictNames"].as_array();
	if (gptDictNames) {
		for (const auto& elem : *gptDictNames) {
			if (auto dictNameOpt = elem.value<std::string>()) {
				gptDictNamesComboBox->addItem(QString::fromStdString(*dictNameOpt));
			}
		}
	}
	gptDictNamesComboBox->addItem("项目GPT字典");
	gptDictNames = _projectConfig["dictionary"]["gptDict"].as_array();
	if (gptDictNames) {
		QList<int> indexesToSelect;
		for (const auto& elem : *gptDictNames) {
			if (auto dictNameOpt = elem.value<std::string>()) {
				int index = gptDictNamesComboBox->findText(QString(fs::path(ascii2Wide(*dictNameOpt)).stem().wstring()));
				if (index < 0) {
					continue;
				}
				indexesToSelect.append(index);
			}
		}
		gptDictNamesComboBox->setCurrentSelection(indexesToSelect);
	}
	gptDictNamesLayout->addWidget(gptDictNamesComboBox);
	mainLayout->addWidget(gptDictNamesArea);

	ElaScrollPageArea* postDictNamesArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* postDictNamesLayout = new QHBoxLayout(postDictNamesArea);
	ElaText* postDictNamesText = new ElaText(postDictNamesArea);
	postDictNamesText->setText("选择要启用的译后字典");
	postDictNamesText->setWordWrap(false);
	postDictNamesText->setTextPixelSize(16);
	postDictNamesLayout->addWidget(postDictNamesText);
	postDictNamesLayout->addStretch();
	ElaMultiSelectComboBox* postDictNamesComboBox = new ElaMultiSelectComboBox(postDictNamesArea);
	postDictNamesComboBox->setFixedWidth(500);
	auto postDictNames = _globalConfig["commonPostDicts"]["dictNames"].as_array();
	if (postDictNames) {
		for (const auto& elem : *postDictNames) {
			if (auto dictNameOpt = elem.value<std::string>()) {
				postDictNamesComboBox->addItem(QString::fromStdString(*dictNameOpt));
			}
		}
	}
	postDictNamesComboBox->addItem("项目字典_译后");
	postDictNames = _projectConfig["dictionary"]["postDict"].as_array();
	if (postDictNames) {
		QList<int> indexesToSelect;
		for (const auto& elem : *postDictNames) {
			if (auto dictNameOpt = elem.value<std::string>()) {
				int index = postDictNamesComboBox->findText(QString(fs::path(ascii2Wide(*dictNameOpt)).stem().wstring()));
				if (index < 0) {
					continue;
				}
				indexesToSelect.append(index);
			}
		}
		postDictNamesComboBox->setCurrentSelection(indexesToSelect);
	}
	postDictNamesLayout->addWidget(postDictNamesComboBox);
	mainLayout->addWidget(postDictNamesArea);

	bool usePreDictInName = _projectConfig["dictionary"]["usePreDictInName"].value_or(false);
	ElaScrollPageArea* usePreDictInNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePreDictInNameLayout = new QHBoxLayout(usePreDictInNameArea);
	ElaText* usePreDictInNameText = new ElaText(usePreDictInNameArea);
	usePreDictInNameText->setText("将译前字典用在name字段");
	usePreDictInNameText->setWordWrap(false);
	usePreDictInNameText->setTextPixelSize(16);
	usePreDictInNameLayout->addWidget(usePreDictInNameText);
	usePreDictInNameLayout->addStretch();
	ElaToggleSwitch* usePreDictInNameSwitch = new ElaToggleSwitch(usePreDictInNameArea);
	usePreDictInNameSwitch->setIsToggled(usePreDictInName);
	usePreDictInNameLayout->addWidget(usePreDictInNameSwitch);
	mainLayout->addWidget(usePreDictInNameArea);

	bool usePostDictInName = _projectConfig["dictionary"]["usePostDictInName"].value_or(false);
	ElaScrollPageArea* usePostDictInNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePostDictInNameLayout = new QHBoxLayout(usePostDictInNameArea);
	ElaText* usePostDictInNameText = new ElaText(usePostDictInNameArea);
	usePostDictInNameText->setText("将译后字典用在name字段");
	usePostDictInNameText->setWordWrap(false);
	usePostDictInNameText->setTextPixelSize(16);
	usePostDictInNameLayout->addWidget(usePostDictInNameText);
	usePostDictInNameLayout->addStretch();
	ElaToggleSwitch* usePostDictInNameSwitch = new ElaToggleSwitch(usePostDictInNameArea);
	usePostDictInNameSwitch->setIsToggled(usePostDictInName);
	usePostDictInNameLayout->addWidget(usePostDictInNameSwitch);
	mainLayout->addWidget(usePostDictInNameArea);

	bool usePreDictInMsg = _projectConfig["dictionary"]["usePreDictInMsg"].value_or(true);
	ElaScrollPageArea* usePreDictInMsgArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePreDictInMsgLayout = new QHBoxLayout(usePreDictInMsgArea);
	ElaText* usePreDictInMsgText = new ElaText(usePreDictInMsgArea);
	usePreDictInMsgText->setText("将译前字典用在msg字段");
	usePreDictInMsgText->setWordWrap(false);
	usePreDictInMsgText->setTextPixelSize(16);
	usePreDictInMsgLayout->addWidget(usePreDictInMsgText);
	usePreDictInMsgLayout->addStretch();
	ElaToggleSwitch* usePreDictInMsgSwitch = new ElaToggleSwitch(usePreDictInMsgArea);
	usePreDictInMsgSwitch->setIsToggled(usePreDictInMsg);
	usePreDictInMsgLayout->addWidget(usePreDictInMsgSwitch);
	mainLayout->addWidget(usePreDictInMsgArea);

	bool usePostDictInMsg = _projectConfig["dictionary"]["usePostDictInMsg"].value_or(true);
	ElaScrollPageArea* usePostDictInMsgArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* usePostDictInMsgLayout = new QHBoxLayout(usePostDictInMsgArea);
	ElaText* usePostDictInMsgText = new ElaText(usePostDictInMsgArea);
	usePostDictInMsgText->setText("将译后字典用在msg字段");
	usePostDictInMsgText->setWordWrap(false);
	usePostDictInMsgText->setTextPixelSize(16);
	usePostDictInMsgLayout->addWidget(usePostDictInMsgText);
	usePostDictInMsgLayout->addStretch();
	ElaToggleSwitch* usePostDictInMsgSwitch = new ElaToggleSwitch(usePostDictInMsgArea);
	usePostDictInMsgSwitch->setIsToggled(usePostDictInMsg);
	usePostDictInMsgLayout->addWidget(usePostDictInMsgSwitch);
	mainLayout->addWidget(usePostDictInMsgArea);

	bool useGPTDictToReplaceName = _projectConfig["dictionary"]["useGPTDictToReplaceName"].value_or(false);
	ElaScrollPageArea* useGPTDictToReplaceNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* useGPTDictToReplaceNameLayout = new QHBoxLayout(useGPTDictToReplaceNameArea);
	ElaText* useGPTDictToReplaceNameText = new ElaText(useGPTDictToReplaceNameArea);
	useGPTDictToReplaceNameText->setText("启用GPT字典替换name字段");
	useGPTDictToReplaceNameText->setWordWrap(false);
	useGPTDictToReplaceNameText->setTextPixelSize(16);
	useGPTDictToReplaceNameLayout->addWidget(useGPTDictToReplaceNameText);
	useGPTDictToReplaceNameLayout->addStretch();
	ElaToggleSwitch* useGPTDictToReplaceNameSwitch = new ElaToggleSwitch(useGPTDictToReplaceNameArea);
	useGPTDictToReplaceNameSwitch->setIsToggled(useGPTDictToReplaceName);
	useGPTDictToReplaceNameLayout->addWidget(useGPTDictToReplaceNameSwitch);
	mainLayout->addWidget(useGPTDictToReplaceNameArea);
	mainLayout->addStretch();

	_refreshCommonDictsListFunc = [=]()
		{
			toml::array* commonPreDictsArrPtr = _globalConfig["commonPreDicts"]["dictNames"].as_array();
			toml::array* commonGptDictsArrPtr = _globalConfig["commonGptDicts"]["dictNames"].as_array();
			toml::array* commonPostDictsArrPtr = _globalConfig["commonPostDicts"]["dictNames"].as_array();
			toml::array commonPreDictsArr = commonPreDictsArrPtr ? *commonPreDictsArrPtr : toml::array{};
			toml::array commonGptDictsArr = commonGptDictsArrPtr ? *commonGptDictsArrPtr : toml::array{};
			toml::array commonPostDictsArr = commonPostDictsArrPtr ? *commonPostDictsArrPtr : toml::array{};

			QList<int> preDictIndexesToRemove;
			for (int i = 0; i < preDictNamesComboBox->count(); i++) {
				if (
					preDictNamesComboBox->itemText(i) != "项目字典_译前" &&
					std::find_if(commonPreDictsArr.begin(), commonPreDictsArr.end(), [=](const auto& elem)
						{
							return elem.value_or(std::string{}) == preDictNamesComboBox->itemText(i).toStdString();
						}) == commonPreDictsArr.end()
							)
				{
					preDictIndexesToRemove.append(i);
				}
			}
			std::ranges::sort(preDictIndexesToRemove, [](int a, int b) { return a > b; });
			for (int index : preDictIndexesToRemove) {
				preDictNamesComboBox->removeItem(index);
			}
			QStringList commonPreDictsChosen = preDictNamesComboBox->getCurrentSelection();
			for (auto& elem : commonPreDictsArr) {
				if (auto dictNameOpt = elem.value<std::string>()) {
					int index = preDictNamesComboBox->findText(QString::fromStdString(*dictNameOpt));
					if (index >= 0) {
						continue;
					}
					preDictNamesComboBox->insertItem(0, QString::fromStdString(*dictNameOpt));
					if (_globalConfig["commonPreDicts"]["spec"][*dictNameOpt]["defaultOn"].value_or(true)) {
						commonPreDictsChosen.append(QString::fromStdString(*dictNameOpt));
					}
				}
			}
			preDictNamesComboBox->setCurrentSelection(commonPreDictsChosen);

			QList<int> gptDictIndexesToRemove;
			for (int i = 0; i < gptDictNamesComboBox->count(); i++) {
				if (
					gptDictNamesComboBox->itemText(i) != "项目GPT字典" &&
					std::find_if(commonGptDictsArr.begin(), commonGptDictsArr.end(), [=](const auto& elem)
						{
							return elem.value_or(std::string{}) == gptDictNamesComboBox->itemText(i).toStdString();
						}) == commonGptDictsArr.end()
							)
				{
					gptDictIndexesToRemove.append(i);
				}
			}
			std::ranges::sort(gptDictIndexesToRemove, [](int a, int b) { return a > b; });
			for (int index : gptDictIndexesToRemove) {
				gptDictNamesComboBox->removeItem(index);
			}
			QStringList commonGptDictsChosen = gptDictNamesComboBox->getCurrentSelection();
			for (auto& elem : commonGptDictsArr) {
				if (auto dictNameOpt = elem.value<std::string>()) {
					int index = gptDictNamesComboBox->findText(QString::fromStdString(*dictNameOpt));
					if (index >= 0) {
						continue;
					}
					gptDictNamesComboBox->insertItem(0, QString::fromStdString(*dictNameOpt));
					if (_globalConfig["commonGptDicts"]["spec"][*dictNameOpt]["defaultOn"].value_or(true)) {
						commonGptDictsChosen.append(QString::fromStdString(*dictNameOpt));
					}
				}
			}
			gptDictNamesComboBox->setCurrentSelection(commonGptDictsChosen);

			QList<int> postDictIndexesToRemove;
			for (int i = 0; i < postDictNamesComboBox->count(); i++) {
				if (
					postDictNamesComboBox->itemText(i) != "项目字典_译后" &&
					std::find_if(commonPostDictsArr.begin(), commonPostDictsArr.end(), [=](const auto& elem)
						{
							return elem.value_or(std::string{}) == postDictNamesComboBox->itemText(i).toStdString();
						}) == commonPostDictsArr.end()
							)
				{
					postDictIndexesToRemove.append(i);
				}
			}
			std::ranges::sort(postDictIndexesToRemove, [](int a, int b) { return a > b; });
			for (int index : postDictIndexesToRemove) {
				postDictNamesComboBox->removeItem(index);
			}
			QStringList commonPostDictsChosen = postDictNamesComboBox->getCurrentSelection();
			for (auto& elem : commonPostDictsArr) {
				if (auto dictNameOpt = elem.value<std::string>()) {
					int index = postDictNamesComboBox->findText(QString::fromStdString(*dictNameOpt));
					if (index >= 0) {
						continue;
					}
					postDictNamesComboBox->insertItem(0, QString::fromStdString(*dictNameOpt));
					if (_globalConfig["commonPostDicts"]["spec"][*dictNameOpt]["defaultOn"].value_or(true)) {
						commonPostDictsChosen.append(QString::fromStdString(*dictNameOpt));
					}
				}
			}
			postDictNamesComboBox->setCurrentSelection(commonPostDictsChosen);
		};


	_applyFunc = [=]()
		{
			toml::array preDictNamesArr, gptDictNamesArr, postDictNamesArr;
			QStringList preDictNamesStr = preDictNamesComboBox->getCurrentSelection(),
				gptDictNames = gptDictNamesComboBox->getCurrentSelection(),
				postDictNames = postDictNamesComboBox->getCurrentSelection();
			for (const auto& name : preDictNamesStr) {
				preDictNamesArr.push_back(name.toStdString() + ".toml");
			}
			for (const auto& name : gptDictNames) {
				gptDictNamesArr.push_back(name.toStdString() + ".toml");
			}
			for (const auto& name : postDictNames) {
				postDictNamesArr.push_back(name.toStdString() + ".toml");
			}
			insertToml(_projectConfig, "dictionary.preDict", preDictNamesArr);
			insertToml(_projectConfig, "dictionary.gptDict", gptDictNamesArr);
			insertToml(_projectConfig, "dictionary.postDict", postDictNamesArr);

			insertToml(_projectConfig, "dictionary.usePreDictInName", usePreDictInNameSwitch->getIsToggled());
			insertToml(_projectConfig, "dictionary.usePostDictInName", usePostDictInNameSwitch->getIsToggled());
			insertToml(_projectConfig, "dictionary.usePreDictInMsg", usePreDictInMsgSwitch->getIsToggled());
			insertToml(_projectConfig, "dictionary.usePostDictInMsg", usePostDictInMsgSwitch->getIsToggled());
			insertToml(_projectConfig, "dictionary.useGPTDictToReplaceName", useGPTDictToReplaceNameSwitch->getIsToggled());
		};

	addCentralWidget(mainWidget);
}
