#include "ApiSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QEvent>
#include <QPointer>

#include "GppApiCard.h"
#include "GppFlowList.h"
#include "ElaContentDialog.h"
#include "ElaMenu.h"

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPlainTextEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaToolButton.h"
#include "GppRadioButton.h"
#include "ElaIconButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "GppDoubleText.h"
#include "ElaCheckBox.h"
#include "GppNoWheelComboBox.h"
#include "ElaTabWidget.h"
#include "ElaScrollArea.h"
#include "TreeSitterHighlighter.h"
#include "ElaScrollBar.h"
#include "ElaMessageBar.h"
#include "ElaWidget.h"
#include "ElaDialog.h"
#include "ValueSliderWidget.h"

import Tool;
import ApiTool;

using namespace gpp;

QSize ApiSettingsPage::s_configWidgetSize(980, 820);

ApiSettingsPage::ApiSettingsPage(tomlkit::Item& globalConfig, tomlkit::Item& projectConfig, QWidget* parent)
    : BasePage(parent), m_globalConfig(globalConfig), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("Api 设置"));
    setTitleVisible(false);

    setupUi();
}

ApiSettingsPage::~ApiSettingsPage()
{
    for (const ApiRowControls& apiRow : m_apiRows) {
        delete apiRow.configWidget;
    }
}

bool ApiSettingsPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Resize && watched->property("apiConfigWidget").toBool()) {
        if (QWidget* configWidget = qobject_cast<QWidget*>(watched)) {
            s_configWidgetSize = configWidget->size();
        }
    }
    return BasePage::eventFilter(watched, event);
}

void ApiSettingsPage::setupUi()
{
    QWidget* centerWidget = new QWidget(this);
    centerWidget->setWindowTitle(tr("Api 设置"));
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);
    mainLayout->setContentsMargins(20, 15, 15, 0);
    mainLayout->setSpacing(8);

    QWidget* toolbarWidget = new QWidget(centerWidget);
    QHBoxLayout* toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(mainLayout->spacing());
    ElaToolButton* addApiButton = new ElaToolButton(centerWidget);
    addApiButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addApiButton->setElaIcon(ElaIconType::Plus);
    addApiButton->setText(tr("增加新 Api"));
    addApiButton->setFixedWidth(120);
    connect(addApiButton, &ElaToolButton::clicked, this, &ApiSettingsPage::addApiInputRow);
    toolbar->addWidget(addApiButton);
    toolbar->addStretch();
    m_layoutMode = tomlkit::findOr(m_projectConfig, "GUIConfig", "apiLayoutMode",
        tomlkit::findOr(m_globalConfig, "defaultApiLayoutMode", 0)) == 1 ? 1 : 0;
    m_flowModeButton = new ElaToolButton(centerWidget);
    m_flowModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_flowModeButton->setElaIcon(ElaIconType::Grid2);
    m_flowModeButton->setText(tr("流式布局"));
    m_listModeButton = new ElaToolButton(centerWidget);
    m_listModeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_listModeButton->setElaIcon(ElaIconType::List);
    m_listModeButton->setText(tr("列表布局"));
    toolbar->addWidget(m_flowModeButton);
    toolbar->addWidget(m_listModeButton);
    connect(m_flowModeButton, &ElaToolButton::clicked, this, [this]()
        {
            switchLayout(0);
        });
    connect(m_listModeButton, &ElaToolButton::clicked, this, [this]()
        {
            switchLayout(1);
        });
    mainLayout->addWidget(toolbarWidget);

    m_flowList = new GppFlowList(centerWidget);
    mainLayout->addWidget(m_flowList);
    m_listWidget = new QWidget(centerWidget);
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(5);
    mainLayout->addWidget(m_listWidget);
    connect(m_flowList, &GppFlowList::cardMoved, this, [this](int from, int to)
        {
            m_apiRows.move(from, to);
            refreshApiLayout();
        });
    const auto apis = tomlkit::findArrayOrEmpty(m_projectConfig, "backend", "apis");
    for (const auto& api : apis) {
        if (api.isTable()) {
            createApiInputRowWidget(api);
        }
    }
    if (m_apiRows.empty()) {
        createApiInputRowWidget();
    }
    refreshApiLayout();

    // Api 使用策略
    const std::string strategy = tomlkit::findOr(m_projectConfig, "backend", "apiStrategy", "random");
    bool isRandom = strategy == "random";
    ElaScrollPageArea* apiStrategyArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* apiStrategyLayout = new QHBoxLayout(apiStrategyArea);
    apiStrategyLayout->setSpacing(8);
    GppDoubleText* apiStrategyTitle = new GppDoubleText(tr("Api 使用策略"), 16,
        tr("令牌策略，random随机轮询，fallback优先第一个，出现非额度/频率错误时使用下一个"), 10,
        "", apiStrategyArea);
    apiStrategyLayout->addWidget(apiStrategyTitle);
    apiStrategyLayout->addStretch();

    ElaRadioButton* apiStrategyRandom = new GppRadioButton("random", apiStrategyArea);
    ElaRadioButton* apiStrategyFallback = new GppRadioButton("fallback", apiStrategyArea);
    apiStrategyRandom->setChecked(isRandom);
    apiStrategyFallback->setChecked(!isRandom);
    apiStrategyLayout->addWidget(apiStrategyRandom);
    apiStrategyLayout->addWidget(apiStrategyFallback);

    QButtonGroup* apiStrategyGroup = new QButtonGroup(this);
    apiStrategyGroup->addButton(apiStrategyRandom, 0);
    apiStrategyGroup->addButton(apiStrategyFallback, 1);

    // Api 超时时间
    int timeout = tomlkit::findOr(m_projectConfig, "backend", "apiTimeout", 300);
    ElaScrollPageArea* apiTimeoutArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* apiTimeoutLayout = new QHBoxLayout(apiTimeoutArea);
    apiTimeoutLayout->setSpacing(8);
    GppDoubleText* apiTimeoutTitle = new GppDoubleText(tr("Api 超时时间"), 16,
        tr("Api 请求超时时间，单位为秒"), 10, "", apiTimeoutArea);
    apiTimeoutLayout->addWidget(apiTimeoutTitle);
    apiTimeoutLayout->addStretch();

    ElaSpinBox* apiTimeoutSpinBox = new ElaSpinBox(apiTimeoutArea);
    apiTimeoutSpinBox->setRange(1, 9999);
    apiTimeoutSpinBox->setValue(timeout);
    apiTimeoutLayout->addWidget(apiTimeoutSpinBox);

    m_applyFunc = [=, this]()
        {
            tomlkit::Item apiArray = tomlkit::Item::arrayOfTables();
            if (!serializeApis(apiArray)) {
                return;
            }
            if (apiArray.empty()) {
                apiArray = tomlkit::Item::array();
            }
            apiStrategyGroup->button(0)->isChecked() ? insertToml(m_projectConfig, "backend.apiStrategy", "random")
                : insertToml(m_projectConfig, "backend.apiStrategy", "fallback");
            insertToml(m_projectConfig, "backend.apiTimeout", apiTimeoutSpinBox->value());
            insertToml(m_projectConfig, "GUIConfig.apiLayoutMode", m_layoutMode);
            insertToml(m_projectConfig, "backend.apis", apiArray);
        };

    mainLayout->addWidget(apiStrategyArea);
    mainLayout->addWidget(apiTimeoutArea);
    mainLayout->addStretch();
    addCentralWidget(centerWidget, true, false, 0);
}

