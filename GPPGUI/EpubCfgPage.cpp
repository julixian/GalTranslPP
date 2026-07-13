#include "EpubCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>

#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaToggleSwitch.h"
#include "ElaColorDialog.h"
#include "ElaDoubleText.h"
#include "ElaPushButton.h"
#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ValueSliderWidget.h"
#include "ElaText.h"
#include "TreeSitterHighlighter.h"

import Tool;

EpubCfgPage::EpubCfgPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("Epub 输出配置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 双语显示
	bool bilingual = toml::find_or(m_projectConfig, "plugins", "Epub", "bilingualOutput", true);
	ElaScrollPageArea* bilingualOutputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* bilingualOutputLayout = new QHBoxLayout(bilingualOutputArea);
	ElaDoubleText* bilingualOutputText = new ElaDoubleText(tr("双语显示"), 16,
		tr("在每句译文下以设置的颜色和比例显示原文"), 10, "");
	bilingualOutputLayout->addWidget(bilingualOutputText);
	bilingualOutputLayout->addStretch();
	ElaToggleSwitch* bilingualOutputSwitch = new ElaToggleSwitch(bilingualOutputArea);
	bilingualOutputSwitch->setIsToggled(bilingual);
	bilingualOutputLayout->addWidget(bilingualOutputSwitch);
	mainLayout->addWidget(bilingualOutputArea);

	// 原文颜色
	const std::string origTextColorStr = toml::find_or(m_projectConfig, "plugins", "Epub", "originalTextColor", "#808080");
	QColor origTextColor = QColor(origTextColorStr.c_str());
	ElaScrollPageArea* origTextColorArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* origTextColorLayout = new QHBoxLayout(origTextColorArea);
	ElaText* origTextColorText = new ElaText(tr("原文颜色"), origTextColorArea);
	origTextColorText->setWordWrap(false);
	origTextColorText->setTextPixelSize(16);
	origTextColorLayout->addWidget(origTextColorText);
	origTextColorLayout->addStretch();
	ElaColorDialog* origTextColorDialog = new ElaColorDialog(origTextColorArea);
	origTextColorDialog->setCurrentColor(origTextColor);
	ElaText* origTextColorHexNumText = new ElaText(origTextColorDialog->getCurrentColorRGB(), origTextColorArea);
	origTextColorHexNumText->setTextPixelSize(15);
	ElaPushButton* origTextColorButton = new ElaPushButton(origTextColorArea);
	origTextColorButton->setFixedSize(35, 35);
	origTextColorButton->setLightDefaultColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setLightHoverColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setLightPressColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setDarkDefaultColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setDarkHoverColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setDarkPressColor(origTextColorDialog->getCurrentColor());
	connect(origTextColorButton, &ElaPushButton::clicked, this, [=]()
		{
			origTextColorDialog->exec();
		});
	connect(origTextColorDialog, &ElaColorDialog::colorSelected, this, [=](const QColor& color)
		{
			origTextColorButton->setLightDefaultColor(color);
			origTextColorButton->setLightHoverColor(color);
			origTextColorButton->setLightPressColor(color);
			origTextColorButton->setDarkDefaultColor(color);
			origTextColorButton->setDarkHoverColor(color);
			origTextColorButton->setDarkPressColor(color);
			origTextColorHexNumText->setText(origTextColorDialog->getCurrentColorRGB());
		});
	origTextColorLayout->addWidget(origTextColorButton);
	origTextColorLayout->addWidget(origTextColorHexNumText);
	mainLayout->addWidget(origTextColorArea);


	// 缩小比例
	double origTextScale = toml::find_or(m_projectConfig, "plugins", "Epub", "originalTextScale", 0.8);
	ElaScrollPageArea* origTextScaleArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* origTextScaleLayout = new QHBoxLayout(origTextScaleArea);
	ElaText* origTextScaleText = new ElaText(tr("缩小比例"), origTextScaleArea);
	origTextScaleText->setTextPixelSize(16);
	origTextScaleLayout->addWidget(origTextScaleText);
	origTextScaleLayout->addStretch();
	ValueSliderWidget* origTextScaleSlider = new ValueSliderWidget(0.0, 1.0, origTextScaleArea);
	origTextScaleSlider->setDecimals(2);
	origTextScaleSlider->setValue(origTextScale);
	origTextScaleLayout->addWidget(origTextScaleSlider);
	mainLayout->addWidget(origTextScaleArea);

	// 预处理正则
	toml::ordered_array preRegexArr = toml::find_or_default<toml::ordered_array>(m_projectConfig, "plugins", "Epub", "preprocRegex");
	ElaScrollPageArea* preRegexArea = new ElaScrollPageArea(centerWidget);
	preRegexArea->setFixedHeight(365);
	QVBoxLayout* preRegexLayout = new QVBoxLayout(preRegexArea);
	preRegexLayout->setContentsMargins(12, 6, 12, 8);
	preRegexLayout->setSpacing(6);
	mainLayout->addSpacing(10);
	preRegexLayout->addWidget(new ElaDoubleText(tr("预处理正则"), 16,
		tr("提取正文后、送入翻译前应用的正则规则"), 10, "", preRegexArea));
	ElaPlainTextEdit* preRegexEdit = new ElaPlainTextEdit(preRegexArea);
	preRegexEdit->setMinimumHeight(300);
	preRegexEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "preprocRegex", preRegexArr }} })));
	installTreeSitterHighlighter(preRegexEdit->document(), SyntaxLanguage::Toml);
	preRegexLayout->addWidget(preRegexEdit);
	mainLayout->addWidget(preRegexArea);

	// 后处理正则
	toml::ordered_array postRegexArr = toml::find_or_default<toml::ordered_array>(m_projectConfig, "plugins", "Epub", "postprocRegex");
	ElaScrollPageArea* postRegexArea = new ElaScrollPageArea(centerWidget);
	postRegexArea->setFixedHeight(365);
	QVBoxLayout* postRegexLayout = new QVBoxLayout(postRegexArea);
	postRegexLayout->setContentsMargins(12, 6, 12, 8);
	postRegexLayout->setSpacing(6);
	mainLayout->addSpacing(10);
	postRegexLayout->addWidget(new ElaDoubleText(tr("后处理正则"), 16,
		tr("翻译完成后、写回 Epub 前应用的正则规则"), 10, "", postRegexArea));
	ElaPlainTextEdit* postRegexEdit = new ElaPlainTextEdit(postRegexArea);
	postRegexEdit->setMinimumHeight(300);
	postRegexEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "postprocRegex", postRegexArr }} })));
	installTreeSitterHighlighter(postRegexEdit->document(), SyntaxLanguage::Toml);
	postRegexLayout->addWidget(postRegexEdit);
	mainLayout->addWidget(postRegexArea);

	QWidget* tipButtonWidget = new QWidget(centerWidget);
	QHBoxLayout* tipLayout = new QHBoxLayout(tipButtonWidget);
	tipLayout->addStretch();
	ElaToolButton* tipButton = new ElaToolButton(tipButtonWidget);
	tipButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	tipButton->setElaIcon(ElaIconType::BookOpen);
	tipButton->setText(tr("说明"));
	tipLayout->addWidget(tipButton);
	connect(tipButton, &ElaToolButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile("BaseConfig/illustration/epub.html"));
		});
	mainLayout->addWidget(tipButtonWidget);

	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "plugins.Epub.bilingualOutput", bilingualOutputSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.Epub.originalTextColor", origTextColorDialog->getCurrentColorRGB().toStdString());
			insertToml(m_projectConfig, "plugins.Epub.originalTextScale", origTextScaleSlider->value());

			try {
				toml::ordered_value newPreRegexTbl = toml::parse_str<toml::ordered_type_config>(preRegexEdit->toPlainText().toStdString());
				auto& newPreRegexArr = newPreRegexTbl["preprocRegex"];
				if (newPreRegexArr.is_array()) {
					insertToml(m_projectConfig, "plugins.Epub.preprocRegex", newPreRegexArr);
				}
				else {
					insertToml(m_projectConfig, "plugins.Epub.preprocRegex", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("Epub 预处理正则不符合 toml 规范"), 3000);
			}
			try {
				toml::ordered_value newPostRegexTbl = toml::parse_str<toml::ordered_type_config>(postRegexEdit->toPlainText().toStdString());
				auto& newPostRegexArr = newPostRegexTbl["postprocRegex"];
				if (newPostRegexArr.is_array()) {
					insertToml(m_projectConfig, "plugins.Epub.postprocRegex", newPostRegexArr);
				}
				else {
					insertToml(m_projectConfig, "plugins.Epub.postprocRegex", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("Epub 后处理正则不符合 toml 规范"), 3000);
			}
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("Epub 输出配置"));
	addCentralWidget(centerWidget, true, false, 0);
}
