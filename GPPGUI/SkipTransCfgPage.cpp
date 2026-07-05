#include "SkipTransCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "ElaToggleSwitch.h"
#include "ElaText.h"
#include "ElaToolTip.h"
#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaMessageBar.h"
#include "ElaDoubleText.h"

import Tool;

SkipTransCfgPage::SkipTransCfgPage(toml::ordered_value& projectConfig, QWidget* parent)
    : BasePage(parent), m_projectConfig(projectConfig)
{
    setWindowTitle(tr("跳过翻译设置"));
    setContentsMargins(30, 15, 15, 0);

    // 创建中心部件和布局
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

    // skipH
    bool skipH = toml::find_or(m_projectConfig, "plugins", "SkipTrans", "skipH", false);
    ElaScrollPageArea* skipHArea = new ElaScrollPageArea(centerWidget);
    QHBoxLayout* skipHLayout = new QHBoxLayout(skipHArea);
    ElaDoubleText* skipHText = new ElaDoubleText(tr("跳过 H 关键字"), 16, tr("关键字列表以 base64 编码形式存储在 SkipTrans.toml 中"), 10, "", skipHArea);
    skipHLayout->addWidget(skipHText);
    skipHLayout->addStretch();
    ElaToggleSwitch* skipHSwitch = new ElaToggleSwitch(skipHArea);
    skipHSwitch->setIsToggled(skipH);
    skipHLayout->addWidget(skipHSwitch);
    mainLayout->addWidget(skipHArea);

	mainLayout->addSpacing(20);

    // skipKeys
	toml::ordered_value skipKeysArr = toml::find_or_default<toml::ordered_value>(m_projectConfig, "plugins", "SkipTrans", "skipKeys");
	if (!skipKeysArr.is_array()) {
		skipKeysArr = toml::array{};
	}
	skipKeysArr.comments().clear();
	ElaText* skipKeysHelperText = new ElaText("skipKeys", 18, centerWidget);
	skipKeysHelperText->setWordWrap(false);
	ElaToolTip* skipKeysHelperTip = new ElaToolTip(skipKeysHelperText);
	skipKeysHelperTip->setToolTip(tr("语法与 retranslKeys 完全相同"));
	mainLayout->addWidget(skipKeysHelperText);
	ElaPlainTextEdit* skipKeysEdit = new ElaPlainTextEdit(centerWidget);
	skipKeysEdit->setMinimumHeight(330);

	QFont font = skipKeysEdit->font();
	font.setPixelSize(14);
	skipKeysEdit->setFont(font);
	skipKeysEdit->setPlainText(QString::fromStdString(toml::format(toml::ordered_value{ toml::ordered_table{{ "skipKeys", skipKeysArr}}})));
	skipKeysEdit->moveCursor(QTextCursor::Start);
	mainLayout->addWidget(skipKeysEdit);


    m_applyFunc = [=]
        {
            insertToml(m_projectConfig, "plugins.SkipTrans.skipH", skipHSwitch->getIsToggled());

			try {
				toml::ordered_value newSkipKeysTbl = toml::parse_str<toml::ordered_type_config>(skipKeysEdit->toPlainText().toStdString());
				auto& newSkipKeysArr = newSkipKeysTbl["skipKeys"];
				if (newSkipKeysArr.is_array()) {
					insertToml(m_projectConfig, "plugins.SkipTrans.skipKeys", newSkipKeysArr);
				}
				else {
					insertToml(m_projectConfig, "plugins.SkipTrans.skipKeys", toml::array{});
				}
			}
			catch (...) {
				ElaMessageBar::error(ElaMessageBarType::TopLeft, tr("解析错误"), tr("skipKeys 不符合 toml 规范"), 3000);
			}
        };

    mainLayout->addStretch();

    centerWidget->setWindowTitle(tr("跳过翻译设置"));
    addCentralWidget(centerWidget, true, false, 0);
}

SkipTransCfgPage::~SkipTransCfgPage()
{

}
