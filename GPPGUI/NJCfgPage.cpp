#include "NJCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaScrollPageArea.h"
#include "ElaToggleSwitch.h"
#include "ElaText.h"

import Tool;

NJCfgPage::NJCfgPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("NormalJson 输出配置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 输出带原文
	bool outputWithSrc = toml::find_or(m_projectConfig, "plugins", "NormalJson", "outputWithSrc", true);
	ElaScrollPageArea* outputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* outputLayout = new QHBoxLayout(outputArea);
	ElaText* outputText = new ElaText(tr("输出带原文"), outputArea);
	outputText->setWordWrap(false);
	outputText->setTextPixelSize(16);
	outputLayout->addWidget(outputText);
	outputLayout->addStretch();
	ElaToggleSwitch* outputSwitch = new ElaToggleSwitch(outputArea);
	outputSwitch->setIsToggled(outputWithSrc);
	outputLayout->addWidget(outputSwitch);
	mainLayout->addWidget(outputArea);

	// 输出带引用信息
	const bool outputWithRefInfo = toml::find_or(m_projectConfig, "plugins", "NormalJson", "outputWithRefInfo", false);
	ElaScrollPageArea* refInfoArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* refInfoLayout = new QHBoxLayout(refInfoArea);
	ElaText* refInfoText = new ElaText(tr("输出带引用信息"), refInfoArea);
	refInfoText->setWordWrap(false);
	refInfoText->setTextPixelSize(16);
	refInfoLayout->addWidget(refInfoText);
	refInfoLayout->addStretch();
	ElaToggleSwitch* refInfoSwitch = new ElaToggleSwitch(refInfoArea);
	refInfoSwitch->setIsToggled(outputWithRefInfo);
	refInfoLayout->addWidget(refInfoSwitch);
	mainLayout->addWidget(refInfoArea);

	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "plugins.NormalJson.outputWithSrc", outputSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.NormalJson.outputWithRefInfo", refInfoSwitch->getIsToggled());
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("NormalJson 输出配置"));
	addCentralWidget(centerWidget, true, false, 0);
}

NJCfgPage::~NJCfgPage()
{

}
