#include "PostFull2HalfCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaScrollPageArea.h"

import Tool;

void PostFull2HalfCfgPage::apply2Config()
{

}

PostFull2HalfCfgPage::PostFull2HalfCfgPage(toml::table& projectConfig, QWidget* parent) 
    : BasePage(parent), _projectConfig(projectConfig)
{
    setWindowTitle("全角半角转换设置");

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    // 标点符号转换配置
    bool convertPunctuation = _projectConfig["plugins"]["TextPostFull2Half"]["是否替换标点"].value_or(true);
    ElaScrollPageArea* punctuationArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* punctuationLayout = new QHBoxLayout(punctuationArea);
    ElaText* punctuationText = new ElaText("转换标点符号", punctuationArea);
    punctuationText->setTextPixelSize(16);
    punctuationLayout->addWidget(punctuationText);
    punctuationLayout->addStretch();
    ElaToggleSwitch* punctuationSwitch = new ElaToggleSwitch(punctuationArea);
    punctuationSwitch->setIsToggled(convertPunctuation);
    insertToml(_projectConfig, "plugins.TextPostFull2Half.是否替换标点", convertPunctuation);
    connect(punctuationSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        insertToml(_projectConfig, "plugins.TextPostFull2Half.是否替换标点", checked);
    });
    punctuationLayout->addWidget(punctuationSwitch);
    mainLayout->addWidget(punctuationArea);

    // 反向替换配置
    bool reverseConvert = _projectConfig["plugins"]["TextPostFull2Half"]["是否反向替换"].value_or(false);
    ElaScrollPageArea* reverseArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* reverseLayout = new QHBoxLayout(reverseArea);
    ElaText* reverseText = new ElaText("反向替换", reverseArea);
    reverseText->setTextPixelSize(16);
    reverseLayout->addWidget(reverseText);
    reverseLayout->addStretch();
    ElaToggleSwitch* reverseSwitch = new ElaToggleSwitch(reverseArea);
    reverseSwitch->setIsToggled(reverseConvert);
    insertToml(_projectConfig, "plugins.TextPostFull2Half.是否反向替换", reverseConvert);
    connect(reverseSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        insertToml(_projectConfig, "plugins.TextPostFull2Half.反向替换", checked);
    });
    reverseLayout->addWidget(reverseSwitch);

    mainLayout->addWidget(reverseArea);
    mainLayout->addStretch();
    centerWidget->setWindowTitle("全角半角转换设置");
    addCentralWidget(centerWidget);
}

PostFull2HalfCfgPage::~PostFull2HalfCfgPage()
{

}
