#include "CommonSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QButtonGroup>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaPlainTextEdit.h"
#include "ElaLineEdit.h"
#include "ElaComboBox.h"
#include "ElaScrollPageArea.h"
#include "ElaRadioButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "ElaPushButton.h"
#include "ElaToolTip.h"

import Tool;

CommonSettingsPage::CommonSettingsPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("一般设置");
	setTitleVisible(false);
	_setupUI();
}

CommonSettingsPage::~CommonSettingsPage()
{

}

void CommonSettingsPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}

void CommonSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(5);

	// 单次请求翻译句子数量
	int requestNum = _projectConfig["common"]["numPerRequestTranslate"].value_or(8);
	ElaScrollPageArea* requestNumArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* requestNumLayout = new QHBoxLayout(requestNumArea);
	ElaText* requestNumText = new ElaText("单次请求翻译句子数量", requestNumArea);
	requestNumText->setWordWrap(false);
	requestNumText->setTextPixelSize(16);
	ElaToolTip* requestNumTip = new ElaToolTip(requestNumText);
	requestNumTip->setToolTip("推荐值 < 15");
	requestNumLayout->addWidget(requestNumText);
	requestNumLayout->addStretch();
	ElaSpinBox* requestNumSpinBox = new ElaSpinBox(requestNumArea);
	requestNumSpinBox->setFocus();
	requestNumSpinBox->setRange(1, 100);
	requestNumSpinBox->setValue(requestNum);
	requestNumLayout->addWidget(requestNumSpinBox);
	connect(requestNumSpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "common.numPerRequestTranslate", value);
		});
	mainLayout->addWidget(requestNumArea);

	// 最大线程数
	int maxThread = _projectConfig["common"]["threadsNum"].value_or(1);
	ElaScrollPageArea* maxThreadArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* maxThreadLayout = new QHBoxLayout(maxThreadArea);
	ElaText* maxThreadText = new ElaText("最大线程数", maxThreadArea);
	maxThreadText->setTextPixelSize(16);
	maxThreadLayout->addWidget(maxThreadText);
	maxThreadLayout->addStretch();
	ElaSpinBox* maxThreadSpinBox = new ElaSpinBox(maxThreadArea);
	maxThreadSpinBox->setRange(1, 100);
	maxThreadSpinBox->setValue(maxThread);
	maxThreadLayout->addWidget(maxThreadSpinBox);
	connect(maxThreadSpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "common.threadsNum", value);
		});
	mainLayout->addWidget(maxThreadArea);

	// 翻译顺序，name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度[name/size]
	std::string order = _projectConfig["common"]["sortMethod"].value_or("size");
	ElaScrollPageArea* orderArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* orderLayout = new QHBoxLayout(orderArea);
	ElaText* orderText = new ElaText("翻译顺序", orderArea);
	orderText->setTextPixelSize(16);
	ElaToolTip* orderTip = new ElaToolTip(orderText);
	orderTip->setToolTip("name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度");
	orderLayout->addWidget(orderText);
	orderLayout->addStretch();
	QButtonGroup* orderGroup = new QButtonGroup(orderArea);
	ElaRadioButton* orderNameRadio = new ElaRadioButton("文件名", orderArea);
	orderNameRadio->setChecked(order == "name");
	orderLayout->addWidget(orderNameRadio);
	ElaRadioButton* orderSizeRadio = new ElaRadioButton("文件大小", orderArea);
	orderSizeRadio->setChecked(order == "size");
	orderLayout->addWidget(orderSizeRadio);
	orderGroup->addButton(orderNameRadio, 0);
	orderGroup->addButton(orderSizeRadio, 1);
	connect(orderGroup, &QButtonGroup::buttonToggled, this, [=](QAbstractButton* button, bool checked)
		{
			if (checked) {
				std::string value = button->text().toStdString();
				if (value == "文件名") {
					value = "name";
				}
				else if (value == "文件大小") {
					value = "size";
				}
				insertToml(_projectConfig, "common.sortMethod", value);
			}
		});
	mainLayout->addWidget(orderArea);

	// 翻译到的目标语言，包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]
	std::string target = _projectConfig["common"]["targetLang"].value_or("zh-cn");
	QString targetStr = QString::fromStdString(target);
	ElaScrollPageArea* targetArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* targetLayout = new QHBoxLayout(targetArea);
	ElaText* targetText = new ElaText("翻译到的目标语言", targetArea);
	targetText->setTextPixelSize(16);
	ElaToolTip* targetTip = new ElaToolTip(targetText);
	targetTip->setToolTip("包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]");
	targetLayout->addWidget(targetText);
	targetLayout->addStretch();
	ElaLineEdit* targetLineEdit = new ElaLineEdit(targetArea);
	targetLineEdit->setFixedWidth(150);
	targetLineEdit->setText(targetStr);
	targetLayout->addWidget(targetLineEdit);
	mainLayout->addWidget(targetArea);

	// 是否启用单文件分割。Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割。[No/Num/Equal]
	std::string split = _projectConfig["common"]["splitFile"].value_or("No");
	ElaScrollPageArea* splitArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* splitLayout = new QHBoxLayout(splitArea);
	ElaText* splitText = new ElaText("单文件分割", splitArea);
	splitText->setTextPixelSize(16);
	ElaToolTip* splitTip = new ElaToolTip(splitText);
	splitTip->setToolTip("Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割。");
	splitLayout->addWidget(splitText);
	splitLayout->addStretch();
	QButtonGroup* splitGroup = new QButtonGroup(splitArea);
	ElaRadioButton* splitNoRadio = new ElaRadioButton("No", splitArea);
	splitNoRadio->setChecked(split == "No");
	splitLayout->addWidget(splitNoRadio);
	ElaRadioButton* splitNumRadio = new ElaRadioButton("Num", splitArea);
	splitNumRadio->setChecked(split == "Num");
	splitLayout->addWidget(splitNumRadio);
	ElaRadioButton* splitEqualRadio = new ElaRadioButton("Equal", splitArea);
	splitEqualRadio->setChecked(split == "Equal");
	splitLayout->addWidget(splitEqualRadio);
	splitGroup->addButton(splitNoRadio, 0);
	splitGroup->addButton(splitNumRadio, 1);
	splitGroup->addButton(splitEqualRadio, 2);
	connect(splitGroup, &QButtonGroup::buttonToggled, this, [=](QAbstractButton* button, bool checked)
		{
			if (checked) {
				std::string value = button->text().toStdString();
				insertToml(_projectConfig, "common.splitFile", value);
			}
		});
	mainLayout->addWidget(splitArea);

	// Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分。
	int splitNum = _projectConfig["common"]["splitFileNum"].value_or(1024);
	ElaScrollPageArea* splitNumArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* splitNumLayout = new QHBoxLayout(splitNumArea);
	ElaText* splitNumText = new ElaText("分割数量", splitNumArea);
	splitNumText->setTextPixelSize(16);
	ElaToolTip* splitNumTip = new ElaToolTip(splitNumText);
	splitNumTip->setToolTip("Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分。");
	splitNumLayout->addWidget(splitNumText);
	splitNumLayout->addStretch();
	ElaSpinBox* splitNumSpinBox = new ElaSpinBox(splitNumArea);
	splitNumSpinBox->setRange(1, 10000);
	splitNumSpinBox->setValue(splitNum);
	splitNumLayout->addWidget(splitNumSpinBox);
	if (splitGroup->button(0)->isChecked()) {
		splitNumArea->setVisible(false);
	}
	// 不启用单文件分割时，隐藏分割数量(或许之后可以做成抽屉)
	connect(splitGroup, &QButtonGroup::buttonToggled, this, [=](QAbstractButton* button, bool checked)
		{
			if (!checked) {
				return;
			}
			if (button == splitNoRadio) {
				splitNumArea->setVisible(false);
			}
			else {
				splitNumArea->setVisible(true);
			}
		});
	connect(splitNumSpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "common.splitFileNum", value);
		});
	mainLayout->addWidget(splitNumArea);

	// 每翻译n次保存一次缓存
	int saveInterval = _projectConfig["common"]["saveCacheInterval"].value_or(1);
	ElaScrollPageArea* cacheArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* cacheLayout = new QHBoxLayout(cacheArea);
	ElaText* cacheText = new ElaText("缓存保存间隔", cacheArea);
	cacheText->setTextPixelSize(16);
	ElaToolTip* cacheTip = new ElaToolTip(cacheText);
	cacheTip->setToolTip("每翻译n次保存一次缓存");
	cacheLayout->addWidget(cacheText);
	cacheLayout->addStretch();
	ElaSpinBox* cacheSpinBox = new ElaSpinBox(cacheArea);
	cacheSpinBox->setRange(1, 10000);
	cacheSpinBox->setValue(saveInterval);
	cacheLayout->addWidget(cacheSpinBox);
	connect(cacheSpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "common.saveCacheInterval", value);
		});
	mainLayout->addWidget(cacheArea);

	// 最大重试次数
	int maxRetries = _projectConfig["common"]["maxRetries"].value_or(5);
	ElaScrollPageArea* retryArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* retryLayout = new QHBoxLayout(retryArea);
	ElaText* retryText = new ElaText("最大重试次数", retryArea);
	retryText->setTextPixelSize(16);
	retryLayout->addWidget(retryText);
	retryLayout->addStretch();
	ElaSpinBox* retrySpinBox = new ElaSpinBox(retryArea);
	retrySpinBox->setRange(1, 100);
	retrySpinBox->setValue(maxRetries);
	retryLayout->addWidget(retrySpinBox);
	connect(retrySpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "common.maxRetries", value);
		});
	mainLayout->addWidget(retryArea);

	// 携带上文数量
	int contextNum = _projectConfig["common"]["contextHistorySize"].value_or(8);
	ElaScrollPageArea* contextArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* contextLayout = new QHBoxLayout(contextArea);
	ElaText* contextText = new ElaText("携带上文数量", contextArea);
	contextText->setTextPixelSize(16);
	contextLayout->addWidget(contextText);
	contextLayout->addStretch();
	ElaSpinBox* contextSpinBox = new ElaSpinBox(contextArea);
	contextSpinBox->setRange(1, 100);
	contextSpinBox->setValue(contextNum);
	contextLayout->addWidget(contextSpinBox);
	connect(contextSpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "common.contextHistorySize", value);
		});
	mainLayout->addWidget(contextArea);

	// 智能重试  # 解析结果失败时尝试折半重翻与清空上下文，避免无效重试。
	bool smartRetry = _projectConfig["common"]["smartRetry"].value_or(true);
	ElaScrollPageArea* smartRetryArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* smartRetryLayout = new QHBoxLayout(smartRetryArea);
	ElaText* smartRetryText = new ElaText("智能重试", smartRetryArea);
	smartRetryText->setTextPixelSize(16);
	ElaToolTip* smartRetryTip = new ElaToolTip(smartRetryText);
	smartRetryTip->setToolTip("解析结果失败时尝试折半重翻与清空上下文，避免无效重试。");
	smartRetryLayout->addWidget(smartRetryText);
	smartRetryLayout->addStretch();
	ElaToggleSwitch* smartRetryToggle = new ElaToggleSwitch(smartRetryArea);
	smartRetryToggle->setIsToggled(smartRetry);
	smartRetryLayout->addWidget(smartRetryToggle);
	connect(smartRetryToggle, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "common.smartRetry", checked);
		});
	mainLayout->addWidget(smartRetryArea);

	// 额度检测 # 运行时动态检测key额度
	bool checkQuota = _projectConfig["common"]["checkQuota"].value_or(true);
	ElaScrollPageArea* checkQuotaArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* checkQuotaLayout = new QHBoxLayout(checkQuotaArea);
	ElaText* checkQuotaText = new ElaText("额度检测", checkQuotaArea);
	checkQuotaText->setTextPixelSize(16);
	ElaToolTip* checkQuotaTip = new ElaToolTip(checkQuotaText);
	checkQuotaTip->setToolTip("运行时动态检测key额度");
	checkQuotaLayout->addWidget(checkQuotaText);
	checkQuotaLayout->addStretch();
	ElaToggleSwitch* checkQuotaToggle = new ElaToggleSwitch(checkQuotaArea);
	checkQuotaToggle->setIsToggled(checkQuota);
	checkQuotaLayout->addWidget(checkQuotaToggle);
	connect(checkQuotaToggle, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "common.checkQuota", checked);
		});
	mainLayout->addWidget(checkQuotaArea);

	// 项目日志级别
	std::string logLevel = _projectConfig["common"]["logLevel"].value_or("info");
	QString logLevelStr = QString::fromStdString(logLevel);
	ElaScrollPageArea* logArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* logLayout = new QHBoxLayout(logArea);
	ElaText* logText = new ElaText("日志级别", logArea);
	logText->setTextPixelSize(16);
	logLayout->addWidget(logText);
	logLayout->addStretch();
	ElaComboBox* logComboBox = new ElaComboBox(logArea);
	logComboBox->addItem("trace");
	logComboBox->addItem("debug");
	logComboBox->addItem("info");
	logComboBox->addItem("warn");
	logComboBox->addItem("err");
	logComboBox->addItem("critical");
	if (!logLevelStr.isEmpty()) {
		int index = logComboBox->findText(logLevelStr);
		if (index >= 0) {
			logComboBox->setCurrentIndex(index);
		}
	}
	logLayout->addWidget(logComboBox);
	connect(logComboBox, &QComboBox::currentTextChanged, this, [=](const QString& text)
		{
			insertToml(_projectConfig, "common.logLevel", text.toStdString());
		});
	mainLayout->addWidget(logArea);

	// 保存项目日志
	bool saveLog = _projectConfig["common"]["saveLog"].value_or(true);
	ElaScrollPageArea* saveLogArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* saveLogLayout = new QHBoxLayout(saveLogArea);
	ElaText* saveLogText = new ElaText("保存项目日志", saveLogArea);
	saveLogText->setTextPixelSize(16);
	saveLogLayout->addWidget(saveLogText);
	saveLogLayout->addStretch();
	ElaToggleSwitch* saveLogToggle = new ElaToggleSwitch(saveLogArea);
	saveLogToggle->setIsToggled(saveLog);
	saveLogLayout->addWidget(saveLogToggle);
	connect(saveLogToggle, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "common.saveLog", checked);
		});
	mainLayout->addWidget(saveLogArea);

	// 分词器所用的词典的路径
	std::string dictPath = _projectConfig["common"]["dictDir"].value_or("DictGenerator/mecab-ipadic-utf8");
	QString dictPathStr = QString::fromStdString(dictPath);
	ElaScrollPageArea* dictArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* dictLayout = new QHBoxLayout(dictArea);
	ElaText* dictText = new ElaText("分词器词典路径", dictArea);
	ElaToolTip* dictTip = new ElaToolTip(dictText);
	dictTip->setToolTip("可以使用相对路径");
	dictText->setTextPixelSize(16);
	dictLayout->addWidget(dictText);
	dictLayout->addStretch();
	ElaLineEdit* dictLineEdit = new ElaLineEdit(dictArea);
	dictLineEdit->setFixedWidth(400);
	dictLineEdit->setText(dictPathStr);
	dictLayout->addWidget(dictLineEdit);
	ElaPushButton* dictButton = new ElaPushButton("浏览", dictArea);
	dictLayout->addWidget(dictButton);

	connect(dictButton, &QPushButton::clicked, this, [=](bool checked)
		{
			// 打开文件夹选择对话框
			QString path = QFileDialog::getExistingDirectory(this, "选择词典文件夹", dictLineEdit->text());
			if (!path.isEmpty()) {
				dictLineEdit->setText(path);
			}
		});
	mainLayout->addWidget(dictArea);

	

	mainLayout->addStretch();

	_applyFunc = [=]()
		{
			insertToml(_projectConfig, "common.targetLang", targetLineEdit->text().toStdString());
			insertToml(_projectConfig, "common.dictDir", dictLineEdit->text().toStdString());
		};
	addCentralWidget(mainWidget, true, true, 0);
}