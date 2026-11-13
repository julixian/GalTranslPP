#include "TF2HCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "GppDoubleText.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "TreeSitterHighlighter.h"

import Tool;

using namespace gpp;

TF2HCfgPage::TF2HCfgPage(tomlkit::Item& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("全角半角转换设置"));
    setContentsMargins(30, 15, 15, 0);

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    // 标点符号转换
    bool convertPunctuation = tomlkit::findOr(m_projectConfig, "plugins", "TextFull2Half", "replacePunctuation", true);
    ElaScrollPageArea* punctuationArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* punctuationLayout = new QHBoxLayout(punctuationArea);
    ElaText* punctuationText = new ElaText(tr("转换标点符号"), punctuationArea);
    punctuationText->setWordWrap(false);
    punctuationText->setTextPixelSize(16);
    punctuationLayout->addWidget(punctuationText);
    punctuationLayout->addStretch();
    ElaToggleSwitch* punctuationSwitch = new ElaToggleSwitch(punctuationArea);
    punctuationSwitch->setIsToggled(convertPunctuation);
    punctuationLayout->addWidget(punctuationSwitch);
    mainLayout->addWidget(punctuationArea);

    // 反向替换
    bool reverseConvert = tomlkit::findOr(m_projectConfig, "plugins", "TextFull2Half", "reverseConversion", false);
    ElaScrollPageArea* reverseArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* reverseLayout = new QHBoxLayout(reverseArea);
    GppDoubleText* reverseText = new GppDoubleText(tr("反向替换"), 16,
        tr("关闭为全转半，开启为半转全"), 10, "", reverseArea);
    reverseLayout->addWidget(reverseText);
    reverseLayout->addStretch();
    ElaToggleSwitch* reverseSwitch = new ElaToggleSwitch(reverseArea);
    reverseSwitch->setIsToggled(reverseConvert);
    reverseLayout->addWidget(reverseSwitch);
    mainLayout->addWidget(reverseArea);

    // 不转换的字符
    const std::string excludeChars = tomlkit::findOr(m_projectConfig, "plugins", "TextFull2Half", "excludeChars", "");
    ElaScrollPageArea* excludeArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* excludeLayout = new QHBoxLayout(excludeArea);
    ElaText* excludeText = new ElaText(tr("不转换的字符"), excludeArea);
    excludeText->setWordWrap(false);
    excludeText->setTextPixelSize(16);
    excludeLayout->addWidget(excludeText);
    excludeLayout->addStretch();
    ElaLineEdit* excludeEdit = new ElaLineEdit(excludeArea);
    excludeEdit->setText(QString::fromStdString(excludeChars));
    excludeLayout->addWidget(excludeEdit);
    mainLayout->addWidget(excludeArea);

    mainLayout->addSpacing(10);
    // notConvertRegs
    tomlkit::Item notConvertRegsArr = tomlkit::findArrayOrEmpty(m_projectConfig, "plugins", "TextFull2Half", "notConvertRegs");
    ElaText* notConvertRegsTitle = new ElaText("不转换的正则组", 18, centerWidget);
    notConvertRegsTitle->setWordWrap(false);
    mainLayout->addWidget(notConvertRegsTitle);
    ElaPlainTextEdit* notConvertRegsEdit = new ElaPlainTextEdit(centerWidget);
	QFont font = notConvertRegsEdit->font();
	font.setPixelSize(14);
	notConvertRegsEdit->setFont(font);
	installTreeSitterHighlighter(notConvertRegsEdit->document(), SyntaxLanguage::Toml);
	tomlkit::Item notConvertRegsDoc = tomlkit::Item::document();
	notConvertRegsDoc["notConvertRegs"] = std::move(notConvertRegsArr);
	notConvertRegsEdit->setPlainText(
        QString::fromStdString(tomlkit::format(notConvertRegsDoc))
    );
    notConvertRegsEdit->moveCursor(QTextCursor::Start);
    mainLayout->addWidget(notConvertRegsEdit);

    m_applyFunc = [=, this]
        {
            insertToml(m_projectConfig, "plugins.TextFull2Half.replacePunctuation", punctuationSwitch->getIsToggled());
            insertToml(m_projectConfig, "plugins.TextFull2Half.reverseConversion", reverseSwitch->getIsToggled());
            insertToml(m_projectConfig, "plugins.TextFull2Half.excludeChars", excludeEdit->text().toStdString());

            try {
                const toml newTbl = tomlkit::parse(notConvertRegsEdit->toPlainText().toStdString());
                if (const toml* pNewArr = newTbl.find("notConvertRegs"); pNewArr && pNewArr->isArray()) {
                    insertToml(m_projectConfig, "plugins.TextFull2Half.notConvertRegs", *pNewArr);
                }
                else {
                    insertToml(m_projectConfig, "plugins.TextFull2Half.notConvertRegs", tomlkit::Item::array());
                }
            }
            catch (...) {
                ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("notConvertRegs 不符合 toml 规范"), 3000);
            }
        };

    mainLayout->addStretch();
    centerWidget->setWindowTitle(tr("全角半角转换设置"));
    addCentralWidget(centerWidget, true, false, 0);
}
