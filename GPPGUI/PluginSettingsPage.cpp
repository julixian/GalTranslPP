// PluginSettingsPage.cpp

#include "PluginSettingsPage.h"
#include "PluginItemWidget.h" // 引入自定义控件

#include <QVBoxLayout>
#include <QDesktopServices>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "ElaToolTip.h"

#include "TF2HCfgPage.h"
#include "TLFCfgPage.h"
#include "SkipTransCfgPage.h"

import Tool;

PluginSettingsPage::PluginSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent) :
    BasePage(parent), _projectConfig(projectConfig), _projectDir(projectDir), _mainWindow(mainWindow)
{
    setWindowTitle(tr("插件设置"));
    setTitleVisible(false);

    _setupUI();
}

PluginSettingsPage::~PluginSettingsPage()
{
}

void PluginSettingsPage::apply2Config()
{
    if (_applyFunc) {
        _applyFunc();
    }
}

void PluginSettingsPage::_setupUI()
{
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(20, 15, 15, 0);

    // 文本插件列表
    ElaText* pluginArrayTitle = new ElaText(mainWidget);
    pluginArrayTitle->setText(tr("文本插件设置"));
    pluginArrayTitle->setTextPixelSize(18);
    mainLayout->addWidget(pluginArrayTitle);

    // 创建一个容器用于放置列表
    QWidget* listContainer = new QWidget(mainWidget);
    _pluginListLayout = new QVBoxLayout(listContainer);
    _pluginListLayout->setContentsMargins(0, 0, 0, 0);

    // 插件名称列表
    QMap<QString, PluginRunTime> pluginNamesMap =
    {
        { "SkipTrans", PluginRunTime::Pre },
        { "TextFull2Half", PluginRunTime::DPost },
        { "TextLinebreakFix", PluginRunTime::DPost },
    };
    toml::ordered_array customPlugins;
    // 先处理项目已经启用的插件
    const auto pluginsArr = toml::find_or_default<toml::ordered_array>(_projectConfig, "plugins", "textPlugins");
    for (const auto& pluginNameStr : pluginsArr) {
        if (!pluginNameStr.is_string()) {
            continue;
        }
        QString pluginName = QString::fromStdString(pluginNameStr.as_string());
        bool isEnabled = true;
        if (pluginName.startsWith('>')) {
            pluginName = pluginName.mid(1);
            isEnabled = false;
        }
        QString pluginNormalName = pluginName.contains(':') ? pluginName.split(':').last() : pluginName;
        if (!pluginNamesMap.contains(pluginNormalName)) {
            customPlugins.push_back(pluginNameStr);
            continue;
        }
        PluginRunTime runTime = choosePluginRunTime(pluginName.toLower().toStdString(), pluginNamesMap[pluginNormalName]);
        QString runTimeStr = QString::fromStdString(pluginRunTimeNames[runTime]);
        PluginItemWidget* item = new PluginItemWidget(pluginNormalName, runTimeStr, this);
        item->setIsToggled(isEnabled);
        _pluginItems.append(item);
        _pluginListLayout->addWidget(item);
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onItemMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onItemMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onItemSettings);
        // 防止重复添加
        pluginNamesMap.erase(pluginNamesMap.find(pluginNormalName));
    }

    // 遍历剩下的名称列表，创建并添加 PluginItemWidget
    for (const QString& name : pluginNamesMap.keys())
    {
        QString runTimeStr = QString::fromStdString(pluginRunTimeNames[pluginNamesMap[name]]);
        PluginItemWidget* item = new PluginItemWidget(name, runTimeStr, this);
        _pluginItems.append(item); // 添加到列表中
        _pluginListLayout->addWidget(item); // 添加到布局中
        // 连接信号
        connect(item, &PluginItemWidget::moveUpRequested, this, &PluginSettingsPage::_onItemMoveUp);
        connect(item, &PluginItemWidget::moveDownRequested, this, &PluginSettingsPage::_onItemMoveDown);
        connect(item, &PluginItemWidget::settingsRequested, this, &PluginSettingsPage::_onItemSettings);
    }
    mainLayout->addWidget(listContainer);

    // 初始化按钮状态
    _updateMoveButtonStates();

    auto createCustomPluginsPlainTextEditFunc = 
        [=](const QString& title, const std::string& configKey, const toml::ordered_array& customPlugins_) -> std::function<void(toml::ordered_array&)>
        {
            QHBoxLayout* customPluginsLayout = new QHBoxLayout(mainWidget);
            customPluginsLayout->setContentsMargins(0, 0, 0, 0);
            ElaText* customPluginsTitle = new ElaText(title, 18, mainWidget);
            customPluginsTitle->setWordWrap(false);
            ElaToolTip* customPluginsTip = new ElaToolTip(customPluginsTitle);
            customPluginsTip->setToolTip("用来加载自定义的 Lua/Python 插件");
            customPluginsLayout->addWidget(customPluginsTitle);
            customPluginsLayout->addStretch();
            ElaPushButton* browserButton = new ElaPushButton(tr("浏览"), mainWidget);
            customPluginsLayout->addWidget(browserButton);

            mainLayout->addLayout(customPluginsLayout);


            ElaPlainTextEdit* customPluginsEdit = new ElaPlainTextEdit(mainWidget);
            //customPluginsEdit->setMaximumHeight(100);
            QFont font = customPluginsEdit->font();
            font.setPixelSize(14);
            customPluginsEdit->setFont(font);
            customPluginsEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ configKey, customPlugins_ }} })));
            customPluginsEdit->moveCursor(QTextCursor::Start);
            mainLayout->addWidget(customPluginsEdit);

            connect(browserButton, &ElaPushButton::clicked, this, [=]()
                {
                    toml::ordered_value newCustomPluginsTbl = toml::parse_str<toml::ordered_type_config>(customPluginsEdit->toPlainText().toStdString());
                    auto& newCustomPluginsArr = newCustomPluginsTbl[configKey];
                    if (!newCustomPluginsArr.is_array()) {
                        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析错误"), 
                            tr("自定义文本处理插件不符合 toml 规范"), 3000);
                        return;
                    }

                    QString fileName = QFileDialog::getOpenFileName(this, tr("选择自定义文本处理插件"),
                        QString::fromStdWString(_projectDir.wstring()), "custom script (*.lua *.py)");
                    if (!fileName.isEmpty()) {
                        newCustomPluginsArr.push_back(fileName.toStdString());
                        customPluginsEdit->setPlainText(QString::fromStdString(toml::format(newCustomPluginsTbl)));
                    }
                });

            std::function<void(toml::ordered_array&)> saveFunc = [=](toml::ordered_array& arr)
                {
                    try {
                        toml::ordered_value newCustomPluginsTbl = toml::parse_str<toml::ordered_type_config>(customPluginsEdit->toPlainText().toStdString());
                        auto& newCustomPluginsArr = newCustomPluginsTbl[configKey];
                        if (newCustomPluginsArr.is_array()) {
                            for (const auto& newCustomPlugin : newCustomPluginsArr.as_array() 
                                | std::views::filter([](const auto& plugin) { return plugin.is_string(); })) 
                            {
                                arr.push_back(newCustomPlugin);
                            }
                        }
                    }
                    catch (...) {
                        ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析错误"), 
                            QString::fromStdString(configKey) + tr(" 不符合 toml 规范"), 3000);
                    }
                };
            return saveFunc;
        };

    mainLayout->addSpacing(10);
    auto saveCustomPluginsFunc = createCustomPluginsPlainTextEditFunc(tr("自定义文本处理插件"), "customTextPlugins", customPlugins);
    mainLayout->addStretch();

    addCentralWidget(mainWidget, true, false, 0);

    // 这里的顺序和 _onItemSettings 中的 navigation 索引对应
    _tf2hCfgPage = new TF2HCfgPage(_projectConfig, this);
    addCentralWidget(_tf2hCfgPage, true, false, 0);;
    _tlfCfgPage = new TLFCfgPage(_projectConfig, this);
    addCentralWidget(_tlfCfgPage, true, false, 0);
    _skipTransCfgPage = new SkipTransCfgPage(_projectConfig, this);
    addCentralWidget(_skipTransCfgPage, true, false, 0);



    _applyFunc = [=]()
        {
            _skipTransCfgPage->apply2Config();
            _tlfCfgPage->apply2Config();
            _tf2hCfgPage->apply2Config();

            toml::ordered_array pluginsToSave;
            for (PluginItemWidget* item : _pluginItems) {
                std::string pluginNameToSave = item->getPluginName().toStdString();
                if (!item->getIsToggled()) {
                    pluginNameToSave = ">" + pluginNameToSave;
                }
                pluginsToSave.push_back(pluginNameToSave);
            }
            saveCustomPluginsFunc(pluginsToSave);
            insertToml(_projectConfig, "plugins.textPlugins", pluginsToSave);
        };
}

