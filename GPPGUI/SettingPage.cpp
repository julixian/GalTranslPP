#include "SettingPage.h"

#include <ElaMessageBar.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QButtonGroup>

#include "ElaApplication.h"
#include "ElaComboBox.h"
#include "ElaRadioButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaLineEdit.h"
#include "ElaToggleSwitch.h"
#include "ElaPushButton.h"
#include "ElaWindow.h"

import Tool;
namespace fs = std::filesystem;

SettingPage::SettingPage(toml::ordered_value& globalConfig, QWidget* parent)
    : BasePage(parent), _globalConfig(globalConfig)
{
    // 预览窗口标题
    ElaWindow* window = qobject_cast<ElaWindow*>(parent);
    setWindowTitle("Setting");
    setContentsMargins(30, 15, 15, 0);

    ElaText* themeText = new ElaText(tr("主题设置"), this);
    themeText->setWordWrap(false);
    themeText->setTextPixelSize(18);

    int themeMode = toml::find_or(_globalConfig, "themeMode", 0);
    eTheme->setThemeMode((ElaThemeType::ThemeMode)themeMode);
    ElaComboBox* themeComboBox = new ElaComboBox(this);
    themeComboBox->addItem(tr("日间模式"));
    themeComboBox->addItem(tr("夜间模式"));
    themeComboBox->setCurrentIndex((int)eTheme->getThemeMode());
    ElaScrollPageArea* themeSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* themeSwitchLayout = new QHBoxLayout(themeSwitchArea);
    ElaText* themeSwitchText = new ElaText(tr("主题切换"), this);
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

    // 窗口效果设置
    ElaRadioButton* normalButton = new ElaRadioButton("Normal", this);
    ElaRadioButton* elaMicaButton = new ElaRadioButton("ElaMica", this);

    ElaRadioButton* micaButton = new ElaRadioButton("Mica", this);
    ElaRadioButton* micaAltButton = new ElaRadioButton("Mica-Alt", this);
    ElaRadioButton* acrylicButton = new ElaRadioButton("Acrylic", this);
    ElaRadioButton* dwmBlurnormalButton = new ElaRadioButton("Dwm-Blur", this);

    QButtonGroup* windowModeButtonGroup = new QButtonGroup(this);
    windowModeButtonGroup->addButton(normalButton, 0);
    windowModeButtonGroup->addButton(elaMicaButton, 1);
    windowModeButtonGroup->addButton(micaButton, 2);
    windowModeButtonGroup->addButton(micaAltButton, 3);
    windowModeButtonGroup->addButton(acrylicButton, 4);
    windowModeButtonGroup->addButton(dwmBlurnormalButton, 5);

    int windowDisplayMode = toml::find_or(_globalConfig, "windowDisplayMode", 0); // 不知道为什么3及以上的值会失效
    QAbstractButton* abstractButton = windowModeButtonGroup->button(windowDisplayMode);
    if (abstractButton)
    {
        abstractButton->setChecked(true);
    }
    eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)windowDisplayMode);

    connect(windowModeButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled)
        {
            eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)windowModeButtonGroup->id(button));
        });
    connect(eApp, &ElaApplication::pWindowDisplayModeChanged, this, [=]()
        {
            auto button = windowModeButtonGroup->button(eApp->getWindowDisplayMode());
            if (ElaRadioButton* elaRadioButton = qobject_cast<ElaRadioButton*>(button))
            {
                elaRadioButton->setChecked(true);
            }
        });

    ElaScrollPageArea* windowModeSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* windowModeSwitchLayout = new QHBoxLayout(windowModeSwitchArea);
    ElaText* micaSwitchText = new ElaText(tr("窗口效果"), this);
    micaSwitchText->setWordWrap(false);
    micaSwitchText->setTextPixelSize(15);
    windowModeSwitchLayout->addWidget(micaSwitchText);
    windowModeSwitchLayout->addStretch();
    windowModeSwitchLayout->addWidget(normalButton);
    windowModeSwitchLayout->addWidget(elaMicaButton);
    windowModeSwitchLayout->addWidget(micaButton);
    windowModeSwitchLayout->addWidget(micaAltButton);
    windowModeSwitchLayout->addWidget(acrylicButton);
    windowModeSwitchLayout->addWidget(dwmBlurnormalButton);

    // 导航栏模式设置
    ElaRadioButton* minimumButton = new ElaRadioButton("Minimum", this);
    ElaRadioButton* compactButton = new ElaRadioButton("Compact", this);
    ElaRadioButton* maximumButton = new ElaRadioButton("Maximum", this);
    ElaRadioButton* autoButton = new ElaRadioButton("Auto", this);
    ElaScrollPageArea* guideBarModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* guideBarModeLayout = new QHBoxLayout(guideBarModeArea);
    ElaText* guideBarModeText = new ElaText(tr("导航栏模式选择"), this);
    guideBarModeText->setWordWrap(false);
    guideBarModeText->setTextPixelSize(15);
    guideBarModeLayout->addWidget(guideBarModeText);
    guideBarModeLayout->addStretch();
    guideBarModeLayout->addWidget(minimumButton);
    guideBarModeLayout->addWidget(compactButton);
    guideBarModeLayout->addWidget(maximumButton);
    guideBarModeLayout->addWidget(autoButton);

    QButtonGroup* navigationGroup = new QButtonGroup(this);
    navigationGroup->addButton(autoButton, 0);
    navigationGroup->addButton(minimumButton, 1);
    navigationGroup->addButton(compactButton, 2);
    navigationGroup->addButton(maximumButton, 3);
    int navigationMode = toml::find_or(_globalConfig, "navigationMode", 0);
    abstractButton = navigationGroup->button(navigationMode);
    if (abstractButton) 
    {
        abstractButton->setChecked(true);
    }
    window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationMode);

    connect(navigationGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) 
        {
            window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationGroup->id(button));
        });


    // 页面切换特效
    ElaRadioButton* noneButton = new ElaRadioButton("None", this);
    ElaRadioButton* popupButton = new ElaRadioButton("Popup", this);
    ElaRadioButton* scaleButton = new ElaRadioButton("Scale", this);
    ElaRadioButton* flipButton = new ElaRadioButton("Flip", this);
    ElaRadioButton* blurButton = new ElaRadioButton("Blur", this);
    ElaScrollPageArea* stackSwitchModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* stackSwitchModeLayout = new QHBoxLayout(stackSwitchModeArea);
    ElaText* stackSwitchModeText = new ElaText(tr("页面切换特效"), this);
    stackSwitchModeText->setWordWrap(false);
    stackSwitchModeText->setTextPixelSize(15);
    stackSwitchModeLayout->addWidget(stackSwitchModeText);
    stackSwitchModeLayout->addStretch();
    stackSwitchModeLayout->addWidget(noneButton);
    stackSwitchModeLayout->addWidget(popupButton);
    stackSwitchModeLayout->addWidget(scaleButton);
    stackSwitchModeLayout->addWidget(flipButton);
    stackSwitchModeLayout->addWidget(blurButton);
    QButtonGroup* stackSwitchGroup = new QButtonGroup(this);
    stackSwitchGroup->addButton(noneButton, 0);
    stackSwitchGroup->addButton(popupButton, 1);
    stackSwitchGroup->addButton(scaleButton, 2);
    stackSwitchGroup->addButton(flipButton, 3);
    stackSwitchGroup->addButton(blurButton, 4);
    int stackSwitchMode = toml::find_or(_globalConfig, "stackSwitchMode", 1);
    abstractButton = stackSwitchGroup->button(stackSwitchMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    window->setStackSwitchMode((ElaWindowType::StackSwitchMode)stackSwitchMode);

    connect(stackSwitchGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled)
        {
            window->setStackSwitchMode((ElaWindowType::StackSwitchMode)stackSwitchGroup->id(button));
        });


    ElaText* helperText = new ElaText(tr("应用程序设置"), this);
    helperText->setWordWrap(false);
    helperText->setTextPixelSize(18);

    // 任务完成后自动刷新人名表和字典
    ElaScrollPageArea* autoRefreshArea = new ElaScrollPageArea(this);
    QHBoxLayout* autoRefreshLayout = new QHBoxLayout(autoRefreshArea);
    ElaText* autoRefreshText = new ElaText(tr("(DumpName/NameTrans)/GenDict任务成功后自动刷新人名表/项目GPT字典"), autoRefreshArea);
    autoRefreshText->setWordWrap(false);
    autoRefreshText->setTextPixelSize(15);
    autoRefreshLayout->addWidget(autoRefreshText);
    autoRefreshLayout->addStretch();
    ElaToggleSwitch* autoRefreshSwitch = new ElaToggleSwitch(autoRefreshArea);
    autoRefreshSwitch->setIsToggled(toml::find_or(_globalConfig, "autoRefreshAfterTranslate", true));
    connect(autoRefreshSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked) 
        {
            insertToml(_globalConfig, "autoRefreshAfterTranslate", isChecked);
        });
    autoRefreshLayout->addWidget(autoRefreshSwitch);

    // 默认以纯文本/表模式打开人名表
    ElaScrollPageArea* nameTableOpenModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* nameTableOpenModeLayout = new QHBoxLayout(nameTableOpenModeArea);
    ElaText* nameTableOpenModeText = new ElaText(tr("新项目人名表默认打开模式"), nameTableOpenModeArea);
    nameTableOpenModeText->setWordWrap(false);
    nameTableOpenModeText->setTextPixelSize(15);
    ElaRadioButton* nameTableOpenModeTextButton = new ElaRadioButton(tr("纯文本模式"), nameTableOpenModeArea);
    ElaRadioButton* nameTableOpenModeTableButton = new ElaRadioButton(tr("表格模式"), nameTableOpenModeArea);
    nameTableOpenModeLayout->addWidget(nameTableOpenModeText);
    nameTableOpenModeLayout->addStretch();
    nameTableOpenModeLayout->addWidget(nameTableOpenModeTextButton);
    nameTableOpenModeLayout->addWidget(nameTableOpenModeTableButton);
    int nameTableOpenMode = toml::find_or(_globalConfig, "defaultNameTableOpenMode", 1);
    QButtonGroup* nameTableOpenModeGroup = new QButtonGroup(nameTableOpenModeArea);
    nameTableOpenModeGroup->addButton(nameTableOpenModeTextButton, 0);
    nameTableOpenModeGroup->addButton(nameTableOpenModeTableButton, 1);
    abstractButton = nameTableOpenModeGroup->button(nameTableOpenMode);
    if (abstractButton)
    {
        abstractButton->setChecked(true);
    }
    connect(nameTableOpenModeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled)
        {
            if (isToggled)
            {
                insertToml(_globalConfig, "defaultNameTableOpenMode", nameTableOpenModeGroup->id(button));
            }
        });

    // 默认以纯文本/表模式打开字典
    ElaScrollPageArea* dictOpenModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* dictOpenModeLayout = new QHBoxLayout(dictOpenModeArea);
    ElaText* dictOpenModeText = new ElaText(tr("新项目字典默认打开模式"), dictOpenModeArea);
    dictOpenModeText->setWordWrap(false);
    dictOpenModeText->setTextPixelSize(15);
    ElaRadioButton* dictOpenModeTextButton = new ElaRadioButton(tr("纯文本模式"), dictOpenModeArea);
    ElaRadioButton* dictOpenModeTableButton = new ElaRadioButton(tr("表格模式"), dictOpenModeArea);
    dictOpenModeLayout->addWidget(dictOpenModeText);
    dictOpenModeLayout->addStretch();
    dictOpenModeLayout->addWidget(dictOpenModeTextButton);
    dictOpenModeLayout->addWidget(dictOpenModeTableButton);
    int dictOpenMode = toml::find_or(_globalConfig, "defaultDictOpenMode", 1);
    QButtonGroup* dictOpenModeGroup = new QButtonGroup(dictOpenModeArea);
    dictOpenModeGroup->addButton(dictOpenModeTextButton, 0);
    dictOpenModeGroup->addButton(dictOpenModeTableButton, 1);
    abstractButton = dictOpenModeGroup->button(dictOpenMode);
    if (abstractButton)
    {
        abstractButton->setChecked(true);
    }
    connect(dictOpenModeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled)
        {
            if (isToggled)
            {
                insertToml(_globalConfig, "defaultDictOpenMode", dictOpenModeGroup->id(button));
            }
        });

    // 允许在项目仍在运行的情况下关闭程序(危险)
    ElaScrollPageArea* allowCloseWhenRunningArea = new ElaScrollPageArea(this);
    QHBoxLayout* allowCloseWhenRunningLayout = new QHBoxLayout(allowCloseWhenRunningArea);
    QWidget* allowCloseWhenRunningWidget = new QWidget(allowCloseWhenRunningArea);
    QVBoxLayout* allowCloseWhenRunningWidgetLayout = new QVBoxLayout(allowCloseWhenRunningWidget);
    ElaText* allowCloseWhenRunningText = new ElaText(tr("允许在项目仍在运行的情况下关闭程序"), allowCloseWhenRunningArea);
    allowCloseWhenRunningText->setWordWrap(false);
    allowCloseWhenRunningText->setTextPixelSize(15);
    allowCloseWhenRunningWidgetLayout->addWidget(allowCloseWhenRunningText);
    ElaText* allowCloseWhenRunningWarningText = new ElaText(tr("危险！"), allowCloseWhenRunningArea);
    allowCloseWhenRunningWarningText->setWordWrap(false);
    allowCloseWhenRunningWarningText->setTextPixelSize(10);
    allowCloseWhenRunningWidgetLayout->addWidget(allowCloseWhenRunningWarningText);
    ElaToggleSwitch* allowCloseWhenRunningSwitch = new ElaToggleSwitch(allowCloseWhenRunningArea);
    allowCloseWhenRunningSwitch->setIsToggled(toml::find_or(_globalConfig, "allowCloseWhenRunning", false));
    allowCloseWhenRunningLayout->addWidget(allowCloseWhenRunningWidget);
    allowCloseWhenRunningLayout->addStretch();
    allowCloseWhenRunningLayout->addWidget(allowCloseWhenRunningSwitch);
    connect(allowCloseWhenRunningSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked)
        {
            insertToml(_globalConfig, "allowCloseWhenRunning", isChecked);
        });


    // 自动检查更新
    ElaScrollPageArea* checkUpdateArea = new ElaScrollPageArea(this);
    QHBoxLayout* checkUpdateLayout = new QHBoxLayout(checkUpdateArea);
    ElaText* checkUpdateText = new ElaText(tr("自动检查更新"), checkUpdateArea);
    checkUpdateText->setWordWrap(false);
    checkUpdateText->setTextPixelSize(15);
    ElaToggleSwitch* checkUpdateSwitch = new ElaToggleSwitch(checkUpdateArea);
    checkUpdateSwitch->setIsToggled(toml::find_or(_globalConfig, "autoCheckUpdate", true));
    checkUpdateLayout->addWidget(checkUpdateText);
    checkUpdateLayout->addStretch();
    checkUpdateLayout->addWidget(checkUpdateSwitch);

    // 检测到更新后自动下载
    ElaScrollPageArea* autoDownloadArea = new ElaScrollPageArea(this);
    QHBoxLayout* autoDownloadLayout = new QHBoxLayout(autoDownloadArea);
    ElaText* autoDownloadText = new ElaText(tr("检测到更新后自动下载"), autoDownloadArea);
    autoDownloadText->setWordWrap(false);
    autoDownloadText->setTextPixelSize(15);
    ElaToggleSwitch* autoDownloadSwitch = new ElaToggleSwitch(autoDownloadArea);
    autoDownloadSwitch->setIsToggled(toml::find_or(_globalConfig, "autoDownloadUpdate", true));
    autoDownloadLayout->addWidget(autoDownloadText);
    autoDownloadLayout->addStretch();
    autoDownloadLayout->addWidget(autoDownloadSwitch);
    connect(autoDownloadSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked)
        {
            insertToml(_globalConfig, "autoDownloadUpdate", isChecked);
        });

    
    // 语言设置
    ElaScrollPageArea* languageArea = new ElaScrollPageArea(this);
    QHBoxLayout* languageLayout = new QHBoxLayout(languageArea);
    QWidget* languageWidget = new QWidget(languageArea);
    QVBoxLayout* languageWidgetLayout = new QVBoxLayout(languageWidget);
    ElaText* languageText = new ElaText(tr("语言设置"), languageArea);
    languageWidgetLayout->addWidget(languageText);
    ElaText* languageTipText = new ElaText(tr("重启生效"), languageArea);
    languageTipText->setWordWrap(false);
    languageTipText->setTextPixelSize(10);
    languageWidgetLayout->addWidget(languageTipText);
    languageText->setWordWrap(false);
    languageText->setTextPixelSize(15);
    languageLayout->addWidget(languageWidget);
    languageLayout->addStretch();
    ElaComboBox* languageComboBox = new ElaComboBox(languageArea);
    languageComboBox->addItem("zh_CN");
    languageComboBox->addItem("en");
    if (int languageIndex = languageComboBox->findText(QString::fromStdString(toml::find_or(_globalConfig, "language", "zh_CN"))); languageIndex >= 0) {
        languageComboBox->setCurrentIndex(languageIndex);
    }
    languageLayout->addWidget(languageComboBox);


    // pyEnvPath(重启生效)
    ElaScrollPageArea* pyEnvPathArea = new ElaScrollPageArea(this);
    QHBoxLayout* pyEnvPathLayout = new QHBoxLayout(pyEnvPathArea);
    QWidget* pyEnvPathWidget = new QWidget(pyEnvPathArea);
    QVBoxLayout* pyEnvPathWidgetLayout = new QVBoxLayout(pyEnvPathWidget);
    ElaText* pyEnvPathText = new ElaText(tr("Python环境路径"), pyEnvPathArea);
    pyEnvPathText->setWordWrap(false);
    pyEnvPathText->setTextPixelSize(15);
    pyEnvPathWidgetLayout->addWidget(pyEnvPathText);
    ElaText* pyEnvPathTipText = new ElaText(tr("重启生效"), pyEnvPathArea);
    pyEnvPathTipText->setWordWrap(false);
    pyEnvPathTipText->setTextPixelSize(10);
    pyEnvPathWidgetLayout->addWidget(pyEnvPathTipText);
    pyEnvPathLayout->addWidget(pyEnvPathWidget);
    pyEnvPathLayout->addStretch();
    ElaLineEdit* pyEnvPathLineEdit = new ElaLineEdit(pyEnvPathArea);
    pyEnvPathLineEdit->setFixedWidth(400);
    pyEnvPathLineEdit->setText(QString::fromStdString(toml::find_or(_globalConfig, "pyEnvPath", "BaseConfig/python-3.12.10-embed-amd64")));
    pyEnvPathLayout->addWidget(pyEnvPathLineEdit);
    ElaPushButton* pyEnvPathButton = new ElaPushButton(tr("浏览"), pyEnvPathArea);
    pyEnvPathLayout->addWidget(pyEnvPathButton);
    connect(pyEnvPathButton, &ElaPushButton::clicked, this, [=]()
        {
            QString pyExePath = QFileDialog::getOpenFileName(this, tr("选择Python.exe"), pyEnvPathLineEdit->text(), "Python.exe (python.exe)");
            if (!pyExePath.isEmpty())
            {
                fs::path newPyEnvPath = fs::path(pyExePath.toStdWString()).parent_path();
                if (!fs::exists(newPyEnvPath / L"python312.zip")) {
                    ElaMessageBar::error(ElaMessageBarType::TopRight, tr("错误"), tr("目录下没有 python{ver}.zip 文件"), 3000);
                    return;
                }
                pyEnvPathLineEdit->setText(QString::fromStdWString(newPyEnvPath.wstring()));
            }
        });
    //ElaPushButton* pyEnvRestartButtom = new ElaPushButton(tr("重启环境"), pyEnvPathArea);
    //pyEnvPathLayout->addWidget(pyEnvRestartButtom);
    //connect(pyEnvRestartButtom, &ElaPushButton::clicked, this, [=]()
    //    {
    //        QString pyEnvPathQStr = pyEnvPathLineEdit->text();
    //        Q_EMIT restartPythonEnvSignal(pyEnvPathQStr);
    //    });
    


    _applyFunc = [=]()
        {
            QRect rect = window->frameGeometry();
            insertToml(_globalConfig, "windowWidth", rect.width());
            insertToml(_globalConfig, "windowHeight", rect.height());
            insertToml(_globalConfig, "windowPosX", rect.x());
            insertToml(_globalConfig, "windowPosY", rect.y());
            insertToml(_globalConfig, "themeMode", (int)eTheme->getThemeMode());
            insertToml(_globalConfig, "windowDisplayMode", windowModeButtonGroup->id(windowModeButtonGroup->checkedButton()));
            insertToml(_globalConfig, "navigationMode", navigationGroup->id(navigationGroup->checkedButton()));
            insertToml(_globalConfig, "stackSwitchMode", stackSwitchGroup->id(stackSwitchGroup->checkedButton()));

            insertToml(_globalConfig, "autoRefreshAfterTranslate", autoRefreshSwitch->getIsToggled());
            insertToml(_globalConfig, "defaultNameTableOpenMode", nameTableOpenModeGroup->id(nameTableOpenModeGroup->checkedButton()));
            insertToml(_globalConfig, "defaultDictOpenMode", dictOpenModeGroup->id(dictOpenModeGroup->checkedButton()));
            insertToml(_globalConfig, "allowCloseWhenRunning", allowCloseWhenRunningSwitch->getIsToggled());
            insertToml(_globalConfig, "autoDownloadUpdate", autoDownloadSwitch->getIsToggled());
            insertToml(_globalConfig, "autoCheckUpdate", checkUpdateSwitch->getIsToggled());
            insertToml(_globalConfig, "language", languageComboBox->currentText().toStdString());
            insertToml(_globalConfig, "pyEnvPath", pyEnvPathLineEdit->text().toStdString());
        };
    

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("Setting");
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);
    centerLayout->addWidget(themeText);
    centerLayout->addWidget(themeSwitchArea);
    centerLayout->addWidget(windowModeSwitchArea);
    centerLayout->addWidget(guideBarModeArea);
    centerLayout->addWidget(stackSwitchModeArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(helperText);
    centerLayout->addWidget(autoRefreshArea);
    centerLayout->addWidget(nameTableOpenModeArea);
    centerLayout->addWidget(dictOpenModeArea);
    centerLayout->addWidget(allowCloseWhenRunningArea);
    centerLayout->addWidget(checkUpdateArea);
    centerLayout->addWidget(autoDownloadArea);
    centerLayout->addWidget(languageArea);
    centerLayout->addWidget(pyEnvPathArea);
    centerLayout->addStretch();
    addCentralWidget(centralWidget, true, true, 0);
}

SettingPage::~SettingPage()
{
}
