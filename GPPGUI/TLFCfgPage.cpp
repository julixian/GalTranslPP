#include "TLFCfgPage.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QDesktopServices>

#include "ElaScrollPageArea.h"
#include "ElaSpinBox.h"
#include "ElaComboBox.h"
#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaToolTip.h"
#include "ValueSliderWidget.h"
#include "ElaDoubleText.h"

import Tool;

TLFCfgPage::TLFCfgPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("换行修复设置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 换行模式
	QStringList fixModes = { "优先标点", "保持位置", "固定字数", "平均", "不修改" };
	QStringList fixModesToShow = { tr("优先标点"), tr("保持位置"), tr("固定字数"), tr("平均"), tr("不修改") };
	QString fixMode = QString::fromStdString(toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "换行模式", ""));
	ElaScrollPageArea* fixModeArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* fixModeLayout = new QHBoxLayout(fixModeArea);
	ElaText* fixModeText = new ElaText(tr("换行模式"), fixModeArea);
	fixModeText->setTextPixelSize(16);
	fixModeLayout->addWidget(fixModeText);
	fixModeLayout->addStretch();
	ElaComboBox* fixModeComboBox = new ElaComboBox(fixModeArea);
	fixModeComboBox->addItems(fixModesToShow);
	if (!fixMode.isEmpty()) {
		int index = fixModes.indexOf(fixMode);
		if (index >= 0) {
			fixModeComboBox->setCurrentIndex(index);
		}
	}
	fixModeLayout->addWidget(fixModeComboBox);
	mainLayout->addWidget(fixModeArea);

	// 优先阈值
	double priorityThreshold = toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "优先阈值", 0.245);
	ElaScrollPageArea* priorityThresholdArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* priorityThresholdLayout = new QHBoxLayout(priorityThresholdArea);
	ElaDoubleText* priorityThresholdText = new ElaDoubleText(tr("优先阈值"), 16, tr("仅在 优先标点 模式有效，值越高，换行的相对位置的可以变动以去匹配标点的限度就越大"), 10, "", priorityThresholdArea);
	priorityThresholdLayout->addWidget(priorityThresholdText);
	priorityThresholdLayout->addStretch();
	ValueSliderWidget* priorityThresholdSlider = new ValueSliderWidget(0.0, 1.0, priorityThresholdArea);
	priorityThresholdSlider->setFixedWidth(400);
	priorityThresholdSlider->setValue(priorityThreshold);
	priorityThresholdSlider->setDecimals(3);
	priorityThresholdLayout->addWidget(priorityThresholdSlider);
	mainLayout->addWidget(priorityThresholdArea);

	// 分段字数阈值
	int threshold = toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "分段字数阈值", 21);
	ElaScrollPageArea* segmentThresholdArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* segmentThresholdLayout = new QHBoxLayout(segmentThresholdArea);
	ElaDoubleText* segmentThresholdText = new ElaDoubleText(tr("分段字数阈值"), 16, tr("仅在固定字数模式有效"), 10, "", segmentThresholdArea);
	segmentThresholdLayout->addWidget(segmentThresholdText);
	segmentThresholdLayout->addStretch();
	ElaSpinBox* segmentThresholdSpinBox = new ElaSpinBox(segmentThresholdArea);
	segmentThresholdSpinBox->setRange(1, 999);
	segmentThresholdSpinBox->setValue(threshold);
	segmentThresholdLayout->addWidget(segmentThresholdSpinBox);
	mainLayout->addWidget(segmentThresholdArea);

	// 强制修复
	bool forceFix = toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "强制修复", false);
	ElaScrollPageArea* forceFixArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* forceFixLayout = new QHBoxLayout(forceFixArea);
	ElaText* forceFixText = new ElaText(tr("强制修复"), forceFixArea);
	forceFixText->setTextPixelSize(16);
	forceFixLayout->addWidget(forceFixText);
	forceFixLayout->addStretch();
	ElaToggleSwitch* forceFixToggleSwitch = new ElaToggleSwitch(forceFixArea);
	forceFixToggleSwitch->setIsToggled(forceFix);
	forceFixLayout->addWidget(forceFixToggleSwitch);
	mainLayout->addWidget(forceFixArea);

	// 报错阈值
	int errorThreshold = toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "报错阈值", 28);
	ElaScrollPageArea* errorThresholdArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* errorThresholdLayout = new QHBoxLayout(errorThresholdArea);
	ElaDoubleText* errorThresholdText = new ElaDoubleText(tr("报错阈值"), 16, tr("单行字符数超过此阈值时报错"), 10, "", errorThresholdArea);
	errorThresholdLayout->addWidget(errorThresholdText);
	errorThresholdLayout->addStretch();
	ElaSpinBox* errorThresholdSpinBox = new ElaSpinBox(errorThresholdArea);
	errorThresholdSpinBox->setRange(1, 9999);
	errorThresholdSpinBox->setValue(errorThreshold);
	errorThresholdLayout->addWidget(errorThresholdSpinBox);
	mainLayout->addWidget(errorThresholdArea);

	mainLayout->addSpacing(15);
	ElaText* tokenizerConfigText = new ElaText(tr("分词器设置"), this);
	tokenizerConfigText->setWordWrap(false);
	tokenizerConfigText->setTextPixelSize(18);
	mainLayout->addWidget(tokenizerConfigText);

	mainLayout->addSpacing(10);

	// 使用分词器
	bool useTokenizer = toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "使用分词器", false);
	ElaScrollPageArea* useTokenizerArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* useTokenizerLayout = new QHBoxLayout(useTokenizerArea);
	ElaDoubleText* useTokenizerText = new ElaDoubleText(tr("使用分词器"), 16, tr("可能可以获得更好的换行效果，其中 pkuseg 的安装需要电脑上有 MS C++ Build Tools"), 10, "", useTokenizerArea);
	useTokenizerLayout->addWidget(useTokenizerText);
	useTokenizerLayout->addStretch();
	ElaToggleSwitch* useTokenizerToggleSwitch = new ElaToggleSwitch(useTokenizerArea);
	useTokenizerToggleSwitch->setIsToggled(useTokenizer);
	useTokenizerLayout->addWidget(useTokenizerToggleSwitch);
	mainLayout->addWidget(useTokenizerArea);

	// tokenizerBackend
	QStringList tokenizerBackends = { "MeCab", "spaCy", "Stanza", "pkuseg" };
	QString tokenizerBackend = QString::fromStdString(toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "tokenizerBackend", "MeCab"));
	ElaScrollPageArea* tokenizerBackendArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* tokenizerBackendLayout = new QHBoxLayout(tokenizerBackendArea);
	ElaDoubleText* tokenizerBackendText = new ElaDoubleText(tr("分词器后端"), 16, tr("应选择适合目标语言的后端/模型/字典"), 10, "", tokenizerBackendArea);
	tokenizerBackendLayout->addWidget(tokenizerBackendText);
	tokenizerBackendLayout->addStretch();
	ElaComboBox* tokenizerBackendComboBox = new ElaComboBox(tokenizerBackendArea);
	tokenizerBackendComboBox->addItems(tokenizerBackends);
	if (!tokenizerBackend.isEmpty()) {
		int index = tokenizerBackends.indexOf(tokenizerBackend);
		if (index >= 0) {
			tokenizerBackendComboBox->setCurrentIndex(index);
		}
	}
	tokenizerBackendLayout->addWidget(tokenizerBackendComboBox);
	mainLayout->addWidget(tokenizerBackendArea);

	// mecabDictDir
	QString mecabDictDir = QString::fromStdString(toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "mecabDictDir", "BaseConfig/mecabDict/mecab-chinese"));
	ElaScrollPageArea* mecabDictDirArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* mecabDictDirLayout = new QHBoxLayout(mecabDictDirArea);
	ElaDoubleText* mecabDictDirText = new ElaDoubleText(tr("MeCab词典目录"), 16, tr("MeCab中文词典需手动下载"), 10, "", mecabDictDirArea);
	mecabDictDirLayout->addWidget(mecabDictDirText);
	mecabDictDirLayout->addStretch();
	ElaLineEdit* mecabDictDirLineEdit = new ElaLineEdit(mecabDictDirArea);
	mecabDictDirLineEdit->setFixedWidth(400);
	mecabDictDirLineEdit->setText(mecabDictDir);
	mecabDictDirLayout->addWidget(mecabDictDirLineEdit);
	ElaPushButton* browseMecabDictDirButton = new ElaPushButton(tr("浏览"), mecabDictDirArea);
	mecabDictDirLayout->addWidget(browseMecabDictDirButton);
	connect(browseMecabDictDirButton, &ElaPushButton::clicked, this, [=]()
		{
			QString dir = QFileDialog::getExistingDirectory(window(), tr("选择MeCab词典目录"), mecabDictDirLineEdit->text());
			if (!dir.isEmpty()) {
				mecabDictDirLineEdit->setText(dir);
			}
		});
	mainLayout->addWidget(mecabDictDirArea);

	// spaCyModelName https://spacy.io/models
	QString spaCyModelName = QString::fromStdString(toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "spaCyModelName", "zh_core_web_lg"));
	ElaScrollPageArea* spaCyModelNameArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* spaCyModelNameLayout = new QHBoxLayout(spaCyModelNameArea);
	ElaDoubleText* spaCyModelNameText = new ElaDoubleText(tr("spaCy模型名称"), 16, tr("spaCy模型名称，新模型下载后需重启程序"), 10, "", spaCyModelNameArea);
	spaCyModelNameLayout->addWidget(spaCyModelNameText);
	spaCyModelNameLayout->addStretch();
	ElaLineEdit* spaCyModelNameLineEdit = new ElaLineEdit(spaCyModelNameArea);
	spaCyModelNameLineEdit->setFixedWidth(200);
	spaCyModelNameLineEdit->setText(spaCyModelName);
	spaCyModelNameLayout->addWidget(spaCyModelNameLineEdit);
	ElaPushButton* browseSpaCyModelButton = new ElaPushButton(tr("浏览"), spaCyModelNameArea);
	browseSpaCyModelButton->setToolTip(tr("浏览模型目录"));
	spaCyModelNameLayout->addWidget(browseSpaCyModelButton);
	connect(browseSpaCyModelButton, &ElaPushButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl("https://spacy.io/models"));
		});
	mainLayout->addWidget(spaCyModelNameArea);

	// Stanza https://stanfordnlp.github.io/stanza/ner_models.html
	QString stanzaLang = QString::fromStdString(toml::find_or(m_projectConfig, "plugins", "TextLinebreakFix", "stanzaLang", "zh"));
	ElaScrollPageArea* stanzaArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* stanzaLayout = new QHBoxLayout(stanzaArea);
	ElaDoubleText* stanzaText = new ElaDoubleText(tr("Stanza语言ID"), 16, tr("Stanza语言ID，新模型下载后需重启程序"), 10, "", stanzaArea);
	stanzaLayout->addWidget(stanzaText);
	stanzaLayout->addStretch();
	ElaLineEdit* stanzaLineEdit = new ElaLineEdit(stanzaArea);
	stanzaLineEdit->setFixedWidth(200);
	stanzaLineEdit->setText(stanzaLang);
	stanzaLayout->addWidget(stanzaLineEdit);
	ElaPushButton* browseStanzaModelButton = new ElaPushButton(tr("浏览"), stanzaArea);
	browseStanzaModelButton->setToolTip(tr("浏览模型目录"));
	stanzaLayout->addWidget(browseStanzaModelButton);
	connect(browseStanzaModelButton, &ElaPushButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl("https://stanfordnlp.github.io/stanza/ner_models.html"));
		});
	mainLayout->addWidget(stanzaArea);


	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.换行模式", fixModes[fixModeComboBox->currentIndex()].toStdString());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.优先阈值", priorityThresholdSlider->value());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.分段字数阈值", segmentThresholdSpinBox->value());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.强制修复", forceFixToggleSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.报错阈值", errorThresholdSpinBox->value());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.使用分词器", useTokenizerToggleSwitch->getIsToggled());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.tokenizerBackend", tokenizerBackends[tokenizerBackendComboBox->currentIndex()].toStdString());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.mecabDictDir", mecabDictDirLineEdit->text().toStdString());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.spaCyModelName", spaCyModelNameLineEdit->text().toStdString());
			insertToml(m_projectConfig, "plugins.TextLinebreakFix.stanzaLang", stanzaLineEdit->text().toStdString());
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("换行修复设置"));
	addCentralWidget(centerWidget, true, false, 0);
}

TLFCfgPage::~TLFCfgPage()
{

}
