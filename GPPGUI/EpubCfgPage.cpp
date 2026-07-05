#include "EpubCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>

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

EpubCfgPage::EpubCfgPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("Epub 输出配置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 双语显示
	bool bilingual = toml::find_or(m_projectConfig, "plugins", "Epub", "双语显示", true);
	ElaScrollPageArea* outputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* outputLayout = new QHBoxLayout(outputArea);
	ElaText* outputText = new ElaText(tr("双语显示"), outputArea);
	outputText->setTextPixelSize(16);
	outputLayout->addWidget(outputText);
	outputLayout->addStretch();
	ElaToggleSwitch* outputSwitch = new ElaToggleSwitch(outputArea);
	outputSwitch->setIsToggled(bilingual);
	outputLayout->addWidget(outputSwitch);
	mainLayout->addWidget(outputArea);

	// 原文颜色
	const std::string colorStr = toml::find_or(m_projectConfig, "plugins", "Epub", "原文颜色", "#808080");
	QColor color = QColor(colorStr.c_str());
	ElaScrollPageArea* colorArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* colorLayout = new QHBoxLayout(colorArea);
	ElaText* colorDialogText = new ElaText(tr("原文颜色"), colorArea);
	colorDialogText->setWordWrap(false);
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
	double scale = toml::find_or(m_projectConfig, "plugins", "Epub", "缩小比例", 0.8);
	ElaScrollPageArea* scaleArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* scaleLayout = new QHBoxLayout(scaleArea);
	ElaText* scaleText = new ElaText(tr("缩小比例"), scaleArea);
	scaleText->setTextPixelSize(16);
	scaleLayout->addWidget(scaleText);
	scaleLayout->addStretch();
	ValueSliderWidget* scaleSlider = new ValueSliderWidget(0.0, 1.0, scaleArea);
	scaleSlider->setDecimals(2);
	scaleSlider->setValue(scale);
	scaleLayout->addWidget(scaleSlider);
	mainLayout->addWidget(scaleArea);

	// 预处理正则
	toml::ordered_value preRegexArr = toml::find_or_default<toml::ordered_value>(m_projectConfig, "plugins", "Epub", "preprocRegex");
	if (!preRegexArr.is_array()) {
		preRegexArr = toml::array{};
	}
	ElaText* preRegexText = new ElaText(tr("预处理正则"), centerWidget);
	preRegexText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(preRegexText);
	ElaPlainTextEdit* preRegexEdit = new ElaPlainTextEdit(centerWidget);
	preRegexEdit->setMinimumHeight(300);
	preRegexEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "preprocRegex", preRegexArr }} })));
	mainLayout->addWidget(preRegexEdit);

	// 后处理正则
	toml::ordered_value postRegexArr = toml::find_or_default<toml::ordered_value>(m_projectConfig, "plugins", "Epub", "postprocRegex");
	if (!postRegexArr.is_array()) {
		postRegexArr = toml::array{};
	}
	ElaText* postRegexText = new ElaText(tr("后处理正则"), centerWidget);
	postRegexText->setTextPixelSize(18);
	mainLayout->addSpacing(10);
	mainLayout->addWidget(postRegexText);
	ElaPlainTextEdit* postRegexEdit = new ElaPlainTextEdit(centerWidget);
	postRegexEdit->setMinimumHeight(300);
	postRegexEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "postprocRegex", postRegexArr }} })));
	mainLayout->addWidget(postRegexEdit);

	QWidget* tipButtonWidget = new QWidget(centerWidget);
	QHBoxLayout* tipLayout = new QHBoxLayout(tipButtonWidget);
	tipLayout->addStretch();
	ElaPushButton* tipButton = new ElaPushButton(tr("说明"), centerWidget);
	tipLayout->addWidget(tipButton);
	connect(tipButton, &ElaPushButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile("BaseConfig/illustration/epub.html"));
		});
	mainLayout->addWidget(tipButtonWidget);

	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "plugins.Epub.双语显示", outputSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.Epub.原文颜色", colorDialog->getCurrentColorRGB().toStdString());
			insertToml(m_projectConfig, "plugins.Epub.缩小比例", scaleSlider->value());

			try {
				toml::ordered_value preTbl = toml::parse_str<toml::ordered_type_config>(preRegexEdit->toPlainText().toStdString());
				auto& preArr = preTbl["preprocRegex"];
				if (preArr.is_array()) {
					insertToml(m_projectConfig, "plugins.Epub.preprocRegex", preArr);
				}
				else {
					insertToml(m_projectConfig, "plugins.Epub.preprocRegex", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("Epub预处理正则格式错误"), 3000);
			}
			try {
				toml::ordered_value postTbl = toml::parse_str<toml::ordered_type_config>(postRegexEdit->toPlainText().toStdString());
				auto& postArr = postTbl["postprocRegex"];
				if (postArr.is_array()) {
					insertToml(m_projectConfig, "plugins.Epub.postprocRegex", postArr);
				}
				else {
					insertToml(m_projectConfig, "plugins.Epub.postprocRegex", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("Epub后处理正则格式错误"), 3000);
			}
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("Epub 输出配置"));
	addCentralWidget(centerWidget, true, false, 0);
}

EpubCfgPage::~EpubCfgPage()
{

}
