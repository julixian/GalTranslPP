#include "APISettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPlainTextEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaPushButton.h"
#include "ElaRadioButton.h"
#include "ElaIconButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaDoubleText.h"
#include "ElaCheckBox.h"
#include "ValueSliderWidget.h"
#include "ElaWidget.h"

import Tool;

APISettingsPage::APISettingsPage(toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("API 设置"));
    setTitleVisible(false);

    setupUi();
}

APISettingsPage::~APISettingsPage()
{
}

void APISettingsPage::apply2Config()
{
    toml::ordered_array apiArray;
    for (const auto& apiRow : m_apiRows) {
        apiRow.applyFunc(apiArray);
    }
    insertToml(m_projectConfig, "backendSpecific.OpenAI-Compatible.apis", apiArray);
    if (m_applyFunc) {
        m_applyFunc();
    }
}

void APISettingsPage::setupUi()
{
    QWidget* centerWidget = new QWidget(this);
    centerWidget->setWindowTitle(tr("API 设置"));
    m_mainLayout = new QVBoxLayout(centerWidget);
    m_mainLayout->setContentsMargins(20, 15, 15, 0);
    m_mainLayout->setSpacing(5);

    const auto apis = toml::find_or_default<toml::array>(m_projectConfig, "backendSpecific", "OpenAI-Compatible", "apis");
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

    // API 使用策略
    const std::string strategy = toml::find_or(m_projectConfig, "backendSpecific", "OpenAI-Compatible", "apiStrategy", "");
    bool isRandom = strategy == "random";
    ElaScrollPageArea* apiStrategyArea = new ElaScrollPageArea(this);
    QHBoxLayout* apiStrategyLayout = new QHBoxLayout(apiStrategyArea);
    ElaDoubleText* apiStrategyTitle = new ElaDoubleText(tr("API 使用策略"), 16, tr("令牌策略，random随机轮询，fallback优先第一个，出现[请求错误]时使用下一个"), 10, "", apiStrategyArea);
    apiStrategyLayout->addWidget(apiStrategyTitle);
    apiStrategyLayout->addStretch();

    ElaRadioButton* apiStrategyRandom = new ElaRadioButton("random", this);
    ElaRadioButton* apiStrategyFallback = new ElaRadioButton("fallback", this);
    apiStrategyRandom->setChecked(isRandom);
    apiStrategyFallback->setChecked(!isRandom);
    apiStrategyLayout->addWidget(apiStrategyRandom);
    apiStrategyLayout->addWidget(apiStrategyFallback);

    QButtonGroup* apiStrategyGroup = new QButtonGroup(this);
    apiStrategyGroup->addButton(apiStrategyRandom, 0);
    apiStrategyGroup->addButton(apiStrategyFallback, 1);

    // API 超时时间
    int timeout = toml::find_or(m_projectConfig, "backendSpecific", "OpenAI-Compatible", "apiTimeout", 180);
    ElaScrollPageArea* apiTimeoutArea = new ElaScrollPageArea(this);
    QHBoxLayout* apiTimeoutLayout = new QHBoxLayout(apiTimeoutArea);
    ElaDoubleText* apiTimeoutTitle = new ElaDoubleText(tr("API 超时时间"), 16, tr("API 请求超时时间，单位为秒"), 10, "", apiTimeoutArea);
    apiTimeoutLayout->addWidget(apiTimeoutTitle);
    apiTimeoutLayout->addStretch();

    ElaSpinBox* apiTimeoutSpinBox = new ElaSpinBox(this);
    apiTimeoutSpinBox->setRange(1, 999);
    apiTimeoutSpinBox->setValue(timeout);
    apiTimeoutLayout->addWidget(apiTimeoutSpinBox);

    // “增加新 API”按钮
    ElaPushButton* addApiButton = new ElaPushButton(tr("增加新 API"), this);
    addApiButton->setFixedWidth(120);
    connect(addApiButton, &ElaPushButton::clicked, this, &APISettingsPage::addApiInputRow);

    m_applyFunc = [=]()
        {
            apiStrategyGroup->button(0)->isChecked() ? insertToml(m_projectConfig, "backendSpecific.OpenAI-Compatible.apiStrategy", "random")
                : insertToml(m_projectConfig, "backendSpecific.OpenAI-Compatible.apiStrategy", "fallback");
            insertToml(m_projectConfig, "backendSpecific.OpenAI-Compatible.apiTimeout", apiTimeoutSpinBox->value());
        };

    // 将按钮添加到布局中
    m_mainLayout->addWidget(apiStrategyArea);
    m_mainLayout->addWidget(apiTimeoutArea);
    m_mainLayout->addWidget(addApiButton);
    m_mainLayout->addStretch();
    addCentralWidget(centerWidget, true, false, 0);
}

