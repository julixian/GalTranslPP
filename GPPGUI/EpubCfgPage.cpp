#include "EpubCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaColorDialog.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ValueSliderWidget.h"
#include "ElaText.h"

import Tool;

void EpubCfgPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

EpubCfgPage::EpubCfgPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("Epub 输出配置");
	setContentsMargins(10, 0, 10, 0);

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
	mainLayout->addWidget(scaleArea);

	// 预处理正则
	toml::table preRegexTable = toml::table{};
	auto preRegexArr = _projectConfig["plugins"]["Epub"]["预处理正则"].as_array();
	if (preRegexArr) {
		for (auto& elem : *preRegexArr) {
			auto tbl = elem.as_table();
			if (!tbl) {
				continue;
			}
			tbl->is_inline(false);
		}
		preRegexTable.insert("预处理正则", *preRegexArr);
	}
	else {
		preRegexTable.insert("预处理正则", toml::array{});
	}
	ElaText* preRegexText = new ElaText("预处理正则", centerWidget);
	preRegexText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(preRegexText);
	ElaPlainTextEdit* preRegexEdit = new ElaPlainTextEdit(centerWidget);
	preRegexEdit->setPlainText(QString::fromStdString(stream2String(preRegexTable)));
	mainLayout->addWidget(preRegexEdit);

	// 后处理正则
	toml::table postRegexTable = toml::table{};
	auto postRegexArr = _projectConfig["plugins"]["Epub"]["后处理正则"].as_array();
	if (postRegexArr) {
		for (auto& elem : *postRegexArr) {
			auto tbl = elem.as_table();
			if (!tbl) {
				continue;
			}
			tbl->is_inline(false);
		}
		postRegexTable.insert("后处理正则", *postRegexArr);
	}
	else {
		postRegexTable.insert("后处理正则", toml::array{});
	}
	ElaText* postRegexText = new ElaText("后处理正则", centerWidget);
	postRegexText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(postRegexText);
	ElaPlainTextEdit* postRegexEdit = new ElaPlainTextEdit(centerWidget);
	postRegexEdit->setPlainText(QString::fromStdString(stream2String(postRegexTable)));
	mainLayout->addWidget(postRegexEdit);

	ElaText* tipText = new ElaText("说明", centerWidget);
	tipText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(tipText);
	ElaPlainTextEdit* tipEdit = new ElaPlainTextEdit(centerWidget);
	tipEdit->setReadOnly(true);
	tipEdit->setPlainText(R"()");
	mainLayout->addWidget(tipEdit);

	_applyFunc = [=]()
		{
			insertToml(_projectConfig, "plugins.Epub.双语显示", outputSwitch->getIsToggled());
			insertToml(_projectConfig, "plugins.Epub.原文颜色", colorDialog->getCurrentColorRGB().toStdString());
			insertToml(_projectConfig, "plugins.Epub.缩小比例", scaleSlider->value());

			try {
				toml::table preTbl = toml::parse(preRegexEdit->toPlainText().toStdString());
				auto preArr = preTbl["预处理正则"].as_array();
				if (preArr) {
					insertToml(_projectConfig, "plugins.Epub.预处理正则", *preArr);
				}
				else {
					insertToml(_projectConfig, "plugins.Epub.预处理正则", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "Epub预处理正则格式错误", 3000);
			}
			try {
				toml::table postTbl = toml::parse(postRegexEdit->toPlainText().toStdString());
				auto postArr = postTbl["后处理正则"].as_array();
				if (postArr) {
					insertToml(_projectConfig, "plugins.Epub.后处理正则", *postArr);
				}
				else {
					insertToml(_projectConfig, "plugins.Epub.后处理正则", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, "解析失败", "Epub后处理正则格式错误", 3000);
			}
		};
	
	mainLayout->addStretch();
	centerWidget->setWindowTitle("Epub 输出配置");
	addCentralWidget(centerWidget, true, true, 0);
}

EpubCfgPage::~EpubCfgPage()
{

}