void ApiSettingsPage::addApiInputRow()
{
    createApiInputRowWidget();
    if (m_layoutMode == 1) {
        m_apiRows.move(m_apiRows.size() - 1, 0);
    }
    refreshApiLayout();
}

ElaScrollPageArea* ApiSettingsPage::createApiInputRowWidget()
{
    return createApiInputRowWidget(tomlkit::Item::table());
}

ElaScrollPageArea* ApiSettingsPage::createApiInputRowWidget(const tomlkit::Item& api)
{
    constexpr int keyEditWidth = 560;
    constexpr int editWidth = 340;
    constexpr int protocolWidth = 130;

    std::vector<std::string> apiKeys;
    if (api.contains("apikeys") && api.at("apikeys").isArray()) {
        for (const tomlkit::Item& keyValue : api.at("apikeys")) {
            if (!keyValue.isString()) {
                continue;
            }
            const std::string& keyValueString = keyValue.asString();
            if (!keyValueString.empty()) {
                apiKeys.push_back(keyValueString);
            }
        }
    }
    const std::string key = apiKeys.empty() ? "" : apiKeys.front();
    const std::string url = tomlkit::findOr(api, "apiurl", "");
    const std::string model = tomlkit::findOr(api, "modelName", "");
    const std::string protocol = tomlkit::findOr(api, "protocol", "openai");
    const std::string thinkingLevel = tomlkit::findOr(api, "thinkingLevel", "off");
    const bool useSystemProxy = tomlkit::findOr(api, "useSystemProxy", true);
    const bool stream = tomlkit::findOr(api, "stream", false);
    const bool fastPause = tomlkit::findOr(api, "fastPause", false);
    const bool enable = tomlkit::findOr(api, "enable", true);
    const bool extraHeadersEnable = tomlkit::findOr(api, "extraHeadersEnable", false);
    const bool extraBodyEnable = tomlkit::findOr(api, "extraBodyEnable", false);

    std::optional<double> temperature;
    std::optional<double> topP;
    std::optional<double> frequencyPenalty;
    std::optional<double> presencePenalty;
    if (api.contains("temperature") && api.at("temperature").isFloat()) {
        temperature = api.at("temperature").asFloat();
    }
    if (api.contains("topP") && api.at("topP").isFloat()) {
        topP = api.at("topP").asFloat();
    }
    if (api.contains("frequencyPenalty") && api.at("frequencyPenalty").isFloat()) {
        frequencyPenalty = api.at("frequencyPenalty").asFloat();
    }
    if (api.contains("presencePenalty") && api.at("presencePenalty").isFloat()) {
        presencePenalty = api.at("presencePenalty").asFloat();
    }

    ElaScrollPageArea* container = new ElaScrollPageArea(this);
    container->setFixedHeight(180);

    QHBoxLayout* containerLayout = new QHBoxLayout(container);
    QWidget* fieldsWidget = new QWidget(container);
    QVBoxLayout* fieldsLayout = new QVBoxLayout(fieldsWidget);
    fieldsLayout->setContentsMargins(0, 0, 0, 0);
    fieldsLayout->setSpacing(containerLayout->spacing());
    ElaText* nameLabel = new ElaText(container);
    nameLabel->setTextPixelSize(16);
    nameLabel->setWordWrap(false);
    nameLabel->setIndent(8);
    fieldsLayout->addSpacing(8);
    fieldsLayout->addWidget(nameLabel);
    containerLayout->addWidget(fieldsWidget, 1);

    QWidget* formWidget = new QWidget(container);
    QGridLayout* formLayout = new QGridLayout(formWidget);

    ElaText* apiKeyLabel = new ElaText("Api key", 13, formWidget);
    apiKeyLabel->setWordWrap(false);
    ElaLineEdit* keyEdit = new ElaLineEdit(formWidget);
    keyEdit->setFixedWidth(keyEditWidth);
    keyEdit->setPlaceholderText(tr("请输入 Api key(Sakura引擎或有Extra keys时可不填)"));
    if (!key.empty()) {
        keyEdit->setText(QString::fromStdString(key));
    }
    formLayout->addWidget(apiKeyLabel, 0, 0);
    formLayout->addWidget(keyEdit, 0, 1, 1, 2);

    ElaText* apiUrlLabel = new ElaText("Api url", 13, formWidget);
    apiUrlLabel->setWordWrap(false);
    ElaLineEdit* urlEdit = new ElaLineEdit(formWidget);
    urlEdit->setFixedWidth(editWidth);
    urlEdit->setPlaceholderText(tr("请输入 Api url"));
    if (!url.empty()) {
        urlEdit->setText(QString::fromStdString(url));
    }
    formLayout->addWidget(apiUrlLabel, 1, 0);
    formLayout->addWidget(urlEdit, 1, 1);

    ElaText* modelLabel = new ElaText(tr("模型名称"), 13, formWidget);
    modelLabel->setWordWrap(false);
    ElaLineEdit* modelEdit = new ElaLineEdit(formWidget);
    modelEdit->setFixedWidth(editWidth);
    modelEdit->setPlaceholderText(tr("请输入模型名称(Sakura引擎可不填)"));
    if (!model.empty()) {
        modelEdit->setText(QString::fromStdString(model));
    }
    formLayout->addWidget(modelLabel, 2, 0);
    formLayout->addWidget(modelEdit, 2, 1);

    ElaText* protocolLabel = new ElaText(tr("接口协议"), 13, formWidget);
    protocolLabel->setWordWrap(false);
    protocolLabel->setFixedWidth(protocolWidth);
    protocolLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    GppNoWheelComboBox* protocolComboBox = new GppNoWheelComboBox(formWidget);
    protocolComboBox->setFixedWidth(protocolWidth);
    protocolComboBox->addItems(QStringList{ "openai", "claude", "gemini", "openaires" });
    if (const int protocolIndex = protocolComboBox->findText(QString::fromStdString(protocol)); protocolIndex >= 0) {
        protocolComboBox->setCurrentIndex(protocolIndex);
    }

    ElaIconButton* moveUpButton = new ElaIconButton(ElaIconType::AngleUp, formWidget);
    connect(moveUpButton, &ElaIconButton::clicked, this, [=, this]()
        {
            moveApiRow(container, -1);
        });
    ElaIconButton* moveDownButton = new ElaIconButton(ElaIconType::AngleDown, formWidget);
    connect(moveDownButton, &ElaIconButton::clicked, this, [=, this]()
        {
            moveApiRow(container, 1);
        });

    QWidget* protocolLabelWidget = new QWidget(formWidget);
    QHBoxLayout* protocolLabelLayout = new QHBoxLayout(protocolLabelWidget);
    protocolLabelLayout->setContentsMargins(0, 0, 0, 0);
    protocolLabelLayout->setSpacing(8);
    protocolLabelLayout->addWidget(protocolLabel);
    protocolLabelLayout->addWidget(moveUpButton);
    protocolLabelLayout->addStretch();
    formLayout->addWidget(protocolLabelWidget, 1, 2);

    QWidget* protocolComboWidget = new QWidget(formWidget);
    QHBoxLayout* protocolComboLayout = new QHBoxLayout(protocolComboWidget);
    protocolComboLayout->setContentsMargins(0, 0, 0, 0);
    protocolComboLayout->setSpacing(8);
    protocolComboLayout->addWidget(protocolComboBox);
    protocolComboLayout->addWidget(moveDownButton);
    protocolComboLayout->addStretch();
    formLayout->addWidget(protocolComboWidget, 2, 2);
    formLayout->setColumnStretch(3, 1);

    QWidget* rightWidget = new QWidget(container);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->addStretch();

    ElaIconButton* deleteButton = new ElaIconButton(ElaIconType::Trash, rightWidget);
    rightLayout->addWidget(deleteButton);
    connect(deleteButton, &ElaIconButton::clicked, this, [=, this]()
        {
            deleteApiRow(container);
        });

    ElaCheckBox* enableCheckBox = new ElaCheckBox(tr("启用"), rightWidget);
    enableCheckBox->setChecked(enable);
    rightLayout->addWidget(enableCheckBox, 0, Qt::AlignHCenter);

    ElaToolButton* configButton = new ElaToolButton(rightWidget);
    configButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    configButton->setElaIcon(ElaIconType::Sliders);
    configButton->setText(tr("详细配置"));
    rightLayout->addWidget(configButton);
    rightLayout->addStretch();

    ElaWidget* configWidget = new ElaWidget();
    configWidget->setWindowTitle(tr("Api 详细配置"));
    configWidget->setWindowModality(Qt::ApplicationModal);
    configWidget->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    configWidget->resize(s_configWidgetSize);
    configWidget->setProperty("apiConfigWidget", true);
    configWidget->installEventFilter(this);
    QVBoxLayout* configLayout = new QVBoxLayout(configWidget);
    configLayout->setContentsMargins(10, 0, 10, 10);
    configLayout->setSpacing(0);

    ElaTabWidget* tabWidget = new ElaTabWidget(configWidget);
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setIsTabTransparent(true);
    tabWidget->setTabsClosable(false);
    tabWidget->setMovable(false);
    configLayout->addWidget(tabWidget);

    struct ScrollableTabPage
    {
        QWidget* page;
        QWidget* content;
        QVBoxLayout* layout;
    };

    auto createScrollablePage = [](QWidget* parent)
        {
            ElaScrollArea* scrollArea = new ElaScrollArea(parent);
            scrollArea->setMouseTracking(true);
            scrollArea->setIsAnimation(Qt::Vertical, true);
            scrollArea->setWidgetResizable(true);
            scrollArea->setIsGrabGesture(false, 0);
            scrollArea->setIsOverShoot(Qt::Vertical, true);
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            ElaScrollBar* floatVScrollBar = new ElaScrollBar(scrollArea->verticalScrollBar(), scrollArea);
            floatVScrollBar->setIsAnimation(true);

            QWidget* content = new QWidget(scrollArea);
            content->setObjectName("ElaScrollPageContainer");
            content->setStyleSheet("#ElaScrollPageContainer{background-color:transparent;}");
            QVBoxLayout* layout = new QVBoxLayout(content);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(8);
            scrollArea->setWidget(content);
            return ScrollableTabPage{ scrollArea, content, layout };
        };

    auto createFormRow = [](QWidget* parent)
        {
            ElaScrollPageArea* area = new ElaScrollPageArea(parent);
            QHBoxLayout* layout = new QHBoxLayout(area);
            layout->setContentsMargins(12, 0, 12, 0);
            layout->setSpacing(8);
            return std::pair{ area, layout };
        };

    const ScrollableTabPage basicTabPage = createScrollablePage(tabWidget);
    QWidget* basicPage = basicTabPage.content;
    QVBoxLayout* basicLayout = basicTabPage.layout;

    auto [nameArea, nameLayout] = createFormRow(basicPage);
    nameLayout->addWidget(new ElaText(tr("名称"), 16, nameArea));
    nameLayout->addStretch();
    ElaLineEdit* nameEdit = new ElaLineEdit(nameArea);
    nameEdit->setMinimumWidth(340);
    nameEdit->setPlaceholderText(tr("自定义 Api"));
    nameEdit->setText(QString::fromStdString(tomlkit::findOr(api, "name", tr("自定义 Api").toStdString())));
    nameLayout->addWidget(nameEdit);
    basicLayout->addWidget(nameArea);
    if (m_layoutMode == 0) {
        basicLayout->addWidget(formWidget);
        moveUpButton->hide();
        moveDownButton->hide();
    }
    else {
        fieldsLayout->addWidget(formWidget);
    }

    ElaScrollPageArea* extraKeysArea = new ElaScrollPageArea(basicPage);
    extraKeysArea->setFixedHeight(230);
    QVBoxLayout* extraKeysLayout = new QVBoxLayout(extraKeysArea);
    extraKeysLayout->setContentsMargins(12, 6, 12, 8);
    extraKeysLayout->setSpacing(6);
    extraKeysLayout->addWidget(new GppDoubleText("ExtraKeys", 16, "", 0,
        tr("一行一个 key，保存时会接在首个 Api key 后面"), extraKeysArea));
    ElaPlainTextEdit* extraKeysEdit = new ElaPlainTextEdit(extraKeysArea);
    extraKeysEdit->setFixedHeight(165);
    extraKeysEdit->setPlaceholderText(tr("sk-...\nsk-..."));
    QStringList extraKeyLines;
    for (size_t i = 1; i < apiKeys.size(); ++i) {
        extraKeyLines.push_back(QString::fromStdString(apiKeys[i]));
    }
    extraKeysEdit->setPlainText(extraKeyLines.join('\n'));
    extraKeysLayout->addWidget(extraKeysEdit);
    basicLayout->addWidget(extraKeysArea);

    auto [modelActionArea, modelActionLayout] = createFormRow(basicPage);
    modelActionLayout->addWidget(new ElaText(tr("模型"), 16, modelActionArea));
    modelActionLayout->addStretch();
    ElaToolButton* fetchModelButton = new ElaToolButton(modelActionArea);
    fetchModelButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    fetchModelButton->setElaIcon(ElaIconType::CloudArrowDown);
    fetchModelButton->setText(tr("获取模型"));
    ElaToolButton* testModelButton = new ElaToolButton(modelActionArea);
    testModelButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    testModelButton->setElaIcon(ElaIconType::Vial);
    testModelButton->setText(tr("测试模型"));
    modelActionLayout->addWidget(fetchModelButton);
    modelActionLayout->addWidget(testModelButton);
    basicLayout->addWidget(modelActionArea);

    auto [thinkingConfigArea, thinkingConfigLayout] = createFormRow(basicPage);
    thinkingConfigLayout->addWidget(new GppDoubleText(tr("思考等级"), 16,
        tr("不传递则使用接口默认行为；其余等级按协议和模型转换，不支持的档位使用最接近的可用档位"), 10,
        "", thinkingConfigArea));
    thinkingConfigLayout->addStretch();
    GppNoWheelComboBox* thinkingComboBox = new GppNoWheelComboBox(thinkingConfigArea);
    thinkingComboBox->setFixedWidth(130);
    thinkingComboBox->addItem(tr("不传递"), "off");
    for (const auto& level : QStringList{ "none", "minimal", "low", "medium", "high", "xhigh", "max" }) {
        thinkingComboBox->addItem(level, level);
    }
    if (const int thinkingIndex = thinkingComboBox->findData(QString::fromStdString(thinkingLevel)); thinkingIndex >= 0) {
        thinkingComboBox->setCurrentIndex(thinkingIndex);
    }
    thinkingConfigLayout->addWidget(thinkingComboBox);
    basicLayout->addWidget(thinkingConfigArea);

    auto [systemProxyArea, systemProxyLayout] = createFormRow(basicPage);
    systemProxyLayout->addWidget(new ElaText(tr("使用系统代理"), 16, systemProxyArea));
    systemProxyLayout->addStretch();
    ElaToggleSwitch* systemProxySwitch = new ElaToggleSwitch(systemProxyArea);
    systemProxySwitch->setIsToggled(useSystemProxy);
    systemProxyLayout->addWidget(systemProxySwitch);
    basicLayout->addWidget(systemProxyArea);

    auto [streamArea, streamLayout] = createFormRow(basicPage);
    streamLayout->addWidget(new ElaText(tr("流式输出"), 16, streamArea));
    streamLayout->addStretch();
    ElaToggleSwitch* streamSwitch = new ElaToggleSwitch(streamArea);
    streamSwitch->setIsToggled(stream);
    streamLayout->addWidget(streamSwitch);
    basicLayout->addWidget(streamArea);

    auto [fastPauseArea, fastPauseLayout] = createFormRow(basicPage);
    fastPauseLayout->addWidget(new GppDoubleText(tr("快速暂停"), 16,
        tr("开启时立即中断请求；关闭时等待当前请求结束后暂停"), 10, "", fastPauseArea));
    fastPauseLayout->addStretch();
    ElaToggleSwitch* fastPauseSwitch = new ElaToggleSwitch(fastPauseArea);
    fastPauseSwitch->setIsToggled(fastPause);
    fastPauseLayout->addWidget(fastPauseSwitch);
    basicLayout->addWidget(fastPauseArea);

    basicLayout->addStretch();
    tabWidget->addTab(basicTabPage.page, tr("基础设置"));

    const ScrollableTabPage agentTabPage = createScrollablePage(tabWidget);
    const auto addAgentSwitch = [&](const std::string& key_, const QString& title, const QString& description, bool defaultValue, const QString& toolTip = {})
        {
            auto [area, layout] = createFormRow(agentTabPage.content);
            layout->addWidget(new GppDoubleText(title, 16, description, 10, toolTip, area));
            layout->addStretch();
            ElaToggleSwitch* toggle = new ElaToggleSwitch(area);
            toggle->setIsToggled(tomlkit::findOr(api, key_, defaultValue));
            layout->addWidget(toggle);
            agentTabPage.layout->addWidget(area);
            return toggle;
        };
    agentTabPage.layout->addSpacing(8);
    agentTabPage.layout->addWidget(new ElaText(tr("以下选项仅在 Agent 模式开启时生效"), 18, agentTabPage.content));
    auto [agentStrictToolsArea, agentStrictToolsLayout] = createFormRow(agentTabPage.content);
    agentStrictToolsLayout->addWidget(new GppDoubleText(tr("严格工具参数"), 16,
        tr("OpenAI/Claude 可选严格范围；Gemini 固定使用原生工具参数约束"), 10,
        tr("仅 commit 严格只约束译文提交工具，查询、搜索和压缩工具不启用 strict；可减少严格工具 schema 的复杂度。"), agentStrictToolsArea));
    agentStrictToolsLayout->addStretch();
    GppNoWheelComboBox* agentStrictToolsComboBox = new GppNoWheelComboBox(agentStrictToolsArea);
    agentStrictToolsComboBox->setFixedWidth(180);
    agentStrictToolsComboBox->addItem(tr("不严格"), "off");
    agentStrictToolsComboBox->addItem(tr("完全严格"), "all");
    agentStrictToolsComboBox->addItem(tr("仅 commit 严格"), "commit");
    const std::string strictTools = tomlkit::findOr(api, "agentStrictTools", "all");
    agentStrictToolsComboBox->setCurrentIndex(agentStrictToolsComboBox->findData(QString::fromStdString(strictTools)));
    agentStrictToolsLayout->addWidget(agentStrictToolsComboBox);
    agentTabPage.layout->addWidget(agentStrictToolsArea);
    ElaToggleSwitch* agentStatefulSwitch = addAgentSwitch("agentStateful", tr("服务端会话续接"),
        tr("仅 Responses/Gemini Interactions 生效；开启后使用会话 ID 并启用服务端存储"), false);
    ElaToggleSwitch* agentNativeAutoCompactionSwitch = addAgentSwitch("agentNativeAutoCompaction", tr("原生自动压缩"),
        tr("Responses/Claude 原生自动压缩；模型不支持时可能报错或超出上下文上限"), false,
        tr("需要模型和中转支持。开启后由服务端执行自动压缩，不再进行本地摘要压缩；不支持的模型或中转可能报错，或忽略压缩参数并最终超出上下文上限。"));
    auto [agentCompactTokensArea, agentCompactTokensLayout] = createFormRow(agentTabPage.content);
    agentCompactTokensLayout->addWidget(new GppDoubleText(tr("上下文压缩 token 阈值"), 16,
        tr("0 使用默认值；本地压缩与 Responses 原生压缩默认 100000，Claude 原生压缩使用服务端默认值"), 10,
        tr("未启用原生自动压缩时，按模型响应返回的 token 用量触发本地摘要压缩；若响应未返回用量信息，则按该阈值的 4 倍字节数回退估算"), agentCompactTokensArea));
    agentCompactTokensLayout->addStretch();
    ElaSpinBox* agentCompactTokensSpinBox = new ElaSpinBox(agentCompactTokensArea);
    agentCompactTokensSpinBox->setRange(0, 1000000000);
    agentCompactTokensSpinBox->setFixedWidth(180);
    agentCompactTokensSpinBox->setValue(tomlkit::findOr(api, "agentCompactThresholdTokens", 0));
    agentCompactTokensLayout->addWidget(agentCompactTokensSpinBox);
    agentTabPage.layout->addWidget(agentCompactTokensArea);
    ElaToggleSwitch* agentGeminiInteractionsSwitch = addAgentSwitch("agentGeminiInteractions",
    	tr("为 TransAgent 启用 Interactions API"), tr("Gemini 使用 Interactions；关闭后使用 generateContent 原生工具调用"), false);
    // 选项位置保持固定，适用协议由说明文字提示，不随协议切换隐藏。
    agentTabPage.layout->addStretch();
    tabWidget->addTab(agentTabPage.page, tr("Agent"));

    const ScrollableTabPage advancedTabPage = createScrollablePage(tabWidget);
    QWidget* advancedPage = advancedTabPage.content;
    QVBoxLayout* advancedLayout = advancedTabPage.layout;

    auto [temperatureConfigArea, temperatureConfigLayout] = createFormRow(advancedPage);
    temperatureConfigLayout->addWidget(new GppDoubleText(tr("温度"), 16,
        tr("勾选选框则使用自定义温度，否则使用供应商默认温度"), 10, "", temperatureConfigArea));
    temperatureConfigLayout->addStretch();
    ValueSliderWidget* temperatureSlider = new ValueSliderWidget(0.0, 2.0, temperatureConfigArea);
    temperatureSlider->setFixedWidth(360);
    temperatureSlider->setDecimals(2);
    temperatureSlider->setValue(temperature.value_or(1.0));
    temperatureConfigLayout->addWidget(temperatureSlider);
    ElaCheckBox* temperatureCheckBox = new ElaCheckBox(temperatureConfigArea);
    temperatureCheckBox->setChecked(temperature.has_value());
    temperatureConfigLayout->addWidget(temperatureCheckBox);
    advancedLayout->addWidget(temperatureConfigArea);

    auto [topPConfigArea, topPConfigLayout] = createFormRow(advancedPage);
    topPConfigLayout->addWidget(new GppDoubleText(tr("top_p"), 16,
        tr("核采样(也是控制随机性的)"), 10, "", topPConfigArea));
    topPConfigLayout->addStretch();
    ValueSliderWidget* topPSlider = new ValueSliderWidget(0.0, 1.0, topPConfigArea);
    topPSlider->setFixedWidth(360);
    topPSlider->setDecimals(2);
    topPSlider->setValue(topP.value_or(1.0));
    topPConfigLayout->addWidget(topPSlider);
    ElaCheckBox* topPCheckBox = new ElaCheckBox(topPConfigArea);
    topPCheckBox->setChecked(topP.has_value());
    topPConfigLayout->addWidget(topPCheckBox);
    advancedLayout->addWidget(topPConfigArea);

    auto [frequencyPenaltyConfigArea, frequencyPenaltyConfigLayout] = createFormRow(advancedPage);
    frequencyPenaltyConfigLayout->addWidget(new GppDoubleText(tr("frequency_penalty"), 16,
        tr("频率惩罚"), 10, "", frequencyPenaltyConfigArea));
    frequencyPenaltyConfigLayout->addStretch();
    ValueSliderWidget* frequencyPenaltySlider = new ValueSliderWidget(-2.0, 2.0, frequencyPenaltyConfigArea);
    frequencyPenaltySlider->setFixedWidth(360);
    frequencyPenaltySlider->setDecimals(2);
    frequencyPenaltySlider->setValue(frequencyPenalty.value_or(0.0));
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltySlider);
    ElaCheckBox* frequencyPenaltyCheckBox = new ElaCheckBox(frequencyPenaltyConfigArea);
    frequencyPenaltyCheckBox->setChecked(frequencyPenalty.has_value());
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltyCheckBox);
    advancedLayout->addWidget(frequencyPenaltyConfigArea);

    auto [presencePenaltyConfigArea, presencePenaltyConfigLayout] = createFormRow(advancedPage);
    presencePenaltyConfigLayout->addWidget(new GppDoubleText(tr("presence_penalty"), 16,
        tr("存在惩罚"), 10, "", presencePenaltyConfigArea));
    presencePenaltyConfigLayout->addStretch();
    ValueSliderWidget* presencePenaltySlider = new ValueSliderWidget(-2.0, 2.0, presencePenaltyConfigArea);
    presencePenaltySlider->setFixedWidth(360);
    presencePenaltySlider->setDecimals(2);
    presencePenaltySlider->setValue(presencePenalty.value_or(0.0));
    presencePenaltyConfigLayout->addWidget(presencePenaltySlider);
    ElaCheckBox* presencePenaltyCheckBox = new ElaCheckBox(presencePenaltyConfigArea);
    presencePenaltyCheckBox->setChecked(presencePenalty.has_value());
    presencePenaltyConfigLayout->addWidget(presencePenaltyCheckBox);
    advancedLayout->addWidget(presencePenaltyConfigArea);

    const auto formatJsonObjectText = [&api](const std::string& key_) -> QString
        {
            if (!api.contains(key_)) {
                return {};
            }
            const tomlkit::Item& value = api.at(key_);
            if (value.isString()) {
                return QString::fromStdString(value.asString());
            }
            if (value.isTable()) {
                return QString::fromStdString(toml2OrderedJson(value).dump(2));
            }
            return {};
        };

    ElaScrollPageArea* extraHeadersArea = new ElaScrollPageArea(advancedPage);
    extraHeadersArea->setFixedHeight(180);
    QVBoxLayout* extraHeadersLayout = new QVBoxLayout(extraHeadersArea);
    extraHeadersLayout->setContentsMargins(12, 6, 12, 8);
    extraHeadersLayout->setSpacing(6);
    QWidget* extraHeadersTitleWidget = new QWidget(extraHeadersArea);
    QHBoxLayout* extraHeadersTitleLayout = new QHBoxLayout(extraHeadersTitleWidget);
    extraHeadersTitleLayout->setContentsMargins(0, 0, 0, 0);
    extraHeadersTitleLayout->setSpacing(8);
    extraHeadersTitleLayout->addWidget(new GppDoubleText("extraHeaders", 16,
        tr("JSON 对象，用于追加自定义 HTTP header"), 10, "", extraHeadersArea));
    extraHeadersTitleLayout->addStretch();
    ElaCheckBox* extraHeadersCheckBox = new ElaCheckBox(extraHeadersArea);
    extraHeadersCheckBox->setChecked(extraHeadersEnable);
    extraHeadersTitleLayout->addWidget(extraHeadersCheckBox);
    extraHeadersLayout->addWidget(extraHeadersTitleWidget);
    ElaPlainTextEdit* extraHeadersEdit = new ElaPlainTextEdit(extraHeadersArea);
    extraHeadersEdit->setFixedHeight(115);
    extraHeadersEdit->setPlaceholderText("{\n  \"HTTP-Referer\": \"https://example.com\",\n  \"X-Title\": \"GalTranslPP\"\n}");
    installTreeSitterHighlighter(extraHeadersEdit->document(), SyntaxLanguage::Json);
    extraHeadersEdit->setPlainText(formatJsonObjectText("extraHeaders"));
    extraHeadersLayout->addWidget(extraHeadersEdit);
    advancedLayout->addWidget(extraHeadersArea);

    ElaScrollPageArea* extraBodyArea = new ElaScrollPageArea(advancedPage);
    extraBodyArea->setFixedHeight(180);
    QVBoxLayout* extraBodyLayout = new QVBoxLayout(extraBodyArea);
    extraBodyLayout->setContentsMargins(12, 6, 12, 8);
    extraBodyLayout->setSpacing(6);
    QWidget* extraBodyTitleWidget = new QWidget(extraBodyArea);
    QHBoxLayout* extraBodyTitleLayout = new QHBoxLayout(extraBodyTitleWidget);
    extraBodyTitleLayout->setContentsMargins(0, 0, 0, 0);
    extraBodyTitleLayout->setSpacing(8);
    extraBodyTitleLayout->addWidget(new GppDoubleText("extraBody", 16,
        tr("JSON 对象，用于追加或覆盖请求体顶层字段；使用模型专用思考参数时请将思考等级设为不传递"),
        10, "", extraBodyArea));
    extraBodyTitleLayout->addStretch();
    ElaCheckBox* extraBodyCheckBox = new ElaCheckBox(extraBodyArea);
    extraBodyCheckBox->setChecked(extraBodyEnable);
    extraBodyTitleLayout->addWidget(extraBodyCheckBox);
    extraBodyLayout->addWidget(extraBodyTitleWidget);
    ElaPlainTextEdit* extraBodyEdit = new ElaPlainTextEdit(extraBodyArea);
    extraBodyEdit->setFixedHeight(115);
    extraBodyEdit->setPlaceholderText("{\n  \"enable_thinking\": true\n}");
    installTreeSitterHighlighter(extraBodyEdit->document(), SyntaxLanguage::Json);
    extraBodyEdit->setPlainText(formatJsonObjectText("extraBody"));
    extraBodyLayout->addWidget(extraBodyEdit);
    advancedLayout->addWidget(extraBodyArea);
    advancedLayout->addStretch();
    tabWidget->addTab(advancedTabPage.page, tr("高级设置"));

    auto showApiResultWindow = [=, this, configGuard = QPointer<ElaWidget>(configWidget)](const QString& title, const QString& content)
        {
            // 切换布局会重建配置窗口，旧请求完成后不再打开结果窗口。
            if (!configGuard) {
                return;
            }
            ElaDialog* resultDialog = new ElaDialog(configWidget);
            resultDialog->setAttribute(Qt::WA_DeleteOnClose);
            resultDialog->setWindowTitle(title);
            resultDialog->setWindowModality(Qt::WindowModal);
            resultDialog->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
            QVBoxLayout* resultLayout = new QVBoxLayout(resultDialog);
            resultLayout->setContentsMargins(10, 0, 10, 10);
            resultLayout->setSpacing(0);

            ElaPlainTextEdit* resultEdit = new ElaPlainTextEdit(resultDialog);
            resultEdit->setReadOnly(true);
            resultEdit->setPlainText(content);
            resultLayout->addWidget(resultEdit);

            const int resultHeight = std::clamp(window()->height() - 160, 420, 620);
            resultDialog->resize(820, resultHeight);
            const QPoint center = configWidget->geometry().center();
            resultDialog->move(center.x() - resultDialog->width() / 2, center.y() - resultDialog->height() / 2);
            resultDialog->show();
            resultDialog->raise();
            resultDialog->activateWindow();
        };

    auto formatModelListResult = [=](const ApiModelListResponse& result)
        {
            QStringList lines;
            lines << tr("请求类型: 获取模型列表");
            lines << tr("请求方法: GET");
            lines << "";
            lines << tr("HTTP 状态: %1").arg(result.models ? 200 : result.models.error().statusCode);
            lines << tr("请求结果: %1").arg(result.models ? tr("成功") : tr("失败"));
            lines << "";
            lines << tr("解析到的模型: ");
            if (!result.models || result.models->empty()) {
                lines << tr("(没有解析到模型)");
            }
            else {
                for (const std::string& model_ : *result.models) {
                    lines << QString::fromStdString(model_);
                }
            }
            if (!result.models) {
                lines << "";
                lines << tr("错误信息: ");
                lines << QString::fromStdString(formatApiError(result.models.error()));
            }
            return lines.join('\n');
        };

    auto formatTestApiResult = [=](const ApiTestResponse& result)
        {
            QStringList lines;
            lines << tr("请求类型: 测试模型回复");
            lines << tr("请求方法: POST");
            lines << tr("发出的请求体: ");
            lines << QString::fromStdString(result.requestBody);
            lines << "";
            lines << tr("HTTP 状态: %1").arg(result.content ? 200 : result.content.error().statusCode);
            lines << tr("请求结果: %1").arg(result.content ? tr("成功") : tr("失败"));
            lines << "";
            lines << (result.content ? tr("模型回复:") : tr("错误信息:"));
            lines << QString::fromStdString(result.content ? *result.content : formatApiError(result.content.error()));
            return lines.join('\n');
        };

    auto parseJsonObjectForRequest = [](const ElaPlainTextEdit* edit, const QString& title, ordered_json& value)
        {
            value = ordered_json::object();
            const QString text = edit->toPlainText().trimmed();
            if (text.isEmpty()) {
                return true;
            }
            try {
                value = ordered_json::parse(text.toStdString());
                if (!value.is_object()) {
                    ElaMessageBar::warning(ElaMessageBarType::TopRight, QObject::tr("解析失败"),
                        QObject::tr("%1 必须是 JSON 对象").arg(title), 3000);
                    return false;
                }
                return true;
            }
            catch (const std::exception& e) {
                ElaMessageBar::warning(ElaMessageBarType::TopRight, QObject::tr("解析失败"),
                    QObject::tr("%1 不是合法 JSON: %2").arg(title).arg(e.what()), 3000);
                return false;
            }
        };

    auto firstConfiguredApiKey = [=]()
        {
            std::string key_ = keyEdit->text().trimmed().toStdString();
            if (!key_.empty()) {
                return key_;
            }
            std::stringstream extraKeysStream(extraKeysEdit->toPlainText().toStdString());
            std::string extraKeyLine;
            while (std::getline(extraKeysStream, extraKeyLine)) {
                if (!extraKeyLine.empty()) {
                    return extraKeyLine;
                }
            }
            return std::string{};
        };

    auto buildCurrentApi = [=](bool requireModel, TranslationApi& api_)
        {
            const QString url_ = urlEdit->text().trimmed();
            if (url_.isEmpty()) {
                ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("请求失败"), tr("Api url 不能为空"), 3000);
                return false;
            }
            const QString modelName = modelEdit->text().trimmed();
            if (requireModel && modelName.isEmpty()) {
                ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("请求失败"), tr("模型名称不能为空"), 3000);
                return false;
            }

            try {
                api_.protocol = parseApiProtocol(protocolComboBox->currentText().toStdString());
            }
            catch (const std::exception& e) {
                ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("请求失败"), e.what(), 3000);
                return false;
            }
            api_.apikey = firstConfiguredApiKey();
            api_.apiurl = cvt2StdApiUrl(url_.toStdString(), api_.protocol);
            api_.modelName = modelName.toStdString();
            api_.thinkingLevel = thinkingComboBox->currentData().toString().toStdString();
            api_.useSystemProxy = systemProxySwitch->getIsToggled();
            api_.stream = streamSwitch->getIsToggled();
            api_.fastPause = fastPauseSwitch->getIsToggled();
            if (temperatureCheckBox->isChecked()) {
                api_.temperature = temperatureSlider->value();
            }
            if (topPCheckBox->isChecked()) {
                api_.topP = topPSlider->value();
            }
            if (frequencyPenaltyCheckBox->isChecked()) {
                api_.frequencyPenalty = frequencyPenaltySlider->value();
            }
            if (presencePenaltyCheckBox->isChecked()) {
                api_.presencePenalty = presencePenaltySlider->value();
            }

            if (extraHeadersCheckBox->isChecked()) {
                ordered_json extraHeaders = ordered_json::object();
                if (!parseJsonObjectForRequest(extraHeadersEdit, "extraHeaders", extraHeaders)) {
                    return false;
                }
                for (auto it = extraHeaders.cbegin(); it != extraHeaders.cend(); ++it) {
                    api_.extraHeaders[it.key()] = it.value().is_string()
                        ? it.value().get<std::string>()
                        : it.value().dump();
                }
            }
            if (extraBodyCheckBox->isChecked()) {
                ordered_json extraBody;
                if (!parseJsonObjectForRequest(extraBodyEdit, "extraBody", extraBody)) {
                    return false;
                }
                api_.extraBody = std::move(extraBody);
            }
            return true;
        };

    auto setModelRequestButtonsEnabled = [=](bool enabled)
        {
            fetchModelButton->setEnabled(enabled);
            testModelButton->setEnabled(enabled);
        };

    auto apiRequestTimeoutMs = [=, this]()
        {
            const int timeoutSeconds = tomlkit::findOr(m_projectConfig, "backend", "apiTimeout", 300);
            return std::max(1, timeoutSeconds) * 1000;
        };

    auto runModelRequest = [=, this](bool queryModels)
        {
            TranslationApi api_;
            if (!buildCurrentApi(!queryModels, api_)) {
                return;
            }

            setModelRequestButtonsEnabled(false);
            ElaMessageBar::information(ElaMessageBarType::TopRight,
                queryModels ? tr("模型获取") : tr("模型测试"),
                queryModels ? tr("正在获取模型列表...") : tr("正在测试模型请求..."), 3000);

            QPointer<ApiSettingsPage> page(this);
            QPointer<ElaLineEdit> modelEditPtr(modelEdit);
            QPointer<ElaToolButton> fetchButtonPtr(fetchModelButton);
            QPointer<ElaToolButton> testButtonPtr(testModelButton);
            const int timeoutMs = apiRequestTimeoutMs();

            std::thread([=, api = std::move(api_)]() mutable
                {
                    if (queryModels) {
                        ApiModelListResponse result = queryApiModels(api, timeoutMs);
                        if (!page) {
                            return;
                        }
                        QMetaObject::invokeMethod(page.data(), [=, result = std::move(result)]() mutable
                            {
                                if (!page) {
                                    return;
                                }
                                if (fetchButtonPtr) {
                                    fetchButtonPtr->setEnabled(true);
                                }
                                if (testButtonPtr) {
                                    testButtonPtr->setEnabled(true);
                                }
                                if (result.models && !result.models->empty()) {
                                    QStringList modelLines;
                                    for (const std::string& model_ : *result.models) {
                                        modelLines.push_back(QString::fromStdString(model_));
                                    }
                                    if (modelEditPtr && modelEditPtr->text().trimmed().isEmpty()) {
                                        modelEditPtr->setText(modelLines.first());
                                    }
                                    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("模型获取"),
                                        tr("获取到 %1 个模型").arg(modelLines.size()), 3000);
                                    showApiResultWindow(tr("模型列表"), formatModelListResult(result));
                                    return;
                                }
                                ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("模型获取"),
                                    result.models ? tr("请求成功，但没有解析到模型") : tr("模型列表请求失败"), 3000);
                                showApiResultWindow(tr("模型列表"), formatModelListResult(result));
                            }, Qt::QueuedConnection);
                        return;
                    }

                    ApiTestResponse result = testApiConnection(api, timeoutMs);
                    if (!page) {
                        return;
                    }
                    QMetaObject::invokeMethod(page.data(), [=, result = std::move(result)]() mutable
                        {
                            if (!page) {
                                return;
                            }
                            if (fetchButtonPtr) {
                                fetchButtonPtr->setEnabled(true);
                            }
                            if (testButtonPtr) {
                                testButtonPtr->setEnabled(true);
                            }
                            if (result.content) {
                                ElaMessageBar::success(ElaMessageBarType::TopRight, tr("模型测试"),
                                    tr("模型请求成功"), 3000);
                            }
                            else {
                                ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("模型测试"),
                                    tr("模型请求失败"), 3000);
                            }
                            showApiResultWindow(tr("模型测试"), formatTestApiResult(result));
                        }, Qt::QueuedConnection);
                }).detach();
        };

    connect(fetchModelButton, &ElaToolButton::clicked, this, [=]()
        {
            runModelRequest(true);
        });
    connect(testModelButton, &ElaToolButton::clicked, this, [=]()
        {
            runModelRequest(false);
        });

    configWidget->hide();
    const auto showConfig = [=, this]()
        {
            const QSize windowSize = configWidget->size();
            configWidget->move(window()->frameGeometry().center() - configWidget->rect().center());
            configWidget->show();
            configWidget->resize(windowSize);
            configWidget->raise();
            configWidget->activateWindow();
        };
    connect(configButton, &ElaToolButton::clicked, this, showConfig);
    containerLayout->addWidget(rightWidget);

    GppApiCard* flowCard = new GppApiCard(m_flowList);
    flowCard->dragHandle()->setToolTip(tr("拖动排序"));
    m_flowList->setDragHandle(flowCard, flowCard->dragHandle());
    ElaMenu* menu = new ElaMenu(flowCard);
    QAction* enableAction = menu->addAction(tr("启用"));
    enableAction->setCheckable(true);
    enableAction->setChecked(enable);
    connect(enableAction, &QAction::toggled, enableCheckBox, &QCheckBox::setChecked);
    connect(enableCheckBox, &QCheckBox::toggled, enableAction, &QAction::setChecked);
    connect(menu->addElaIconAction(ElaIconType::Sliders, tr("详细设置")), &QAction::triggered, this, showConfig);
    connect(menu->addElaIconAction(ElaIconType::Copy, tr("生成副本")), &QAction::triggered,
        this, [=, this]()
        {
            duplicateApiRow(container);
        });
    menu->addSeparator();
    connect(menu->addElaIconAction(ElaIconType::Trash, tr("删除模型")), &QAction::triggered,
        this, [=, this]()
        {
            deleteApiRow(container);
        });
    flowCard->setMenu(menu);
    const auto updateName = [=, this]()
        {
            const QString name = nameEdit->text().trimmed().isEmpty()
                ? tr("自定义 Api")
                : nameEdit->text().trimmed();
            nameLabel->setText(name);
            flowCard->setApiName(name);
            m_flowList->layout()->invalidate();
            configWidget->setWindowTitle(tr("Api 详细配置 — %1").arg(name));
        };
    connect(nameEdit, &QLineEdit::textChanged, this, updateName);
    // 停用后保留菜单入口，用文字颜色区分状态。
    connect(enableCheckBox, &QCheckBox::toggled, flowCard, &GppApiCard::setApiEnabled);
    flowCard->setApiEnabled(enable);
    updateName();

    ApiRowControls newRowControls;
    newRowControls.container = container;
    newRowControls.configWidget = configWidget;
    newRowControls.flowCard = flowCard;
    newRowControls.moveUpButton = moveUpButton;
    newRowControls.moveDownButton = moveDownButton;
    newRowControls.serializeFunc = [=](tomlkit::Item& apiArray)
        {
            tomlkit::Item apiTable = tomlkit::Item::table(tomlkit::TableStyle::ArrayOfTablesElement);
            apiTable["name"] = (nameEdit->text().trimmed().isEmpty()
                ? tr("自定义 Api")
                : nameEdit->text().trimmed()).toStdString();
            apiTable["protocol"] = protocolComboBox->currentText().toStdString();
            tomlkit::Item apiKeysArray = tomlkit::Item::array();
            const std::string primaryKey = keyEdit->text().toStdString();
            if (!primaryKey.empty()) {
                apiKeysArray.push_back(primaryKey);
            }
            std::stringstream extraKeysStream(extraKeysEdit->toPlainText().toStdString());
            std::string extraKeyLine;
            while (std::getline(extraKeysStream, extraKeyLine)) {
                if (!extraKeyLine.empty()) {
                    apiKeysArray.push_back(extraKeyLine);
                }
            }
            apiTable["apikeys"] = std::move(apiKeysArray);
            apiTable["apiurl"] = urlEdit->text().toStdString();
            apiTable["modelName"] = modelEdit->text().toStdString();
            apiTable["useSystemProxy"] = systemProxySwitch->getIsToggled();
            apiTable["stream"] = streamSwitch->getIsToggled();
            apiTable["fastPause"] = fastPauseSwitch->getIsToggled();
            apiTable["enable"] = enableCheckBox->isChecked();
            apiTable["thinkingLevel"] = thinkingComboBox->currentData().toString().toStdString();
            apiTable["agentStateful"] = agentStatefulSwitch->getIsToggled();
            apiTable["agentNativeAutoCompaction"] = agentNativeAutoCompactionSwitch->getIsToggled();
            apiTable["agentGeminiInteractions"] = agentGeminiInteractionsSwitch->getIsToggled();
            apiTable["agentStrictTools"] = agentStrictToolsComboBox->currentData().toString().toStdString();
            apiTable["agentCompactThresholdTokens"] = agentCompactTokensSpinBox->value();
            if (temperatureCheckBox->isChecked()) {
                apiTable["temperature"] = temperatureSlider->value();
            }
            if (topPCheckBox->isChecked()) {
                apiTable["topP"] = topPSlider->value();
            }
            if (frequencyPenaltyCheckBox->isChecked()) {
                apiTable["frequencyPenalty"] = frequencyPenaltySlider->value();
            }
            if (presencePenaltyCheckBox->isChecked()) {
                apiTable["presencePenalty"] = presencePenaltySlider->value();
            }
            apiTable["extraHeadersEnable"] = extraHeadersCheckBox->isChecked();
            apiTable["extraBodyEnable"] = extraBodyCheckBox->isChecked();
            if (!extraHeadersEdit->toPlainText().trimmed().isEmpty()) {
                ordered_json extraHeaders;
                if (!parseJsonObjectForRequest(extraHeadersEdit, "extraHeaders", extraHeaders)) {
                    return false;
                }
                apiTable["extraHeaders"] = json2Toml(extraHeaders);
            }
            if (!extraBodyEdit->toPlainText().trimmed().isEmpty()) {
                ordered_json extraBody;
                if (!parseJsonObjectForRequest(extraBodyEdit, "extraBody", extraBody)) {
                    return false;
                }
                apiTable["extraBody"] = json2Toml(extraBody);
            }
            apiArray.push_back(std::move(apiTable));
            return true;
        };
    m_apiRows.append(newRowControls);

    return container;
}

