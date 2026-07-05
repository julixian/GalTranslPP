#include "TF2HCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaDoubleText.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"

import Tool;

TF2HCfgPage::TF2HCfgPage(toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("全角半角转换设置"));
    setContentsMargins(30, 15, 15, 0);

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    // 标点符号转换
    bool convertPunctuation = toml::find_or(m_projectConfig, "plugins", "TextFull2Half", "是否替换标点", true);
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
    bool reverseConvert = toml::find_or(m_projectConfig, "plugins", "TextFull2Half", "是否反向替换", false);
    ElaScrollPageArea* reverseArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* reverseLayout = new QHBoxLayout(reverseArea);
    ElaDoubleText* reverseText = new ElaDoubleText(tr("反向替换"), 16,
        tr("关闭为全转半，开启为半转全"), 10, "", reverseArea);
    reverseLayout->addWidget(reverseText);
    reverseLayout->addStretch();
    ElaToggleSwitch* reverseSwitch = new ElaToggleSwitch(reverseArea);
    reverseSwitch->setIsToggled(reverseConvert);
    reverseLayout->addWidget(reverseSwitch);
    mainLayout->addWidget(reverseArea);

    // 不转换的字符
    const std::string excludeChars = toml::find_or(m_projectConfig, "plugins", "TextFull2Half", "不转换的字符", "");
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
    toml::ordered_array notConvertRegsArr = toml::find_or_default<toml::ordered_array>(m_projectConfig, "plugins", "TextFull2Half", "notConvertRegs");
    ElaText* notConvertRegsTitle = new ElaText("Not convert regular expressions", 18, centerWidget);
    notConvertRegsTitle->setWordWrap(false);
    mainLayout->addWidget(notConvertRegsTitle);
    ElaPlainTextEdit* notConvertRegsEdit = new ElaPlainTextEdit(centerWidget);
    QFont font = notConvertRegsEdit->font();
    font.setPixelSize(14);
    notConvertRegsEdit->setFont(font);
    notConvertRegsEdit->setPlainText(
        QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "notConvertRegs", notConvertRegsArr }} }))
    );
    notConvertRegsEdit->moveCursor(QTextCursor::Start);
    mainLayout->addWidget(notConvertRegsEdit);

    m_applyFunc = [=]
        {
            insertToml(m_projectConfig, "plugins.TextFull2Half.是否替换标点", punctuationSwitch->getIsToggled());
            insertToml(m_projectConfig, "plugins.TextFull2Half.是否反向替换", reverseSwitch->getIsToggled());
            insertToml(m_projectConfig, "plugins.TextFull2Half.不转换的字符", excludeEdit->text().toStdString());

            toml::ordered_array newNotConvertRegsArr;
            try {
                toml::ordered_value newTbl = toml::parse_str<toml::ordered_type_config>(notConvertRegsEdit->toPlainText().toStdString());
                auto& newArr = newTbl["notConvertRegs"];
                if (newArr.is_array()) {
                    for (const auto& item : newArr.as_array()
                        | std::views::filter([](const auto& item_) { return item_.is_string(); }))
                    {
                        newNotConvertRegsArr.push_back(item);
                    }
                }
            }
            catch (...) {
                ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("notConvertRegs 不符合 toml 规范"), 3000);
            }
            insertToml(m_projectConfig, "plugins.TextFull2Half.notConvertRegs", newNotConvertRegsArr);
        };

    mainLayout->addStretch();
    centerWidget->setWindowTitle(tr("全角半角转换设置"));
    addCentralWidget(centerWidget, true, false, 0);
}

TF2HCfgPage::~TF2HCfgPage()
{

}
