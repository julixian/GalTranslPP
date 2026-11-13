#include "EpubCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>

#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaToggleSwitch.h"
#include "ElaColorDialog.h"
#include "GppDoubleText.h"
#include "GppPushButton.h"
#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ValueSliderWidget.h"
#include "ElaText.h"
#include "TreeSitterHighlighter.h"

import Tool;

using namespace gpp;

EpubCfgPage::EpubCfgPage(tomlkit::Item& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("Epub 输出配置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 双语显示
	bool bilingual = tomlkit::findOr(m_projectConfig, "plugins", "Epub", "bilingualOutput", true);
	ElaScrollPageArea* bilingualOutputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* bilingualOutputLayout = new QHBoxLayout(bilingualOutputArea);
	GppDoubleText* bilingualOutputText = new GppDoubleText(tr("双语显示"), 16,
		tr("在每句译文下以设置的颜色和比例显示原文"), 10, "");
	bilingualOutputLayout->addWidget(bilingualOutputText);
	bilingualOutputLayout->addStretch();
	ElaToggleSwitch* bilingualOutputSwitch = new ElaToggleSwitch(bilingualOutputArea);
	bilingualOutputSwitch->setIsToggled(bilingual);
	bilingualOutputLayout->addWidget(bilingualOutputSwitch);
	mainLayout->addWidget(bilingualOutputArea);

	// 原文颜色
	const std::string origTextColorStr = tomlkit::findOr(m_projectConfig, "plugins", "Epub", "originalTextColor", "#808080");
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
	GppPushButton* origTextColorButton = new GppPushButton(origTextColorArea);
	origTextColorButton->setFixedSize(35, 35);
	origTextColorButton->setLightDefaultColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setLightHoverColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setLightPressColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setDarkDefaultColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setDarkHoverColor(origTextColorDialog->getCurrentColor());
	origTextColorButton->setDarkPressColor(origTextColorDialog->getCurrentColor());
	connect(origTextColorButton, &GppPushButton::clicked, this, [=]()
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
	double origTextScale = tomlkit::findOr(m_projectConfig, "plugins", "Epub", "originalTextScale", 0.8);
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
	tomlkit::Item preRegexArr = tomlkit::findArrayOrEmpty(m_projectConfig, "plugins", "Epub", "preprocRegex");
	ElaScrollPageArea* preRegexArea = new ElaScrollPageArea(centerWidget);
	preRegexArea->setFixedHeight(365);
	QVBoxLayout* preRegexLayout = new QVBoxLayout(preRegexArea);
	preRegexLayout->setContentsMargins(12, 6, 12, 8);
	preRegexLayout->setSpacing(6);
	mainLayout->addSpacing(10);
	preRegexLayout->addWidget(new GppDoubleText(tr("预处理正则"), 16,
		tr("提取正文后、送入翻译前应用的正则规则"), 10, "", preRegexArea));
	ElaPlainTextEdit* preRegexEdit = new ElaPlainTextEdit(preRegexArea);
	preRegexEdit->setMinimumHeight(300);
	tomlkit::Item preRegexDoc = tomlkit::Item::document();
	preRegexDoc["preprocRegex"] = std::move(preRegexArr);
	QString preRegexText = QString::fromStdString(tomlkit::format(preRegexDoc));
	while (preRegexText.startsWith('\r') || preRegexText.startsWith('\n')) {
		preRegexText.remove(0, 1);
	}
	preRegexEdit->setPlainText(preRegexText);
	installTreeSitterHighlighter(preRegexEdit->document(), SyntaxLanguage::Toml);
	preRegexLayout->addWidget(preRegexEdit);
	mainLayout->addWidget(preRegexArea);

	// 后处理正则
	tomlkit::Item postRegexArr = tomlkit::findArrayOrEmpty(m_projectConfig, "plugins", "Epub", "postprocRegex");
	ElaScrollPageArea* postRegexArea = new ElaScrollPageArea(centerWidget);
	postRegexArea->setFixedHeight(365);
	QVBoxLayout* postRegexLayout = new QVBoxLayout(postRegexArea);
	postRegexLayout->setContentsMargins(12, 6, 12, 8);
	postRegexLayout->setSpacing(6);
	mainLayout->addSpacing(10);
	postRegexLayout->addWidget(new GppDoubleText(tr("后处理正则"), 16,
		tr("翻译完成后、写回 Epub 前应用的正则规则"), 10, "", postRegexArea));
	ElaPlainTextEdit* postRegexEdit = new ElaPlainTextEdit(postRegexArea);
	postRegexEdit->setMinimumHeight(300);
	tomlkit::Item postRegexDoc = tomlkit::Item::document();
	postRegexDoc["postprocRegex"] = std::move(postRegexArr);
	QString postRegexText = QString::fromStdString(tomlkit::format(postRegexDoc));
	while (postRegexText.startsWith('\r') || postRegexText.startsWith('\n')) {
		postRegexText.remove(0, 1);
	}
	postRegexEdit->setPlainText(postRegexText);
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

	m_applyFunc = [=, this]()
		{
			insertToml(m_projectConfig, "plugins.Epub.bilingualOutput", bilingualOutputSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.Epub.originalTextColor", origTextColorDialog->getCurrentColorRGB().toStdString());
			insertToml(m_projectConfig, "plugins.Epub.originalTextScale", origTextScaleSlider->value());

			auto saveRegexEdit = [this](ElaPlainTextEdit* edit, const std::string& key, const QString& errMsg)
				{
					try {
						const toml newRegexTbl = tomlkit::parse(edit->toPlainText().toStdString());
						if (const toml* pArr = newRegexTbl.find(key); pArr && pArr->isArray()) {
							insertToml(m_projectConfig, "plugins.Epub." + key, *pArr);
						}
						else if (const toml* pFullArr = newRegexTbl.find("plugins", "Epub", key); pFullArr && pFullArr->isArray()) {
							insertToml(m_projectConfig, "plugins.Epub." + key, *pFullArr);
						}
						else if (newRegexTbl.empty()) {
							toml emptyAot = toml::arrayOfTables();
							emptyAot.push_back(toml::table(tomlkit::TableStyle::ArrayOfTablesElement));
							insertToml(m_projectConfig, "plugins.Epub." + key, emptyAot);
						}
						else {
							ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), errMsg, 3000);
						}
					}
					catch (...) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), errMsg, 3000);
					}
				};

			saveRegexEdit(preRegexEdit, "preprocRegex", tr("Epub 预处理正则不符合 toml 规范"));
			saveRegexEdit(postRegexEdit, "postprocRegex", tr("Epub 后处理正则不符合 toml 规范"));
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("Epub 输出配置"));
	addCentralWidget(centerWidget, true, false, 0);
}