bool ApiSettingsPage::serializeApis(tomlkit::Item& apis) const
{
    for (const auto& row : m_apiRows) {
        if (!row.serializeFunc(apis)) {
            return false;
        }
    }
    return true;
}

void ApiSettingsPage::switchLayout(int mode)
{
    if (mode == m_layoutMode) {
        return;
    }
    tomlkit::Item apis = tomlkit::Item::arrayOfTables();
    // 先读取当前控件和排序，成功后才销毁旧布局；无效 JSON 不会因切换而丢失。
    if (!serializeApis(apis)) {
        return;
    }
    m_flowList->setCards({});
    for (const auto& row : m_apiRows) {
        delete row.configWidget;
        delete row.container;
        delete row.flowCard;
    }
    m_apiRows.clear();
    m_layoutMode = mode;
    for (const auto& api : apis) {
        createApiInputRowWidget(api);
    }
    refreshApiLayout();
    insertToml(m_projectConfig, "GUIConfig.apiLayoutMode", mode);
}

void ApiSettingsPage::refreshApiLayout()
{
    while (auto* item = m_listLayout->takeAt(0)) {
        delete item;
    }
    QList<QWidget*> cards;
    for (const auto& row : m_apiRows) {
        m_listLayout->addWidget(row.container);
        cards.append(row.flowCard);
    }
    m_flowList->setCards(cards);
    m_flowList->setVisible(m_layoutMode == 0);
    m_listWidget->setVisible(m_layoutMode == 1);
    m_flowModeButton->setEnabled(m_layoutMode != 0);
    m_listModeButton->setEnabled(m_layoutMode != 1);
    updateMoveButtonStates();
}

