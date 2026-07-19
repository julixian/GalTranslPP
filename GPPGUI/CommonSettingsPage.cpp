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
#include "ElaNoWheelComboBox.h"
#include "ElaScrollPageArea.h"
#include "ElaRadioButton.h"
#include "ElaSpinBox.h"
#include "ElaToggleSwitch.h"
#include "ElaToolButton.h"
#include "ElaToolTip.h"
#include "ElaDoubleText.h"
#include "TreeSitterHighlighter.h"

import Tool;

CommonSettingsPage::CommonSettingsPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("一般设置"));
	setTitleVisible(false);

	setupUi();
}

void CommonSettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(20, 15, 15, 0);

	// 单次请求翻译句子数量
	int numPerRequestTranslate = toml::find_or(m_projectConfig, "common", "numPerRequestTranslate", 16);
	ElaScrollPageArea* numPerRequestTranslateArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* numPerRequestTranslateLayout = new QHBoxLayout(numPerRequestTranslateArea);
	ElaDoubleText* requestNumText = new ElaDoubleText(tr("单次请求翻译句子数量"), 16,
		tr("根据模型从十几到一百多不等"), 10, "", numPerRequestTranslateArea);
	numPerRequestTranslateLayout->addWidget(requestNumText);
	numPerRequestTranslateLayout->addStretch();
	ElaSpinBox* numPerRequestTranslateSpinBox = new ElaSpinBox(numPerRequestTranslateArea);
	numPerRequestTranslateSpinBox->setFocus();
	numPerRequestTranslateSpinBox->setRange(1, 9999);
	numPerRequestTranslateSpinBox->setValue(numPerRequestTranslate);
	numPerRequestTranslateLayout->addWidget(numPerRequestTranslateSpinBox);
	mainLayout->addWidget(numPerRequestTranslateArea);

	// 单次请求翻译人名数量
	int numPerRequestNameTranslate = toml::find_or(m_projectConfig, "common", "numPerRequestNameTranslate", 50);
	ElaScrollPageArea* numPerRequestNameTranslateArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* numPerRequestNameTranslateLayout = new QHBoxLayout(numPerRequestNameTranslateArea);
	ElaDoubleText* requestNameNumText = new ElaDoubleText(tr("单次请求翻译人名数量"), 16,
		tr("NameTrans 每个线程单次处理的人名数量"), 10, "", numPerRequestNameTranslateArea);
	numPerRequestNameTranslateLayout->addWidget(requestNameNumText);
	numPerRequestNameTranslateLayout->addStretch();
	ElaSpinBox* numPerRequestNameTranslateSpinBox = new ElaSpinBox(numPerRequestNameTranslateArea);
	numPerRequestNameTranslateSpinBox->setRange(1, 9999);
	numPerRequestNameTranslateSpinBox->setValue(numPerRequestNameTranslate);
	numPerRequestNameTranslateLayout->addWidget(numPerRequestNameTranslateSpinBox);
	mainLayout->addWidget(numPerRequestNameTranslateArea);

	// 最大线程数
	int threadsNum = toml::find_or(m_projectConfig, "common", "threadsNum", 5);
	ElaScrollPageArea* threadsNumArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* threadsNumLayout = new QHBoxLayout(threadsNumArea);
	ElaText* threadsNumText = new ElaText(tr("最大线程数"), 16, threadsNumArea);
	threadsNumLayout->addWidget(threadsNumText);
	threadsNumLayout->addStretch();
	ElaSpinBox* threadsNumSpinBox = new ElaSpinBox(threadsNumArea);
	threadsNumSpinBox->setRange(1, 9999);
	threadsNumSpinBox->setValue(threadsNum);
	threadsNumLayout->addWidget(threadsNumSpinBox);
	mainLayout->addWidget(threadsNumArea);

	// 翻译顺序，name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度[name/size]
	const std::string sortMethod = toml::find_or(m_projectConfig, "common", "sortMethod", "size");
	ElaScrollPageArea* sortMethodArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* sortMethodLayout = new QHBoxLayout(sortMethodArea);
	ElaDoubleText* sortMethodText = new ElaDoubleText(tr("翻译顺序"), 16,
		tr("name为文件名，size为大文件优先，多线程时大文件优先可以提高整体速度"), 10, "", sortMethodArea);
	sortMethodLayout->addWidget(sortMethodText);
	sortMethodLayout->addStretch();
	QButtonGroup* sortMethodButtonGroup = new QButtonGroup(sortMethodArea);
	ElaRadioButton* sortMethodNameRadio = new ElaRadioButton(tr("文件名"), sortMethodArea);
	sortMethodNameRadio->setChecked(sortMethod == "name");
	sortMethodLayout->addWidget(sortMethodNameRadio);
	ElaRadioButton* sortMethodSizeRadio = new ElaRadioButton(tr("文件大小"), sortMethodArea);
	sortMethodSizeRadio->setChecked(sortMethod == "size");
	sortMethodLayout->addWidget(sortMethodSizeRadio);
	sortMethodButtonGroup->addButton(sortMethodNameRadio, 0);
	sortMethodButtonGroup->addButton(sortMethodSizeRadio, 1);
	mainLayout->addWidget(sortMethodArea);

	// 翻译到的目标语言，包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]
	const std::string targetLang = toml::find_or(m_projectConfig, "common", "targetLang", "zh-cn");
	QString targetLangQStr = QString::fromStdString(targetLang);
	ElaScrollPageArea* targetLangArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* targetLangLayout = new QHBoxLayout(targetLangArea);
	ElaDoubleText* targetLangText = new ElaDoubleText(tr("翻译到的目标语言"), 16,
		tr("包括但不限于[zh-cn/zh-tw/en/ja/ko/ru/fr]"), 10, "", targetLangArea);
	targetLangLayout->addWidget(targetLangText);
	targetLangLayout->addStretch();
	ElaLineEdit* targetLangLineEdit = new ElaLineEdit(targetLangArea);
	targetLangLineEdit->setFixedWidth(150);
	targetLangLineEdit->setText(targetLangQStr);
	targetLangLayout->addWidget(targetLangLineEdit);
	mainLayout->addWidget(targetLangArea);

	// 是否启用单文件分割。Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割。[No/Num/Equal]
	const std::string splitFileMethod = toml::find_or(m_projectConfig, "common", "split", "method", "No");
	ElaDrawerArea* splitSettingsDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* splitFileMethodArea = new QWidget(splitSettingsDrawerArea);
	splitSettingsDrawerArea->setDrawerHeader(splitFileMethodArea);
	QHBoxLayout* splitFileMethodLayout = new QHBoxLayout(splitFileMethodArea);
	ElaDoubleText* splitFileMethodText = new ElaDoubleText(tr("单文件分割"), 16,
		tr("Num: 每n条分割一次，Equal: 每个文件均分n份，No: 关闭单文件分割"), 10, "", splitFileMethodArea);
	splitFileMethodLayout->addWidget(splitFileMethodText);
	splitFileMethodLayout->addStretch();
	QButtonGroup* splitFileMethodButtonGroup = new QButtonGroup(splitFileMethodArea);
	ElaRadioButton* splitFileMethodNoRadio = new ElaRadioButton("No", splitFileMethodArea);
	splitFileMethodNoRadio->setChecked(splitFileMethod == "No");
	splitFileMethodLayout->addWidget(splitFileMethodNoRadio);
	ElaRadioButton* splitNumRadio = new ElaRadioButton("Num", splitFileMethodArea);
	splitNumRadio->setChecked(splitFileMethod == "Num");
	splitFileMethodLayout->addWidget(splitNumRadio);
	ElaRadioButton* splitEqualRadio = new ElaRadioButton("Equal", splitFileMethodArea);
	splitEqualRadio->setChecked(splitFileMethod == "Equal");
	splitFileMethodLayout->addWidget(splitEqualRadio);
	splitFileMethodButtonGroup->addButton(splitFileMethodNoRadio, 0);
	splitFileMethodButtonGroup->addButton(splitNumRadio, 1);
	splitFileMethodButtonGroup->addButton(splitEqualRadio, 2);
	mainLayout->addWidget(splitSettingsDrawerArea);

	// Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分。
	int splitNum = toml::find_or(m_projectConfig, "common", "split", "num", 10);
	ElaScrollPageArea* splitNumArea = new ElaScrollPageArea(splitSettingsDrawerArea);
	QHBoxLayout* splitNumLayout = new QHBoxLayout(splitNumArea);
	ElaDoubleText* splitNumText = new ElaDoubleText(tr("分割数量"), 16,
		tr("Num时，表示n句拆分一次；Equal时，表示每个文件均分拆成n部分"), 10, "", splitNumArea);
	splitNumLayout->addWidget(splitNumText);
	splitNumLayout->addStretch();
	ElaSpinBox* splitNumSpinBox = new ElaSpinBox(splitNumArea);
	splitNumSpinBox->setRange(1, 10000);
	splitNumSpinBox->setValue(splitNum);
	splitNumLayout->addWidget(splitNumSpinBox);
	splitSettingsDrawerArea->addDrawer(splitNumArea);

	// 分割缓存查找距离
	int cacheSearchDistance = toml::find_or(m_projectConfig, "common", "split", "cacheSearchDistance", 5);
	ElaScrollPageArea* cacheSearchDistanceArea = new ElaScrollPageArea(splitSettingsDrawerArea);
	QHBoxLayout* cacheSearchDistanceLayout = new QHBoxLayout(cacheSearchDistanceArea);
	ElaDoubleText* cacheSearchDistanceText = new ElaDoubleText(tr("分割缓存查找距离"), 16,
		tr("将自身索引 ±N 的分割文件均视为当前分割文件的缓存"), 10,
		tr("数值越大可能占用更多内存"), cacheSearchDistanceArea);
	cacheSearchDistanceLayout->addWidget(cacheSearchDistanceText);
	cacheSearchDistanceLayout->addStretch();
	ElaSpinBox* cacheSearchDistanceSpinBox = new ElaSpinBox(cacheSearchDistanceArea);
	cacheSearchDistanceSpinBox->setRange(0, 9999);
	cacheSearchDistanceSpinBox->setValue(cacheSearchDistance);
	cacheSearchDistanceLayout->addWidget(cacheSearchDistanceSpinBox);
	splitSettingsDrawerArea->addDrawer(cacheSearchDistanceArea);
	if (toml::find_or(m_projectConfig, "GUIConfig", "commonSplitFileSettingsExpanded", splitFileMethod != "No")) {
		splitSettingsDrawerArea->expand();
	}

	// 连续重复块引用复用
	bool reuseRepeatedBlocks = toml::find_or(m_projectConfig, "common", "repeatedBlock", "enabled", false);
	ElaDrawerArea* repeatedBlockDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* reuseRepeatedBlocksArea = new QWidget(repeatedBlockDrawerArea);
	repeatedBlockDrawerArea->setDrawerHeader(reuseRepeatedBlocksArea);
	QHBoxLayout* reuseRepeatedBlocksLayout = new QHBoxLayout(reuseRepeatedBlocksArea);
	ElaDoubleText* reuseRepeatedBlocksText = new ElaDoubleText(tr("连续重复块引用复用"), 16,
		tr("重复脚本块只翻译首次出现的片段，后续句子引用复制结果"), 10,
		tr("会把 onFileProcessed 延迟到翻译结束再执行。启用时建议将翻译顺序改为文件名排序"), reuseRepeatedBlocksArea);
	reuseRepeatedBlocksLayout->addWidget(reuseRepeatedBlocksText);
	reuseRepeatedBlocksLayout->addStretch();
	ElaToggleSwitch* reuseRepeatedBlocksToggle = new ElaToggleSwitch(reuseRepeatedBlocksArea);
	reuseRepeatedBlocksToggle->setIsToggled(reuseRepeatedBlocks);
	reuseRepeatedBlocksLayout->addWidget(reuseRepeatedBlocksToggle);
	mainLayout->addWidget(repeatedBlockDrawerArea);

	int repeatedBlockMinSize = toml::find_or(m_projectConfig, "common", "repeatedBlock", "minSize", 5);
	ElaScrollPageArea* repeatedBlockMinSizeArea = new ElaScrollPageArea(repeatedBlockDrawerArea);
	QHBoxLayout* repeatedBlockMinSizeLayout = new QHBoxLayout(repeatedBlockMinSizeArea);
	ElaDoubleText* repeatedBlockMinSizeText = new ElaDoubleText(tr("重复块最小句数"), 16,
		tr("连续 n 句的说话人和原文完全相同才建立引用"), 10, "", repeatedBlockMinSizeArea);
	repeatedBlockMinSizeLayout->addWidget(repeatedBlockMinSizeText);
	repeatedBlockMinSizeLayout->addStretch();
	ElaSpinBox* repeatedBlockMinSizeSpinBox = new ElaSpinBox(repeatedBlockMinSizeArea);
	repeatedBlockMinSizeSpinBox->setRange(2, 10000);
	repeatedBlockMinSizeSpinBox->setValue(repeatedBlockMinSize);
	repeatedBlockMinSizeLayout->addWidget(repeatedBlockMinSizeSpinBox);
	repeatedBlockDrawerArea->addDrawer(repeatedBlockMinSizeArea);
	if (toml::find_or(m_projectConfig, "GUIConfig", "commonRepeatedBlockExpanded", false)) {
		repeatedBlockDrawerArea->expand();
	}

	// Agent 模式
	const bool agentEnabled = toml::find_or(m_projectConfig, "common", "agent", "enabled", false);
	ElaDrawerArea* agentSettingsDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* agentEnabledArea = new QWidget(agentSettingsDrawerArea);
	agentSettingsDrawerArea->setDrawerHeader(agentEnabledArea);
	QHBoxLayout* agentEnabledLayout = new QHBoxLayout(agentEnabledArea);
	ElaDoubleText* agentEnabledText = new ElaDoubleText(tr("Agent 模式"), 16,
		tr("当前仅 ForGalTsv、ForNovelTsv、GenDict 会实际启用"), 10,
		tr("让模型可以调用一定的工具以获取更多上下文"), agentEnabledArea);
	agentEnabledLayout->addWidget(agentEnabledText);
	agentEnabledLayout->addStretch();
	ElaToggleSwitch* agentEnabledToggle = new ElaToggleSwitch(agentEnabledArea);
	agentEnabledToggle->setIsToggled(agentEnabled);
	agentEnabledLayout->addWidget(agentEnabledToggle);
	mainLayout->addWidget(agentSettingsDrawerArea);

	const int agentMaxTurnsPerChunk = toml::find_or(m_projectConfig, "common", "agent", "maxTurnsPerChunk", 20);
	ElaScrollPageArea* agentMaxTurnsArea = new ElaScrollPageArea(agentSettingsDrawerArea);
	QHBoxLayout* agentMaxTurnsLayout = new QHBoxLayout(agentMaxTurnsArea);
	ElaDoubleText* agentMaxTurnsText = new ElaDoubleText(tr("单块最大轮数"), 16,
		tr("Agent 处理一个文本块时允许的最大问答轮数"), 10, "", agentMaxTurnsArea);
	agentMaxTurnsLayout->addWidget(agentMaxTurnsText);
	agentMaxTurnsLayout->addStretch();
	ElaSpinBox* agentMaxTurnsSpinBox = new ElaSpinBox(agentMaxTurnsArea);
	agentMaxTurnsSpinBox->setRange(1, 9999);
	agentMaxTurnsSpinBox->setValue(agentMaxTurnsPerChunk);
	agentMaxTurnsLayout->addWidget(agentMaxTurnsSpinBox);
	agentSettingsDrawerArea->addDrawer(agentMaxTurnsArea);

	const int agentCompactContextThresholdBytes = toml::find_or(m_projectConfig, "common", "agent", "compactContextThresholdBytes", 150000);
	ElaScrollPageArea* agentCompactThresholdArea = new ElaScrollPageArea(agentSettingsDrawerArea);
	QHBoxLayout* agentCompactThresholdLayout = new QHBoxLayout(agentCompactThresholdArea);
	ElaDoubleText* agentCompactThresholdText = new ElaDoubleText(tr("压缩上下文阈值"), 16,
		tr("Agent 消息上下文超过该字节数后触发压缩"), 10, tr("单位为字节"), agentCompactThresholdArea);
	agentCompactThresholdLayout->addWidget(agentCompactThresholdText);
	agentCompactThresholdLayout->addStretch();
	ElaSpinBox* agentCompactThresholdSpinBox = new ElaSpinBox(agentCompactThresholdArea);
	agentCompactThresholdSpinBox->setRange(1, 1000000000);
	agentCompactThresholdSpinBox->setFixedWidth(180);
	agentCompactThresholdSpinBox->setValue(agentCompactContextThresholdBytes);
	agentCompactThresholdLayout->addWidget(agentCompactThresholdSpinBox);
	agentSettingsDrawerArea->addDrawer(agentCompactThresholdArea);

	const int agentSearchResultLimit = toml::find_or(m_projectConfig, "common", "agent", "searchResultLimit", 80);
	ElaScrollPageArea* agentSearchResultLimitArea = new ElaScrollPageArea(agentSettingsDrawerArea);
	QHBoxLayout* agentSearchResultLimitLayout = new QHBoxLayout(agentSearchResultLimitArea);
	ElaDoubleText* agentSearchResultLimitText = new ElaDoubleText(tr("工具搜索结果上限"), 16,
		tr("Agent 工具一次返回的搜索结果数量上限"), 10, "", agentSearchResultLimitArea);
	agentSearchResultLimitLayout->addWidget(agentSearchResultLimitText);
	agentSearchResultLimitLayout->addStretch();
	ElaSpinBox* agentSearchResultLimitSpinBox = new ElaSpinBox(agentSearchResultLimitArea);
	agentSearchResultLimitSpinBox->setRange(1, 9999);
	agentSearchResultLimitSpinBox->setValue(agentSearchResultLimit);
	agentSearchResultLimitLayout->addWidget(agentSearchResultLimitSpinBox);
	agentSettingsDrawerArea->addDrawer(agentSearchResultLimitArea);

	const int agentContextLinesLimit = toml::find_or(m_projectConfig, "common", "agent", "contextLinesLimit", 20);
	ElaScrollPageArea* agentContextLinesLimitArea = new ElaScrollPageArea(agentSettingsDrawerArea);
	QHBoxLayout* agentContextLinesLimitLayout = new QHBoxLayout(agentContextLinesLimitArea);
	ElaDoubleText* agentContextLinesLimitText = new ElaDoubleText(tr("工具上下文行数上限"), 16,
		tr("search_text 附近行数上限，0 表示不返回附近行"), 10, "", agentContextLinesLimitArea);
	agentContextLinesLimitLayout->addWidget(agentContextLinesLimitText);
	agentContextLinesLimitLayout->addStretch();
	ElaSpinBox* agentContextLinesLimitSpinBox = new ElaSpinBox(agentContextLinesLimitArea);
	agentContextLinesLimitSpinBox->setRange(0, 9999);
	agentContextLinesLimitSpinBox->setValue(agentContextLinesLimit);
	agentContextLinesLimitLayout->addWidget(agentContextLinesLimitSpinBox);
	agentSettingsDrawerArea->addDrawer(agentContextLinesLimitArea);

	const std::string agentProjectNotePath = toml::find_or(m_projectConfig, "common", "agent", "projectNotePath", "ProjectNote.md");
	ElaScrollPageArea* agentProjectNotePathArea = new ElaScrollPageArea(agentSettingsDrawerArea);
	QHBoxLayout* agentProjectNotePathLayout = new QHBoxLayout(agentProjectNotePathArea);
	ElaDoubleText* agentProjectNotePathText = new ElaDoubleText(tr("ProjectNote 路径"), 16,
		tr("Agent 可选读取的项目说明文件，需自己加 `get_project_note()` 的工具提示词"), 10,
		"", agentProjectNotePathArea);
	agentProjectNotePathLayout->addWidget(agentProjectNotePathText);
	agentProjectNotePathLayout->addStretch();
	ElaLineEdit* agentProjectNotePathLineEdit = new ElaLineEdit(agentProjectNotePathArea);
	agentProjectNotePathLineEdit->setFixedWidth(400);
	agentProjectNotePathLineEdit->setText(QString::fromStdString(agentProjectNotePath));
	agentProjectNotePathLayout->addWidget(agentProjectNotePathLineEdit);
	agentSettingsDrawerArea->addDrawer(agentProjectNotePathArea);
	if (toml::find_or(m_projectConfig, "GUIConfig", "commonAgentSettingsExpanded", agentEnabled)) {
		agentSettingsDrawerArea->expand();
	}
	else {
		agentSettingsDrawerArea->collapse();
	}

	// 每翻译n次保存一次缓存
	int cacheSaveInterval = toml::find_or(m_projectConfig, "common", "saveCacheInterval", 1);
	ElaScrollPageArea* cacheSaveIntervalArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* cacheSaveIntervalLayout = new QHBoxLayout(cacheSaveIntervalArea);
	ElaDoubleText* cacheSaveIntervalText = new ElaDoubleText(tr("缓存保存间隔"), 16,
		tr("每翻译n次保存一次缓存"), 10, "", cacheSaveIntervalArea);
	cacheSaveIntervalLayout->addWidget(cacheSaveIntervalText);
	cacheSaveIntervalLayout->addStretch();
	ElaSpinBox* cacheSaveIntervalSpinBox = new ElaSpinBox(cacheSaveIntervalArea);
	cacheSaveIntervalSpinBox->setRange(1, 9999);
	cacheSaveIntervalSpinBox->setValue(cacheSaveInterval);
	cacheSaveIntervalLayout->addWidget(cacheSaveIntervalSpinBox);
	mainLayout->addWidget(cacheSaveIntervalArea);

	// 最大请求次数
	int maxRequestCount = toml::find_or(m_projectConfig, "common", "maxRequestCount", 4);
	ElaScrollPageArea* maxRequestArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* maxRequestLayout = new QHBoxLayout(maxRequestArea);
	ElaText* requestText = new ElaText(tr("最大请求次数"), 16, maxRequestArea);
	maxRequestLayout->addWidget(requestText);
	maxRequestLayout->addStretch();
	ElaSpinBox* requestSpinBox = new ElaSpinBox(maxRequestArea);
	requestSpinBox->setRange(1, 9999);
	requestSpinBox->setValue(maxRequestCount);
	maxRequestLayout->addWidget(requestSpinBox);
	mainLayout->addWidget(maxRequestArea);

	// 携带上文数量
	int contextNum = toml::find_or(m_projectConfig, "common", "contextHistorySize", 8);
	ElaScrollPageArea* contextNumArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* contextNumLayout = new QHBoxLayout(contextNumArea);
	ElaDoubleText* contextNumText = new ElaDoubleText(tr("携带上文数量"), 16,
		tr("对现代模型而言意义不大了，推荐值 ≤ 10"), 10, "", contextNumArea);
	contextNumLayout->addWidget(contextNumText);
	contextNumLayout->addStretch();
	ElaSpinBox* contextNumSpinBox = new ElaSpinBox(contextNumArea);
	contextNumSpinBox->setRange(1, 9999);
	contextNumSpinBox->setValue(contextNum);
	contextNumLayout->addWidget(contextNumSpinBox);
	mainLayout->addWidget(contextNumArea);

	// 智能重试
	bool useSmartRetry = toml::find_or(m_projectConfig, "common", "smartRetry", false);
	ElaScrollPageArea* smartRetryArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* smartRetryLayout = new QHBoxLayout(smartRetryArea);
	ElaDoubleText* smartRetryTextWidget = new ElaDoubleText(tr("智能重试"), 16,
		tr("解析结果失败时尝试折半重翻与清空上下文"), 10,
		"如果用的打野 key 其实不建议开这个", smartRetryArea);
	smartRetryLayout->addWidget(smartRetryTextWidget);
	smartRetryLayout->addStretch();
	ElaToggleSwitch* smartRetryToggle = new ElaToggleSwitch(smartRetryArea);
	smartRetryToggle->setIsToggled(useSmartRetry);
	smartRetryLayout->addWidget(smartRetryToggle);
	mainLayout->addWidget(smartRetryArea);

	// 额度检测
	bool shouldCheckQuota = toml::find_or(m_projectConfig, "common", "checkQuota", true);
	ElaScrollPageArea* checkQuotaArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* checkQuotaLayout = new QHBoxLayout(checkQuotaArea);
	ElaDoubleText* checkQuotaTextWidget = new ElaDoubleText(tr("额度检测"), 16,
		tr("运行时动态检测 key 额度，自动从 Api 池中删除额度不足的 key"), 10, "", checkQuotaArea);
	checkQuotaLayout->addWidget(checkQuotaTextWidget);
	checkQuotaLayout->addStretch();
	ElaToggleSwitch* checkQuotaToggle = new ElaToggleSwitch(checkQuotaArea);
	checkQuotaToggle->setIsToggled(shouldCheckQuota);
	checkQuotaLayout->addWidget(checkQuotaToggle);
	mainLayout->addWidget(checkQuotaArea);

	// retransAllWhenFail/解析不完整时重翻整段
	bool retransAllWhenFail = toml::find_or(m_projectConfig, "common", "retransAllWhenFail", false);
	ElaScrollPageArea* retransAllWhenFailArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* retransAllWhenFailLayout = new QHBoxLayout(retransAllWhenFailArea);
	ElaDoubleText* retransAllWhenFailText = new ElaDoubleText(tr("解析不完整时重翻整段"), 16,
		tr("不开启则仅重翻漏掉的部分，开启可增加模型因串行而导致解析失败时的容错"), 10,
		tr("默认关闭以节省token/防止因模型截断造成无限循环"), retransAllWhenFailArea);
	retransAllWhenFailLayout->addWidget(retransAllWhenFailText);
	retransAllWhenFailLayout->addStretch();
	ElaToggleSwitch* retransAllWhenFailToggle = new ElaToggleSwitch(retransAllWhenFailArea);
	retransAllWhenFailToggle->setIsToggled(retransAllWhenFail);
	retransAllWhenFailLayout->addWidget(retransAllWhenFailToggle);
	mainLayout->addWidget(retransAllWhenFailArea);

	// 项目日志设置
	bool shouldSaveLog = toml::find_or(m_projectConfig, "common", "log", "saveLog", true);
	ElaDrawerArea* logSettingsDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* shouldSaveLogArea = new QWidget(logSettingsDrawerArea);
	logSettingsDrawerArea->setDrawerHeader(shouldSaveLogArea);
	QHBoxLayout* shouldSaveLogLayout = new QHBoxLayout(shouldSaveLogArea);
	ElaText* logSettingsText = new ElaText(tr("项目日志设置"), 16, shouldSaveLogArea);
	logSettingsText->setWordWrap(false);
	shouldSaveLogLayout->addWidget(logSettingsText);
	shouldSaveLogLayout->addStretch();
	ElaText* shouldSaveLogText = new ElaText(tr("保存项目日志文件"), 16, shouldSaveLogArea);
	shouldSaveLogText->setWordWrap(false);
	ElaToggleSwitch* shouldSaveLogToggle = new ElaToggleSwitch(shouldSaveLogArea);
	shouldSaveLogToggle->setIsToggled(shouldSaveLog);
	shouldSaveLogLayout->addWidget(shouldSaveLogText);
	shouldSaveLogLayout->addWidget(shouldSaveLogToggle);
	mainLayout->addWidget(logSettingsDrawerArea);


	// 项目日志级别
	const std::string logLevel = toml::find_or(m_projectConfig, "common", "log", "level", "info");
	QString logLevelQStr = QString::fromStdString(logLevel);
	ElaScrollPageArea* logLevelArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* logLevelLayout = new QHBoxLayout(logLevelArea);
	ElaText* logLevelText = new ElaText(tr("日志级别"), 16, logLevelArea);
	logLevelLayout->addWidget(logLevelText);
	logLevelLayout->addStretch();
	ElaNoWheelComboBox* logLevelComboBox = new ElaNoWheelComboBox(logLevelArea);
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
	int logFileMaxSize = toml::find_or(m_projectConfig, "common", "log", "fileMaxSize", 1024 * 1024 * 10);
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
	logFileMaxSizeSpinBox->setFixedWidth(180);
	logFileMaxSizeLayout->addWidget(logFileMaxSizeSpinBox);
	ElaText* kbText = new ElaText("KB", 16, logFileMaxSizeArea);
	logFileMaxSizeLayout->addWidget(kbText);
	logSettingsDrawerArea->addDrawer(logFileMaxSizeArea);

	// maxRotateFiles/log 文件滚动数量上限
	int maxRotateFiles = toml::find_or(m_projectConfig, "common", "log", "maxRotateFiles", 10);
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

	int inputBlockMaxLines = toml::find_or(m_projectConfig, "common", "log", "inputBlockMaxLines", 10);
	ElaScrollPageArea* inputBlockMaxLinesArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* inputBlockMaxLinesLayout = new QHBoxLayout(inputBlockMaxLinesArea);
	ElaDoubleText* inputBlockMaxLinesText = new ElaDoubleText(tr("单次日志翻译文本行数上限"), 16,
		tr("限制日志里 inputBlock 和解析结果显示的行数"), 10,
		tr("实际请求和解析仍使用完整内容"), inputBlockMaxLinesArea);
	inputBlockMaxLinesLayout->addWidget(inputBlockMaxLinesText);
	inputBlockMaxLinesLayout->addStretch();
	ElaSpinBox* inputBlockMaxLinesSpinBox = new ElaSpinBox(inputBlockMaxLinesArea);
	inputBlockMaxLinesSpinBox->setRange(1, 9999);
	inputBlockMaxLinesSpinBox->setValue(inputBlockMaxLines);
	inputBlockMaxLinesLayout->addWidget(inputBlockMaxLinesSpinBox);
	logSettingsDrawerArea->addDrawer(inputBlockMaxLinesArea);

	int problemMaxLines = toml::find_or(m_projectConfig, "common", "log", "problemMaxLines", 3);
	ElaScrollPageArea* problemMaxLinesArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* problemMaxLinesLayout = new QHBoxLayout(problemMaxLinesArea);
	ElaDoubleText* problemMaxLinesText = new ElaDoubleText(tr("单次日志问题行数上限"), 16,
		tr("只限制日志里 Problems 显示的行数"), 10,
		tr("实际请求仍发送完整内容"), problemMaxLinesArea);
	problemMaxLinesLayout->addWidget(problemMaxLinesText);
	problemMaxLinesLayout->addStretch();
	ElaSpinBox* problemMaxLinesSpinBox = new ElaSpinBox(problemMaxLinesArea);
	problemMaxLinesSpinBox->setRange(1, 9999);
	problemMaxLinesSpinBox->setValue(problemMaxLines);
	problemMaxLinesLayout->addWidget(problemMaxLinesSpinBox);
	logSettingsDrawerArea->addDrawer(problemMaxLinesArea);

	int glossaryMaxLines = toml::find_or(m_projectConfig, "common", "log", "glossaryMaxLines", 5);
	ElaScrollPageArea* glossaryMaxLinesArea = new ElaScrollPageArea(logSettingsDrawerArea);
	QHBoxLayout* glossaryMaxLinesLayout = new QHBoxLayout(glossaryMaxLinesArea);
	ElaDoubleText* glossaryMaxLinesText = new ElaDoubleText(tr("单次日志字典行数上限"), 16,
		tr("只限制日志里 Dict/Glossary 显示的行数"), 10,
		tr("实际请求仍发送完整内容"), glossaryMaxLinesArea);
	glossaryMaxLinesLayout->addWidget(glossaryMaxLinesText);
	glossaryMaxLinesLayout->addStretch();
	ElaSpinBox* glossaryMaxLinesSpinBox = new ElaSpinBox(glossaryMaxLinesArea);
	glossaryMaxLinesSpinBox->setRange(1, 9999);
	glossaryMaxLinesSpinBox->setValue(glossaryMaxLines);
	glossaryMaxLinesLayout->addWidget(glossaryMaxLinesSpinBox);
	logSettingsDrawerArea->addDrawer(glossaryMaxLinesArea);
	if (toml::find_or(m_projectConfig, "GUIConfig", "commonLogSettingsExpanded", false)) {
		logSettingsDrawerArea->expand();
	}


	mainLayout->addSpacing(15);
	ElaText* tokenizerConfigText = new ElaText(tr("分词器设置"), 18, this);
	ElaToolTip* tokenizerConfigTip = new ElaToolTip(tokenizerConfigText);
	tokenizerConfigTip->setToolTip(tr("用于生成字典和查错的分词器后端及其设置 (应选择适合原文的后端/模型/字典)"));
	mainLayout->addWidget(tokenizerConfigText);

	// tokenizerBackend
	const std::string tokenizerBackend = toml::find_or(m_projectConfig, "common", "tokenize", "backend", "MeCab");
	ElaDrawerArea* tokenizerSettingsDrawerArea = new ElaDrawerArea(mainWidget);
	QWidget* tokenizerBackendArea = new QWidget(tokenizerSettingsDrawerArea);
	tokenizerSettingsDrawerArea->setDrawerHeader(tokenizerBackendArea);
	QHBoxLayout* tokenizerBackendLayout = new QHBoxLayout(tokenizerBackendArea);
	ElaDoubleText* tokenizerBackendTextWidget = new ElaDoubleText(tr("分词器后端"), 16,
		tr("除了MeCab，剩下的都依赖Python，速度会比较慢"), 10, "", tokenizerBackendArea);
	tokenizerBackendLayout->addWidget(tokenizerBackendTextWidget);
	tokenizerBackendLayout->addStretch();
	ElaNoWheelComboBox* tokenizerBackendComboBox = new ElaNoWheelComboBox(tokenizerBackendArea);
	tokenizerBackendComboBox->addItem("MeCab");
	tokenizerBackendComboBox->addItem("spaCy");
	tokenizerBackendComboBox->addItem("Stanza");
	if (int index = tokenizerBackendComboBox->findText(QString::fromStdString(tokenizerBackend)); index >= 0) {
		tokenizerBackendComboBox->setCurrentIndex(index);
	}
	tokenizerBackendLayout->addWidget(tokenizerBackendComboBox);
	mainLayout->addWidget(tokenizerSettingsDrawerArea);

	// mecabDictDir
	const std::string mecabDictDir = toml::find_or(m_projectConfig, "common", "tokenize", "mecabDictDir",
		"BaseConfig/mecab/mecab-ipadic-utf8");
	ElaScrollPageArea* mecabDictDirArea = new ElaScrollPageArea(tokenizerSettingsDrawerArea);
	QHBoxLayout* mecabDictDirLayout = new QHBoxLayout(mecabDictDirArea);
	ElaDoubleText* mecabDictDirText = new ElaDoubleText(tr("MeCab词典目录"), 16,
		tr("MeCab词典目录，程序自带一个日文词典"), 10, "", mecabDictDirArea);
	mecabDictDirLayout->addWidget(mecabDictDirText);
	mecabDictDirLayout->addStretch();
	ElaLineEdit* mecabDictDirLineEdit = new ElaLineEdit(mecabDictDirArea);
	mecabDictDirLineEdit->setFixedWidth(400);
	mecabDictDirLineEdit->setText(QString::fromStdString(mecabDictDir));
	mecabDictDirLayout->addWidget(mecabDictDirLineEdit);
	tokenizerSettingsDrawerArea->addDrawer(mecabDictDirArea);
	ElaToolButton* mecabDictDirButton = new ElaToolButton(mecabDictDirArea);
	mecabDictDirButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	mecabDictDirButton->setElaIcon(ElaIconType::FolderOpen);
	mecabDictDirButton->setText(tr("浏览"));
	mecabDictDirLayout->addWidget(mecabDictDirButton);
	connect(mecabDictDirButton, &ElaToolButton::clicked, this, [=]()
		{
			QString dir = QFileDialog::getExistingDirectory(window(), tr("选择MeCab词典目录"), mecabDictDirLineEdit->text());
			if (!dir.isEmpty()) {
				mecabDictDirLineEdit->setText(dir);
			}
		});

	// spaCyModelName  https://spacy.io/models
	const std::string spaCyModelName = toml::find_or(m_projectConfig, "common", "tokenize", "spaCyModelName", "ja_core_news_lg");
	ElaScrollPageArea* spaCyModelNameArea = new ElaScrollPageArea(tokenizerSettingsDrawerArea);
	QHBoxLayout* spaCyModelNameLayout = new QHBoxLayout(spaCyModelNameArea);
	ElaDoubleText* spaCyModelNameText = new ElaDoubleText(tr("spaCy模型名称"), 16,
		tr("sm模型的效果有点一言难尽，有条件的建议上trf模型"), 10,
		"", spaCyModelNameArea);
	spaCyModelNameLayout->addWidget(spaCyModelNameText);
	spaCyModelNameLayout->addStretch();
	ElaLineEdit* spaCyModelNameLineEdit = new ElaLineEdit(spaCyModelNameArea);
	spaCyModelNameLineEdit->setFixedWidth(200);
	spaCyModelNameLineEdit->setText(QString::fromStdString(spaCyModelName));
	spaCyModelNameLayout->addWidget(spaCyModelNameLineEdit);
	tokenizerSettingsDrawerArea->addDrawer(spaCyModelNameArea);
	ElaToolButton* spaCyModelNameButton = new ElaToolButton(spaCyModelNameArea);
	spaCyModelNameButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	spaCyModelNameButton->setElaIcon(ElaIconType::ArrowUpRightFromSquare);
	spaCyModelNameButton->setText(tr("浏览"));
	spaCyModelNameButton->setToolTip(tr("打开模型列表网页"));
	spaCyModelNameLayout->addWidget(spaCyModelNameButton);
	connect(spaCyModelNameButton, &ElaToolButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl("https://spacy.io/models"));
		});

	// stanzaLang https://stanfordnlp.github.io/stanza/ner_models.html
	const std::string stanzaLang = toml::find_or(m_projectConfig, "common", "tokenize", "stanzaLang", "ja");
	ElaScrollPageArea* stanzaLangArea = new ElaScrollPageArea(tokenizerSettingsDrawerArea);
	QHBoxLayout* stanzaLangLayout = new QHBoxLayout(stanzaLangArea);
	ElaDoubleText* stanzaLangTextWidget = new ElaDoubleText(tr("Stanza语言ID"), 16,
		tr("感觉不如 spaCy"), 10, "", stanzaLangArea);
	stanzaLangLayout->addWidget(stanzaLangTextWidget);
	stanzaLangLayout->addStretch();
	ElaLineEdit* stanzaLangLineEdit = new ElaLineEdit(stanzaLangArea);
	stanzaLangLineEdit->setFixedWidth(200);
	stanzaLangLineEdit->setText(QString::fromStdString(stanzaLang));
	stanzaLangLayout->addWidget(stanzaLangLineEdit);
	tokenizerSettingsDrawerArea->addDrawer(stanzaLangArea);
	ElaToolButton* stanzaLangButton = new ElaToolButton(stanzaLangArea);
	stanzaLangButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	stanzaLangButton->setElaIcon(ElaIconType::ArrowUpRightFromSquare);
	stanzaLangButton->setText(tr("浏览"));
	stanzaLangButton->setToolTip(tr("打开模型列表网页"));
	stanzaLangLayout->addWidget(stanzaLangButton);
	connect(stanzaLangButton, &ElaToolButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl("https://stanfordnlp.github.io/stanza/ner_models.html"));
		});
	if (toml::find_or(m_projectConfig, "GUIConfig", "commonTokenizerSettingsExpanded", false)) {
		tokenizerSettingsDrawerArea->expand();
	}


	// 项目所用的换行
	const std::string linebreakSymbol = toml::find_or(m_projectConfig, "common", "linebreakSymbol", "auto");
	toml::ordered_value lbsVal = linebreakSymbol;
	lbsVal.as_string_fmt().fmt = toml::string_format::basic;
	QString linebreakSymbolStr = QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{"linebreakSymbol", lbsVal}} }));
	mainLayout->addSpacing(15);
	ElaScrollPageArea* linebreakArea = new ElaScrollPageArea(mainWidget);
	linebreakArea->setFixedHeight(165);
	QVBoxLayout* linebreakLayout = new QVBoxLayout(linebreakArea);
	linebreakLayout->setContentsMargins(12, 6, 12, 8);
	linebreakLayout->setSpacing(6);
	linebreakLayout->addWidget(new ElaDoubleText(tr("本项目所使用的换行符"), 16,
		tr("将换行符统一规范为 &lt;br&gt; 以方便检错和修复，也可以让如全角半角转化等插件方便忽略换行。具体替换时机详见使用说明，auto 为自动检测"),
		8, "", linebreakArea));
	ElaPlainTextEdit* linebreakEdit = new ElaPlainTextEdit(linebreakArea);
	linebreakEdit->setPlainText(linebreakSymbolStr);
	linebreakEdit->setFixedHeight(100);
	installTreeSitterHighlighter(linebreakEdit->document(), SyntaxLanguage::Toml);
	linebreakLayout->addWidget(linebreakEdit);
	mainLayout->addWidget(linebreakArea);
	mainLayout->addSpacing(8);


	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "common.numPerRequestTranslate", numPerRequestTranslateSpinBox->value());
			insertToml(m_projectConfig, "common.numPerRequestNameTranslate", numPerRequestNameTranslateSpinBox->value());
			insertToml(m_projectConfig, "common.threadsNum", threadsNumSpinBox->value());
			int orderValue = sortMethodButtonGroup->id(sortMethodButtonGroup->checkedButton());
			QString orderValueQStr;
			if (orderValue == 0) {
				orderValueQStr = "name";
			}
			else if (orderValue == 1) {
				orderValueQStr = "size";
			}
			insertToml(m_projectConfig, "common.sortMethod", orderValueQStr.toStdString());
			insertToml(m_projectConfig, "common.targetLang", targetLangLineEdit->text().toStdString());
			insertToml(m_projectConfig, "common.split.method", splitFileMethodButtonGroup->checkedButton()->text().toStdString());
			insertToml(m_projectConfig, "common.split.num", splitNumSpinBox->value());
			insertToml(m_projectConfig, "common.split.cacheSearchDistance", cacheSearchDistanceSpinBox->value());
			insertToml(m_projectConfig, "GUIConfig.commonSplitSettingsExpanded", splitSettingsDrawerArea->getIsExpand());
			insertToml(m_projectConfig, "common.repeatedBlock.enabled", reuseRepeatedBlocksToggle->getIsToggled());
			insertToml(m_projectConfig, "common.repeatedBlock.minSize", repeatedBlockMinSizeSpinBox->value());
			insertToml(m_projectConfig, "GUIConfig.commonRepeatedBlockExpanded", repeatedBlockDrawerArea->getIsExpand());
			insertToml(m_projectConfig, "common.agent.enabled", agentEnabledToggle->getIsToggled());
			insertToml(m_projectConfig, "common.agent.maxTurnsPerChunk", agentMaxTurnsSpinBox->value());
			insertToml(m_projectConfig, "common.agent.compactContextThresholdBytes", agentCompactThresholdSpinBox->value());
			insertToml(m_projectConfig, "common.agent.searchResultLimit", agentSearchResultLimitSpinBox->value());
			insertToml(m_projectConfig, "common.agent.contextLinesLimit", agentContextLinesLimitSpinBox->value());
			insertToml(m_projectConfig, "common.agent.projectNotePath", agentProjectNotePathLineEdit->text().toStdString());
			insertToml(m_projectConfig, "GUIConfig.commonAgentSettingsExpanded", agentSettingsDrawerArea->getIsExpand());
			insertToml(m_projectConfig, "common.saveCacheInterval", cacheSaveIntervalSpinBox->value());
			insertToml(m_projectConfig, "common.maxRequestCount", requestSpinBox->value());
			insertToml(m_projectConfig, "common.contextHistorySize", contextNumSpinBox->value());
			insertToml(m_projectConfig, "common.smartRetry", smartRetryToggle->getIsToggled());
			insertToml(m_projectConfig, "common.checkQuota", checkQuotaToggle->getIsToggled());
			insertToml(m_projectConfig, "common.retransAllWhenFail", retransAllWhenFailToggle->getIsToggled());

			insertToml(m_projectConfig, "common.log.saveLog", shouldSaveLogToggle->getIsToggled());
			insertToml(m_projectConfig, "common.log.level", logLevelComboBox->currentText().toStdString());
			insertToml(m_projectConfig, "common.log.fileMaxSize", logFileMaxSizeSpinBox->value() * 1024);
			insertToml(m_projectConfig, "common.log.maxRotateFiles", maxRotateFilesSpinBox->value());
			insertToml(m_projectConfig, "common.log.inputBlockMaxLines", inputBlockMaxLinesSpinBox->value());
			insertToml(m_projectConfig, "common.log.problemMaxLines", problemMaxLinesSpinBox->value());
			insertToml(m_projectConfig, "common.log.glossaryMaxLines", glossaryMaxLinesSpinBox->value());
			insertToml(m_projectConfig, "GUIConfig.commonLogSettingsExpanded", logSettingsDrawerArea->getIsExpand());

			insertToml(m_projectConfig, "common.tokenize.backend", tokenizerBackendComboBox->currentText().toStdString());
			insertToml(m_projectConfig, "common.tokenize.mecabDictDir", mecabDictDirLineEdit->text().toStdString());
			insertToml(m_projectConfig, "common.tokenize.spaCyModelName", spaCyModelNameLineEdit->text().toStdString());
			insertToml(m_projectConfig, "common.tokenize.stanzaLang", stanzaLangLineEdit->text().toStdString());
			insertToml(m_projectConfig, "GUIConfig.commonTokenizerSettingsExpanded", tokenizerSettingsDrawerArea->getIsExpand());

			try {
				toml::ordered_value newTbl = toml::parse_str<toml::ordered_type_config>(linebreakEdit->toPlainText().toStdString());
				auto& newLinebreakSymbol = newTbl["linebreakSymbol"];
				if (newLinebreakSymbol.is_string()) {
					insertToml(m_projectConfig, "common.linebreakSymbol", newLinebreakSymbol);
				}
				else {
					insertToml(m_projectConfig, "common.linebreakSymbol", "auto");
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析失败"), tr("linebreakSymbol 不符合 toml 规范"), 3000);
			}
		};

	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, false, 0);
}
