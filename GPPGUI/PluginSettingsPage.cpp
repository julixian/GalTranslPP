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

using namespace gpp;

PluginSettingsPage::PluginSettingsPage(fs::path& projectDir, tomlkit::Item& projectConfig, QWidget* parent) :
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
    tomlkit::Item customPlugins = tomlkit::findArrayOrEmpty(m_projectConfig, "plugins", "customTextPlugins");
    // 先处理项目已经启用的插件
    const auto pluginsArr = tomlkit::findArrayOrEmpty(m_projectConfig, "plugins", "textPlugins");
    for (const auto& pluginNameStr : pluginsArr) {
        if (!pluginNameStr.isString()) {
            continue;
        }
        QString logicalPluginName = QString::fromStdString(pluginNameStr.asString());
        bool isEnabled = true;
        if (logicalPluginName.startsWith('>')) {
            logicalPluginName = logicalPluginName.mid(1);
            isEnabled = false;
        }
        QString pluginName = logicalPluginName.contains(':') ? logicalPluginName.split(':').last() : logicalPluginName;
        if (!pluginNamesMap.contains(pluginName)) {
            // TODO: 未来大版本更新中移除从 textPlugins 读取自定义插件的兼容逻辑
            customPlugins.setMultilineArray(true);
            customPlugins.push_back(pluginNameStr.asString());
            continue;
        }
        PluginRunTime runTime = choosePluginRunTime(logicalPluginName.toLower().toStdString(), pluginNamesMap[pluginName]);
        QString runTimeQStr = QString::fromUtf8(pluginRunTime2Names[runTime]);
        PluginItemWidget* item = new PluginItemWidget(pluginName, runTimeQStr, this);
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
        QString runTimeQStr = QString::fromUtf8(pluginRunTime2Names[pluginNamesMap[name]]);
        PluginItemWidget* item = new PluginItemWidget(name, runTimeQStr, this);
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
        [=, this](const QString& title, const std::string& configKey, tomlkit::Item customPlugins_) -> std::function<void()>
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
            tomlkit::Item customPluginsDoc = tomlkit::Item::document();
            customPluginsDoc[configKey] = std::move(customPlugins_);
            customPluginsEdit->setPlainText(QString::fromStdString(tomlkit::format(customPluginsDoc)));
            customPluginsEdit->moveCursor(QTextCursor::Start);
            installTreeSitterHighlighter(customPluginsEdit->document(), SyntaxLanguage::Toml);
            mainLayout->addWidget(customPluginsEdit);

            connect(browserButton, &ElaToolButton::clicked, this, [=, this]()
                {
                    toml newCustomPluginsTbl;
                    try {
                        newCustomPluginsTbl = tomlkit::parse(customPluginsEdit->toPlainText().toStdString());
                    }
                    catch (...) {
                        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析错误"),
                            tr("自定义文本处理插件不符合 toml 规范"), 3000);
                        return;
                    }
                    auto& newCustomPluginsArr = newCustomPluginsTbl[configKey];
                    if (!newCustomPluginsArr.isArray()) {
                        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析错误"),
                            tr("自定义文本处理插件不符合 toml 规范"), 3000);
                        return;
                    }

                    QString fileName = QFileDialog::getOpenFileName(window(), tr("选择自定义文本处理插件"),
                        QString::fromStdWString(m_projectDir.wstring()), "custom script (*.lua *.py)");
                    if (!fileName.isEmpty()) {
                        newCustomPluginsArr.setMultilineArray(true);
                        newCustomPluginsArr.push_back(fileName.toStdString());
                        customPluginsEdit->setPlainText(QString::fromStdString(tomlkit::format(newCustomPluginsTbl)));
                    }
                });

            std::function<void()> saveFunc = [=, this]()
                {
                    try {
                        toml newCustomPluginsTbl = tomlkit::parse(customPluginsEdit->toPlainText().toStdString());
                        auto& newCustomPluginsArr = newCustomPluginsTbl[configKey];
                        if (newCustomPluginsArr.isArray()) {
                            insertToml(m_projectConfig, "plugins." + configKey, newCustomPluginsArr);
                        }
                        else {
                            insertToml(m_projectConfig, "plugins." + configKey, tomlkit::Item::array());
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
    auto saveCustomPluginsFunc = createCustomPluginsPlainTextEditFunc(tr("自定义文本处理插件"), "customTextPlugins", std::move(customPlugins));
    mainLayout->addStretch();

    addCentralWidget(mainWidget, true, false, 0);

    // 这里的顺序和 onItemSettings 中的 navigation 索引对应
    m_tf2hCfgPage = new TF2HCfgPage(m_projectConfig, this);
    addCentralWidget(m_tf2hCfgPage, true, false, 0);;
    m_tlfCfgPage = new TLFCfgPage(m_projectConfig, this);
    addCentralWidget(m_tlfCfgPage, true, false, 0);
    m_skipTransCfgPage = new SkipTransCfgPage(m_projectConfig, this);
    addCentralWidget(m_skipTransCfgPage, true, false, 0);



    m_applyFunc = [=, this]()
        {
            m_skipTransCfgPage->apply2Config();
            m_tlfCfgPage->apply2Config();
            m_tf2hCfgPage->apply2Config();

            tomlkit::Item pluginsToSave = tomlkit::Item::array();
            pluginsToSave.setMultilineArray(true);
            for (PluginItemWidget* item : m_pluginItems) {
                std::string pluginNameToSave = item->getPluginName().toStdString();
                if (!item->getIsToggled()) {
                    pluginNameToSave = ">" + pluginNameToSave;
                }
                pluginsToSave.push_back(pluginNameToSave);
            }
            insertToml(m_projectConfig, "plugins.textPlugins", pluginsToSave);
            saveCustomPluginsFunc();
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