void ApiSettingsPage::moveApiRow(ElaScrollPageArea* container, int offset)
{
    for (int i = 0; i < m_apiRows.size(); ++i) {
        if (m_apiRows.at(i).container != container) {
            continue;
        }
        const int target = i + offset;
        if (target < 0 || target >= m_apiRows.size()) {
            return;
        }
        m_apiRows.move(i, target);
        refreshApiLayout();
        return;
    }
}

void ApiSettingsPage::updateMoveButtonStates()
{
    for (int i = 0; i < m_apiRows.size(); ++i) {
        m_apiRows.at(i).moveUpButton->setEnabled(i > 0);
        m_apiRows.at(i).moveDownButton->setEnabled(i < m_apiRows.size() - 1);
    }
}

void ApiSettingsPage::deleteApiRow(ElaScrollPageArea* container)
{
    for (int i = 0; i < m_apiRows.size(); ++i) {
        if (m_apiRows.at(i).container != container) {
            continue;
        }
        ElaContentDialog dialog(window());
        dialog.setLeftButtonText(tr("否"));
        dialog.setMiddleButtonText(tr("思考人生"));
        dialog.setRightButtonText(tr("是"));
        QWidget* widget = new QWidget(&dialog);
        QVBoxLayout* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(15, 25, 15, 10);
        ElaText* title = new ElaText(tr("删除 Api"), widget);
        title->setTextStyle(ElaTextType::Title);
        title->setWordWrap(false);
        layout->addWidget(title);
        layout->addSpacing(2);
        ElaText* message = new ElaText(tr("确定删除『%1』吗？").arg(m_apiRows.at(i).flowCard->text()), 16, widget);
        message->setTextStyle(ElaTextType::Body);
        message->setWordWrap(true);
        layout->addWidget(message);
        layout->addStretch();
        dialog.setCentralWidget(widget);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        const auto row = m_apiRows.takeAt(i);
        m_listLayout->removeWidget(row.container);
        row.container->hide();
        row.flowCard->hide();
        row.configWidget->deleteLater();
        row.container->deleteLater();
        row.flowCard->deleteLater();
        refreshApiLayout();
        return;
    }
}

void ApiSettingsPage::duplicateApiRow(ElaScrollPageArea* container)
{
    for (int i = 0; i < m_apiRows.size(); ++i) {
        if (m_apiRows.at(i).container != container) {
            continue;
        }
        tomlkit::Item apis = tomlkit::Item::arrayOfTables();
        if (!m_apiRows.at(i).serializeFunc(apis)) {
            return;
        }
        auto& api = apis.front();
        api["name"] = tr("%1（副本）").arg(QString::fromStdString(api.at("name").asString())).toStdString();
        createApiInputRowWidget(api);
        m_apiRows.move(m_apiRows.size() - 1, i + 1);
        refreshApiLayout();
        return;
    }
}
