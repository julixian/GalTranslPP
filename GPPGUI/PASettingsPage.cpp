#include "PASettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QButtonGroup>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaMessageBar.h"
#include "ElaToolButton.h"
#include "ElaPlainTextEdit.h"
#include "ElaToggleButton.h"
#include "ElaNoWheelComboBox.h"
#include "ElaScrollArea.h"
#include "ElaScrollBar.h"
#include "ElaWidget.h"
#include "ValueSliderWidget.h"
#include "ElaFlowLayout.h"
#include "ElaDoubleText.h"
#include "TreeSitterHighlighter.h"

import Tool;

PASettingsPage::PASettingsPage(toml::ordered_value& projectConfig, QWidget* parent) : BasePage(parent), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("问题分析"));
	setTitleVisible(false);

	setupUi();
}

PASettingsPage::~PASettingsPage()
{
	delete m_compareConfigWidget;
}

void PASettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(20, 15, 15, 0);

	struct ProblemDefinition {
		std::string key;
		QString label;
		bool defaultEnabled;
		std::string defaultBase;
		std::string defaultCheck;
	};
	struct ProblemRow {
		std::string key;
		QString label;
		ElaToggleButton* enableButton = nullptr;
		ElaNoWheelComboBox* baseComboBox = nullptr;
		ElaNoWheelComboBox* checkComboBox = nullptr;
		QString base;
		QString check;
	};
	const std::vector<ProblemDefinition> problemDefinitions = {
		{ "HighFrequency", tr("词频过高"), false, "orig", "transview" },
		{ "PunctuationMismatch", tr("标点错漏"), true, "orig", "transview" },
		{ "LinebreakLost", tr("丢失换行"), true, "orig", "transview" },
		{ "LinebreakAdded", tr("多加换行"), true, "orig", "transview" },
		{ "LongerThanSource", tr("比原文长"), false, "orig", "transview" },
		{ "StrictlyLongerThanSource", tr("比原文长严格"), false, "orig", "transview" },
		{ "DictionaryUnused", tr("字典未使用"), true, "preproc", "transview" },
		{ "JapaneseRemains", tr("残留日文"), true, "orig", "transview" },
		{ "LatinIntroduced", tr("引入拉丁字母"), false, "orig", "transview" },
		{ "HangulIntroduced", tr("引入韩文"), true, "preproc", "transview" },
		{ "TraditionalChineseIntroduced", tr("引入繁体字"), true, "orig", "transview" },
		{ "NotTargetLanguage", tr("语言不通"), false, "orig", "transview" },
		{ "InvalidCharacter", tr("非法字符"), false, "orig", "transview" },
	};
	const QStringList cachePartNames = {
		"orig", "preproc", "transraw", "transview",
	};
	std::vector<ProblemRow> problemRows;
	ElaText* problemListTitle = new ElaText(tr("要发现的问题清单"), mainWidget);
	problemListTitle->setTextPixelSize(18);
	mainLayout->addWidget(problemListTitle);
	ElaFlowLayout* problemListLayout = new ElaFlowLayout(0, 8, 8);
	const toml::ordered_value& projectConfig = m_projectConfig;
	for (const ProblemDefinition& definition : problemDefinitions) {
		const auto problemConfigTableOpt = toml::find<
			std::optional<toml::ordered_table>
		>(projectConfig, "problemAnalyze", definition.key);
		bool enabled = definition.defaultEnabled;
		std::string base = definition.defaultBase;
		std::string check = definition.defaultCheck;
		if (problemConfigTableOpt) {
			const toml::ordered_value problemConfigValue = problemConfigTableOpt.value();
			enabled = toml::find_or(problemConfigValue, "enable", definition.defaultEnabled);
			base = toml::find_or(problemConfigValue, "base", definition.defaultBase);
			check = toml::find_or(problemConfigValue, "check", definition.defaultCheck);
		}
		if (!cachePartNames.contains(QString::fromStdString(base))) {
			base = definition.defaultBase;
		}
		if (!cachePartNames.contains(QString::fromStdString(check))) {
			check = definition.defaultCheck;
		}

		ElaToggleButton* enableButton = new ElaToggleButton(definition.label, mainWidget);
		enableButton->setFixedWidth(150);
		enableButton->setIsToggled(enabled);
		problemRows.push_back({
			definition.key,
			definition.label,
			enableButton,
			nullptr,
			nullptr,
			QString::fromStdString(base),
			QString::fromStdString(check),
		});
		problemListLayout->addWidget(enableButton);
	}
	mainLayout->addLayout(problemListLayout);
	mainLayout->addSpacing(10);

	// 规定标点错漏要查哪些标点
	const std::string punctuationSet = toml::find_or(m_projectConfig, "problemAnalyze", "punctSet",
		"（()）：:*[]{}<>『』「」“”;；'/\\");
	QString punctuationSetStr = QString::fromStdString(punctuationSet);
	ElaScrollPageArea* punctuationListArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* punctuationListLayout = new QHBoxLayout(punctuationListArea);
	ElaDoubleText* punctuationListTitle = new ElaDoubleText(tr("标点查错"), 16,
		tr("规定标点错漏要查哪些标点"), 10, "", punctuationListArea);
	punctuationListLayout->addWidget(punctuationListTitle);
	punctuationListLayout->addStretch();
	ElaLineEdit* punctuationList = new ElaLineEdit(punctuationListArea);
	punctuationList->setText(punctuationSetStr);
	punctuationListLayout->addWidget(punctuationList);
	mainLayout->addWidget(punctuationListArea);

	// 语言不通检测的语言置信度，设置越高则检测越精准，但可能遗漏，反之亦然
	double languageProbability = toml::find_or(m_projectConfig, "problemAnalyze", "langProbability", 0.94);
	ElaScrollPageArea* languageProbabilityArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* languageProbabilityLayout = new QHBoxLayout(languageProbabilityArea);
	ElaDoubleText* languageProbabilityTitle = new ElaDoubleText(tr("语言置信度"), 16,
		tr("语言不通检测的语言置信度(0-1)，设置越高则检测越精准，但可能遗漏，反之亦然"), 10, "", languageProbabilityArea);
	languageProbabilityLayout->addWidget(languageProbabilityTitle);
	languageProbabilityLayout->addStretch();
	ValueSliderWidget* languageProbabilitySlider = new ValueSliderWidget(0.0, 1.0, languageProbabilityArea);
	languageProbabilitySlider->setFixedWidth(500);
	languageProbabilitySlider->setValue(languageProbability);
	languageProbabilityLayout->addWidget(languageProbabilitySlider);
	mainLayout->addWidget(languageProbabilityArea);

	// 非法字符要检查的字符集
	const std::string codePage = toml::find_or(m_projectConfig, "problemAnalyze", "codePage", "gbk");
	ElaScrollPageArea* codePageArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* codePageLayout = new QHBoxLayout(codePageArea);
	ElaDoubleText* codePageTitle = new ElaDoubleText(tr("字符集"), 16,
		tr("非法字符要检查的字符集"), 10, "", codePageArea);
	codePageLayout->addWidget(codePageTitle);
	codePageLayout->addStretch();
	ElaLineEdit* codePageEdit = new ElaLineEdit(codePageArea);
	codePageEdit->setFixedWidth(150);
	codePageEdit->setText(QString::fromStdString(codePage));
	codePageLayout->addWidget(codePageEdit);
	mainLayout->addWidget(codePageArea);

	m_compareConfigWidget = new ElaWidget();
	ElaWidget* compareConfigWidget = m_compareConfigWidget;
	compareConfigWidget->setWindowTitle(tr("比较对象设置"));
	compareConfigWidget->setWindowModality(Qt::ApplicationModal);
	compareConfigWidget->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
	compareConfigWidget->resize(760, 760);
	QVBoxLayout* compareConfigLayout = new QVBoxLayout(compareConfigWidget);
	compareConfigLayout->setContentsMargins(10, 0, 10, 10);
	compareConfigLayout->setSpacing(0);

	ElaScrollArea* compareScrollArea = new ElaScrollArea(compareConfigWidget);
	compareScrollArea->setMouseTracking(true);
	compareScrollArea->setIsAnimation(Qt::Vertical, true);
	compareScrollArea->setWidgetResizable(true);
	compareScrollArea->setIsGrabGesture(false, 0);
	compareScrollArea->setIsOverShoot(Qt::Vertical, true);
	compareScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ElaScrollBar* compareScrollBar = new ElaScrollBar(compareScrollArea->verticalScrollBar(), compareScrollArea);
	compareScrollBar->setIsAnimation(true);

	QWidget* compareContent = new QWidget(compareScrollArea);
	compareContent->setObjectName("ElaScrollPageContainer");
	compareContent->setStyleSheet("#ElaScrollPageContainer{background-color:transparent;}");
	QVBoxLayout* compareContentLayout = new QVBoxLayout(compareContent);
	compareContentLayout->setContentsMargins(0, 0, 10, 0);
	compareContentLayout->setSpacing(8);

	QWidget* compareIntroWidget = new QWidget(compareContent);
	QVBoxLayout* compareIntroLayout = new QVBoxLayout(compareIntroWidget);
	compareIntroLayout->setContentsMargins(12, 8, 12, 8);
	compareIntroLayout->setSpacing(8);
	ElaText* compareIntroTitle = new ElaText(tr("比较对象"), compareIntroWidget);
	compareIntroTitle->setTextPixelSize(18);
	ElaText* compareIntroDescription = new ElaText(tr("base 是比较基准字段，check 是被检查字段。"), compareIntroWidget);
	compareIntroDescription->setTextPixelSize(11);
	compareIntroLayout->addWidget(compareIntroTitle);
	compareIntroLayout->addWidget(compareIntroDescription);
	compareContentLayout->addWidget(compareIntroWidget);

	for (ProblemRow& row : problemRows) {
		ElaScrollPageArea* rowArea = new ElaScrollPageArea(compareContent);
		QHBoxLayout* rowLayout = new QHBoxLayout(rowArea);
		rowLayout->setContentsMargins(12, 0, 12, 0);
		rowLayout->setSpacing(8);

		rowLayout->addWidget(new ElaText(row.label, 16, rowArea));
		rowLayout->addStretch();

		rowLayout->addWidget(new ElaText("base", 13, rowArea));
		ElaNoWheelComboBox* baseComboBox = new ElaNoWheelComboBox(rowArea);
		baseComboBox->setFixedWidth(150);
		baseComboBox->addItems(cachePartNames);
		baseComboBox->setCurrentText(row.base);
		rowLayout->addWidget(baseComboBox);

		rowLayout->addWidget(new ElaText("check", 13, rowArea));
		ElaNoWheelComboBox* checkComboBox = new ElaNoWheelComboBox(rowArea);
		checkComboBox->setFixedWidth(150);
		checkComboBox->addItems(cachePartNames);
		checkComboBox->setCurrentText(row.check);
		rowLayout->addWidget(checkComboBox);

		row.baseComboBox = baseComboBox;
		row.checkComboBox = checkComboBox;
		compareContentLayout->addWidget(rowArea);
	}
	compareContentLayout->addStretch();
	compareScrollArea->setWidget(compareContent);
	compareConfigLayout->addWidget(compareScrollArea);
	compareConfigWidget->hide();

	ElaScrollPageArea* compareObjectArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* compareObjectLayout = new QHBoxLayout(compareObjectArea);
	ElaDoubleText* compareObjectTitle = new ElaDoubleText(tr("比较对象设置"), 16,
		tr("设置每个问题分析规则使用的 base/check 字段"), 10, "", compareObjectArea);
	compareObjectLayout->addWidget(compareObjectTitle);
	compareObjectLayout->addStretch();
	ElaToolButton* compareObjectButton = new ElaToolButton(compareObjectArea);
	compareObjectButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	compareObjectButton->setElaIcon(ElaIconType::CodeCompare);
	compareObjectButton->setText(tr("进入设置"));
	compareObjectButton->setFixedWidth(110);
	compareObjectLayout->addWidget(compareObjectButton);
	connect(compareObjectButton, &ElaToolButton::clicked, this, [=]()
		{
			compareConfigWidget->move(window()->frameGeometry().center()
				- compareConfigWidget->rect().center());
			compareConfigWidget->show();
			compareConfigWidget->raise();
			compareConfigWidget->activateWindow();
		});
	mainLayout->addWidget(compareObjectArea);

	mainLayout->addSpacing(20);

	auto createPAPlainTextEditAreaFunc =
		[=](const std::string& configKey, const QString& title, std::optional<int> minHeight = std::nullopt)
		-> std::function<void()>
		{
			toml::ordered_array PASettingsArr = toml::find_or_default<toml::ordered_array>(m_projectConfig, "problemAnalyze", configKey);
			ElaScrollPageArea* PASettingsArea = new ElaScrollPageArea(mainWidget);
			QVBoxLayout* PASettingsLayout = new QVBoxLayout(PASettingsArea);
			PASettingsLayout->setContentsMargins(12, 6, 12, 8);
			PASettingsLayout->setSpacing(6);
			PASettingsLayout->addWidget(new ElaDoubleText(title, 16,
				tr("正则表达式数组，具体规则见下方语法示例"), 10, "", PASettingsArea));
			ElaPlainTextEdit* PASettingsEdit = new ElaPlainTextEdit(PASettingsArea);
			if (minHeight) {
				PASettingsArea->setFixedHeight(*minHeight + 65);
				PASettingsEdit->setMinimumHeight(*minHeight);
			}
			QFont font = PASettingsEdit->font();
			font.setPixelSize(14);
			PASettingsEdit->setFont(font);
			PASettingsEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ configKey, PASettingsArr }} })));
			PASettingsEdit->moveCursor(QTextCursor::Start);
			installTreeSitterHighlighter(PASettingsEdit->document(), SyntaxLanguage::Toml);
			PASettingsLayout->addWidget(PASettingsEdit);
			mainLayout->addWidget(PASettingsArea);

			std::function<void()> saveFunc = [=]()
				{
					try {
						toml::ordered_value newPASettingsTbl = toml::parse_str<toml::ordered_type_config>(PASettingsEdit->toPlainText().toStdString());
						auto& newPASettingsArr = newPASettingsTbl[configKey];
						if (newPASettingsArr.is_array()) {
							insertToml(m_projectConfig, "problemAnalyze." + configKey, newPASettingsArr);
						}
						else {
							insertToml(m_projectConfig, "problemAnalyze." + configKey, toml::array{});
						}
					}
					catch (...) {
						ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析错误"),
							tr("%1 不符合 toml 规范").arg(QString::fromStdString(configKey)), 3000);
					}
				};
			return saveFunc;
		};

	// 正则表达式列表，重翻正则在缓存的 orig 或 某条 problem 中能 search 通过的句子。
	auto retranslKeysSaveFunc = createPAPlainTextEditAreaFunc("retranslKeys", tr("重翻关键字设定"), 330);
	mainLayout->addSpacing(20);

	// 正则表达式列表，如果一条 problem 能被以下正则 search 通过，则不加入 problems 列表
	auto skipProblemsSaveFunc = createPAPlainTextEditAreaFunc("skipProblems", tr("跳过问题关键字设定"), 330);

	QWidget* illusButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* illusButtonLayout = new QHBoxLayout(illusButtonWidget);
	illusButtonLayout->addStretch();
	ElaToolButton* illusButton = new ElaToolButton(illusButtonWidget);
	illusButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	illusButton->setElaIcon(ElaIconType::BookOpen);
	illusButton->setText(tr("语法示例"));
	ElaToolTip* illusButtonTip = new ElaToolTip(illusButton);
	illusButtonTip->setToolTip(tr("查看 重翻关键字/跳过问题关键字 设定的语法示例"));
	illusButtonLayout->addWidget(illusButton);
	connect(illusButton, &ElaToolButton::clicked, this, [=]()
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile("BaseConfig/illustration/pahelper.html"));
		});
	mainLayout->addWidget(illusButtonWidget);

	mainLayout->addStretch();

	m_applyFunc = [=]()
		{
			for (const ProblemRow& row : problemRows) {
				const bool enabled = row.enableButton->getIsToggled();
				toml::ordered_table problemConfig;
				problemConfig["enable"] = enabled;
				if (enabled) {
					problemConfig["base"] = row.baseComboBox->currentText().toStdString();
					problemConfig["check"] = row.checkComboBox->currentText().toStdString();
				}
				insertToml(m_projectConfig, "problemAnalyze." + row.key, problemConfig);
			}
			insertToml(m_projectConfig, "problemAnalyze.punctSet", punctuationList->text().toStdString());
			insertToml(m_projectConfig, "problemAnalyze.codePage", codePageEdit->text().toStdString());
			insertToml(m_projectConfig, "problemAnalyze.langProbability", languageProbabilitySlider->value());

			retranslKeysSaveFunc();
			skipProblemsSaveFunc();
		};

	addCentralWidget(mainWidget, true, false, 0);
}
