#include "PromptSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QStackedWidget>

#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaTabWidget.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"

import Tool;

PromptSettingsPage::PromptSettingsPage(fs::path& projectDir, toml::ordered_value& projectConfig, QWidget* parent) :
	BasePage(parent), m_projectConfig(projectConfig), m_projectDir(projectDir)
{
	setWindowTitle(tr("项目提示词设置"));
	setTitleVisible(false);

	if (fs::exists(m_projectDir / L"Prompt.toml")) {
		try {
			m_promptConfig = toml::uoparse(m_projectDir / L"Prompt.toml");
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("项目 ") +
				QString::fromStdWString(m_projectDir.filename().wstring()) + tr(" 的提示词配置文件不符合标准。"), 3000);
		}
	}
	else if (fs::exists(defaultPromptPath)) {
		try {
			m_promptConfig = toml::uoparse(m_projectDir / L"Prompt.toml");
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

PromptSettingsPage::~PromptSettingsPage()
{

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
			const std::optional<std::string>& agentUserPromptKey = std::nullopt, const std::optional<std::string>& agentSystemPromptKey = std::nullopt) -> std::function<void()>
		{
			const bool hasAgentPrompt = agentUserPromptKey.has_value() && agentSystemPromptKey.has_value();
			QWidget* promptWidget = new QWidget(mainWidget);
			QVBoxLayout* promptLayout = new QVBoxLayout(promptWidget);
			promptLayout->setContentsMargins(0, 0, 0, 0);

			QHBoxLayout* promptButtonLayout = new QHBoxLayout(promptWidget);
			ElaPushButton* promptUserModeButtom = new ElaPushButton(promptWidget);
			promptUserModeButtom->setText(tr("用户提示词"));
			promptUserModeButtom->setEnabled(false);
			ElaPushButton* promptSystemModeButtom = new ElaPushButton(promptWidget);
			promptSystemModeButtom->setText(tr("系统提示词"));
			promptSystemModeButtom->setEnabled(true);
			promptButtonLayout->addWidget(promptUserModeButtom);
			promptButtonLayout->addWidget(promptSystemModeButtom);
			ElaPushButton* agentPromptUserModeButtom = nullptr;
			ElaPushButton* agentPromptSystemModeButtom = nullptr;
			if (hasAgentPrompt) {
				agentPromptUserModeButtom = new ElaPushButton(promptWidget);
				agentPromptUserModeButtom->setText(tr("agent用户"));
				agentPromptUserModeButtom->setEnabled(true);
				agentPromptSystemModeButtom = new ElaPushButton(promptWidget);
				agentPromptSystemModeButtom->setText(tr("agent系统"));
				agentPromptSystemModeButtom->setEnabled(true);
				promptButtonLayout->addWidget(agentPromptUserModeButtom);
				promptButtonLayout->addWidget(agentPromptSystemModeButtom);
			}
			promptButtonLayout->addStretch();
			promptLayout->addLayout(promptButtonLayout);

			QStackedWidget* promptStackedWidget = new QStackedWidget(promptWidget);
			auto addPlainTextEditFunc = [=](const std::string& key)
				{
					ElaPlainTextEdit* promptTextEdit = new ElaPlainTextEdit(promptStackedWidget);
					QFont plainTextFont = promptTextEdit->font();
					plainTextFont.setPixelSize(15);
					promptTextEdit->setFont(plainTextFont);
					promptTextEdit->setPlainText(
						QString::fromStdString(toml::find_or(m_promptConfig, key, ""))
					);
					promptStackedWidget->addWidget(promptTextEdit);
					return promptTextEdit;
				};
			ElaPlainTextEdit* promptUserModeEdit = addPlainTextEditFunc(userPromptKey);
			ElaPlainTextEdit* promptSystemModeEdit = addPlainTextEditFunc(systemPromptKey);
			ElaPlainTextEdit* agentPromptUserModeEdit = nullptr;
			ElaPlainTextEdit* agentPromptSystemModeEdit = nullptr;
			if (hasAgentPrompt) {
				agentPromptUserModeEdit = addPlainTextEditFunc(agentUserPromptKey.value());
				agentPromptSystemModeEdit = addPlainTextEditFunc(agentSystemPromptKey.value());
			}

			QButtonGroup* promptButtomGroup = new QButtonGroup(promptWidget);
			promptButtomGroup->addButton(promptUserModeButtom, 0);
			promptButtomGroup->addButton(promptSystemModeButtom, 1);
			if (hasAgentPrompt) {
				promptButtomGroup->addButton(agentPromptUserModeButtom, 2);
				promptButtomGroup->addButton(agentPromptSystemModeButtom, 3);
			}
			connect(promptButtomGroup, &QButtonGroup::buttonClicked, this, [=](QAbstractButton* button)
				{
					for (const auto& b : promptButtomGroup->buttons()) {
						b->setEnabled(true);
					}
					button->setEnabled(false);
					promptStackedWidget->setCurrentIndex(promptButtomGroup->id(button));
				});

			promptStackedWidget->setCurrentIndex(0);
			promptLayout->addWidget(promptStackedWidget);
			tabWidget->addTab(promptWidget, promptName);

			auto result = [=]()
				{
					toml::ordered_value userPromptVal = promptUserModeEdit->toPlainText().toStdString();
					toml::ordered_value systemPromptVal = promptSystemModeEdit->toPlainText().toStdString();
					userPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
					systemPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
					insertToml(m_promptConfig, userPromptKey, userPromptVal);
					insertToml(m_promptConfig, systemPromptKey, systemPromptVal);
					if (hasAgentPrompt) {
						toml::ordered_value agentUserPromptVal = agentPromptUserModeEdit->toPlainText().toStdString();
						toml::ordered_value agentSystemPromptVal = agentPromptSystemModeEdit->toPlainText().toStdString();
						agentUserPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
						agentSystemPromptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
						insertToml(m_promptConfig, agentUserPromptKey.value(), agentUserPromptVal);
						insertToml(m_promptConfig, agentSystemPromptKey.value(), agentSystemPromptVal);
					}
				};
			return result;
		};

	auto forgalTsvApplyFunc = createPromptWidgetFunc("ForGalTsv", "FORGALTSV_TRANS_PROMPT_EN", "FORGALTSV_SYSTEM",
		"FORGALTSV_AGENT_PROMPT_EN", "FORGALTSV_AGENT_SYSTEM");
	auto forNovelTsvApplyFunc = createPromptWidgetFunc("ForNovelTsv", "FORNOVELTSV_TRANS_PROMPT_EN", "FORNOVELTSV_SYSTEM",
		"FORNOVELTSV_AGENT_PROMPT_EN", "FORNOVELTSV_AGENT_SYSTEM");
	auto forgalJsonApplyFunc = createPromptWidgetFunc("ForGalJson", "FORGALJSON_TRANS_PROMPT_EN", "FORGALJSON_SYSTEM");
	auto sakuraApplyFunc = createPromptWidgetFunc("Sakura", "SAKURA_TRANS_PROMPT", "SAKURA_SYSTEM_PROMPT");
	auto gendictApplyFunc = createPromptWidgetFunc("GenDict", "GENDICT_PROMPT", "GENDICT_SYSTEM",
		"GENDICT_REVIEW_PROMPT", "GENDICT_REVIEW_SYSTEM");
	auto nametransApplyFunc = createPromptWidgetFunc("NameTrans", "NAMETRANS_PROMPT", "NAMETRANS_SYSTEM");

	m_applyFunc = [=]()
		{
			forgalJsonApplyFunc();
			forgalTsvApplyFunc();
			forNovelTsvApplyFunc();
			sakuraApplyFunc();
			gendictApplyFunc();
			nametransApplyFunc();
			std::ofstream ofs(m_projectDir / L"Prompt.toml", std::ios::binary);
			ofs << m_promptConfig;
			ofs.close();
		};

	mainLayout->addWidget(tabWidget);
	addCentralWidget(mainWidget, true, false, 0);
}
