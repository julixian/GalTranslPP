#include "PostFull2HalfCfgDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDebug>
#include <QHeaderView>
#include <QLabel>

#include "ElaScrollPageArea.h"
#include "ElaText.h"

PostFull2HalfCfgDialog::PostFull2HalfCfgDialog(toml::table& projectConfig, QWidget* parent) 
    : ElaContentDialog(parent), _projectConfig(projectConfig)
{
    setWindowTitle("TextPostFull2Half Configuration");
    
    setLeftButtonText("Cancel");
    setMiddleButtonText("Reset"); 
    setRightButtonText("OK");

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    connect(this, &PostFull2HalfCfgDialog::rightButtonClicked, this, &PostFull2HalfCfgDialog::onRightButtonClicked);
    connect(this, &PostFull2HalfCfgDialog::middleButtonClicked, this, &PostFull2HalfCfgDialog::onMiddleButtonClicked);
    connect(this, &PostFull2HalfCfgDialog::leftButtonClicked, this, &PostFull2HalfCfgDialog::onLeftButtonClicked);



    // 标点符号转换配置
    bool convertPunctuation = true;
    if (auto node = _projectConfig["plugins"]["TextPostFull2Half"]["是否替换标点"]) {
        convertPunctuation = node.value_or(true);
    }
    ElaScrollPageArea* punctuationArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* punctuationLayout = new QHBoxLayout(punctuationArea);
    ElaText* punctuationText = new ElaText("转换标点符号", punctuationArea);
    punctuationText->setTextPixelSize(16);
    punctuationLayout->addWidget(punctuationText);
    punctuationLayout->addStretch();
    _punctuationSwitch = new ElaToggleSwitch(punctuationArea);
    _punctuationSwitch->setIsToggled(convertPunctuation);
    connect(_punctuationSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        insertToml(_projectConfig, "plugins.TextPostFull2Half.是否替换标点", checked);
    });
    punctuationLayout->addWidget(_punctuationSwitch);

    _resetFunc = [=]() {
        _punctuationSwitch->setIsToggled(true);
    };

    _cancelFunc = [=]() {
        _punctuationSwitch->setIsToggled(convertPunctuation);
    };

    mainLayout->addWidget(punctuationArea);

    // 反向替换配置
    bool reverseConvert = false;
    if (auto node = _projectConfig["plugins"]["TextPostFull2Half"]["反向替换"]) {
        reverseConvert = node.value_or(false);
    }
    ElaScrollPageArea* reverseArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* reverseLayout = new QHBoxLayout(reverseArea);
    ElaText* reverseText = new ElaText("反向替换", reverseArea);
    reverseText->setTextPixelSize(16);
    reverseLayout->addWidget(reverseText);
    reverseLayout->addStretch();
    _reverseSwitch = new ElaToggleSwitch(reverseArea);
    _reverseSwitch->setIsToggled(reverseConvert);
    connect(_reverseSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        insertToml(_projectConfig, "plugins.TextPostFull2Half.反向替换", checked);
    });
    reverseLayout->addWidget(_reverseSwitch);

    _resetFunc = [=]() {
        _punctuationSwitch->setIsToggled(true);
        _reverseSwitch->setIsToggled(false);
    };

    _cancelFunc = [=]() {
        _punctuationSwitch->setIsToggled(convertPunctuation);
        _reverseSwitch->setIsToggled(reverseConvert);
    };

    mainLayout->addWidget(reverseArea);

    setCentralWidget(centerWidget);
}

PostFull2HalfCfgDialog::~PostFull2HalfCfgDialog()
{
}

void PostFull2HalfCfgDialog::onRightButtonClicked() 
{
    accept();
}

void PostFull2HalfCfgDialog::onMiddleButtonClicked()
{
    if (_resetFunc) {
        _resetFunc();
    }
}

void PostFull2HalfCfgDialog::onLeftButtonClicked()
{
    if (_cancelFunc) {
        _cancelFunc();
    }
}
