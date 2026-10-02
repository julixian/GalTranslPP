#include "PromptSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QStackedWidget>
#include <utility>
#include <vector>

#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaTabWidget.h"
#include "ElaToolButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "ElaFlowLayout.h"

import Tool;

using namespace gpp;

PromptSettingsPage::PromptSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), m_projectConfig(projectConfig), m_projectDir(projectDir)
{
	setWindowTitle(tr("项目提示词设置"));
	setTitleVisible(false);

	if (fs::exists(m_projectDir / L"Prompt.toml")) {
		try {
			m_promptConfig = gpp::uoparse(m_projectDir / L"Prompt.toml");
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"),
				tr("项目 %1 的提示词配置文件不符合标准。")
				.arg(QString::fromStdWString(m_projectDir.filename().wstring())), 3000);
		}
	}
	else if (fs::exists(defaultPromptPath)) {
		try {
			m_promptConfig = gpp::uoparse(defaultPromptPath);
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("默认提示词文件不符合 toml 规范"), 3000);
		}
	}
	else {
		ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("找不到提示词文件"), 3000);
	}

	setupUi();
}


void PromptSettingsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 10, 10, 0);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);


	auto createPromptWidgetFunc =
		[=](const QString& promptName, const std::string& userPromptKey, const std::string& systemPromptKey,
			const std::optional<std::string>& agentUserPromptKey = std::nullopt, const std::optional<std::string>& agentSystemPromptKey = std::nullopt,
			const std::optional<std::string>& advancedUserPromptKey = std::nullopt, const std::optional<std::string>& advancedSystemPromptKey = std::nullopt) -> std::function<void()>
		{
			QWidget* promptWidget = new QWidget(mainWidget);
			QVBoxLayout* promptLayout = new QVBoxLayout(promptWidget);
			promptLayout->setContentsMargins(0, 0, 0, 0);
			ElaFlowLayout* promptButtonLayout = new ElaFlowLayout(0, 6, 6);
			QStackedWidget* promptStackedWidget = new QStackedWidget(promptWidget);
			QButtonGroup* promptButtonGroup = new QButtonGroup(promptWidget);
			std::vector<std::pair<std::string, ElaPlainTextEdit*>> promptEdits;

			// 按同一顺序登记按钮、编辑器和配置键，保存时统一写回对应提示词。
			auto addPromptEditFunc = [&](const QString& title, auto icon, const std::string& key)
				{
					ElaToolButton* promptButton = new ElaToolButton(promptWidget);
					promptButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
					promptButton->setElaIcon(icon);
					promptButton->setText(title);
					promptButton->setEnabled(!promptEdits.empty());
					promptButtonGroup->addButton(promptButton, static_cast<int>(promptEdits.size()));
					promptButtonLayout->addWidget(promptButton);

					ElaPlainTextEdit* promptTextEdit = new ElaPlainTextEdit(promptStackedWidget);
					QFont plainTextFont = promptTextEdit->font();
					plainTextFont.setPixelSize(15);
					promptTextEdit->setFont(plainTextFont);
					promptTextEdit->setPlainText(QString::fromStdString(toml::find_or(m_promptConfig, key, "")));
					promptStackedWidget->addWidget(promptTextEdit);
					promptEdits.emplace_back(key, promptTextEdit);
				};
			addPromptEditFunc(tr("用户提示词"), ElaIconType::User, userPromptKey);
			addPromptEditFunc(tr("系统提示词"), ElaIconType::Gear, systemPromptKey);
			if (agentUserPromptKey.has_value() && agentSystemPromptKey.has_value()) {
				addPromptEditFunc(tr("Agent 用户"), ElaIconType::UserRobot, agentUserPromptKey.value());
				addPromptEditFunc(tr("Agent 系统"), ElaIconType::Robot, agentSystemPromptKey.value());
			}
			if (advancedUserPromptKey.has_value() && advancedSystemPromptKey.has_value()) {
				addPromptEditFunc(tr("高级 Agent 用户"), ElaIconType::UserRobot, advancedUserPromptKey.value());
				addPromptEditFunc(tr("高级 Agent 系统"), ElaIconType::Robot, advancedSystemPromptKey.value());
			}
			connect(promptButtonGroup, &QButtonGroup::buttonClicked, this, [=](QAbstractButton* button)
				{
					for (const auto& b : promptButtonGroup->buttons()) {
						b->setEnabled(true);
					}
					button->setEnabled(false);
					promptStackedWidget->setCurrentIndex(promptButtonGroup->id(button));
				});
			promptStackedWidget->setCurrentIndex(0);

			auto resultApply2ConfigFunc = [=]()
				{
					for (const auto& [key, edit] : promptEdits) {
						toml::ordered_value promptVal = edit->toPlainText().toStdString();
						promptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
						insertToml(m_promptConfig, key, promptVal);
					}
				};

			promptLayout->addLayout(promptButtonLayout);
			promptLayout->addWidget(promptStackedWidget);
			tabWidget->addTab(promptWidget, promptName);
			return resultApply2ConfigFunc;
		};

	auto forgalTsvApplyFunc = createPromptWidgetFunc("ForGalTsv", "FORGALTSV_USER", "FORGALTSV_SYSTEM",
		"FORGALTSV_AGENT_USER", "FORGALTSV_AGENT_SYSTEM",
		"FORGALTSV_AGENT_ADVANCED_USER", "FORGALTSV_AGENT_ADVANCED_SYSTEM");
	auto forNovelTsvApplyFunc = createPromptWidgetFunc("ForNovelTsv", "FORNOVELTSV_USER", "FORNOVELTSV_SYSTEM",
		"FORNOVELTSV_AGENT_USER", "FORNOVELTSV_AGENT_SYSTEM",
		"FORNOVELTSV_AGENT_ADVANCED_USER", "FORNOVELTSV_AGENT_ADVANCED_SYSTEM");
	auto forgalJsonApplyFunc = createPromptWidgetFunc("ForGalJson", "FORGALJSON_USER", "FORGALJSON_SYSTEM");
	auto sakuraApplyFunc = createPromptWidgetFunc("Sakura", "SAKURA_USER", "SAKURA_SYSTEM");
	auto gendictApplyFunc = createPromptWidgetFunc("GenDict", "GENDICT_USER", "GENDICT_SYSTEM");
	auto nametransApplyFunc = createPromptWidgetFunc("NameTrans", "NAMETRANS_USER", "NAMETRANS_SYSTEM");

	m_applyFunc = [=]()
		{
			forgalJsonApplyFunc();
			forgalTsvApplyFunc();
			forNovelTsvApplyFunc();
			sakuraApplyFunc();
			gendictApplyFunc();
			nametransApplyFunc();
			atomicOutputFile(m_projectDir / L"Prompt.toml", toml::format(m_promptConfig));
		};

	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