// 【重构】这个函数现在只负责调用创建函数并插入到正确位置
void APISettingsPage::addApiInputRow()
{
    ElaScrollPageArea* newRowWidget = createApiInputRowWidget();

    // 获取当前布局中的项目数量
    int count = m_mainLayout->count();
    // count - 4 是因为最后是 stretch, addApiButton, apiStrategyArea, apiTimeoutArea
    m_mainLayout->insertWidget(count - 4, newRowWidget);
}

// 【新增】这个函数创建一整行带边框和删除按钮的UI
ElaScrollPageArea* APISettingsPage::createApiInputRowWidget(const toml::value& api)
{
    const std::string key = toml::find_or(api, "apikey", "");
    const std::string url = toml::find_or(api, "apiurl", "");
    const std::string model = toml::find_or(api, "modelName", "");
    bool stream = toml::find_or(api, "stream", false);
    bool enable = toml::find_or(api, "enable", true);
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

    // 1. 创建带边框的容器 ElaScrollPageArea
    ElaScrollPageArea* container = new ElaScrollPageArea(this);
    container->setFixedHeight(150);

    // 2. 创建水平主布局
    QHBoxLayout* containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(10, 0, 10, 0);

    // 3. 创建左侧的表单布局
    QWidget* formWidget = new QWidget(container);
    formWidget->setFixedWidth(650);
    QVBoxLayout* formLayout = new QVBoxLayout(formWidget);
    formLayout->setContentsMargins(0, 10, 0, 10);

    QHBoxLayout* apiKeyLayout = new QHBoxLayout(formWidget);
    ElaText* apiKeyLabel = new ElaText("API key", formWidget);
    apiKeyLabel->setTextPixelSize(13);
    apiKeyLabel->setFixedWidth(100);
    apiKeyLayout->addWidget(apiKeyLabel);
    ElaLineEdit* keyEdit = new ElaLineEdit(formWidget);
    if (!key.empty()) {
        keyEdit->setText(QString::fromStdString(key));
    }
    else {
        keyEdit->setPlaceholderText(tr("请输入 API key(Sakura引擎或有Extra keys时可不填)"));
    }
    apiKeyLayout->addWidget(keyEdit);
    formLayout->addLayout(apiKeyLayout);

    QHBoxLayout* apiSecretLayout = new QHBoxLayout(formWidget);
    ElaText* apiUrlLabel = new ElaText("API url", formWidget);
    apiUrlLabel->setTextPixelSize(13);
    apiUrlLabel->setFixedWidth(100);
    apiSecretLayout->addWidget(apiUrlLabel);
    ElaLineEdit* urlEdit = new ElaLineEdit(formWidget);
    if (!url.empty()) {
        urlEdit->setText(QString::fromStdString(url));
    }
    else {
        urlEdit->setPlaceholderText(tr("请输入 API url"));
    }
    apiSecretLayout->addWidget(urlEdit);
    formLayout->addLayout(apiSecretLayout);

    QHBoxLayout* modelLayout = new QHBoxLayout(formWidget);
    ElaText* modelLabel = new ElaText(tr("模型名称"), formWidget);
    modelLabel->setWordWrap(false);
    modelLabel->setTextPixelSize(13);
    modelLabel->setFixedWidth(100);
    modelLayout->addWidget(modelLabel);
    ElaLineEdit* modelEdit = new ElaLineEdit(formWidget);
    if (!model.empty()) {
        modelEdit->setText(QString::fromStdString(model));
    }
    else {
        modelEdit->setPlaceholderText(tr("请输入模型名称(Sakura引擎可不填)"));
    }
    modelLayout->addWidget(modelEdit);
    formLayout->addLayout(modelLayout);

    // 4. 创建右侧的删除按钮
    QVBoxLayout* rightLayout = new QVBoxLayout(container);
    rightLayout->addStretch();

    ElaIconButton* deleteButton = new ElaIconButton(ElaIconType::Trash, this);
    // 使用 QObject::setProperty 来给按钮附加它所属容器的指针
    deleteButton->setProperty("containerWidget", QVariant::fromValue<QWidget*>(container));
    rightLayout->addWidget(deleteButton, 0, Qt::AlignCenter);
    connect(deleteButton, &ElaIconButton::clicked, this, &APISettingsPage::onDeleteApiRow);

    QHBoxLayout* enableLayout = new QHBoxLayout(container);
    ElaText* enableLabel = new ElaText(tr("启用"), 13, container);
    enableLayout->addWidget(enableLabel, 0, Qt::AlignCenter);
    ElaToggleSwitch* enableSwitch = new ElaToggleSwitch(container);
    enableSwitch->setIsToggled(enable);
    enableLayout->addWidget(enableSwitch, 0, Qt::AlignCenter);
    rightLayout->addLayout(enableLayout);

    ElaPushButton* configButton = new ElaPushButton(tr("高级配置"), container);
    ElaWidget* configWidget = new ElaWidget();
    configWidget->setFixedWidth(950);
    configWidget->setFixedHeight(650);
    configWidget->setWindowTitle(tr("API 高级配置"));
    configWidget->setWindowModality(Qt::ApplicationModal);
    configWidget->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    QVBoxLayout* configLayout = new QVBoxLayout(configWidget);
    configLayout->setContentsMargins(5, 25, 5, 0);

    ElaScrollPageArea* streamConfigArea = new ElaScrollPageArea(configWidget);
    QHBoxLayout* streamConfigLayout = new QHBoxLayout(streamConfigArea);
    ElaText* streamConfigTitle = new ElaText(tr("流式输出"), 16, streamConfigArea);
    streamConfigLayout->addWidget(streamConfigTitle);
    streamConfigLayout->addStretch();
    ElaToggleSwitch* streamConfigSwitch = new ElaToggleSwitch(streamConfigArea);
    streamConfigSwitch->setIsToggled(stream);
    streamConfigLayout->addWidget(streamConfigSwitch);
    configLayout->addWidget(streamConfigArea);

    ElaScrollPageArea* temperatureConfigArea = new ElaScrollPageArea(configWidget);
    QHBoxLayout* temperatureConfigLayout = new QHBoxLayout(temperatureConfigArea);
    ElaDoubleText* temperatureConfigTitle = new ElaDoubleText(tr("温度"), 16, tr("勾选选框则使用自定义温度，否则使用供应商默认温度"), 10, "", temperatureConfigArea);
    temperatureConfigLayout->addWidget(temperatureConfigTitle);
    temperatureConfigLayout->addStretch();
    ValueSliderWidget* temperatureSlider = new ValueSliderWidget(0.0, 2.0, temperatureConfigArea);
    temperatureSlider->setFixedWidth(400);
    temperatureSlider->setDecimals(2);
    if (temperature.has_value()) {
        temperatureSlider->setValue(*temperature);
    }
    else {
        temperatureSlider->setValue(1.0);
    }
    temperatureConfigLayout->addWidget(temperatureSlider);
    ElaCheckBox* temperatureCheckBox = new ElaCheckBox(temperatureConfigArea);
    temperatureCheckBox->setChecked(temperature.has_value());
    temperatureConfigLayout->addWidget(temperatureCheckBox);
    configLayout->addWidget(temperatureConfigArea);

    ElaScrollPageArea* topPConfigArea = new ElaScrollPageArea(configWidget);
    topPConfigArea->setFixedHeight(80);
    QHBoxLayout* topPConfigLayout = new QHBoxLayout(topPConfigArea);
    ElaDoubleText* topPConfigTitle = new ElaDoubleText(tr("top_p"), 16, tr("核采样(也是控制随机性的)"), 10, "", topPConfigArea);
    topPConfigLayout->addWidget(topPConfigTitle);
    topPConfigLayout->addStretch();
    ValueSliderWidget* topPSlider = new ValueSliderWidget(0.0, 1.0, topPConfigArea);
    topPSlider->setFixedWidth(400);
    topPSlider->setDecimals(2);
    if (topP.has_value()) {
        topPSlider->setValue(*topP);
    }
    else {
        topPSlider->setValue(1.0);
    }
    topPConfigLayout->addWidget(topPSlider);
    ElaCheckBox* topPCheckBox = new ElaCheckBox(topPConfigArea);
    topPCheckBox->setChecked(topP.has_value());
    topPConfigLayout->addWidget(topPCheckBox);
    configLayout->addWidget(topPConfigArea);

    ElaScrollPageArea* frequencyPenaltyConfigArea = new ElaScrollPageArea(configWidget);
    frequencyPenaltyConfigArea->setFixedHeight(80);
    QHBoxLayout* frequencyPenaltyConfigLayout = new QHBoxLayout(frequencyPenaltyConfigArea);
    ElaDoubleText* frequencyPenaltyConfigTitle = new ElaDoubleText(tr("frequency_penalty"), 16, tr("频率惩罚"), 10, "", frequencyPenaltyConfigArea);
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltyConfigTitle);
    frequencyPenaltyConfigLayout->addStretch();
    ValueSliderWidget* frequencyPenaltySlider = new ValueSliderWidget(-2.0, 2.0, frequencyPenaltyConfigArea);
    frequencyPenaltySlider->setFixedWidth(400);
    frequencyPenaltySlider->setDecimals(2);
    if (frequencyPenalty.has_value()) {
        frequencyPenaltySlider->setValue(*frequencyPenalty);
    }
    else {
        frequencyPenaltySlider->setValue(0.0);
    }
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltySlider);
    ElaCheckBox* frequencyPenaltyCheckBox = new ElaCheckBox(frequencyPenaltyConfigArea);
    frequencyPenaltyCheckBox->setChecked(frequencyPenalty.has_value());
    frequencyPenaltyConfigLayout->addWidget(frequencyPenaltyCheckBox);
    configLayout->addWidget(frequencyPenaltyConfigArea);

    ElaScrollPageArea* presencePenaltyConfigArea = new ElaScrollPageArea(configWidget);
    presencePenaltyConfigArea->setFixedHeight(80);
    QHBoxLayout* presencePenaltyConfigLayout = new QHBoxLayout(presencePenaltyConfigArea);
    ElaDoubleText* presencePenaltyConfigTitle = new ElaDoubleText(tr("presence_penalty"), 16, tr("存在惩罚"), 10, "", presencePenaltyConfigArea);
    presencePenaltyConfigLayout->addWidget(presencePenaltyConfigTitle);
    presencePenaltyConfigLayout->addStretch();
    ValueSliderWidget* presencePenaltySlider = new ValueSliderWidget(-2.0, 2.0, presencePenaltyConfigArea);
    presencePenaltySlider->setFixedWidth(400);
    presencePenaltySlider->setDecimals(2);
    if (presencePenalty.has_value()) {
        presencePenaltySlider->setValue(*presencePenalty);
    }
    else {
        presencePenaltySlider->setValue(0.0);
    }
    presencePenaltyConfigLayout->addWidget(presencePenaltySlider);
    ElaCheckBox* presencePenaltyCheckBox = new ElaCheckBox(presencePenaltyConfigArea);
    presencePenaltyCheckBox->setChecked(presencePenalty.has_value());
    presencePenaltyConfigLayout->addWidget(presencePenaltyCheckBox);
    configLayout->addWidget(presencePenaltyConfigArea);

    configLayout->addSpacing(10);
    ElaDoubleText* extraKeysTitle = new ElaDoubleText(tr("Extra keys"), 18, "", 0, "一行一个 key", configWidget);
    configLayout->addWidget(extraKeysTitle);
    ElaPlainTextEdit* extraKeysEdit = new ElaPlainTextEdit(configWidget);
    if (api.contains("extraKeys") && api.at("extraKeys").is_array()) {
        std::string extraKeysStr = api.at("extraKeys").as_array()
            | std::views::filter([](const toml::value& v) { return v.is_string(); })
            | std::views::transform([](const toml::value& v) { return v.as_string(); })
            | std::views::join_with('\n')
            | std::ranges::to<std::string>();
        extraKeysEdit->setPlainText(QString::fromStdString(extraKeysStr));
    }
    configLayout->addWidget(extraKeysEdit);


    configLayout->addStretch();
    configWidget->hide();
    connect(configButton, &ElaPushButton::clicked, this, [=]()
        {
            configWidget->moveToCenter();
            configWidget->resize(950, 650);
            configWidget->show();
        });
    rightLayout->addWidget(configButton, 0, Qt::AlignCenter);
    rightLayout->addStretch();

    // 5. 组合布局
    containerLayout->addWidget(formWidget);
    containerLayout->addStretch();
    containerLayout->addLayout(rightLayout);

    // 6. 存储这组控件的引用
    ApiRowControls newRowControls;
    newRowControls.container = container;
    newRowControls.configWidget = configWidget;
    newRowControls.applyFunc = [=](toml::ordered_array& apiArray)
        {
            if (urlEdit->text().isEmpty()) {
                return;
            }
            toml::ordered_table apiTable;
            apiTable.insert({ "apikey", keyEdit->text().toStdString() });
            apiTable.insert({ "apiurl", urlEdit->text().toStdString() });
            apiTable.insert({ "modelName", modelEdit->text().toStdString() });
            apiTable.insert({ "stream", streamConfigSwitch->getIsToggled() });
            apiTable.insert({ "enable", enableSwitch->getIsToggled() });
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
            if (!extraKeysEdit->toPlainText().isEmpty()) {
                toml::ordered_array newExtraKeysArr;
                std::stringstream ss(extraKeysEdit->toPlainText().toStdString());
                std::string line;
                while (std::getline(ss, line)) {
                    if (!line.empty()) {
                        newExtraKeysArr.push_back(line);
                    }
                }
                if (!newExtraKeysArr.empty()) {
                     apiTable.insert({ "extraKeys", newExtraKeysArr });
                }
            }
            apiArray.push_back(std::move(apiTable));
        };
    m_apiRows.append(newRowControls);

    return container;
}

void APISettingsPage::onDeleteApiRow()
{
    // 获取信号的发送者，即被点击的删除按钮
    ElaIconButton* deleteButton = qobject_cast<ElaIconButton*>(sender());
    if (!deleteButton) {
        return;
    }

    // 通过 property 获取它所关联的容器 QFrame
    QWidget* containerWidget = deleteButton->property("containerWidget").value<QWidget*>();
    if (!containerWidget) {
        return;
    }

    // 从列表中找到并移除对应的控件组
    for (int i = 0; i < m_apiRows.size(); ++i) {
        if (m_apiRows.at(i).container == containerWidget) {
            m_apiRows.at(i).configWidget->deleteLater();
            m_apiRows.removeAt(i);
            break;
        }
    }

    // 从布局中移除并删除该容器
    m_mainLayout->removeWidget(containerWidget);
    containerWidget->deleteLater(); // 使用 deleteLater() 更安全
}
