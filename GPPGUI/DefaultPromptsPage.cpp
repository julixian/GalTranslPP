#include "DefaultPromptsPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QStackedWidget>
#include <utility>
#include <vector>

#include "ElaToolButton.h"
#include "ElaPlainTextEdit.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaTabWidget.h"

import Tool;

using namespace gpp;
namespace fs = std::filesystem;

DefaultPromptsPage::DefaultPromptsPage(QWidget* parent)
    : BasePage(parent)
{
    setWindowTitle(tr("默认提示词管理"));
    setTitleVisible(false);

	if (fs::exists(defaultPromptPath)) {
		try {
			m_promptConfig = gpp::uoparseToml(defaultPromptPath);
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, tr("解析失败"), tr("默认提示词配置文件不符合 toml 规范"), 3000);
		}
	}

    setupUi();
}

void DefaultPromptsPage::setupUi()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(10, 20, 10, 0);

	ElaTabWidget* tabWidget = new ElaTabWidget(mainWidget);
	tabWidget->setTabsClosable(false);
	tabWidget->setIsTabTransparent(true);


	auto createPromptWidgetFunc =
		[=, this](const QString& promptName, const std::string& userPromptKey, const std::string& systemPromptKey,
			const std::optional<std::string>& agentUserPromptKey = std::nullopt, const std::optional<std::string>& agentSystemPromptKey = std::nullopt) -> std::function<void()>
		{
			QWidget* promptWidget = new QWidget(mainWidget);
			QVBoxLayout* promptLayout = new QVBoxLayout(promptWidget);
			promptLayout->setContentsMargins(0, 0, 0, 0);
			QWidget* promptButtonWidget = new QWidget(promptWidget);
			QHBoxLayout* promptButtonLayout = new QHBoxLayout(promptButtonWidget);
			promptButtonLayout->setContentsMargins(0, 0, 0, 0);
			promptButtonLayout->setSpacing(6);
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
			connect(promptButtonGroup, &QButtonGroup::buttonClicked, this, [=](QAbstractButton* button)
				{
					for (const auto& b : promptButtonGroup->buttons()) {
						b->setEnabled(true);
					}
					button->setEnabled(false);
					promptStackedWidget->setCurrentIndex(promptButtonGroup->id(button));
				});
			promptStackedWidget->setCurrentIndex(0);

			auto resultApply2ConfigFunc = [=, this]()
				{
					for (const auto& [key, edit] : promptEdits) {
						toml::ordered_value promptVal = edit->toPlainText().toStdString();
						promptVal.as_string_fmt().fmt = toml::string_format::multiline_basic;
						insertToml(m_promptConfig, key, promptVal);
					}
				};

			ElaToolButton* promptSaveAllButton = new ElaToolButton(promptWidget);
			promptSaveAllButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			promptSaveAllButton->setElaIcon(ElaIconType::CheckDouble);
			promptSaveAllButton->setText(tr("全部保存"));
			ElaToolButton* promptSaveButton = new ElaToolButton(promptWidget);
			promptSaveButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			promptSaveButton->setElaIcon(ElaIconType::Check);
			promptSaveButton->setText(tr("保存"));
			promptButtonLayout->addStretch();
			promptButtonLayout->addWidget(promptSaveAllButton);
			promptButtonLayout->addWidget(promptSaveButton);
			connect(promptSaveAllButton, &ElaToolButton::clicked, this, [=, this]()
				{
					this->apply2Config();
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"), tr("所有默认提示词配置已保存。"), 3000);
				});
			connect(promptSaveButton, &ElaToolButton::clicked, this, [=, this]()
				{
					resultApply2ConfigFunc();
					atomicOutputFile(defaultPromptPath, toml::format(m_promptConfig));
					ElaMessageBar::success(ElaMessageBarType::TopRight, tr("保存成功"),
						tr("默认 %1 提示词配置已保存。").arg(promptName), 3000);
				});

			promptLayout->addWidget(promptButtonWidget);
			promptLayout->addWidget(promptStackedWidget);
			tabWidget->addTab(promptWidget, promptName);
			return resultApply2ConfigFunc;
		};

	auto forgalJsonApplyFunc = createPromptWidgetFunc("ForGalJson", "FORGALJSON_USER", "FORGALJSON_SYSTEM",
		"FORGALJSON_AGENT_USER", "FORGALJSON_AGENT_SYSTEM");
	auto forNovelJsonApplyFunc = createPromptWidgetFunc("ForNovelJson", "FORNOVELJSON_USER", "FORNOVELJSON_SYSTEM",
		"FORNOVELJSON_AGENT_USER", "FORNOVELJSON_AGENT_SYSTEM");
	auto forgalTsvApplyFunc = createPromptWidgetFunc("ForGalTsv", "FORGALTSV_USER", "FORGALTSV_SYSTEM",
		"FORGALTSV_AGENT_USER", "FORGALTSV_AGENT_SYSTEM");
	auto forNovelTsvApplyFunc = createPromptWidgetFunc("ForNovelTsv", "FORNOVELTSV_USER", "FORNOVELTSV_SYSTEM",
		"FORNOVELTSV_AGENT_USER", "FORNOVELTSV_AGENT_SYSTEM");
	auto sakuraApplyFunc = createPromptWidgetFunc("Sakura", "SAKURA_USER", "SAKURA_SYSTEM");
	auto gendictApplyFunc = createPromptWidgetFunc("GenDict", "GENDICT_USER", "GENDICT_SYSTEM");
	auto nametransApplyFunc = createPromptWidgetFunc("NameTrans", "NAMETRANS_USER", "NAMETRANS_SYSTEM");


	m_applyFunc = [=, this]()
		{
			forgalJsonApplyFunc();
			forNovelJsonApplyFunc();
			forgalTsvApplyFunc();
			forNovelTsvApplyFunc();
			sakuraApplyFunc();
			gendictApplyFunc();
			nametransApplyFunc();
			atomicOutputFile(defaultPromptPath, toml::format(m_promptConfig));
		};

	mainLayout->addWidget(tabWidget);
    addCentralWidget(mainWidget, true, false, 0);
}
