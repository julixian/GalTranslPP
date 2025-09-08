#include "Full2HalfCfgDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDebug>
#include <QHeaderView>
#include <QLabel>

#include "ElaScrollPageArea.h"
#include "ElaText.h"

Full2HalfCfgDialog::Full2HalfCfgDialog(toml::table& projectConfig, QWidget* parent) 
    : ElaContentDialog(parent), _projectConfig(projectConfig)
{
    setWindowTitle("TextFull2Half Configuration");
    
    setLeftButtonText("Cancel");
    setMiddleButtonText("Reset"); 
    setRightButtonText("OK");

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    connect(this, &Full2HalfCfgDialog::rightButtonClicked, this, &Full2HalfCfgDialog::onRightButtonClicked);
    connect(this, &Full2HalfCfgDialog::middleButtonClicked, this, &Full2HalfCfgDialog::onMiddleButtonClicked);
    connect(this, &Full2HalfCfgDialog::leftButtonClicked, this, &Full2HalfCfgDialog::onLeftButtonClicked);



    // 标点符号转换配置
    bool convertPunctuation = true;
    if (auto node = _projectConfig["plugins"]["TextFull2Half"]["是否替换标点"]) {
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
        insertToml(_projectConfig, "plugins.TextFull2Half.是否替换标点", checked);
    });
    punctuationLayout->addWidget(_punctuationSwitch);

    _resetFunc = [=]() {
        _punctuationSwitch->setIsToggled(true);
    };

    _cancelFunc = [=]() {
        _punctuationSwitch->setIsToggled(convertPunctuation);
    };

    mainLayout->addWidget(punctuationArea);

    // 替换时机配置
    std::string_view timing = _projectConfig.at_path("plugins.TextFull2Half.替换时机").value_or("before_dst_processed");
    ElaScrollPageArea* timingArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* timingLayout = new QHBoxLayout(timingArea);
    ElaText* timingText = new ElaText("替换时机", timingArea);
    timingText->setTextPixelSize(16);
    timingLayout->addWidget(timingText);
    timingLayout->addStretch();
    _timingCombo = new ElaComboBox(timingArea);
    _timingCombo->addItems(_timingOptions);
    _timingCombo->setCurrentIndex(_timingOptions.indexOf(QString::fromUtf8(timing.data(), timing.size())));
    connect(_timingCombo, &QComboBox::currentTextChanged, this, [=](const QString& text) {
        insertToml(_projectConfig, "plugins.TextFull2Half.替换时机", text.toStdString());
    });
    timingLayout->addWidget(_timingCombo);

    // 反向替换配置
    bool reverseConvert = false;
    if (auto node = _projectConfig["plugins"]["TextFull2Half"]["反向替换"]) {
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
        insertToml(_projectConfig, "plugins.TextFull2Half.反向替换", checked);
    });
    reverseLayout->addWidget(_reverseSwitch);

    _resetFunc = [=]() {
        _punctuationSwitch->setIsToggled(true);
        _timingCombo->setCurrentText("before_dst_processed");
        _reverseSwitch->setIsToggled(false);
    };

    _cancelFunc = [=]() {
        _punctuationSwitch->setIsToggled(convertPunctuation);
        _timingCombo->setCurrentText(QString::fromUtf8(timing.data(), timing.size()));
        _reverseSwitch->setIsToggled(reverseConvert);
    };

    mainLayout->addWidget(timingArea);
    mainLayout->addWidget(reverseArea);

    mainLayout->addWidget(reverseArea);

    setCentralWidget(centerWidget);
}

Full2HalfCfgDialog::~Full2HalfCfgDialog()
{
}

void Full2HalfCfgDialog::onRightButtonClicked() 
{
    accept();
}

void Full2HalfCfgDialog::onMiddleButtonClicked()
{
    if (_resetFunc) {
        _resetFunc();
    }
}

void Full2HalfCfgDialog::onLeftButtonClicked()
{
    if (_cancelFunc) {
        _cancelFunc();
    }
}
