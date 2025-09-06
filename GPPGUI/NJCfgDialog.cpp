#include "NJCfgDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>

#include "ElaScrollPageArea.h"
#include "ElaSpinBox.h"
#include "ElaComboBox.h"
#include "ElaToggleSwitch.h"
#include "ElaText.h"

import Tool;

NJCfgDialog::NJCfgDialog(toml::table& projectConfig, QWidget* parent) : ElaContentDialog(parent), _projectConfig(projectConfig)
{
	setWindowTitle("NormalJson Configuration");

	setLeftButtonText("Cancel");
	setMiddleButtonText("Reset");
	setRightButtonText("OK");

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

	connect(this, &NJCfgDialog::rightButtonClicked, this, &NJCfgDialog::onRightButtonClicked);
	connect(this, &NJCfgDialog::middleButtonClicked, this, &NJCfgDialog::onMiddleButtonClicked);
	connect(this, &NJCfgDialog::leftButtonClicked, this, &NJCfgDialog::onLeftButtonClicked);

	_resetFunc = [=]()
		{
			outputSwitch->setIsToggled(true);
		};

	_cancelFunc = [=]()
		{
			outputSwitch->setIsToggled(outputWithSrc);
		};

	setCentralWidget(centerWidget);
}

NJCfgDialog::~NJCfgDialog()
{

}

void NJCfgDialog::onRightButtonClicked()
{

}

void NJCfgDialog::onMiddleButtonClicked()
{
	if (_resetFunc) {
		_resetFunc();
	}
}

void NJCfgDialog::onLeftButtonClicked()
{
	if (_cancelFunc) {
		_cancelFunc();
	}
}
