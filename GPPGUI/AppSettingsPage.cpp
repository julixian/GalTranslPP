#include "AppSettingsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QButtonGroup>

#include "ElaApplication.h"
#include "ElaMessageBar.h"
#include "ElaNoWheelComboBox.h"
#include "ElaDoubleText.h"
#include "ElaRadioButton.h"
#include "ElaScrollPageArea.h"
#include "ElaTheme.h"
#include "ElaLineEdit.h"
#include "ElaToggleSwitch.h"
#include "ElaToolButton.h"
#include "ElaWindow.h"

import Tool;
namespace fs = std::filesystem;

AppSettingsPage::AppSettingsPage(toml::ordered_value& globalConfig, QWidget* parent)
    : BasePage(parent), m_globalConfig(globalConfig)
{
    setWindowTitle(tr("应用设置"));
    setContentsMargins(30, 15, 15, 0);

    setupUi();
}

AppSettingsPage::~AppSettingsPage() = default;

void AppSettingsPage::setupUi()
{
    ElaWindow* elaWindow = qobject_cast<ElaWindow*>(this->window());
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle(tr("应用设置"));
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);

    ElaText* themeText = new ElaText(tr("主题设置"), centralWidget);
    themeText->setWordWrap(false);
    themeText->setTextPixelSize(18);
    centerLayout->addWidget(themeText);

    // 主题切换
    int themeMode = toml::find_or(m_globalConfig, "themeMode", 0);
    eTheme->setThemeMode((ElaThemeType::ThemeMode)themeMode);
    ElaScrollPageArea* themeSwitchArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* themeSwitchLayout = new QHBoxLayout(themeSwitchArea);
    ElaText* themeSwitchText = new ElaText(tr("主题切换"), themeSwitchArea);
    themeSwitchText->setWordWrap(false);
    themeSwitchText->setTextPixelSize(16);
    themeSwitchLayout->addWidget(themeSwitchText);
    themeSwitchLayout->addStretch();
    ElaNoWheelComboBox* themeComboBox = new ElaNoWheelComboBox(themeSwitchArea);
    themeComboBox->addItem(tr("日间模式"));
    themeComboBox->addItem(tr("夜间模式"));
    themeComboBox->setCurrentIndex((int)eTheme->getThemeMode());
    themeSwitchLayout->addWidget(themeComboBox);
    connect(themeComboBox, QOverload<int>::of(&ElaNoWheelComboBox::currentIndexChanged), this, [=](int index)
        {
            if (index == 0) {
                eTheme->setThemeMode(ElaThemeType::Light);
            }
            else {
                eTheme->setThemeMode(ElaThemeType::Dark);
            }
        });
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode_)
        {
            themeComboBox->blockSignals(true);
            if (themeMode_ == ElaThemeType::Light) {
                themeComboBox->setCurrentIndex(0);
            }
            else {
                themeComboBox->setCurrentIndex(1);
            }
            themeComboBox->blockSignals(false);
        });
    centerLayout->addWidget(themeSwitchArea);

    // 窗口效果设置
    ElaScrollPageArea* windowModeSwitchArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* windowModeSwitchLayout = new QHBoxLayout(windowModeSwitchArea);
    ElaText* micaSwitchText = new ElaText(tr("窗口效果"), windowModeSwitchArea);
    micaSwitchText->setWordWrap(false);
    micaSwitchText->setTextPixelSize(16);
    windowModeSwitchLayout->addWidget(micaSwitchText);
    windowModeSwitchLayout->addStretch();
    ElaRadioButton* normalButton = new ElaRadioButton("Normal", windowModeSwitchArea);
    ElaRadioButton* elaMicaButton = new ElaRadioButton("ElaMica", windowModeSwitchArea);
    ElaRadioButton* micaButton = new ElaRadioButton("Mica", windowModeSwitchArea);
    ElaRadioButton* micaAltButton = new ElaRadioButton("Mica-Alt", windowModeSwitchArea);
    ElaRadioButton* acrylicButton = new ElaRadioButton("Acrylic", windowModeSwitchArea);
    ElaRadioButton* dwmBlurnormalButton = new ElaRadioButton("Dwm-Blur", windowModeSwitchArea);
    QButtonGroup* windowModeButtonGroup = new QButtonGroup(windowModeSwitchArea);
    windowModeButtonGroup->addButton(normalButton, 0);
    windowModeButtonGroup->addButton(elaMicaButton, 1);
    windowModeButtonGroup->addButton(micaButton, 2);
    windowModeButtonGroup->addButton(micaAltButton, 3);
    windowModeButtonGroup->addButton(acrylicButton, 4);
    windowModeButtonGroup->addButton(dwmBlurnormalButton, 5);
    int windowDisplayMode = toml::find_or(m_globalConfig, "windowDisplayMode", 1); // 不知道为什么3及以上的值会失效
    QAbstractButton* abstractButton = windowModeButtonGroup->button(windowDisplayMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)windowDisplayMode);
    connect(windowModeButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this,
        [=](QAbstractButton* button, bool isToggled)
        {
            eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)windowModeButtonGroup->id(button));
        });
    connect(eApp, &ElaApplication::pWindowDisplayModeChanged, this, [=]()
        {
            const auto button = windowModeButtonGroup->button(eApp->getWindowDisplayMode());
            if (ElaRadioButton* elaRadioButton = qobject_cast<ElaRadioButton*>(button)) {
                elaRadioButton->setChecked(true);
            }
        });
    windowModeSwitchLayout->addWidget(normalButton);
    windowModeSwitchLayout->addWidget(elaMicaButton);
    windowModeSwitchLayout->addWidget(micaButton);
    windowModeSwitchLayout->addWidget(micaAltButton);
    windowModeSwitchLayout->addWidget(acrylicButton);
    windowModeSwitchLayout->addWidget(dwmBlurnormalButton);
    centerLayout->addWidget(windowModeSwitchArea);

    // 导航栏模式设置
    ElaScrollPageArea* guideBarModeArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* guideBarModeLayout = new QHBoxLayout(guideBarModeArea);
    ElaText* guideBarModeText = new ElaText(tr("导航栏模式选择"), guideBarModeArea);
    guideBarModeText->setWordWrap(false);
    guideBarModeText->setTextPixelSize(16);
    guideBarModeLayout->addWidget(guideBarModeText);
    guideBarModeLayout->addStretch();
    ElaRadioButton* minimumButton = new ElaRadioButton("Minimum", guideBarModeArea);
    ElaRadioButton* compactButton = new ElaRadioButton("Compact", guideBarModeArea);
    ElaRadioButton* maximumButton = new ElaRadioButton("Maximum", guideBarModeArea);
    ElaRadioButton* autoButton = new ElaRadioButton("Auto", guideBarModeArea);
    QButtonGroup* navigationGroup = new QButtonGroup(guideBarModeArea);
    navigationGroup->addButton(autoButton, 0);
    navigationGroup->addButton(minimumButton, 1);
    navigationGroup->addButton(compactButton, 2);
    navigationGroup->addButton(maximumButton, 3);
    int navigationMode = toml::find_or(m_globalConfig, "navigationMode", 0);
    abstractButton = navigationGroup->button(navigationMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    elaWindow->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationMode);
    connect(navigationGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this,
        [=](QAbstractButton* button, bool isToggled)
        {
            elaWindow->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationGroup->id(button));
        });
    guideBarModeLayout->addWidget(minimumButton);
    guideBarModeLayout->addWidget(compactButton);
    guideBarModeLayout->addWidget(maximumButton);
    guideBarModeLayout->addWidget(autoButton);
    centerLayout->addWidget(guideBarModeArea);

    // 页面切换特效
    ElaScrollPageArea* stackSwitchModeArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* stackSwitchModeLayout = new QHBoxLayout(stackSwitchModeArea);
    ElaText* stackSwitchModeText = new ElaText(tr("页面切换特效"), stackSwitchModeArea);
    stackSwitchModeText->setWordWrap(false);
    stackSwitchModeText->setTextPixelSize(16);
    stackSwitchModeLayout->addWidget(stackSwitchModeText);
    stackSwitchModeLayout->addStretch();
    ElaRadioButton* noneButton = new ElaRadioButton("None", stackSwitchModeArea);
    ElaRadioButton* popupButton = new ElaRadioButton("Popup", stackSwitchModeArea);
    ElaRadioButton* scaleButton = new ElaRadioButton("Scale", stackSwitchModeArea);
    ElaRadioButton* flipButton = new ElaRadioButton("Flip", stackSwitchModeArea);
    ElaRadioButton* blurButton = new ElaRadioButton("Blur", stackSwitchModeArea);
    QButtonGroup* stackSwitchGroup = new QButtonGroup(stackSwitchModeArea);
    stackSwitchGroup->addButton(noneButton, 0);
    stackSwitchGroup->addButton(popupButton, 1);
    stackSwitchGroup->addButton(scaleButton, 2);
    stackSwitchGroup->addButton(flipButton, 3);
    stackSwitchGroup->addButton(blurButton, 4);
    int stackSwitchMode = toml::find_or(m_globalConfig, "stackSwitchMode", 1);
    abstractButton = stackSwitchGroup->button(stackSwitchMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    elaWindow->setStackSwitchMode((ElaWindowType::StackSwitchMode)stackSwitchMode);
    connect(stackSwitchGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this,
        [=](QAbstractButton* button, bool isToggled)
        {
            elaWindow->setStackSwitchMode((ElaWindowType::StackSwitchMode)stackSwitchGroup->id(button));
        });
    stackSwitchModeLayout->addWidget(noneButton);
    stackSwitchModeLayout->addWidget(popupButton);
    stackSwitchModeLayout->addWidget(scaleButton);
    stackSwitchModeLayout->addWidget(flipButton);
    stackSwitchModeLayout->addWidget(blurButton);
    centerLayout->addWidget(stackSwitchModeArea);


    ElaText* helperText = new ElaText(tr("GalTransl++ 设置"), centralWidget);
    helperText->setWordWrap(false);
    helperText->setTextPixelSize(18);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(helperText);

    // 任务完成后自动刷新人名表和字典
    ElaScrollPageArea* autoRefreshArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* autoRefreshLayout = new QHBoxLayout(autoRefreshArea);
    ElaText* autoRefreshText = new ElaText(tr("(DumpName/NameTrans)/GenDict任务成功后自动刷新人名表/项目GPT字典"), autoRefreshArea);
    autoRefreshText->setWordWrap(false);
    autoRefreshText->setTextPixelSize(16);
    autoRefreshLayout->addWidget(autoRefreshText);
    autoRefreshLayout->addStretch();
    ElaToggleSwitch* autoRefreshSwitch = new ElaToggleSwitch(autoRefreshArea);
    autoRefreshSwitch->setIsToggled(toml::find_or(m_globalConfig, "autoRefreshAfterTranslate", true));
    connect(autoRefreshSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked)
        {
            insertToml(m_globalConfig, "autoRefreshAfterTranslate", isChecked);
        });
    autoRefreshLayout->addWidget(autoRefreshSwitch);
    centerLayout->addWidget(autoRefreshArea);

    // 默认以纯文本/表模式打开人名表
    ElaScrollPageArea* nameTableOpenModeArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* nameTableOpenModeLayout = new QHBoxLayout(nameTableOpenModeArea);
    ElaText* nameTableOpenModeText = new ElaText(tr("新项目人名表默认打开模式"), nameTableOpenModeArea);
    nameTableOpenModeText->setWordWrap(false);
    nameTableOpenModeText->setTextPixelSize(16);
    nameTableOpenModeLayout->addWidget(nameTableOpenModeText);
    nameTableOpenModeLayout->addStretch();
    ElaRadioButton* nameTableOpenModeTextButton = new ElaRadioButton(tr("纯文本模式"), nameTableOpenModeArea);
    ElaRadioButton* nameTableOpenModeTableButton = new ElaRadioButton(tr("表格模式"), nameTableOpenModeArea);
    QButtonGroup* nameTableOpenModeGroup = new QButtonGroup(nameTableOpenModeArea);
    nameTableOpenModeGroup->addButton(nameTableOpenModeTextButton, 0);
    nameTableOpenModeGroup->addButton(nameTableOpenModeTableButton, 1);
    int nameTableOpenMode = toml::find_or(m_globalConfig, "defaultNameTableOpenMode", 1);
    abstractButton = nameTableOpenModeGroup->button(nameTableOpenMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    connect(nameTableOpenModeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this,
        [=](QAbstractButton* button, bool isToggled)
        {
            if (isToggled) {
                insertToml(m_globalConfig, "defaultNameTableOpenMode", nameTableOpenModeGroup->id(button));
            }
        });
    nameTableOpenModeLayout->addWidget(nameTableOpenModeTextButton);
    nameTableOpenModeLayout->addWidget(nameTableOpenModeTableButton);
    centerLayout->addWidget(nameTableOpenModeArea);

    // 默认以纯文本/表模式打开字典
    ElaScrollPageArea* dictOpenModeArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* dictOpenModeLayout = new QHBoxLayout(dictOpenModeArea);
    ElaText* dictOpenModeText = new ElaText(tr("新项目字典默认打开模式"), dictOpenModeArea);
    dictOpenModeText->setWordWrap(false);
    dictOpenModeText->setTextPixelSize(16);
    dictOpenModeLayout->addWidget(dictOpenModeText);
    dictOpenModeLayout->addStretch();
    ElaRadioButton* dictOpenModeTextButton = new ElaRadioButton(tr("纯文本模式"), dictOpenModeArea);
    ElaRadioButton* dictOpenModeTableButton = new ElaRadioButton(tr("表格模式"), dictOpenModeArea);
    QButtonGroup* dictOpenModeGroup = new QButtonGroup(dictOpenModeArea);
    dictOpenModeGroup->addButton(dictOpenModeTextButton, 0);
    dictOpenModeGroup->addButton(dictOpenModeTableButton, 1);
    int dictOpenMode = toml::find_or(m_globalConfig, "defaultDictOpenMode", 1);
    abstractButton = dictOpenModeGroup->button(dictOpenMode);
    if (abstractButton) {
        abstractButton->setChecked(true);
    }
    connect(dictOpenModeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this,
        [=](QAbstractButton* button, bool isToggled)
        {
            if (isToggled) {
                insertToml(m_globalConfig, "defaultDictOpenMode", dictOpenModeGroup->id(button));
            }
        });
    dictOpenModeLayout->addWidget(dictOpenModeTextButton);
    dictOpenModeLayout->addWidget(dictOpenModeTableButton);
    centerLayout->addWidget(dictOpenModeArea);

    // 允许在项目仍在运行的情况下关闭程序(危险)
    ElaScrollPageArea* allowCloseWhenRunningArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* allowCloseWhenRunningLayout = new QHBoxLayout(allowCloseWhenRunningArea);
    ElaDoubleText* allowCloseWhenRunningText = new ElaDoubleText(tr("允许在项目仍在运行的情况下关闭程序"), 16,
        tr("危险！"), 10, "", allowCloseWhenRunningArea);
    allowCloseWhenRunningLayout->addWidget(allowCloseWhenRunningText);
    allowCloseWhenRunningLayout->addStretch();
    ElaToggleSwitch* allowCloseWhenRunningSwitch = new ElaToggleSwitch(allowCloseWhenRunningArea);
    allowCloseWhenRunningSwitch->setIsToggled(toml::find_or(m_globalConfig, "allowCloseWhenRunning", false));
    allowCloseWhenRunningLayout->addWidget(allowCloseWhenRunningSwitch);
    connect(allowCloseWhenRunningSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked)
        {
            insertToml(m_globalConfig, "allowCloseWhenRunning", isChecked);
        });
    centerLayout->addWidget(allowCloseWhenRunningArea);

    // 允许应用进程多开
    ElaScrollPageArea* allowMultiInstanceArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* allowMultiInstanceLayout = new QHBoxLayout(allowMultiInstanceArea);
    ElaDoubleText* allowMultiInstanceText = new ElaDoubleText(tr("允许应用进程多开"), 16,
        tr("通常建议由一个进程管理多个项目而不是多开进程"), 10, "", allowMultiInstanceArea);
    allowMultiInstanceLayout->addWidget(allowMultiInstanceText);
    allowMultiInstanceLayout->addStretch();
    ElaToggleSwitch* allowMultiInstanceSwitch = new ElaToggleSwitch(allowMultiInstanceArea);
    allowMultiInstanceSwitch->setIsToggled(toml::find_or(m_globalConfig, "allowMultiInstance", false));
    allowMultiInstanceLayout->addWidget(allowMultiInstanceSwitch);
    connect(allowMultiInstanceSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked)
        {
            insertToml(m_globalConfig, "allowMultiInstance", isChecked);
        });
    centerLayout->addWidget(allowMultiInstanceArea);

    // 自动检查更新
    ElaScrollPageArea* checkUpdateArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* checkUpdateLayout = new QHBoxLayout(checkUpdateArea);
    ElaText* checkUpdateText = new ElaText(tr("自动检查更新"), checkUpdateArea);
    checkUpdateText->setWordWrap(false);
    checkUpdateText->setTextPixelSize(16);
    checkUpdateLayout->addWidget(checkUpdateText);
    checkUpdateLayout->addStretch();
    ElaToggleSwitch* checkUpdateSwitch = new ElaToggleSwitch(checkUpdateArea);
    checkUpdateSwitch->setIsToggled(toml::find_or(m_globalConfig, "autoCheckUpdate", true));
    checkUpdateLayout->addWidget(checkUpdateSwitch);
    centerLayout->addWidget(checkUpdateArea);

    // 检测到更新后自动下载
    ElaScrollPageArea* autoDownloadArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* autoDownloadLayout = new QHBoxLayout(autoDownloadArea);
    ElaText* autoDownloadText = new ElaText(tr("检测到更新后自动下载"), autoDownloadArea);
    autoDownloadText->setWordWrap(false);
    autoDownloadText->setTextPixelSize(16);
    autoDownloadLayout->addWidget(autoDownloadText);
    autoDownloadLayout->addStretch();
    ElaToggleSwitch* autoDownloadSwitch = new ElaToggleSwitch(autoDownloadArea);
    autoDownloadSwitch->setIsToggled(toml::find_or(m_globalConfig, "autoDownloadUpdate", true));
    autoDownloadLayout->addWidget(autoDownloadSwitch);
    connect(autoDownloadSwitch, &ElaToggleSwitch::toggled, this, [=](bool isChecked)
        {
            insertToml(m_globalConfig, "autoDownloadUpdate", isChecked);
        });
    centerLayout->addWidget(autoDownloadArea);

    // 语言设置
    ElaScrollPageArea* languageArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* languageLayout = new QHBoxLayout(languageArea);
    ElaDoubleText* languageText = new ElaDoubleText(tr("语言设置"), 16,
        tr("重启生效"), 10, "", languageArea);
    languageLayout->addWidget(languageText);
    languageLayout->addStretch();
    ElaNoWheelComboBox* languageComboBox = new ElaNoWheelComboBox(languageArea);
    languageComboBox->addItem("zh_CN");
    languageComboBox->addItem("en");
    if (int languageIndex = languageComboBox->findText(QString::fromStdString(toml::find_or(m_globalConfig, "language", "zh_CN")));
        languageIndex >= 0)
    {
        languageComboBox->setCurrentIndex(languageIndex);
    }
    languageLayout->addWidget(languageComboBox);
    centerLayout->addWidget(languageArea);

    // pythonEnvPath(重启生效)
    ElaScrollPageArea* pythonEnvPathArea = new ElaScrollPageArea(centralWidget);
    QHBoxLayout* pythonEnvPathLayout = new QHBoxLayout(pythonEnvPathArea);
    ElaDoubleText* pythonEnvPathText = new ElaDoubleText(tr("Python环境路径"), 16,
        tr("重启生效"), 10, "", pythonEnvPathArea);
    pythonEnvPathLayout->addWidget(pythonEnvPathText);
    pythonEnvPathLayout->addStretch();
    ElaLineEdit* pythonEnvPathLineEdit = new ElaLineEdit(pythonEnvPathArea);
    pythonEnvPathLineEdit->setFixedWidth(400);
    pythonEnvPathLineEdit->setText(QString::fromStdString(toml::find_or(m_globalConfig, "pythonEnvPath", "BaseConfig/Python-3.12.10-embed-amd64")));
    pythonEnvPathLayout->addWidget(pythonEnvPathLineEdit);
    ElaToolButton* pythonEnvPathButton = new ElaToolButton(pythonEnvPathArea);
    pythonEnvPathButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    pythonEnvPathButton->setElaIcon(ElaIconType::FolderOpen);
    pythonEnvPathButton->setText(tr("浏览"));
    pythonEnvPathLayout->addWidget(pythonEnvPathButton);
    connect(pythonEnvPathButton, &ElaToolButton::clicked, this, [=]()
        {
            const QString pyExePath = QFileDialog::getOpenFileName(this->window(), tr("选择Python.exe"),
                pythonEnvPathLineEdit->text(), "Python.exe (python.exe)");
            if (!pyExePath.isEmpty()) {
                const fs::path newPythonEnvPath = fs::path(pyExePath.toStdWString()).parent_path();
                if (std::ranges::all_of(std::initializer_list<std::wstring_view>{ L"310", L"311", L"312", L"313", L"314", L"315" },
                    [&](const auto& pyVer)
	                {
                        return !fs::exists(newPythonEnvPath / std::format(L"python{}.zip", pyVer));
	                }))
                {
                    ElaMessageBar::error(ElaMessageBarType::TopRight, tr("错误"),
                        tr("目录下没有 python{ver}.zip 文件"), 3000);
                    return;
                }
                pythonEnvPathLineEdit->setText(QString::fromStdWString(newPythonEnvPath.wstring()));
            }
        });
    centerLayout->addWidget(pythonEnvPathArea);


    m_applyFunc = [=]()
        {
            const QRect rect = elaWindow->frameGeometry();
            insertToml(m_globalConfig, "windowWidth", rect.width());
            insertToml(m_globalConfig, "windowHeight", rect.height());
            insertToml(m_globalConfig, "windowPosX", rect.x());
            insertToml(m_globalConfig, "windowPosY", rect.y());
            insertToml(m_globalConfig, "themeMode", (int)eTheme->getThemeMode());
            insertToml(m_globalConfig, "windowDisplayMode", windowModeButtonGroup->id(windowModeButtonGroup->checkedButton()));
            insertToml(m_globalConfig, "navigationMode", navigationGroup->id(navigationGroup->checkedButton()));
            insertToml(m_globalConfig, "stackSwitchMode", stackSwitchGroup->id(stackSwitchGroup->checkedButton()));

            insertToml(m_globalConfig, "autoRefreshAfterTranslate", autoRefreshSwitch->getIsToggled());
            insertToml(m_globalConfig, "defaultNameTableOpenMode", nameTableOpenModeGroup->id(nameTableOpenModeGroup->checkedButton()));
            insertToml(m_globalConfig, "defaultDictOpenMode", dictOpenModeGroup->id(dictOpenModeGroup->checkedButton()));
            insertToml(m_globalConfig, "allowCloseWhenRunning", allowCloseWhenRunningSwitch->getIsToggled());
            insertToml(m_globalConfig, "allowMultiInstance", allowMultiInstanceSwitch->getIsToggled());
            insertToml(m_globalConfig, "autoDownloadUpdate", autoDownloadSwitch->getIsToggled());
            insertToml(m_globalConfig, "autoCheckUpdate", checkUpdateSwitch->getIsToggled());
            insertToml(m_globalConfig, "language", languageComboBox->currentText().toStdString());
            insertToml(m_globalConfig, "pythonEnvPath", pythonEnvPathLineEdit->text().toStdString());
        };

    centerLayout->addStretch();
    addCentralWidget(centralWidget, true, true, 0);
}
