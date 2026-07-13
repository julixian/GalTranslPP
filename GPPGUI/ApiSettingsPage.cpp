#include "ApiSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QEvent>
#include <QPointer>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPlainTextEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaPushButton.h"
#include "ElaToolButton.h"
#include "ElaRadioButton.h"
#include "ElaIconButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "ElaDoubleText.h"
#include "ElaAlignedCheckBox.h"
#include "ElaNoWheelComboBox.h"
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

QSize ApiSettingsPage::s_configWidgetSize(980, 820);

ApiSettingsPage::ApiSettingsPage(toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectConfig(projectConfig)
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

void ApiSettingsPage::apply2Config()
{
    toml::ordered_array apiArray;
    for (const auto& apiRow : m_apiRows) {
        apiRow.applyFunc(apiArray);
    }
    insertToml(m_projectConfig, "backend.apis", apiArray);
    if (m_applyFunc) {
        m_applyFunc();
    }
}

void ApiSettingsPage::setupUi()
{
    QWidget* centerWidget = new QWidget(this);
    centerWidget->setWindowTitle(tr("Api 设置"));
    m_mainLayout = new QVBoxLayout(centerWidget);
    m_mainLayout->setContentsMargins(20, 15, 15, 0);
    m_mainLayout->setSpacing(5);

    const auto apis = toml::find_or_default<toml::array>(m_projectConfig, "backend", "apis");
    for (const auto& api : apis) {
        if (!api.is_table()) {
            continue;
        }
        ElaScrollPageArea* newRowWidget = createApiInputRowWidget(api);
        m_mainLayout->addWidget(newRowWidget);
    }
    if (apis.size() == 0) {
        addApiInputRow();
    }
    updateMoveButtonStates();

    // Api 使用策略
    const std::string strategy = toml::find_or(m_projectConfig, "backend", "apiStrategy", "random");
    bool isRandom = strategy == "random";
    ElaScrollPageArea* apiStrategyArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* apiStrategyLayout = new QHBoxLayout(apiStrategyArea);
    apiStrategyLayout->setSpacing(8);
    ElaDoubleText* apiStrategyTitle = new ElaDoubleText(tr("Api 使用策略"), 16,
        tr("令牌策略，random随机轮询，fallback优先第一个，出现非额度/频率错误时使用下一个"), 10,
        "", apiStrategyArea);
    apiStrategyLayout->addWidget(apiStrategyTitle);
    apiStrategyLayout->addStretch();

    ElaRadioButton* apiStrategyRandom = new ElaRadioButton("random", apiStrategyArea);
    ElaRadioButton* apiStrategyFallback = new ElaRadioButton("fallback", apiStrategyArea);
    apiStrategyRandom->setChecked(isRandom);
    apiStrategyFallback->setChecked(!isRandom);
    apiStrategyLayout->addWidget(apiStrategyRandom);
    apiStrategyLayout->addWidget(apiStrategyFallback);

    QButtonGroup* apiStrategyGroup = new QButtonGroup(this);
    apiStrategyGroup->addButton(apiStrategyRandom, 0);
    apiStrategyGroup->addButton(apiStrategyFallback, 1);

    // Api 超时时间
    int timeout = toml::find_or(m_projectConfig, "backend", "apiTimeout", 300);
    ElaScrollPageArea* apiTimeoutArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* apiTimeoutLayout = new QHBoxLayout(apiTimeoutArea);
    apiTimeoutLayout->setSpacing(8);
    ElaDoubleText* apiTimeoutTitle = new ElaDoubleText(tr("Api 超时时间"), 16,
        tr("Api 请求超时时间，单位为秒"), 10, "", apiTimeoutArea);
    apiTimeoutLayout->addWidget(apiTimeoutTitle);
    apiTimeoutLayout->addStretch();

    ElaSpinBox* apiTimeoutSpinBox = new ElaSpinBox(apiTimeoutArea);
    apiTimeoutSpinBox->setRange(1, 9999);
    apiTimeoutSpinBox->setValue(timeout);
    apiTimeoutLayout->addWidget(apiTimeoutSpinBox);

    // “增加新 Api”按钮
    ElaToolButton* addApiButton = new ElaToolButton(this);
    addApiButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addApiButton->setElaIcon(ElaIconType::Plus);
    addApiButton->setText(tr("增加新 Api"));
    addApiButton->setFixedWidth(120);
    connect(addApiButton, &ElaToolButton::clicked, this, &ApiSettingsPage::addApiInputRow);

    m_applyFunc = [=]()
        {
            apiStrategyGroup->button(0)->isChecked() ? insertToml(m_projectConfig, "backend.apiStrategy", "random")
                : insertToml(m_projectConfig, "backend.apiStrategy", "fallback");
            insertToml(m_projectConfig, "backend.apiTimeout", apiTimeoutSpinBox->value());
        };

    // 将按钮添加到布局中
    m_mainLayout->addWidget(apiStrategyArea);
    m_mainLayout->addWidget(apiTimeoutArea);
    m_mainLayout->addWidget(addApiButton);
    m_mainLayout->addStretch();
    addCentralWidget(centerWidget, true, false, 0);
}

void ApiSettingsPage::addApiInputRow()
{
    ElaScrollPageArea* newRowWidget = createApiInputRowWidget();

    // 获取当前布局中的项目数量
    int count = m_mainLayout->count();
    // count - 4 是因为最后是 stretch, addApiButton, apiStrategyArea, apiTimeoutArea
    m_mainLayout->insertWidget(count - 4, newRowWidget);
    updateMoveButtonStates();
}

ElaScrollPageArea* ApiSettingsPage::createApiInputRowWidget(const toml::value& api)
{
    constexpr int keyEditWidth = 560;
    constexpr int editWidth = 340;
    constexpr int protocolWidth = 130;

    std::vector<std::string> apiKeys;
    if (api.contains("apikeys")) {
        for (const toml::value& keyValue : api.at("apikeys").as_array()) {
            const std::string& keyValueString = keyValue.as_string();
            if (!keyValueString.empty()) {
                apiKeys.push_back(keyValueString);
            }
        }
    }
    const std::string key = apiKeys.empty() ? "" : apiKeys.front();
    const std::string url = toml::find_or(api, "apiurl", "");
    const std::string model = toml::find_or(api, "modelName", "");
    const std::string protocol = toml::find_or(api, "protocol", "openai");
    const std::string thinkingLevel = toml::find_or(api, "thinkingLevel", "off");
    const bool stream = toml::find_or(api, "stream", false);
    const bool enable = toml::find_or(api, "enable", true);
    const bool extraHeadersEnable = toml::find_or(api, "extraHeadersEnable", false);
    const bool extraBodyEnable = toml::find_or(api, "extraBodyEnable", false);

    std::optional<double> temperature;
    std::optional<double> topP;
    std::optional<double> frequencyPenalty;
    std::optional<double> presencePenalty;
    if (api.contains("temperature") && api.at("temperature").is_floating()) {
        temperature = api.at("temperature").as_floating();
    }
    if (api.contains("topP") && api.at("topP").is_floating()) {
        topP = api.at("topP").as_floating();
    }
    if (api.contains("frequencyPenalty") && api.at("frequencyPenalty").is_floating()) {
        frequencyPenalty = api.at("frequencyPenalty").as_floating();
    }
    if (api.contains("presencePenalty") && api.at("presencePenalty").is_floating()) {
        presencePenalty = api.at("presencePenalty").as_floating();
    }

    ElaScrollPageArea* container = new ElaScrollPageArea(this);
    container->setFixedHeight(150);

    QHBoxLayout* containerLayout = new QHBoxLayout(container);

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
    ElaNoWheelComboBox* protocolComboBox = new ElaNoWheelComboBox(formWidget);
    protocolComboBox->setFixedWidth(protocolWidth);
    protocolComboBox->addItems(QStringList{ "openai", "claude", "gemini" });
    if (const int protocolIndex = protocolComboBox->findText(QString::fromStdString(protocol)); protocolIndex >= 0) {
        protocolComboBox->setCurrentIndex(protocolIndex);
    }

    ElaIconButton* moveUpButton = new ElaIconButton(ElaIconType::AngleUp, formWidget);
    connect(moveUpButton, &ElaIconButton::clicked, this, [=]()
        {
            moveApiRow(container, -1);
        });
    ElaIconButton* moveDownButton = new ElaIconButton(ElaIconType::AngleDown, formWidget);
    connect(moveDownButton, &ElaIconButton::clicked, this, [=]()
        {
            moveApiRow(container, 1);
        });

    QHBoxLayout* protocolLabelLayout = new QHBoxLayout();
    protocolLabelLayout->setContentsMargins(0, 0, 0, 0);
    protocolLabelLayout->setSpacing(8);
    protocolLabelLayout->addWidget(protocolLabel);
    protocolLabelLayout->addWidget(moveUpButton);
    protocolLabelLayout->addStretch();
    formLayout->addLayout(protocolLabelLayout, 1, 2);

    QHBoxLayout* protocolComboLayout = new QHBoxLayout();
    protocolComboLayout->setContentsMargins(0, 0, 0, 0);
    protocolComboLayout->setSpacing(8);
    protocolComboLayout->addWidget(protocolComboBox);
    protocolComboLayout->addWidget(moveDownButton);
    protocolComboLayout->addStretch();
    formLayout->addLayout(protocolComboLayout, 2, 2);
    formLayout->setColumnStretch(3, 1);

    QWidget* rightWidget = new QWidget(container);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->addStretch();

    ElaIconButton* deleteButton = new ElaIconButton(ElaIconType::Trash, rightWidget);
    deleteButton->setProperty("containerWidget", QVariant::fromValue<QWidget*>(container));
    rightLayout->addWidget(deleteButton);
    connect(deleteButton, &ElaIconButton::clicked, this, &ApiSettingsPage::onDeleteApiRow);

    ElaText* enableLabel = new ElaText(tr("启用"), 13, rightWidget);
    enableLabel->setWordWrap(false);
    ElaAlignedCheckBox* enableCheckBox = new ElaAlignedCheckBox(rightWidget);
    enableCheckBox->setChecked(enable);
    QHBoxLayout* enableLayout = new QHBoxLayout();
    enableLayout->setContentsMargins(0, 0, 0, 0);
    enableLayout->setSpacing(6);
    enableLayout->addWidget(enableLabel);
    enableLayout->addWidget(enableCheckBox);
    rightLayout->addLayout(enableLayout);

    ElaPushButton* configButton = new ElaPushButton(tr("详细配置"), rightWidget);
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

    ElaScrollPageArea* extraKeysArea = new ElaScrollPageArea(basicPage);
    extraKeysArea->setFixedHeight(230);
    QVBoxLayout* extraKeysLayout = new QVBoxLayout(extraKeysArea);
    extraKeysLayout->setContentsMargins(12, 6, 12, 8);
    extraKeysLayout->setSpacing(6);
    extraKeysLayout->addWidget(new ElaDoubleText("ExtraKeys", 16, "", 0,
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
    ElaPushButton* fetchModelButton = new ElaPushButton(tr("获取模型"), modelActionArea);
    ElaPushButton* testModelButton = new ElaPushButton(tr("测试模型"), modelActionArea);
    modelActionLayout->addWidget(fetchModelButton);
    modelActionLayout->addWidget(testModelButton);
    basicLayout->addWidget(modelActionArea);

    auto [thinkingConfigArea, thinkingConfigLayout] = createFormRow(basicPage);
    thinkingConfigLayout->addWidget(new ElaDoubleText(tr("思考等级"), 16,
        tr("off/low/medium/high，具体效果由接口协议和模型支持情况决定"), 10, "", thinkingConfigArea));
    thinkingConfigLayout->addStretch();
    ElaNoWheelComboBox* thinkingComboBox = new ElaNoWheelComboBox(thinkingConfigArea);
    thinkingComboBox->setFixedWidth(130);
    thinkingComboBox->addItems(QStringList{ "off", "low", "medium", "high" });
    if (const int thinkingIndex = thinkingComboBox->findText(QString::fromStdString(thinkingLevel)); thinkingIndex >= 0) {
        thinkingComboBox->setCurrentIndex(thinkingIndex);
    }
    thinkingConfigLayout->addWidget(thinkingComboBox);
    basicLayout->addWidget(thinkingConfigArea);

    auto [streamConfigArea, streamConfigLayout] = createFormRow(basicPage);
    streamConfigLayout->addWidget(new ElaText(tr("流式输出"), 16, streamConfigArea));
    streamConfigLayout->addStretch();
    ElaToggleSwitch* streamConfigSwitch = new ElaToggleSwitch(streamConfigArea);
    streamConfigSwitch->setIsToggled(stream);
    streamConfigLayout->addWidget(streamConfigSwitch);
    basicLayout->addWidget(streamConfigArea);
    basicLayout->addStretch();
    tabWidget->addTab(basicTabPage.page, tr("基础设置"));

    const ScrollableTabPage advancedTabPage = createScrollablePage(tabWidget);
    QWidget* advancedPage = advancedTabPage.content;
    QVBoxLayout* advancedLayout = advancedTabPage.layout;

    auto [temperatureConfigArea, temperatureConfigLayout] = createFormRow(advancedPage);
    temperatureConfigLayout->addWidget(new ElaDoubleText(tr("温度"), 16,
        tr("勾选选框则使用自定义温度，否则使用供应商默认温度"), 10, "", temperatureConfigArea));
    temperatureConfigLayout->addStretch();
    ValueSliderWidget* temperatureSlider = new ValueSliderWidget(0.0, 2.0, temperatureConfigArea);
    temperatureSlider->setFixedWidth(360);
    temperatureSlider->setDecimals(2);
    temperatureSlider->setValue(temperature.value_or(1.0));
    temperatureConfigLayout->addWidget(temperatureSlider);
    ElaAlignedCheckBox* temperatureCheckBox = new ElaAlignedCheckBox(temperatureConfigArea);
    temperatureCheckBox->setChecked(temperature.has_value());
    temperatureConfigLayout->addWidget(temperatureCheckBox);
    advancedLayout->addWidget(temperatureConfigArea);

    auto [topPConfigArea, topPConfigLayout] = createFormRow(advancedPage);
    topPConfigLayout->addWidget(new ElaDoubleText(tr("top_p"), 16,
        tr("核采样(也是控制随机性的)"), 10, "", topPConfigArea));
    topPConfigLayout->addStretch();
    ValueSliderWidget* topPSlider = new ValueSliderWidget(0.0, 1.0, topPConfigArea);
    topPSlider->setFixedWidth(360);
    topPSlider->setDecimals(2);
    topPSlider->setValue(topP.value_or(1.0));
    topPConfigLayout->addWidget(topPSlider);
    ElaAlignedCheckBox* topPCheckBox = new ElaAlignedCheckBox(topPConfigArea);
    topPCheckBox->setChecked(topP.has_value());
    topPConfigLayout->addWidget(topPCheckBox);
    advancedLayout->addWidget(topPConfigArea);

    auto [frequencyPenaltyConfigArea, frequencyPenaltyConfigLayout] = createFormRow(advancedPage);
    frequencyPenaltyConfigLayout->addWidget(new ElaDoubleText(tr("frequency_penalty"), 16,
        tr("频率惩罚"), 10, "", frequencyPenaltyConfigArea));
    frequencyPenaltyConfigLayout->addStretch();
    ValueSliderWidget* frequencyPenaltySlider = new ValueSliderWidget(-2.0, 2.0, frequencyPenaltyConfigArea);
    frequencyPenaltySlider->setFixedWidth(360);
    frequencyPenaltySlider->setDecimals(2);
    frequencyPenaltySlider->setValue(frequencyPenalty.value_or(0.0));
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltySlider);
    ElaAlignedCheckBox* frequencyPenaltyCheckBox = new ElaAlignedCheckBox(frequencyPenaltyConfigArea);
    frequencyPenaltyCheckBox->setChecked(frequencyPenalty.has_value());
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltyCheckBox);
    advancedLayout->addWidget(frequencyPenaltyConfigArea);

    auto [presencePenaltyConfigArea, presencePenaltyConfigLayout] = createFormRow(advancedPage);
    presencePenaltyConfigLayout->addWidget(new ElaDoubleText(tr("presence_penalty"), 16,
        tr("存在惩罚"), 10, "", presencePenaltyConfigArea));
    presencePenaltyConfigLayout->addStretch();
    ValueSliderWidget* presencePenaltySlider = new ValueSliderWidget(-2.0, 2.0, presencePenaltyConfigArea);
    presencePenaltySlider->setFixedWidth(360);
    presencePenaltySlider->setDecimals(2);
    presencePenaltySlider->setValue(presencePenalty.value_or(0.0));
    presencePenaltyConfigLayout->addWidget(presencePenaltySlider);
    ElaAlignedCheckBox* presencePenaltyCheckBox = new ElaAlignedCheckBox(presencePenaltyConfigArea);
    presencePenaltyCheckBox->setChecked(presencePenalty.has_value());
    presencePenaltyConfigLayout->addWidget(presencePenaltyCheckBox);
    advancedLayout->addWidget(presencePenaltyConfigArea);

    const auto formatJsonObjectText = [&api](const std::string& key) -> QString
        {
            if (!api.contains(key)) {
                return {};
            }
            const toml::value& value = api.at(key);
            if (value.is_string()) {
                return QString::fromStdString(value.as_string());
            }
            if (value.is_table()) {
                return QString::fromStdString(toml2Json(value).dump(2));
            }
            return {};
        };

    ElaScrollPageArea* extraHeadersArea = new ElaScrollPageArea(advancedPage);
    extraHeadersArea->setFixedHeight(180);
    QVBoxLayout* extraHeadersLayout = new QVBoxLayout(extraHeadersArea);
    extraHeadersLayout->setContentsMargins(12, 6, 12, 8);
    extraHeadersLayout->setSpacing(6);
    QHBoxLayout* extraHeadersTitleLayout = new QHBoxLayout();
    extraHeadersTitleLayout->setContentsMargins(0, 0, 0, 0);
    extraHeadersTitleLayout->setSpacing(8);
    extraHeadersTitleLayout->addWidget(new ElaDoubleText("extraHeaders", 16,
        tr("JSON 对象，用于追加自定义 HTTP header"), 10, "", extraHeadersArea));
    extraHeadersTitleLayout->addStretch();
    ElaAlignedCheckBox* extraHeadersCheckBox = new ElaAlignedCheckBox(extraHeadersArea);
    extraHeadersCheckBox->setChecked(extraHeadersEnable);
    extraHeadersTitleLayout->addWidget(extraHeadersCheckBox);
    extraHeadersLayout->addLayout(extraHeadersTitleLayout);
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
    QHBoxLayout* extraBodyTitleLayout = new QHBoxLayout();
    extraBodyTitleLayout->setContentsMargins(0, 0, 0, 0);
    extraBodyTitleLayout->setSpacing(8);
    extraBodyTitleLayout->addWidget(new ElaDoubleText("extraBody", 16,
        tr("JSON 对象，用于追加或覆盖请求 body 字段"), 10, "", extraBodyArea));
    extraBodyTitleLayout->addStretch();
    ElaAlignedCheckBox* extraBodyCheckBox = new ElaAlignedCheckBox(extraBodyArea);
    extraBodyCheckBox->setChecked(extraBodyEnable);
    extraBodyTitleLayout->addWidget(extraBodyCheckBox);
    extraBodyLayout->addLayout(extraBodyTitleLayout);
    ElaPlainTextEdit* extraBodyEdit = new ElaPlainTextEdit(extraBodyArea);
    extraBodyEdit->setFixedHeight(115);
    extraBodyEdit->setPlaceholderText("{\n  \"response_format\": { \"type\": \"json_object\" },\n  \"seed\": 1\n}");
    installTreeSitterHighlighter(extraBodyEdit->document(), SyntaxLanguage::Json);
    extraBodyEdit->setPlainText(formatJsonObjectText("extraBody"));
    extraBodyLayout->addWidget(extraBodyEdit);
    advancedLayout->addWidget(extraBodyArea);
    advancedLayout->addStretch();
    tabWidget->addTab(advancedTabPage.page, tr("高级设置"));

    auto showApiResultWindow = [=](const QString& title, const QString& content)
        {
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

            QWidget* mainWindow = window();
            const int resultHeight = mainWindow ? std::clamp(mainWindow->height() - 160, 420, 620) : 520;
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
            lines << tr("HTTP 状态: %1").arg(result.statusCode);
            lines << tr("请求结果: %1").arg(result.success ? tr("成功") : tr("失败"));
            lines << "";
            lines << tr("解析到的模型: ");
            if (result.models.empty()) {
                lines << tr("(没有解析到模型)");
            }
            else {
                for (const std::string& model_ : result.models) {
                    lines << QString::fromStdString(model_);
                }
            }
            if (!result.success && !result.content.empty()) {
                lines << "";
                lines << tr("错误信息: ");
                lines << QString::fromStdString(result.content);
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
            lines << tr("HTTP 状态: %1").arg(result.statusCode);
            lines << tr("请求结果: %1").arg(result.success ? tr("成功") : tr("失败"));
            lines << "";
            lines << (result.success ? tr("解析出的模型回复: ") : tr("错误信息: "));
            lines << (result.content.empty() ? tr("(空)") : QString::fromStdString(result.content));
            return lines.join('\n');
        };

    auto parseJsonObjectForRequest = [](const ElaPlainTextEdit* edit, const QString& title, json& value)
        {
            value = json::object();
            const QString text = edit->toPlainText().trimmed();
            if (text.isEmpty()) {
                return true;
            }
            try {
                value = json::parse(text.toStdString());
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
            api_.thinkingLevel = thinkingComboBox->currentText().toStdString();
            api_.stream = streamConfigSwitch->getIsToggled();
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
                json extraHeaders = json::object();
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
                if (!parseJsonObjectForRequest(extraBodyEdit, "extraBody", api_.extraBody)) {
                    return false;
                }
            }
            return true;
        };

    auto setModelRequestButtonsEnabled = [=](bool enabled)
        {
            fetchModelButton->setEnabled(enabled);
            testModelButton->setEnabled(enabled);
        };

    auto apiRequestTimeoutMs = [=]()
        {
            const int timeoutSeconds = toml::find_or(m_projectConfig, "backend", "apiTimeout", 300);
            return std::max(1, timeoutSeconds) * 1000;
        };

    auto runModelRequest = [=](bool queryModels)
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
            QPointer<ElaPushButton> fetchButtonPtr(fetchModelButton);
            QPointer<ElaPushButton> testButtonPtr(testModelButton);
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
                                if (result.success && !result.models.empty()) {
                                    QStringList modelLines;
                                    for (const std::string& model_ : result.models) {
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
                                    result.success ? tr("请求成功，但没有解析到模型") : tr("模型列表请求失败"), 3000);
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
                            if (result.success) {
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

    connect(fetchModelButton, &ElaPushButton::clicked, this, [=]()
        {
            runModelRequest(true);
        });
    connect(testModelButton, &ElaPushButton::clicked, this, [=]()
        {
            runModelRequest(false);
        });

    configWidget->hide();
    connect(configButton, &ElaPushButton::clicked, this, [=]()
        {
            QWidget* mainWindow = window();
            if (mainWindow) {
                configWidget->move(mainWindow->frameGeometry().center()
                    - configWidget->rect().center());
            }
            configWidget->show();
            configWidget->raise();
            configWidget->activateWindow();
        });

    containerLayout->addWidget(formWidget, 1);
    containerLayout->addWidget(rightWidget);

    ApiRowControls newRowControls;
    newRowControls.container = container;
    newRowControls.configWidget = configWidget;
    newRowControls.moveUpButton = moveUpButton;
    newRowControls.moveDownButton = moveDownButton;
    newRowControls.applyFunc = [=](toml::ordered_array& apiArray)
        {
            if (urlEdit->text().isEmpty()) {
                return;
            }
            toml::ordered_table apiTable;
            apiTable.insert({ "protocol", protocolComboBox->currentText().toStdString() });
            toml::ordered_array apiKeysArray;
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
            apiTable.insert({ "apikeys", apiKeysArray });
            apiTable.insert({ "apiurl", urlEdit->text().toStdString() });
            apiTable.insert({ "modelName", modelEdit->text().toStdString() });
            apiTable.insert({ "stream", streamConfigSwitch->getIsToggled() });
            apiTable.insert({ "enable", enableCheckBox->isChecked() });
            apiTable.insert({ "thinkingLevel", thinkingComboBox->currentText().toStdString() });
            if (temperatureCheckBox->isChecked()) {
                apiTable.insert({ "temperature", temperatureSlider->value() });
            }
            if (topPCheckBox->isChecked()) {
                apiTable.insert({ "topP", topPSlider->value() });
            }
            if (frequencyPenaltyCheckBox->isChecked()) {
                apiTable.insert({ "frequencyPenalty", frequencyPenaltySlider->value() });
            }
            if (presencePenaltyCheckBox->isChecked()) {
                apiTable.insert({ "presencePenalty", presencePenaltySlider->value() });
            }
            apiTable.insert({ "extraHeadersEnable", extraHeadersCheckBox->isChecked() });
            apiTable.insert({ "extraBodyEnable", extraBodyCheckBox->isChecked() });
            if (!extraHeadersEdit->toPlainText().trimmed().isEmpty()) {
                json extraHeaders;
                if (parseJsonObjectForRequest(extraHeadersEdit, "extraHeaders", extraHeaders)) {
                    apiTable.insert({ "extraHeaders", json2Toml(extraHeaders) });
                }
            }
            if (!extraBodyEdit->toPlainText().trimmed().isEmpty()) {
                json extraBody;
                if (parseJsonObjectForRequest(extraBodyEdit, "extraBody", extraBody)) {
                    apiTable.insert({ "extraBody", json2Toml(extraBody) });
                }
            }
            apiArray.push_back(std::move(apiTable));
        };
    m_apiRows.append(newRowControls);

    return container;
}

void ApiSettingsPage::moveApiRow(ElaScrollPageArea* container, int offset)
{
    const int rowIndex = [&]()
        {
            for (int i = 0; i < m_apiRows.size(); ++i) {
                if (m_apiRows.at(i).container == container) {
                    return i;
                }
            }
            return -1;
        }();
    const int targetIndex = rowIndex + offset;
    if (rowIndex < 0 || targetIndex < 0 || targetIndex >= m_apiRows.size()) {
        return;
    }

    const int layoutIndex = m_mainLayout->indexOf(container);
    if (layoutIndex < 0) {
        return;
    }

    m_mainLayout->removeWidget(container);
    m_mainLayout->insertWidget(layoutIndex + offset, container);
    m_apiRows.move(rowIndex, targetIndex);
    updateMoveButtonStates();
}

void ApiSettingsPage::updateMoveButtonStates()
{
    for (int i = 0; i < m_apiRows.size(); ++i) {
        m_apiRows.at(i).moveUpButton->setEnabled(i > 0);
        m_apiRows.at(i).moveDownButton->setEnabled(i < m_apiRows.size() - 1);
    }
}

void ApiSettingsPage::onDeleteApiRow()
{
    ElaIconButton* deleteButton = qobject_cast<ElaIconButton*>(sender());
    if (!deleteButton) {
        return;
    }

    QWidget* containerWidget = deleteButton->property("containerWidget").value<QWidget*>();
    if (!containerWidget) {
        return;
    }

    for (int i = 0; i < m_apiRows.size(); ++i) {
        if (m_apiRows.at(i).container == containerWidget) {
            m_apiRows.at(i).configWidget->deleteLater();
            m_apiRows.removeAt(i);
            break;
        }
    }

    m_mainLayout->removeWidget(containerWidget);
    containerWidget->deleteLater();
    updateMoveButtonStates();
}
