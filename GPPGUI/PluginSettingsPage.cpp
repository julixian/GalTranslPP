#include "PluginSettingsPage.h"
#include "PluginItemWidget.h" // 引入自定义控件

#include <QVBoxLayout>
#include <QDesktopServices>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaToolButton.h"
#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "ElaToolTip.h"

#include "TF2HCfgPage.h"
#include "TLFCfgPage.h"
#include "SkipTransCfgPage.h"
#include "TreeSitterHighlighter.h"

import Tool;

PluginSettingsPage::PluginSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent) :
    BasePage(parent), m_projectDir(projectDir), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("插件设置"));
    setTitleVisible(false);

    setupUi();
}


void PluginSettingsPage::setupUi()
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
    m_pluginListLayout = new QVBoxLayout(listContainer);
    m_pluginListLayout->setContentsMargins(0, 0, 0, 0);

    // 插件名称列表
    QMap<QString, PluginRunTime> pluginNamesMap =
    {
        { "SkipTrans", PluginRunTime::Pre },
        { "TextFull2Half", PluginRunTime::DPost },
        { "TextLinebreakFix", PluginRunTime::DPost },
    };
    toml::ordered_array customPlugins;
    // 先处理项目已经启用的插件
    const auto pluginsArr = toml::find_or_default<toml::array>(m_projectConfig, "plugins", "textPlugins");
    for (const auto& pluginNameStr : pluginsArr) {
        if (!pluginNameStr.is_string()) {
            continue;
        }
        QString logicalPluginName = QString::fromStdString(pluginNameStr.as_string());
        bool isEnabled = true;
        if (logicalPluginName.startsWith('>')) {
            logicalPluginName = logicalPluginName.mid(1);
            isEnabled = false;
        }
        QString pluginName = logicalPluginName.contains(':') ? logicalPluginName.split(':').last() : logicalPluginName;
        if (!pluginNamesMap.contains(pluginName)) {
            customPlugins.push_back(pluginNameStr);
            continue;
        }
        PluginRunTime runTime = choosePluginRunTime(logicalPluginName.toLower().toStdString(), pluginNamesMap[pluginName]);
        QString runTimeStr = QString::fromStdString(pluginRunTimeNames[runTime]);
        PluginItemWidget* item = new PluginItemWidget(pluginName, runTimeStr, this);
        item->setIsToggled(isEnabled);
        m_pluginItems.append(item);
        m_pluginListLayout->addWidget(item);
        connect(item, &PluginItemWidget::moveUpRequestedSignal, this, &PluginSettingsPage::onItemMoveUp);
        connect(item, &PluginItemWidget::moveDownRequestedSignal, this, &PluginSettingsPage::onItemMoveDown);
        connect(item, &PluginItemWidget::settingsRequestedSignal, this, &PluginSettingsPage::onItemSettings);
        // 防止重复添加
        pluginNamesMap.erase(pluginNamesMap.find(pluginName));
    }

    // 遍历剩下的名称列表，创建并添加 PluginItemWidget
    for (const QString& name : pluginNamesMap.keys())
    {
        QString runTimeStr = QString::fromStdString(pluginRunTimeNames[pluginNamesMap[name]]);
        PluginItemWidget* item = new PluginItemWidget(name, runTimeStr, this);
        m_pluginItems.append(item); // 添加到列表中
        m_pluginListLayout->addWidget(item); // 添加到布局中
        // 连接信号
        connect(item, &PluginItemWidget::moveUpRequestedSignal, this, &PluginSettingsPage::onItemMoveUp);
        connect(item, &PluginItemWidget::moveDownRequestedSignal, this, &PluginSettingsPage::onItemMoveDown);
        connect(item, &PluginItemWidget::settingsRequestedSignal, this, &PluginSettingsPage::onItemSettings);
    }
    mainLayout->addWidget(listContainer);

    // 初始化按钮状态
    updateMoveButtonStates();

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
            ElaToolButton* browserButton = new ElaToolButton(mainWidget);
            browserButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            browserButton->setElaIcon(ElaIconType::FolderOpen);
            browserButton->setText(tr("浏览"));
            customPluginsLayout->addWidget(browserButton);

            mainLayout->addLayout(customPluginsLayout);


            ElaPlainTextEdit* customPluginsEdit = new ElaPlainTextEdit(mainWidget);
            //customPluginsEdit->setMaximumHeight(100);
            QFont font = customPluginsEdit->font();
            font.setPixelSize(14);
            customPluginsEdit->setFont(font);
            customPluginsEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ configKey, customPlugins_ }} })));
            customPluginsEdit->moveCursor(QTextCursor::Start);
            installTreeSitterHighlighter(customPluginsEdit->document(), SyntaxLanguage::Toml);
            mainLayout->addWidget(customPluginsEdit);

            connect(browserButton, &ElaToolButton::clicked, this, [=]()
                {
                    toml::ordered_value newCustomPluginsTbl = toml::parse_str<toml::ordered_type_config>(customPluginsEdit->toPlainText().toStdString());
                    auto& newCustomPluginsArr = newCustomPluginsTbl[configKey];
                    if (!newCustomPluginsArr.is_array()) {
                        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析错误"),
                            tr("自定义文本处理插件不符合 toml 规范"), 3000);
                        return;
                    }

                    QString fileName = QFileDialog::getOpenFileName(window(), tr("选择自定义文本处理插件"),
                        QString::fromStdWString(m_projectDir.wstring()), "custom script (*.lua *.py)");
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
                            tr("%1 不符合 toml 规范").arg(QString::fromStdString(configKey)), 3000);
                    }
                };
            return saveFunc;
        };

    mainLayout->addSpacing(10);
    auto saveCustomPluginsFunc = createCustomPluginsPlainTextEditFunc(tr("自定义文本处理插件"), "customTextPlugins", customPlugins);
    mainLayout->addStretch();

    addCentralWidget(mainWidget, true, false, 0);

    // 这里的顺序和 onItemSettings 中的 navigation 索引对应
    m_tf2hCfgPage = new TF2HCfgPage(m_projectConfig, this);
    addCentralWidget(m_tf2hCfgPage, true, false, 0);;
    m_tlfCfgPage = new TLFCfgPage(m_projectConfig, this);
    addCentralWidget(m_tlfCfgPage, true, false, 0);
    m_skipTransCfgPage = new SkipTransCfgPage(m_projectConfig, this);
    addCentralWidget(m_skipTransCfgPage, true, false, 0);



    m_applyFunc = [=]()
        {
            m_skipTransCfgPage->apply2Config();
            m_tlfCfgPage->apply2Config();
            m_tf2hCfgPage->apply2Config();

            toml::ordered_array pluginsToSave;
            for (PluginItemWidget* item : m_pluginItems) {
                std::string pluginNameToSave = item->getPluginName().toStdString();
                if (!item->getIsToggled()) {
                    pluginNameToSave = ">" + pluginNameToSave;
                }
                pluginsToSave.push_back(pluginNameToSave);
            }
            saveCustomPluginsFunc(pluginsToSave);
            insertToml(m_projectConfig, "plugins.textPlugins", pluginsToSave);
        };
}

