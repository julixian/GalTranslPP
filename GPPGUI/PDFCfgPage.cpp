#include "pdfCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>

#include "ElaScrollPageArea.h"
#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaDoubleText.h"

import Tool;

PDFCfgPage::PDFCfgPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("PDF 输出配置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 输出双语翻译文件
	bool outputDual = toml::find_or(m_projectConfig, "plugins", "PDF", "bilingualOutput", true);
	ElaScrollPageArea* outputArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* outputLayout = new QHBoxLayout(outputArea);
	ElaText* outputText = new ElaText(tr("输出双语翻译文件"), outputArea);
	outputText->setWordWrap(false);
	outputText->setTextPixelSize(16);
	outputLayout->addWidget(outputText);
	outputLayout->addStretch();
	ElaToggleSwitch* outputSwitch = new ElaToggleSwitch(outputArea);
	outputSwitch->setIsToggled(outputDual);
	outputLayout->addWidget(outputSwitch);
	mainLayout->addWidget(outputArea);

	// BabelDOC 目标语言码，影响 PDF 回注时的字体和排版。
	const std::string babeldocLangOut = toml::find_or(m_projectConfig, "plugins", "PDF", "babeldocLangOut", "zh-CN");
	ElaScrollPageArea* babeldocLangOutArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* babeldocLangOutLayout = new QHBoxLayout(babeldocLangOutArea);
	ElaDoubleText* babeldocLangOutText = new ElaDoubleText(tr("BabelDOC 目标语言码"), 16,
		tr("影响 PDF 字体和排版，如 zh-CN/zh-TW/en/ja/ko"), 10, "", babeldocLangOutArea);
	babeldocLangOutLayout->addWidget(babeldocLangOutText);
	babeldocLangOutLayout->addStretch();
	ElaLineEdit* babeldocLangOutEdit = new ElaLineEdit(babeldocLangOutArea);
	babeldocLangOutEdit->setFixedWidth(150);
	babeldocLangOutEdit->setText(QString::fromStdString(babeldocLangOut));
	babeldocLangOutLayout->addWidget(babeldocLangOutEdit);
	mainLayout->addWidget(babeldocLangOutArea);

	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "plugins.PDF.bilingualOutput", outputSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.PDF.babeldocLangOut", babeldocLangOutEdit->text().toStdString());
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("PDF 输出配置"));
	addCentralWidget(centerWidget, true, false, 0);
}
