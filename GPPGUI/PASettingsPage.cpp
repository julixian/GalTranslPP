#include "PASettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QButtonGroup>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaSlider.h"
#include "ElaDoubleSpinBox.h"
#include "ElaToggleButton.h"
#include "ValueSliderWidget.h"
#include "ElaFlowLayout.h"

import Tool;

PASettingsPage::PASettingsPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("问题分析");
	setTitleVisible(false);

	_setupUI();
}

PASettingsPage::~PASettingsPage()
{

}

void PASettingsPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

void PASettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(5);

	// 要发现的问题清单
	std::set<std::string> problemListSet;
	auto problemListOrgArray = _projectConfig["problemAnalyze"]["problemList"].as_array();
	if (problemListOrgArray) {
		for (const auto& problem : *problemListOrgArray) {
			if (auto optProblem = problem.value<std::string>()) {
				problemListSet.insert(*optProblem);
			}
		}
	}
	ElaText* problemListTitle = new ElaText("要发现的问题清单", mainWidget);
	problemListTitle->setTextPixelSize(18);
	mainLayout->addWidget(problemListTitle);
	ElaFlowLayout* problemListLayout = new ElaFlowLayout();
	QStringList problemList = { "词频过高","标点错漏","丢失换行","多加换行","比原文长","比原文长严格","字典未使用","残留日文","引入拉丁字母","引入韩文","语言不通", };
	QList<ElaToggleButton*> problemListButtons;
	for (const QString& problem : problemList) {
		ElaToggleButton* button = new ElaToggleButton(problem, this);
		button->setFixedWidth(120);
		if (problemListSet.count(problem.toStdString())) {
			button->setIsToggled(true);
		}
		problemListLayout->addWidget(button);
		problemListButtons.append(button);
	}
	mainLayout->addLayout(problemListLayout);
	mainLayout->addSpacing(20);

	// 规定标点错漏要查哪些标点
	std::string punctuationSet = _projectConfig["problemAnalyze"]["punctSet"].value_or("");
	QString punctuationSetStr = QString::fromStdString(punctuationSet);
	ElaScrollPageArea* punctuationListArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* punctuationListLayout = new QHBoxLayout(punctuationListArea);
	ElaText* punctuationListTitle = new ElaText("标点查错", punctuationListArea);
	punctuationListTitle->setTextPixelSize(16);
	ElaToolTip* punctuationListTip = new ElaToolTip(punctuationListTitle);
	punctuationListTip->setToolTip("规定标点错漏要查哪些标点");
	punctuationListLayout->addWidget(punctuationListTitle);
	punctuationListLayout->addStretch();
	ElaLineEdit* punctuationList = new ElaLineEdit(punctuationListArea);
	punctuationList->setText(punctuationSetStr);
	punctuationListLayout->addWidget(punctuationList);
	mainLayout->addWidget(punctuationListArea);

	// 语言不通检测的语言置信度，设置越高则检测越精准，但可能遗漏，反之亦然
	double languageProbability = _projectConfig["problemAnalyze"]["langProbability"].value_or(0.85);
	ElaScrollPageArea* languageProbabilityArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* languageProbabilityLayout = new QHBoxLayout(languageProbabilityArea);
	ElaText* languageProbabilityTitle = new ElaText("语言置信度", languageProbabilityArea);
	languageProbabilityTitle->setTextPixelSize(16);
	ElaToolTip* languageProbabilityTip = new ElaToolTip(languageProbabilityTitle);
	languageProbabilityTip->setToolTip("语言不通检测的语言置信度(0-1)，设置越高则检测越精准，但可能遗漏，反之亦然");
	languageProbabilityLayout->addWidget(languageProbabilityTitle);
	languageProbabilityLayout->addStretch();
	ValueSliderWidget* languageProbabilitySlider = new ValueSliderWidget(languageProbabilityArea);
	languageProbabilitySlider->setFixedWidth(500);
	languageProbabilitySlider->setValue(languageProbability);
	languageProbabilityLayout->addWidget(languageProbabilitySlider);
	connect(languageProbabilitySlider, &ValueSliderWidget::valueChanged, this, [=](double value)
		{
			insertToml(_projectConfig, "problemAnalyze.langProbability", value);
		});
	mainLayout->addWidget(languageProbabilityArea);
	
	mainLayout->addStretch();

	_applyFunc = [=]()
		{
			toml::array problemListArray;
			for (qsizetype i = 0; i < problemListButtons.size(); i++) {
				if (!problemListButtons[i]->getIsToggled()) {
					continue;
				}
				problemListArray.push_back(problemList[i].toStdString());
			}
			insertToml(_projectConfig, "problemAnalyze.problemList", problemListArray);
			insertToml(_projectConfig, "problemAnalyze.punctSet", punctuationList->text().toStdString());
		};

	addCentralWidget(mainWidget, true, true, 0);
}