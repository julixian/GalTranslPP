#include "SettingPage.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaApplication.h"
#include "ElaComboBox.h"
#include "ElaLog.h"
#include "ElaRadioButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToggleSwitch.h"
#include "ElaWindow.h"
#include <QButtonGroup>

import Tool;

SettingPage::SettingPage(toml::table& globalConfig, QWidget* parent)
    : BasePage(parent), _globalConfig(globalConfig)
{
    // 预览窗口标题
    ElaWindow* window = dynamic_cast<ElaWindow*>(parent);
    setWindowTitle("Setting");

    ElaText* themeText = new ElaText("主题设置", this);
    themeText->setWordWrap(false);
    themeText->setTextPixelSize(18);

    _themeComboBox = new ElaComboBox(this);
    _themeComboBox->addItem("日间模式");
    _themeComboBox->addItem("夜间模式");
    _themeComboBox->setCurrentIndex((int)eTheme->getThemeMode());
    ElaScrollPageArea* themeSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* themeSwitchLayout = new QHBoxLayout(themeSwitchArea);
    ElaText* themeSwitchText = new ElaText("主题切换", this);
    themeSwitchText->setWordWrap(false);
    themeSwitchText->setTextPixelSize(15);
    themeSwitchLayout->addWidget(themeSwitchText);
    themeSwitchLayout->addStretch();
    themeSwitchLayout->addWidget(_themeComboBox);
    connect(_themeComboBox, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [=](int index) {
        if (index == 0)
        {
            eTheme->setThemeMode(ElaThemeType::Light);
        }
        else
        {
            eTheme->setThemeMode(ElaThemeType::Dark);
        }
    });
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        _themeComboBox->blockSignals(true);
        if (themeMode == ElaThemeType::Light)
        {
            _themeComboBox->setCurrentIndex(0);
        }
        else
        {
            _themeComboBox->setCurrentIndex(1);
        }
        _themeComboBox->blockSignals(false);
    });

    _normalButton = new ElaRadioButton("Normal", this);
    _elaMicaButton = new ElaRadioButton("ElaMica", this);

    _micaButton = new ElaRadioButton("Mica", this);
    _micaAltButton = new ElaRadioButton("Mica-Alt", this);
    _acrylicButton = new ElaRadioButton("Acrylic", this);
    _dwmBlurnormalButton = new ElaRadioButton("Dwm-Blur", this);

    QButtonGroup* displayButtonGroup = new QButtonGroup(this);
    displayButtonGroup->addButton(_normalButton, 0);
    displayButtonGroup->addButton(_elaMicaButton, 1);

    displayButtonGroup->addButton(_micaButton, 2);
    displayButtonGroup->addButton(_micaAltButton, 3);
    displayButtonGroup->addButton(_acrylicButton, 4);
    displayButtonGroup->addButton(_dwmBlurnormalButton, 5);
    int windowDisplayMode = _globalConfig["windowDisplayMode"].value_or(0); // 不知道为什么3及以上的值会失效
    displayButtonGroup->button((int)windowDisplayMode)->setChecked(true);
    eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)windowDisplayMode);

    connect(displayButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            insertToml(_globalConfig, "windowDisplayMode", (int)displayButtonGroup->id(button));
            eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)displayButtonGroup->id(button));
        }
    });
    connect(eApp, &ElaApplication::pWindowDisplayModeChanged, this, [=]() {
        auto button = displayButtonGroup->button(eApp->getWindowDisplayMode());
        ElaRadioButton* elaRadioButton = dynamic_cast<ElaRadioButton*>(button);
        if (elaRadioButton)
        {
            elaRadioButton->setChecked(true);
        }
    });

    ElaScrollPageArea* micaSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* micaSwitchLayout = new QHBoxLayout(micaSwitchArea);
    ElaText* micaSwitchText = new ElaText("窗口效果", this);
    micaSwitchText->setWordWrap(false);
    micaSwitchText->setTextPixelSize(15);
    micaSwitchLayout->addWidget(micaSwitchText);
    micaSwitchLayout->addStretch();
    micaSwitchLayout->addWidget(_normalButton);
    micaSwitchLayout->addWidget(_elaMicaButton);

    micaSwitchLayout->addWidget(_micaButton);
    micaSwitchLayout->addWidget(_micaAltButton);
    micaSwitchLayout->addWidget(_acrylicButton);
    micaSwitchLayout->addWidget(_dwmBlurnormalButton);


    _minimumButton = new ElaRadioButton("Minimum", this);
    _compactButton = new ElaRadioButton("Compact", this);
    _maximumButton = new ElaRadioButton("Maximum", this);
    _autoButton = new ElaRadioButton("Auto", this);
    ElaScrollPageArea* displayModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* displayModeLayout = new QHBoxLayout(displayModeArea);
    ElaText* displayModeText = new ElaText("导航栏模式选择", this);
    displayModeText->setWordWrap(false);
    displayModeText->setTextPixelSize(15);
    displayModeLayout->addWidget(displayModeText);
    displayModeLayout->addStretch();
    displayModeLayout->addWidget(_minimumButton);
    displayModeLayout->addWidget(_compactButton);
    displayModeLayout->addWidget(_maximumButton);
    displayModeLayout->addWidget(_autoButton);

    QButtonGroup* navigationGroup = new QButtonGroup(this);
    navigationGroup->addButton(_autoButton, 0);
    navigationGroup->addButton(_minimumButton, 1);
    navigationGroup->addButton(_compactButton, 2);
    navigationGroup->addButton(_maximumButton, 3);
    int navigationMode = _globalConfig["navigationMode"].value_or(3);
    window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationMode);
    connect(navigationGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            insertToml(_globalConfig, "navigationMode", (int)navigationGroup->id(button));
            window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationGroup->id(button));
        }
    });

    ElaText* helperText = new ElaText("应用程序设置", this);
    helperText->setWordWrap(false);
    helperText->setTextPixelSize(18);

    // 任务完成后自动刷新人名表和字典
    ElaScrollPageArea* autoRefreshArea = new ElaScrollPageArea(this);
    QHBoxLayout* autoRefreshLayout = new QHBoxLayout(autoRefreshArea);
    ElaText* autoRefreshText = new ElaText("DumpName/GenDict任务完成后自动刷新人名表和字典", autoRefreshArea);
    autoRefreshText->setWordWrap(false);
    autoRefreshText->setTextPixelSize(15);
    autoRefreshLayout->addWidget(autoRefreshText);
    autoRefreshLayout->addStretch();
    ElaToggleSwitch* autoRefreshSwitch = new ElaToggleSwitch(autoRefreshArea);
    autoRefreshSwitch->setIsToggled(_globalConfig["autoRefreshAfterTranslate"].value_or(true));
    connect(autoRefreshSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
        {
            insertToml(_globalConfig, "autoRefreshAfterTranslate", checked);
        });
    autoRefreshLayout->addWidget(autoRefreshSwitch);

    // 默认以纯文本/表模式打开人名表
    ElaScrollPageArea* nameTableOpenModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* nameTableOpenModeLayout = new QHBoxLayout(nameTableOpenModeArea);
    ElaText* nameTableOpenModeText = new ElaText("新项目人名表默认打开模式", nameTableOpenModeArea);
    nameTableOpenModeText->setWordWrap(false);
    nameTableOpenModeText->setTextPixelSize(15);
    ElaRadioButton* nameTableOpenModeTextButton = new ElaRadioButton("纯文本模式", nameTableOpenModeArea);
    ElaRadioButton* nameTableOpenModeTableButton = new ElaRadioButton("表格模式", nameTableOpenModeArea);
    nameTableOpenModeLayout->addWidget(nameTableOpenModeText);
    nameTableOpenModeLayout->addStretch();
    nameTableOpenModeLayout->addWidget(nameTableOpenModeTextButton);
    nameTableOpenModeLayout->addWidget(nameTableOpenModeTableButton);
    int nameTableOpenMode = _globalConfig["defaultNameTableOpenMode"].value_or(0);
    QButtonGroup* nameTableOpenModeGroup = new QButtonGroup(nameTableOpenModeArea);
    nameTableOpenModeGroup->addButton(nameTableOpenModeTextButton, 0);
    nameTableOpenModeGroup->addButton(nameTableOpenModeTableButton, 1);
    nameTableOpenModeGroup->button(nameTableOpenMode)->setChecked(true);
    connect(nameTableOpenModeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            insertToml(_globalConfig, "defaultNameTableOpenMode", nameTableOpenModeGroup->id(button));
        }
    });

    // 默认以纯文本/表模式打开字典
    ElaScrollPageArea* dictOpenModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* dictOpenModeLayout = new QHBoxLayout(dictOpenModeArea);
    ElaText* dictOpenModeText = new ElaText("新项目字典默认打开模式", dictOpenModeArea);
    dictOpenModeText->setWordWrap(false);
    dictOpenModeText->setTextPixelSize(15);
    ElaRadioButton* dictOpenModeTextButton = new ElaRadioButton("纯文本模式", dictOpenModeArea);
    ElaRadioButton* dictOpenModeTableButton = new ElaRadioButton("表格模式", dictOpenModeArea);
    dictOpenModeLayout->addWidget(dictOpenModeText);
    dictOpenModeLayout->addStretch();
    dictOpenModeLayout->addWidget(dictOpenModeTextButton);
    dictOpenModeLayout->addWidget(dictOpenModeTableButton);
    int dictOpenMode = _globalConfig["defaultDictOpenMode"].value_or(0);
    QButtonGroup* dictOpenModeGroup = new QButtonGroup(dictOpenModeArea);
    dictOpenModeGroup->addButton(dictOpenModeTextButton, 0);
    dictOpenModeGroup->addButton(dictOpenModeTableButton, 1);
    dictOpenModeGroup->button(dictOpenMode)->setChecked(true);
    connect(dictOpenModeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            insertToml(_globalConfig, "defaultDictOpenMode", dictOpenModeGroup->id(button));
        }
    });

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("Setting");
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);
    centerLayout->addSpacing(30);
    centerLayout->addWidget(themeText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(themeSwitchArea);
    centerLayout->addWidget(micaSwitchArea);
    centerLayout->addWidget(displayModeArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(helperText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(autoRefreshArea);
    centerLayout->addWidget(nameTableOpenModeArea);
    centerLayout->addWidget(dictOpenModeArea);
    centerLayout->addStretch();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    addCentralWidget(centralWidget, true, true, 0);
}

SettingPage::~SettingPage()
{
}
