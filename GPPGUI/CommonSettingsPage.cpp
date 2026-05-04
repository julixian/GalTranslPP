#include "CommonSettingsPage.h"

#include <QHBoxLayout>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "ElaDrawerArea.h"
#include "ElaLineEdit.h"
#include "ElaComboBox.h"
#include "ElaScrollPageArea.h"
#include "ElaRadioButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "ElaPushButton.h"
#include "ElaToolTip.h"
#include "ElaDoubleText.h"

import Tool;

CommonSettingsPage::CommonSettingsPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle(tr("一般设置"));
	setTitleVisible(false);

	_setupUI();
}

CommonSettingsPage::~CommonSettingsPage()
{

}

void CommonSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(20, 15, 15, 0);

	// 单次请求翻译句子数量
	int requestNum = toml::find_or(_projectConfig, "common", "numPerRequestTranslate", 16);
	ElaScrollPageArea* requestNumArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* requestNumLayout = new QHBoxLayout(requestNumArea);
	ElaDoubleText* requestNumText = new ElaDoubleText(requestNumArea,
		tr("单次请求翻译句子数量"), 16, 
		tr("推荐值 <= 16"), 10, "");
	requestNumLayout->addWidget(requestNumText);
	requestNumLayout->addStretch();
	ElaSpinBox* requestNumSpinBox = new ElaSpinBox(requestNumArea);
	requestNumSpinBox->setFocus();
	requestNumSpinBox->setRange(1, 9999);
	requestNumSpinBox->setValue(requestNum);
	requestNumLayout->addWidget(requestNumSpinBox);
	mainLayout->addWidget(requestNumArea);

	// 最大线程数
	int maxThread = toml::find_or(_projectConfig, "common", "threadsNum", 1);
	ElaScrollPageArea* maxThreadArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* maxThreadLayout = new QHBoxLayout(maxThreadArea);
	ElaText* maxThreadText = new ElaText(tr("最大线程数"), 16, maxThreadArea);
	maxThreadLayout->addWidget(maxThreadText);
	maxThreadLayout->addStretch();
	ElaSpinBox* maxThreadSpinBox = new ElaSpinBox(maxThreadArea);
	maxThreadSpinBox->setRange(1, 9999);
	maxThreadSpinBox->setValue(maxThread);
	maxThreadLayout->addWidget(maxThreadSpinBox);
	mainLayout->addWidget(maxThreadArea);

	// 翻译顺序，name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度[name/size]
	std::string transOrder = toml::find_or(_projectConfig, "common", "sortMethod", "size");
	ElaScrollPageArea* transOrderArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* transOrderLayout = new QHBoxLayout(transOrderArea);
	ElaDoubleText* transOrderText = new ElaDoubleText(transOrderArea,
		tr("翻译顺序"), 16, 
		tr("name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度"), 10, "");
	transOrderLayout->addWidget(transOrderText);
	transOrderLayout->addStretch();
	QButtonGroup* transOrderGroup = new QButtonGroup(transOrderArea);
	ElaRadioButton* transOrderNameRadio = new ElaRadioButton(tr("文件名"), transOrderArea);
	transOrderNameRadio->setChecked(transOrder == "name");
	transOrderLayout->addWidget(transOrderNameRadio);
	ElaRadioButton* transOrderSizeRadio = new ElaRadioButton(tr("文件大小"), transOrderArea);
	transOrderSizeRadio->setChecked(transOrder == "size");
	transOrderLayout->addWidget(transOrderSizeRadio);
	transOrderGroup->addButton(transOrderNameRadio, 0);
	transOrderGroup->addButton(transOrderSizeRadio, 1);
	mainLayout->addWidget(transOrderArea);

	// 翻译到的目标语言，包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]
	std::string targetLang = toml::find_or(_projectConfig, "common", "targetLang", "zh-cn");
	QString targetLangQStr = QString::fromStdString(targetLang);
	ElaScrollPageArea* targetLangArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* targetLangLayout = new QHBoxLayout(targetLangArea);
	ElaDoubleText* targetLangText = new ElaDoubleText(targetLangArea,
		tr("翻译到的目标语言"), 16, 
		tr("包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]"), 10, "");
	targetLangLayout->addWidget(targetLangText);
	targetLangLayout->addStretch();
	ElaLineEdit* targetLangLineEdit = new ElaLineEdit(targetLangArea);
	targetLangLineEdit->setFixedWidth(150);
	targetLangLineEdit->setText(targetLangQStr);
	targetLangLayout->addWidget(targetLangLineEdit);
	mainLayout->addWidget(targetLangArea);

	// 是否启用单文件分割。Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割。[No/Num/Equal]
	std::string splitFileSetting = toml::find_or(_projectConfig, "common", "splitFile", "No");
	ElaDrawerArea* splitSettingsDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* splitSettingsArea = new QWidget(splitSettingsDrawerArea);
	splitSettingsDrawerArea->setDrawerHeader(splitSettingsArea);
	QHBoxLayout* splitSettingsLayout = new QHBoxLayout(splitSettingsArea);
	ElaDoubleText* splitSettingsText = new ElaDoubleText(splitSettingsArea,
		tr("单文件分割"), 16, 
		tr("Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割"), 10, "");
	splitSettingsLayout->addWidget(splitSettingsText);
	splitSettingsLayout->addStretch();
	QButtonGroup* splitSettingsGroup = new QButtonGroup(splitSettingsArea);
	ElaRadioButton* splitNoRadio = new ElaRadioButton("No", splitSettingsArea);
	splitNoRadio->setChecked(splitFileSetting == "No");
	splitSettingsLayout->addWidget(splitNoRadio);
	ElaRadioButton* splitNumRadio = new ElaRadioButton("Num", splitSettingsArea);
	splitNumRadio->setChecked(splitFileSetting == "Num");
	splitSettingsLayout->addWidget(splitNumRadio);
	ElaRadioButton* splitEqualRadio = new ElaRadioButton("Equal", splitSettingsArea);
	splitEqualRadio->setChecked(splitFileSetting == "Equal");
	splitSettingsLayout->addWidget(splitEqualRadio);
	splitSettingsGroup->addButton(splitNoRadio, 0);
	splitSettingsGroup->addButton(splitNumRadio, 1);
	splitSettingsGroup->addButton(splitEqualRadio, 2);
	connect(splitSettingsGroup, &QButtonGroup::buttonToggled, this, [=](QAbstractButton* button, bool checked)
		{
			if (checked) {
				if (button->text() == "No") {
					splitSettingsDrawerArea->collapse();
				}
				else {
					splitSettingsDrawerArea->expand();
				}
			}
		});
	connect(splitSettingsDrawerArea, &ElaDrawerArea::expandStateChanged, this, [=](bool expanded)
		{
			if (expanded && splitSettingsGroup->button(0)->isChecked()) {
				splitSettingsDrawerArea->collapse();
			}
		});
	mainLayout->addWidget(splitSettingsDrawerArea);

	// Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分。
	int splitNum = toml::find_or(_projectConfig, "common", "splitFileNum", 1024);
	ElaScrollPageArea* splitNumArea = new ElaScrollPageArea(splitSettingsDrawerArea);
	QHBoxLayout* splitNumLayout = new QHBoxLayout(splitNumArea);
	ElaDoubleText* splitNumText = new ElaDoubleText(splitNumArea,
		tr("分割数量"), 16, 
		tr("Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分"), 10, "");
	splitNumLayout->addWidget(splitNumText);
	splitNumLayout->addStretch();
	ElaSpinBox* splitNumSpinBox = new ElaSpinBox(splitNumArea);
	splitNumSpinBox->setRange(1, 10000);
	splitNumSpinBox->setValue(splitNum);
	splitNumLayout->addWidget(splitNumSpinBox);
	splitSettingsDrawerArea->addDrawer(splitNumArea);

	// 分割缓存查找距离
	int cacheSearchDistance = toml::find_or(_projectConfig, "common", "cacheSearchDistance", 5);
	ElaScrollPageArea* cacheSearchDistanceArea = new ElaScrollPageArea(splitSettingsDrawerArea);
	QHBoxLayout* cacheSearchDistanceLayout = new QHBoxLayout(cacheSearchDistanceArea);
	ElaDoubleText* cacheSearchDistanceText = new ElaDoubleText(cacheSearchDistanceArea,
		tr("分割缓存查找距离"), 16, 
		tr("将自身索引 ±N 的分割文件均视为当前分割文件的缓存"), 10, tr("数值越大可能占用更多内存"));
	cacheSearchDistanceLayout->addWidget(cacheSearchDistanceText);
	cacheSearchDistanceLayout->addStretch();
	ElaSpinBox* cacheSearchDistanceSpinBox = new ElaSpinBox(cacheSearchDistanceArea);
	cacheSearchDistanceSpinBox->setRange(0, 9999);
	cacheSearchDistanceSpinBox->setValue(cacheSearchDistance);
	cacheSearchDistanceLayout->addWidget(cacheSearchDistanceSpinBox);
	splitSettingsDrawerArea->addDrawer(cacheSearchDistanceArea);
	if (splitFileSetting == "No") {
		splitSettingsDrawerArea->collapse();
	}
	else {
		splitSettingsDrawerArea->expand();
	}

	// 每翻译n次保存一次缓存
	int cacheSaveInterval = toml::find_or(_projectConfig, "common", "saveCacheInterval", 1);
	ElaScrollPageArea* cacheSaveIntervalArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* cacheSaveIntervalLayout = new QHBoxLayout(cacheSaveIntervalArea);
	ElaDoubleText* cacheSaveIntervalText = new ElaDoubleText(cacheSaveIntervalArea,
		tr("缓存保存间隔"), 16, 
		tr("每翻译n次保存一次缓存"), 10, "");
	cacheSaveIntervalLayout->addWidget(cacheSaveIntervalText);
	cacheSaveIntervalLayout->addStretch();
	ElaSpinBox* cacheSaveIntervalSpinBox = new ElaSpinBox(cacheSaveIntervalArea);
	cacheSaveIntervalSpinBox->setRange(1, 9999);
	cacheSaveIntervalSpinBox->setValue(cacheSaveInterval);
	cacheSaveIntervalLayout->addWidget(cacheSaveIntervalSpinBox);
	mainLayout->addWidget(cacheSaveIntervalArea);

	// 最大重试次数
	int maxRetryNum = toml::find_or(_projectConfig, "common", "maxRetries", 5);
	ElaScrollPageArea* maxRetryArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* maxRetryLayout = new QHBoxLayout(maxRetryArea);
	ElaText* retryText = new ElaText(tr("最大重试次数"), 16, maxRetryArea);
	maxRetryLayout->addWidget(retryText);
	maxRetryLayout->addStretch();
	ElaSpinBox* retrySpinBox = new ElaSpinBox(maxRetryArea);
	retrySpinBox->setRange(1, 9999);
	retrySpinBox->setValue(maxRetryNum);
	maxRetryLayout->addWidget(retrySpinBox);
	mainLayout->addWidget(maxRetryArea);

	// 携带上文数量
	int contextNum = toml::find_or(_projectConfig, "common", "contextHistorySize", 8);
	ElaScrollPageArea* contextNumArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* contextNumLayout = new QHBoxLayout(contextNumArea);
	ElaDoubleText* contextNumText = new ElaDoubleText(contextNumArea,
		tr("携带上文数量"), 16,
		tr("推荐值 ≤ 10"), 10, "");
	contextNumLayout->addWidget(contextNumText);
	contextNumLayout->addStretch();
	ElaSpinBox* contextNumSpinBox = new ElaSpinBox(contextNumArea);
	contextNumSpinBox->setRange(1, 9999);
	contextNumSpinBox->setValue(contextNum);
	contextNumLayout->addWidget(contextNumSpinBox);
	mainLayout->addWidget(contextNumArea);

	// 智能重试
	bool useSmartRetry = toml::find_or(_projectConfig, "common", "smartRetry", false);
	ElaScrollPageArea* smartRetryArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* smartRetryLayout = new QHBoxLayout(smartRetryArea);
	ElaDoubleText* smartRetryTextWidget = new ElaDoubleText(smartRetryArea,
		tr("智能重试"), 16, 
		tr("解析结果失败时尝试折半重翻与清空上下文"), 10, "如果用的打野 key 其实不建议开这个");
	smartRetryLayout->addWidget(smartRetryTextWidget);
	smartRetryLayout->addStretch();
	ElaToggleSwitch* smartRetryToggle = new ElaToggleSwitch(smartRetryArea);
	smartRetryToggle->setIsToggled(useSmartRetry);
	smartRetryLayout->addWidget(smartRetryToggle);
	mainLayout->addWidget(smartRetryArea);

	// 额度检测
	bool shouldCheckQuota = toml::find_or(_projectConfig, "common", "checkQuota", true);
	ElaScrollPageArea* checkQuotaArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* checkQuotaLayout = new QHBoxLayout(checkQuotaArea);
	ElaDoubleText* checkQuotaTextWidget = new ElaDoubleText(checkQuotaArea,
		tr("额度检测"), 16, 
		tr("运行时动态检测 key 额度，自动从 API 池中删除额度不足的 key"), 10, "");
	checkQuotaLayout->addWidget(checkQuotaTextWidget);
	checkQuotaLayout->addStretch();
	ElaToggleSwitch* checkQuotaToggle = new ElaToggleSwitch(checkQuotaArea);
	checkQuotaToggle->setIsToggled(shouldCheckQuota);
	checkQuotaLayout->addWidget(checkQuotaToggle);
	mainLayout->addWidget(checkQuotaArea);

	// retransAllWhenFail/解析不完整时重翻整段
	bool retransAllWhenFail = toml::find_or(_projectConfig, "common", "retransAllWhenFail", false);
	ElaScrollPageArea* retransAllWhenFailArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* retransAllWhenFailLayout = new QHBoxLayout(retransAllWhenFailArea);
	ElaDoubleText* retransAllWhenFailText = new ElaDoubleText(retransAllWhenFailArea,
		tr("解析不完整时重翻整段"), 16,
		tr("不开启则仅重翻漏掉的部分，开启可增加模型因串行而导致解析失败时的容错"), 10,
		tr("默认关闭以节省token/防止因模型截断造成无限循环"));
	retransAllWhenFailLayout->addWidget(retransAllWhenFailText);
	retransAllWhenFailLayout->addStretch();
	ElaToggleSwitch* retransAllWhenFailToggle = new ElaToggleSwitch(retransAllWhenFailArea);
	retransAllWhenFailToggle->setIsToggled(retransAllWhenFail);
	retransAllWhenFailLayout->addWidget(retransAllWhenFailToggle);
	mainLayout->addWidget(retransAllWhenFailArea);

	// 项目日志设置
	bool shouldSaveLog = toml::find_or(_projectConfig, "common", "saveLog", true);
	ElaDrawerArea* logSettingsDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* logSettingsArea = new QWidget(logSettingsDrawerArea);
	logSettingsDrawerArea->setDrawerHeader(logSettingsArea);
	QHBoxLayout* logSettingsLayout = new QHBoxLayout(logSettingsArea);
	ElaText* logSettingsText = new ElaText(tr("项目日志设置"), 16, logSettingsArea);
	logSettingsLayout->addWidget(logSettingsText);
	logSettingsLayout->addStretch();
	ElaText* saveLogText = new ElaText(tr("保存项目日志"), 16, logSettingsArea);
	ElaToggleSwitch* saveLogToggle = new ElaToggleSwitch(logSettingsArea);
	saveLogToggle->setIsToggled(shouldSaveLog);
	logSettingsLayout->addWidget(saveLogText);
	logSettingsLayout->addWidget(saveLogToggle);

	mainLayout->addWidget(logSettingsDrawerArea);


	// 项目日志级别
	std::string logLevel = toml::find_or(_projectConfig, "common", "logLevel", "info");
	QString logLevelQStr = QString::fromStdString(logLevel);
	ElaScrollPageArea* logLevelArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* logLevelLayout = new QHBoxLayout(logLevelArea);
	ElaText* logLevelText = new ElaText(tr("日志级别"), 16, logLevelArea);
	logLevelLayout->addWidget(logLevelText);
	logLevelLayout->addStretch();
	ElaComboBox* logLevelComboBox = new ElaComboBox(logLevelArea);
	logLevelComboBox->addItem("trace");
	logLevelComboBox->addItem("debug");
	logLevelComboBox->addItem("info");
	logLevelComboBox->addItem("warn");
	logLevelComboBox->addItem("err");
	logLevelComboBox->addItem("critical");
	if (!logLevelQStr.isEmpty()) {
		int index = logLevelComboBox->findText(logLevelQStr);
		if (index >= 0) {
			logLevelComboBox->setCurrentIndex(index);
		}
	}
	logLevelLayout->addWidget(logLevelComboBox);
	logSettingsDrawerArea->addDrawer(logLevelArea);

	// logFileMaxSize/log 文件大小限制
	int logFileMaxSize = toml::find_or(_projectConfig, "common", "logFileMaxSize", 1024 * 1024 * 10);
	int logFileMaxSizeKb = logFileMaxSize / 1024;
	if (logFileMaxSizeKb == 0) {
		logFileMaxSizeKb = 1;
	}
	ElaScrollPageArea* logFileMaxSizeArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* logFileMaxSizeLayout = new QHBoxLayout(logFileMaxSizeArea);
	ElaText* logFileMaxSizeText = new ElaText(tr("单个 log 文件大小限制"), 16, logFileMaxSizeArea);
	logFileMaxSizeText->setWordWrap(false);
	logFileMaxSizeLayout->addWidget(logFileMaxSizeText);
	logFileMaxSizeLayout->addStretch();
	ElaSpinBox* logFileMaxSizeSpinBox = new ElaSpinBox(logFileMaxSizeArea);
	logFileMaxSizeSpinBox->setRange(1, 1024 * 1024);
	logFileMaxSizeSpinBox->setValue(logFileMaxSizeKb);
	logFileMaxSizeLayout->addWidget(logFileMaxSizeSpinBox);
	ElaText* kbText = new ElaText("KB", 16, logFileMaxSizeArea);
	logFileMaxSizeLayout->addWidget(kbText);
	logSettingsDrawerArea->addDrawer(logFileMaxSizeArea);

	// maxRotateFiles/log 文件滚动数量上限
	int maxRotateFiles = toml::find_or(_projectConfig, "common", "maxRotateFiles", 1);
	if (maxRotateFiles == 0) {
		maxRotateFiles = 1;
	}
	ElaScrollPageArea* maxRotateFilesArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* maxRotateFilesLayout = new QHBoxLayout(maxRotateFilesArea);
	ElaText* maxRotateFilesText = new ElaText(tr("log 文件滚动数量上限"), 16, maxRotateFilesArea);
	maxRotateFilesText->setWordWrap(false);
	maxRotateFilesLayout->addWidget(maxRotateFilesText);
	maxRotateFilesLayout->addStretch();
	ElaSpinBox* maxRotateFilesSpinBox = new ElaSpinBox(maxRotateFilesArea);
	maxRotateFilesSpinBox->setRange(1, 9999);
	maxRotateFilesSpinBox->setValue(maxRotateFiles);
	maxRotateFilesLayout->addWidget(maxRotateFilesSpinBox);
	logSettingsDrawerArea->addDrawer(maxRotateFilesArea);


	mainLayout->addSpacing(15);
	ElaText* tokenizerConfigText = new ElaText(tr("分词器设置"), 18, this);
	ElaToolTip* tokenizerConfigTip = new ElaToolTip(tokenizerConfigText);
	tokenizerConfigTip->setToolTip(tr("用于生成字典和查错的分词器后端及其设置(应选择适合原文的后端/模型/字典)"));
	mainLayout->addWidget(tokenizerConfigText);

	// tokenizerBackend
	std::string tokenizerBackend = toml::find_or(_projectConfig, "common", "tokenizerBackend", "MeCab");
	ElaScrollPageArea* tokenizerBackendArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* tokenizerBackendLayout = new QHBoxLayout(tokenizerBackendArea);
	ElaDoubleText* tokenizerBackendTextWidget = new ElaDoubleText(tokenizerBackendArea,
		tr("分词器后端"), 16, 
		tr("除了MeCab，剩下的都依赖Python，所以速度变慢或内存占用变大是正常的"), 10, "");
	tokenizerBackendLayout->addWidget(tokenizerBackendTextWidget);
	tokenizerBackendLayout->addStretch();
	ElaComboBox* tokenizerBackendComboBox = new ElaComboBox(tokenizerBackendArea);
	tokenizerBackendComboBox->addItem("MeCab");
	tokenizerBackendComboBox->addItem("spaCy");
	tokenizerBackendComboBox->addItem("Stanza");
	if (int index = tokenizerBackendComboBox->findText(QString::fromStdString(tokenizerBackend)); index >= 0) {
		tokenizerBackendComboBox->setCurrentIndex(index);
	}
	tokenizerBackendLayout->addWidget(tokenizerBackendComboBox);
	mainLayout->addWidget(tokenizerBackendArea);

	// mecabDictDir
	std::string mecabDictDir = toml::find_or(_projectConfig, "common", "mecabDictDir", "BaseConfig/mecabDict/mecab-ipadic-utf8");
	ElaScrollPageArea* mecabDictDirArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* mecabDictDirLayout = new QHBoxLayout(mecabDictDirArea);
	ElaDoubleText* mecabDictDirText = new ElaDoubleText(mecabDictDirArea,
		tr("MeCab词典目录"), 16, 
		tr("MeCab词典目录，程序自带一个"), 10, "");
	mecabDictDirLayout->addWidget(mecabDictDirText);
	mecabDictDirLayout->addStretch();
	ElaLineEdit* mecabDictDirLineEdit = new ElaLineEdit(mecabDictDirArea);
	mecabDictDirLineEdit->setFixedWidth(400);
	mecabDictDirLineEdit->setText(QString::fromStdString(mecabDictDir));
	mecabDictDirLayout->addWidget(mecabDictDirLineEdit);
	mainLayout->addWidget(mecabDictDirArea);
	ElaPushButton* mecabDictDirButton = new ElaPushButton(tr("浏览"), mecabDictDirArea);
	mecabDictDirLayout->addWidget(mecabDictDirButton);
	connect(mecabDictDirButton, &QPushButton::clicked, this, [=]()
		{
			QString dir = QFileDialog::getExistingDirectory(this, tr("选择MeCab词典目录"), mecabDictDirLineEdit->text());
			if (!dir.isEmpty()) {
				mecabDictDirLineEdit->setText(dir);
			}
		});

	// spaCyModelName  https://spacy.io/models
	std::string spaCyModelName = toml::find_or(_projectConfig, "common", "spaCyModelName", "ja_core_news_lg");
	ElaScrollPageArea* spaCyModelNameArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* spaCyModelNameLayout = new QHBoxLayout(spaCyModelNameArea);
	ElaDoubleText* spaCyModelNameText = new ElaDoubleText(spaCyModelNameArea,
		tr("spaCy模型名称"), 16, 
		tr("spaCy模型名称，新模型下载后需重启程序"), 10, 
		tr("sm模型的效果有点一言难尽，有条件的建议上trf模型"));
	spaCyModelNameLayout->addWidget(spaCyModelNameText);
	spaCyModelNameLayout->addStretch();
	ElaLineEdit* spaCyModelNameLineEdit = new ElaLineEdit(spaCyModelNameArea);
	spaCyModelNameLineEdit->setFixedWidth(200);
	spaCyModelNameLineEdit->setText(QString::fromStdString(spaCyModelName));
	spaCyModelNameLayout->addWidget(spaCyModelNameLineEdit);
	mainLayout->addWidget(spaCyModelNameArea);
	ElaPushButton* spaCyModelNameButton = new ElaPushButton(tr("浏览"), spaCyModelNameArea);
	spaCyModelNameLayout->addWidget(spaCyModelNameButton);
	connect(spaCyModelNameButton, &QPushButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl("https://spacy.io/models"));
		});

	// stanzaLang https://stanfordnlp.github.io/stanza/ner_models.html
	std::string stanzaLang = toml::find_or(_projectConfig, "common", "stanzaLang", "ja");
	ElaScrollPageArea* stanzaLangArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* stanzaLangLayout = new QHBoxLayout(stanzaLangArea);
	ElaDoubleText* stanzaLangTextWidget = new ElaDoubleText(stanzaLangArea,
		tr("Stanza语言ID"), 16, 
		tr("Stanza语言ID，新模型下载后需重启程序"), 10, "");
	stanzaLangLayout->addWidget(stanzaLangTextWidget);
	stanzaLangLayout->addStretch();
	ElaLineEdit* stanzaLangLineEdit = new ElaLineEdit(stanzaLangArea);
	stanzaLangLineEdit->setFixedWidth(200);
	stanzaLangLineEdit->setText(QString::fromStdString(stanzaLang));
	stanzaLangLayout->addWidget(stanzaLangLineEdit);
	mainLayout->addWidget(stanzaLangArea);
	ElaPushButton* stanzaLangButton = new ElaPushButton(tr("浏览"), stanzaLangArea);
	stanzaLangLayout->addWidget(stanzaLangButton);
	connect(stanzaLangButton, &QPushButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl("https://stanfordnlp.github.io/stanza/ner_models.html"));
		});


	// 项目所用的换行
	std::string linebreakSymbol = toml::find_or(_projectConfig, "common", "linebreakSymbol", "auto");
	toml::ordered_value lbsVal = linebreakSymbol;
	lbsVal.as_string_fmt().fmt = toml::string_format::basic;
	QString linebreakSymbolStr = QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{"linebreakSymbol", lbsVal}} }));
	mainLayout->addSpacing(15);
	ElaText* linebreakText = new ElaText(tr("本项目所使用的换行符"), 18, mainWidget);
	ElaToolTip* linebreakTip = new ElaToolTip(linebreakText);
	linebreakTip->setToolTip(tr("将换行符统一规范为 &lt;br&gt; 以方便检错和修复，也可以让如全角半角转化等插件方便忽略换行。<br>具体替换时机详见使用说明，auto为自动检测"));
	mainLayout->addWidget(linebreakText);
	ElaPlainTextEdit* linebreakEdit = new ElaPlainTextEdit(mainWidget);
	linebreakEdit->setPlainText(linebreakSymbolStr);
	linebreakEdit->setFixedHeight(100);
	mainLayout->addWidget(linebreakEdit);


	_applyFunc = [=]()
		{
			insertToml(_projectConfig, "common.numPerRequestTranslate", requestNumSpinBox->value());
			insertToml(_projectConfig, "common.threadsNum", maxThreadSpinBox->value());
			int orderValue = transOrderGroup->id(transOrderGroup->checkedButton());
			QString orderValueStr;
			if (orderValue == 0) {
				orderValueStr = "name";
			}
			else if (orderValue == 1) {
				orderValueStr = "size";
			}
			insertToml(_projectConfig, "common.sortMethod", orderValueStr.toStdString());
			insertToml(_projectConfig, "common.targetLang", targetLangLineEdit->text().toStdString());
			insertToml(_projectConfig, "common.splitFile", splitSettingsGroup->checkedButton()->text().toStdString());
			insertToml(_projectConfig, "common.splitFileNum", splitNumSpinBox->value());
			insertToml(_projectConfig, "common.cacheSearchDistance", cacheSearchDistanceSpinBox->value());
			insertToml(_projectConfig, "common.saveCacheInterval", cacheSaveIntervalSpinBox->value());
			insertToml(_projectConfig, "common.maxRetries", retrySpinBox->value());
			insertToml(_projectConfig, "common.contextHistorySize", contextNumSpinBox->value());
			insertToml(_projectConfig, "common.smartRetry", smartRetryToggle->getIsToggled());
			insertToml(_projectConfig, "common.checkQuota", checkQuotaToggle->getIsToggled());
			insertToml(_projectConfig, "common.retransAllWhenFail", retransAllWhenFailToggle->getIsToggled());

			insertToml(_projectConfig, "common.saveLog", saveLogToggle->getIsToggled());
			insertToml(_projectConfig, "common.logLevel", logLevelComboBox->currentText().toStdString());
			insertToml(_projectConfig, "common.logFileMaxSize", logFileMaxSizeSpinBox->value() * 1024);
			insertToml(_projectConfig, "common.maxRotateFiles", maxRotateFilesSpinBox->value());

			insertToml(_projectConfig, "common.tokenizerBackend", tokenizerBackendComboBox->currentText().toStdString());
			insertToml(_projectConfig, "common.mecabDictDir", mecabDictDirLineEdit->text().toStdString());
			insertToml(_projectConfig, "common.spaCyModelName", spaCyModelNameLineEdit->text().toStdString());
			insertToml(_projectConfig, "common.stanzaLang", stanzaLangLineEdit->text().toStdString());

			try {
				toml::ordered_value newTbl = toml::parse_str<toml::ordered_type_config>(linebreakEdit->toPlainText().toStdString());
				auto& newLinebreakSymbol = newTbl["linebreakSymbol"];
				if (newLinebreakSymbol.is_string()) {
					insertToml(_projectConfig, "common.linebreakSymbol", newLinebreakSymbol);
				}
				else {
					insertToml(_projectConfig, "common.linebreakSymbol", "auto");
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("linebreakSymbol 不符合 toml 规范"), 3000);
			}
		};

	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, false, 0);
}