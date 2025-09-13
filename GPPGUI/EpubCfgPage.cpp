#include "EpubCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaScrollPageArea.h"
#include "ElaToggleSwitch.h"
#include "ElaColorDialog.h"
#include "ElaPushButton.h"
#include "ValueSliderWidget.h"
#include "ElaText.h"

import Tool;

void EpubCfgPage::apply2Config()
{

}

EpubCfgPage::EpubCfgPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("Epub 输出配置");

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 双语显示
	bool bilingual = _projectConfig["plugins"]["Epub"]["双语显示"].value_or(true);
	ElaScrollPageArea* outputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* outputLayout = new QHBoxLayout(outputArea);
	ElaText* outputText = new ElaText("双语显示", outputArea);
	outputText->setTextPixelSize(16);
	outputLayout->addWidget(outputText);
	outputLayout->addStretch();
	ElaToggleSwitch* outputSwitch = new ElaToggleSwitch(outputArea);
	outputSwitch->setIsToggled(bilingual);
	outputLayout->addWidget(outputSwitch);
	connect(outputSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "plugins.Epub.双语显示", checked);
		});
	mainLayout->addWidget(outputArea);

	// 原文颜色
	std::string colorStr = _projectConfig["plugins"]["Epub"]["原文颜色"].value_or("#808080");
	QColor color = QColor(colorStr.c_str());
	ElaScrollPageArea* colorArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* colorLayout = new QHBoxLayout(colorArea);
	ElaText* colorDialogText = new ElaText("原文颜色", colorArea);
	colorDialogText->setTextPixelSize(16);
	colorLayout->addWidget(colorDialogText);
	colorLayout->addStretch();
	ElaColorDialog* colorDialog = new ElaColorDialog(colorArea);
	colorDialog->setCurrentColor(color);
	ElaText* colorText = new ElaText(colorDialog->getCurrentColorRGB(), colorArea);
	colorText->setTextPixelSize(15);
	ElaPushButton* colorButton = new ElaPushButton(colorArea);
	colorButton->setFixedSize(35, 35);
	colorButton->setLightDefaultColor(colorDialog->getCurrentColor());
	colorButton->setLightHoverColor(colorDialog->getCurrentColor());
	colorButton->setLightPressColor(colorDialog->getCurrentColor());
	colorButton->setDarkDefaultColor(colorDialog->getCurrentColor());
	colorButton->setDarkHoverColor(colorDialog->getCurrentColor());
	colorButton->setDarkPressColor(colorDialog->getCurrentColor());
	connect(colorButton, &ElaPushButton::clicked, this, [=]() {
		colorDialog->exec();
		});
	connect(colorDialog, &ElaColorDialog::colorSelected, this, [=](const QColor& color) {
		colorButton->setLightDefaultColor(color);
		colorButton->setLightHoverColor(color);
		colorButton->setLightPressColor(color);
		colorButton->setDarkDefaultColor(color);
		colorButton->setDarkHoverColor(color);
		colorButton->setDarkPressColor(color);
		colorText->setText(colorDialog->getCurrentColorRGB());
		insertToml(_projectConfig, "plugins.Epub.原文颜色", colorDialog->getCurrentColorRGB().toStdString());
		});
	colorLayout->addWidget(colorButton);
	colorLayout->addWidget(colorText);
	mainLayout->addWidget(colorArea);


	// 缩小比例
	double scale = _projectConfig["plugins"]["Epub"]["缩小比例"].value_or(0.8);
	ElaScrollPageArea* scaleArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* scaleLayout = new QHBoxLayout(scaleArea);
	ElaText* scaleText = new ElaText("缩小比例", scaleArea);
	scaleText->setTextPixelSize(16);
	scaleLayout->addWidget(scaleText);
	scaleLayout->addStretch();
	ValueSliderWidget* scaleSlider = new ValueSliderWidget(scaleArea);
	scaleSlider->setDecimals(2);
	scaleSlider->setValue(scale);
	scaleLayout->addWidget(scaleSlider);
	connect(scaleSlider, &ValueSliderWidget::valueChanged, this, [=](double value)
		{
			insertToml(_projectConfig, "plugins.Epub.缩小比例", value);
		});
	mainLayout->addWidget(scaleArea);
	
	mainLayout->addStretch();
	centerWidget->setWindowTitle("Epub 输出配置");
	addCentralWidget(centerWidget);
}

EpubCfgPage::~EpubCfgPage()
{

}