void PluginSettingsPage::onItemSettings(PluginItemWidget* item)
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

void PluginSettingsPage::onItemMoveUp(PluginItemWidget* item)
{
    int index = m_pluginListLayout->indexOf(item);
    if (index > 0) // 确保不是第一个
    {
        // 从布局和列表中移除
        m_pluginListLayout->removeWidget(item);
        m_pluginItems.removeOne(item);

        // 插入到新位置
        m_pluginListLayout->insertWidget(index - 1, item);
        m_pluginItems.insert(index - 1, item);

        updateMoveButtonStates();
    }
}

void PluginSettingsPage::onItemMoveDown(PluginItemWidget* item)
{
    int index = m_pluginListLayout->indexOf(item);
    // 确保不是最后一个有效的item
    if (index < m_pluginItems.count() - 1)
    {
        m_pluginListLayout->removeWidget(item);
        m_pluginItems.removeOne(item);

        m_pluginListLayout->insertWidget(index + 1, item);
        m_pluginItems.insert(index + 1, item);

        updateMoveButtonStates();
    }
}

void PluginSettingsPage::updateMoveButtonStates()
{
    for (int i = 0; i < m_pluginItems.count(); ++i)
    {
        PluginItemWidget* item = m_pluginItems.at(i);
        // 第一个不能上移
        item->setMoveUpButtonEnabled(i > 0);
        // 最后一个不能下移
        item->setMoveDownButtonEnabled(i < m_pluginItems.count() - 1);
    }
}
