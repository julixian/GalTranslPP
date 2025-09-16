#include "SettingPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>

#include "ElaApplication.h"
#include "ElaComboBox.h"
#include "ElaRadioButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToggleSwitch.h"
#include "ElaWindow.h"

import Tool;

SettingPage::SettingPage(toml::table& globalConfig, QWidget* parent)
    : BasePage(parent), _globalConfig(globalConfig)
{
    // 预览窗口标题
    ElaWindow* window = qobject_cast<ElaWindow*>(parent);
    setWindowTitle("Setting");
    setContentsMargins(20, 10, 20, 10);

    ElaText* themeText = new ElaText("主题设置", this);
    themeText->setWordWrap(false);
    themeText->setTextPixelSize(18);

    int themeMode = _globalConfig["themeMode"].value_or(0);
    eTheme->setThemeMode((ElaThemeType::ThemeMode)themeMode);
    ElaComboBox* themeComboBox = new ElaComboBox(this);
    themeComboBox->addItem("日间模式");
    themeComboBox->addItem("夜间模式");
    themeComboBox->setCurrentIndex((int)eTheme->getThemeMode());
    ElaScrollPageArea* themeSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* themeSwitchLayout = new QHBoxLayout(themeSwitchArea);
    ElaText* themeSwitchText = new ElaText("主题切换", this);
    themeSwitchText->setWordWrap(false);
    themeSwitchText->setTextPixelSize(15);
    themeSwitchLayout->addWidget(themeSwitchText);
    themeSwitchLayout->addStretch();
    themeSwitchLayout->addWidget(themeComboBox);
    connect(themeComboBox, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [=](int index) {
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
        themeComboBox->blockSignals(true);
        if (themeMode == ElaThemeType::Light)
        {
            themeComboBox->setCurrentIndex(0);
        }
        else
        {
            themeComboBox->setCurrentIndex(1);
        }
        themeComboBox->blockSignals(false);
    });

    ElaRadioButton* normalButton = new ElaRadioButton("Normal", this);
    ElaRadioButton* elaMicaButton = new ElaRadioButton("ElaMica", this);

    ElaRadioButton* micaButton = new ElaRadioButton("Mica", this);
    ElaRadioButton* micaAltButton = new ElaRadioButton("Mica-Alt", this);
    ElaRadioButton* acrylicButton = new ElaRadioButton("Acrylic", this);
    ElaRadioButton* dwmBlurnormalButton = new ElaRadioButton("Dwm-Blur", this);

    QButtonGroup* displayButtonGroup = new QButtonGroup(this);
    displayButtonGroup->addButton(normalButton, 0);
    displayButtonGroup->addButton(elaMicaButton, 1);
    displayButtonGroup->addButton(micaButton, 2);
    displayButtonGroup->addButton(micaAltButton, 3);
    displayButtonGroup->addButton(acrylicButton, 4);
    displayButtonGroup->addButton(dwmBlurnormalButton, 5);

    int windowDisplayMode = _globalConfig["windowDisplayMode"].value_or(0); // 不知道为什么3及以上的值会失效
    QAbstractButton* abstractButton = displayButtonGroup->button(windowDisplayMode);
    if (abstractButton)
    {
        abstractButton->setChecked(true);
    }
    eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)windowDisplayMode);

    connect(displayButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)displayButtonGroup->id(button));
        }
    });
    connect(eApp, &ElaApplication::pWindowDisplayModeChanged, this, [=]() {
        auto button = displayButtonGroup->button(eApp->getWindowDisplayMode());
        ElaRadioButton* elaRadioButton = qobject_cast<ElaRadioButton*>(button);
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
    micaSwitchLayout->addWidget(normalButton);
    micaSwitchLayout->addWidget(elaMicaButton);
    micaSwitchLayout->addWidget(micaButton);
    micaSwitchLayout->addWidget(micaAltButton);
    micaSwitchLayout->addWidget(acrylicButton);
    micaSwitchLayout->addWidget(dwmBlurnormalButton);


    ElaRadioButton* minimumButton = new ElaRadioButton("Minimum", this);
    ElaRadioButton* compactButton = new ElaRadioButton("Compact", this);
    ElaRadioButton* maximumButton = new ElaRadioButton("Maximum", this);
    ElaRadioButton* autoButton = new ElaRadioButton("Auto", this);
    ElaScrollPageArea* displayModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* displayModeLayout = new QHBoxLayout(displayModeArea);
    ElaText* displayModeText = new ElaText("导航栏模式选择", this);
    displayModeText->setWordWrap(false);
    displayModeText->setTextPixelSize(15);
    displayModeLayout->addWidget(displayModeText);
    displayModeLayout->addStretch();
    displayModeLayout->addWidget(minimumButton);
    displayModeLayout->addWidget(compactButton);
    displayModeLayout->addWidget(maximumButton);
    displayModeLayout->addWidget(autoButton);

    QButtonGroup* navigationGroup = new QButtonGroup(this);
    navigationGroup->addButton(autoButton, 0);
    navigationGroup->addButton(minimumButton, 1);
    navigationGroup->addButton(compactButton, 2);
    navigationGroup->addButton(maximumButton, 3);
    int navigationMode = _globalConfig["navigationMode"].value_or(0);
    abstractButton = navigationGroup->button(navigationMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationMode);

    connect(navigationGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationGroup->id(button));
        }
    });

    ElaText* helperText = new ElaText("应用程序设置", this);
    helperText->setWordWrap(false);
    helperText->setTextPixelSize(18);

    // 任务完成后自动刷新人名表和字典
    ElaScrollPageArea* autoRefreshArea = new ElaScrollPageArea(this);
    QHBoxLayout* autoRefreshLayout = new QHBoxLayout(autoRefreshArea);
    ElaText* autoRefreshText = new ElaText("DumpName/GenDict任务成功后自动刷新人名表/项目GPT字典", autoRefreshArea);
    autoRefreshText->setWordWrap(false);
    autoRefreshText->setTextPixelSize(15);
    autoRefreshLayout->addWidget(autoRefreshText);
    autoRefreshLayout->addStretch();
    ElaToggleSwitch* autoRefreshSwitch = new ElaToggleSwitch(autoRefreshArea);
    autoRefreshSwitch->setIsToggled(_globalConfig["autoRefreshAfterTranslate"].value_or(true));
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
    abstractButton = nameTableOpenModeGroup->button(nameTableOpenMode);
    if (abstractButton)
    {
        abstractButton->setChecked(true);
    }

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
    abstractButton = dictOpenModeGroup->button(dictOpenMode);
    if (abstractButton)
    {
        abstractButton->setChecked(true);
    }

    // 允许在项目仍在运行的情况下关闭程序(危险)
    ElaScrollPageArea* allowCloseWhenRunningArea = new ElaScrollPageArea(this);
    QHBoxLayout* allowCloseWhenRunningLayout = new QHBoxLayout(allowCloseWhenRunningArea);
    ElaText* allowCloseWhenRunningText = new ElaText("允许在项目仍在运行的情况下关闭程序(危险)", allowCloseWhenRunningArea);
    allowCloseWhenRunningText->setWordWrap(false);
    allowCloseWhenRunningText->setTextPixelSize(15);
    ElaToggleSwitch* allowCloseWhenRunningSwitch = new ElaToggleSwitch(allowCloseWhenRunningArea);
    allowCloseWhenRunningSwitch->setIsToggled(_globalConfig["allowCloseWhenRunning"].value_or(false));
    allowCloseWhenRunningLayout->addWidget(allowCloseWhenRunningText);
    allowCloseWhenRunningLayout->addStretch();
    allowCloseWhenRunningLayout->addWidget(allowCloseWhenRunningSwitch);

    _applyFunc = [=]()
        {
            QRect rect = window->frameGeometry();
            insertToml(_globalConfig, "windowWidth", rect.width());
            insertToml(_globalConfig, "windowHeight", rect.height());
            insertToml(_globalConfig, "windowPosX", rect.x());
            insertToml(_globalConfig, "windowPosY", rect.y());
            insertToml(_globalConfig, "themeMode", (int)eTheme->getThemeMode());
            insertToml(_globalConfig, "windowDisplayMode", displayButtonGroup->id(displayButtonGroup->checkedButton()));
            insertToml(_globalConfig, "navigationMode", navigationGroup->id(navigationGroup->checkedButton()));
            insertToml(_globalConfig, "autoRefreshAfterTranslate", autoRefreshSwitch->getIsToggled());
            insertToml(_globalConfig, "defaultNameTableOpenMode", nameTableOpenModeGroup->id(nameTableOpenModeGroup->checkedButton()));
            insertToml(_globalConfig, "defaultDictOpenMode", dictOpenModeGroup->id(dictOpenModeGroup->checkedButton()));
            insertToml(_globalConfig, "allowCloseWhenRunning", allowCloseWhenRunningSwitch->getIsToggled());
        };
    

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
    centerLayout->addWidget(allowCloseWhenRunningArea);
    centerLayout->addStretch();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    addCentralWidget(centralWidget, true, true, 0);
}

SettingPage::~SettingPage()
{
}
