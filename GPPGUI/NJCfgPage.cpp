#include "NJCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaScrollPageArea.h"
#include "ElaToggleSwitch.h"
#include "ElaText.h"

import Tool;

void NJCfgPage::apply2Config()
{

}

NJCfgPage::NJCfgPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("NormalJson 输出配置");

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 输出带原文
	bool outputWithSrc = _projectConfig["plugins"]["NormalJson"]["output_with_src"].value_or(true);
	ElaScrollPageArea* outputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* outputLayout = new QHBoxLayout(outputArea);
	ElaText* outputText = new ElaText("输出带原文", outputArea);
	outputText->setTextPixelSize(16);
	outputLayout->addWidget(outputText);
	outputLayout->addStretch();
	ElaToggleSwitch* outputSwitch = new ElaToggleSwitch(outputArea);
	outputSwitch->setIsToggled(outputWithSrc);
	outputLayout->addWidget(outputSwitch);
	connect(outputSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "plugins.NormalJson.output_with_src", checked);
		});
	mainLayout->addWidget(outputArea);

	mainLayout->addStretch();
	centerWidget->setWindowTitle("NormalJson 输出配置");
	addCentralWidget(centerWidget);
}

NJCfgPage::~NJCfgPage()
{

}