void PluginSettingsPage::_onItemSettings(PluginItemWidget* item)
{
    if (!item) {
        return;
    }
    QString pluginName = item->getPluginName();

    if (pluginName.contains("TextFull2Half")) {
        this->navigation(1);
    }
    else if (pluginName.contains("TextLinebreakFix")) {
        this->navigation(2);
    }
    else if (pluginName.contains("SkipTrans")) {
        this->navigation(3);
    }
}

// 下面不用看，没什么用

void PluginSettingsPage::_onItemMoveUp(PluginItemWidget* item)
{
    int index = _pluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        _pluginListLayout->removeWidget(item);
        _pluginItems.removeOne(item);

        // 插入到新位置
        _pluginListLayout->insertWidget(index - 1, item);
        _pluginItems.insert(index - 1, item);

        _updateMoveButtonStates();
    }
}

void PluginSettingsPage::_onItemMoveDown(PluginItemWidget* item)
{
    int index = _pluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < _pluginItems.count() - 1)
    {
        _pluginListLayout->removeWidget(item);
        _pluginItems.removeOne(item);

        _pluginListLayout->insertWidget(index + 1, item);
        _pluginItems.insert(index + 1, item);

        _updateMoveButtonStates();
    }
}

void PluginSettingsPage::_updateMoveButtonStates()
{
    for (int i = 0; i < _pluginItems.count(); ++i)
    {
        PluginItemWidget* item = _pluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < _pluginItems.count() - 1);
    }
}
