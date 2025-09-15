// APISettingsPage.cpp

#include "APISettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaPushButton.h"
#include "ElaRadioButton.h"
#include "ElaIconButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "ElaToolTip.h"
#include "ElaIcon.h"

import Tool;

APISettingsPage::APISettingsPage(toml::table& projectConfig, QWidget* parent)
    : BasePage(parent), _projectConfig(projectConfig)
{
    setWindowTitle("API 设置");
    setTitleVisible(false);
    _setupUI();
}

APISettingsPage::~APISettingsPage()
{
}

void APISettingsPage::apply2Config()
{
    toml::array apiArray;
    for (const auto& apiRow : _apiRows) {
        if (apiRow.urlEdit->text().isEmpty())
        {
            continue;
        }
        toml::table apiTable;
        apiTable.insert("apikey", apiRow.keyEdit->text().toStdString());
        apiTable.insert("apiurl", apiRow.urlEdit->text().toStdString());
        apiTable.insert("modelName", apiRow.modelEdit->text().toStdString());
        apiTable.insert("stream", apiRow.streamSwitch->getIsToggled());
        apiArray.push_back(apiTable);
    }
    insertToml(_projectConfig, "backendSpecific.OpenAI-Compatible.apis", apiArray);
    if (_applyFunc) {
        _applyFunc();
    }
}

void APISettingsPage::_setupUI()
{

    QWidget* centerWidget = new QWidget(this);
    centerWidget->setWindowTitle("API 设置");
    _mainLayout = new QVBoxLayout(centerWidget);
    _mainLayout->setContentsMargins(5, 5, 5, 5);
    _mainLayout->setSpacing(15);

    auto apis = _projectConfig["backendSpecific"]["OpenAI-Compatible"]["apis"].as_array();
    if (apis) {
        for (const auto& api : *apis) {
            auto tbl = api.as_table();
            if (!tbl) {
                continue;
            }
            std::string key = (*tbl)["apikey"].value_or("");
            std::string url = (*tbl)["apiurl"].value_or("");
            std::string model = (*tbl)["modelName"].value_or("");
            bool stream = (*tbl)["stream"].value_or(false);
            ElaScrollPageArea* newRowWidget = _createApiInputRowWidget(QString::fromStdString(key), QString::fromStdString(url), QString::fromStdString(model), stream);
            _mainLayout->addWidget(newRowWidget);
        }
        if (apis->size() == 0) {
            _addApiInputRow();
        }
    }

    // API 使用策略
    std::string strategy = _projectConfig["backendSpecific"]["OpenAI-Compatible"]["apiStrategy"].value_or("");
    bool isRandom = strategy == "random";
    ElaScrollPageArea* apiStrategyArea = new ElaScrollPageArea(this);
    QHBoxLayout* apiStrategyLayout = new QHBoxLayout(apiStrategyArea);
    ElaText* apiStrategyTitle = new ElaText("API 使用策略", apiStrategyArea);
    ElaToolTip* apiStrategyTip = new ElaToolTip(apiStrategyTitle);
    apiStrategyTip->setToolTip("令牌策略，random随机轮询，fallback优先第一个，出现[请求错误]时使用下一个");
    apiStrategyTitle->setTextPixelSize(15);
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
    int timeout = _projectConfig["backendSpecific"]["OpenAI-Compatible"]["apiTimeout"].value_or(180);
    ElaScrollPageArea* apiTimeoutArea = new ElaScrollPageArea(this);
    QHBoxLayout* apiTimeoutLayout = new QHBoxLayout(apiTimeoutArea);
    ElaText* apiTimeoutTitle = new ElaText("API 超时时间", apiTimeoutArea);
    ElaToolTip* apiTimeoutTip = new ElaToolTip(apiTimeoutTitle);
    apiTimeoutTip->setToolTip("API 请求超时时间，单位为秒");
    apiTimeoutTitle->setWordWrap(false);
    apiTimeoutTitle->setTextPixelSize(15);
    apiTimeoutLayout->addWidget(apiTimeoutTitle);
    apiTimeoutLayout->addStretch();

    ElaSpinBox* apiTimeoutSpinBox = new ElaSpinBox(this);
    apiTimeoutSpinBox->setRange(1, 999);
    apiTimeoutSpinBox->setValue(timeout);
    apiTimeoutLayout->addWidget(apiTimeoutSpinBox);

    // “增加新 API”按钮
    ElaPushButton* addApiButton = new ElaPushButton("增加新 API", this);
    addApiButton->setFixedWidth(120);
    connect(addApiButton, &ElaPushButton::clicked, this, &APISettingsPage::_addApiInputRow);

    _applyFunc = [=]()
        {
            apiStrategyGroup->button(0)->isChecked() ? insertToml(_projectConfig, "backendSpecific.OpenAI-Compatible.apiStrategy", "random")
                : insertToml(_projectConfig, "backendSpecific.OpenAI-Compatible.apiStrategy", "fallback");
            insertToml(_projectConfig, "backendSpecific.OpenAI-Compatible.apiTimeout", apiTimeoutSpinBox->value());
        };

    // 将按钮添加到布局中
    _mainLayout->addWidget(apiStrategyArea);
    _mainLayout->addWidget(apiTimeoutArea);
    _mainLayout->addWidget(addApiButton);
    _mainLayout->addStretch(); // 弹簧，将内容向上推
    addCentralWidget(centerWidget, true, true, 0);
}

// 【重构】这个函数现在只负责调用创建函数并插入到正确位置
void APISettingsPage::_addApiInputRow()
{
    ElaScrollPageArea* newRowWidget = _createApiInputRowWidget();

    // 获取当前布局中的项目数量
    int count = _mainLayout->count();
    // count - 4 是因为最后是 stretch, addApiButton, apiStrategyArea, apiTimeoutArea
    _mainLayout->insertWidget(count - 4, newRowWidget);
}

// 【新增】这个函数创建一整行带边框和删除按钮的UI
ElaScrollPageArea* APISettingsPage::_createApiInputRowWidget(const QString& key, const QString& url, const QString& model, bool stream)
{
    // 1. 创建带边框的容器 ElaScrollPageArea
    ElaScrollPageArea* container = new ElaScrollPageArea(this);
    container->setFixedHeight(200);

    // 2. 创建水平主布局
    QHBoxLayout* containerLayout = new QHBoxLayout(container);

    // 3. 创建左侧的表单布局
    QWidget* formContainer = new QWidget(container);
    formContainer->setFixedWidth(650);
    QVBoxLayout* formLayout = new QVBoxLayout(formContainer);
    formLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* apiKeyContainer = new QWidget(formContainer);
    QHBoxLayout* apiKeyLayout = new QHBoxLayout(apiKeyContainer);
    ElaText* apiKeyLabel = new ElaText("API Key", apiKeyContainer);
    apiKeyLabel->setTextPixelSize(13);
    apiKeyLabel->setFixedWidth(80);
    apiKeyLayout->addWidget(apiKeyLabel);
    ElaLineEdit* keyEdit = new ElaLineEdit(apiKeyContainer);
    if (!key.isEmpty()) {
        keyEdit->setText(key);
    }
    else {
        keyEdit->setPlaceholderText("请输入 API Key(Sakura引擎可不填)");
    }
    apiKeyLayout->addWidget(keyEdit);
    formLayout->addWidget(apiKeyContainer);

    QWidget* apiUrlContainer = new QWidget(formContainer);
    QHBoxLayout* apiSecretLayout = new QHBoxLayout(apiUrlContainer);
    ElaText* apiUrlLabel = new ElaText("API Url", apiUrlContainer);
    apiUrlLabel->setTextPixelSize(13);
    apiUrlLabel->setFixedWidth(80);
    apiSecretLayout->addWidget(apiUrlLabel);
    ElaLineEdit* urlEdit = new ElaLineEdit(apiUrlContainer);
    if (!url.isEmpty()) {
        urlEdit->setText(url);
    }
    else {
        urlEdit->setPlaceholderText("请输入 API Url");
    }
    apiSecretLayout->addWidget(urlEdit);
    formLayout->addWidget(apiUrlContainer);

    QWidget* modelContainer = new QWidget(formContainer);
    QHBoxLayout* modelLayout = new QHBoxLayout(modelContainer);
    ElaText* modelLabel = new ElaText("模型名称", modelContainer);
    modelLabel->setTextPixelSize(13);
    modelLabel->setFixedWidth(80);
    modelLayout->addWidget(modelLabel);
    ElaLineEdit* modelEdit = new ElaLineEdit(modelContainer);
    if (!model.isEmpty()) {
        modelEdit->setText(model);
    }
    else {
        modelEdit->setPlaceholderText("请输入模型名称(Sakura引擎可不填)");
    }
    modelLayout->addWidget(modelEdit);
    formLayout->addWidget(modelContainer);

    // 4. 创建右侧的删除按钮 和 流式开关
    QWidget* rightContainer = new QWidget(container);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->addStretch();

    ElaIconButton* deleteButton = new ElaIconButton(ElaIconType::Trash, this);
    // 使用 QObject::setProperty 来给按钮附加它所属容器的指针
    deleteButton->setProperty("containerWidget", QVariant::fromValue<QWidget*>(container));
    rightLayout->addWidget(deleteButton);
    connect(deleteButton, &ElaIconButton::clicked, this, &APISettingsPage::_onDeleteApiRow);
    QWidget* streamContainer = new QWidget(rightContainer);
    QHBoxLayout* streamLayout = new QHBoxLayout(streamContainer);
    streamLayout->addStretch();
    ElaText* streamLabel = new ElaText("流式", streamContainer);
    streamLabel->setTextPixelSize(13);
    streamLayout->addWidget(streamLabel);
    ElaToggleSwitch* streamSwitch = new ElaToggleSwitch(streamContainer);
    streamSwitch->setIsToggled(stream);
    streamLayout->addWidget(streamSwitch);
    rightLayout->addWidget(streamContainer);
    rightLayout->addStretch();

    // 5. 组合布局
    containerLayout->addWidget(formContainer);
    containerLayout->addWidget(rightContainer, 0, Qt::AlignRight);

    // 6. 存储这组控件的引用
    ApiRowControls newRowControls;
    newRowControls.container = container;
    newRowControls.keyEdit = keyEdit;
    newRowControls.urlEdit = urlEdit;
    newRowControls.modelEdit = modelEdit;
    newRowControls.streamSwitch = streamSwitch;
    _apiRows.append(newRowControls);

    return container;
}

void APISettingsPage::_onDeleteApiRow()
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
    for (int i = 0; i < _apiRows.size(); ++i) {
        if (_apiRows.at(i).container == containerWidget) {
            _apiRows.removeAt(i);
            break;
        }
    }

    // 从布局中移除并删除该容器
    _mainLayout->removeWidget(containerWidget);
    containerWidget->deleteLater(); // 使用 deleteLater() 更安全
}
