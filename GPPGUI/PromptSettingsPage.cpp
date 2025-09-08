#include "PromptSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QStackedWidget>

#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaTableView.h"
#include "ElaPushButton.h"
#include "ElaMessageBar.h"
#include "ElaPivot.h"
#include "ElaToggleSwitch.h"
#include "ElaPlainTextEdit.h"

import Tool;

PromptSettingsPage::PromptSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent) :
	BasePage(parent), _projectConfig(projectConfig), _projectDir(projectDir)
{
	setWindowTitle("项目提示词设置");
	setTitleVisible(false);

	if (fs::exists(_projectDir / L"Prompt.toml")) {
		try {
			std::ifstream ifs(_projectDir / L"Prompt.toml");
			_promptConfig = toml::parse(ifs);
			ifs.close();
		}
		catch (...) {
			ElaMessageBar::error(ElaMessageBarType::TopRight, "解析失败", "项目 " +
				QString(_projectDir.filename().wstring()) + " 的提示词配置文件不符合标准。", 3000);
		}
	}
	
	_setupUI();
}

PromptSettingsPage::~PromptSettingsPage()
{

}

void PromptSettingsPage::apply2Config()
{
	if (_applyFunc) {
		_applyFunc();
	}
}


void PromptSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	// 创建 Pivot 导航和 StackedWidget 内容区
	_pivot = new ElaPivot(mainWidget);
	_stackedWidget = new QStackedWidget(mainWidget);

	// 建立 Pivot 和 StackedWidget 的连接
	connect(_pivot, &ElaPivot::pivotClicked, _stackedWidget, &QStackedWidget::setCurrentIndex);

	// 把pivot包到scrollarea中
	ElaScrollPageArea* pivotScrollArea = new ElaScrollPageArea(mainWidget);
	QVBoxLayout* pivotLayout = new QVBoxLayout(pivotScrollArea);
	pivotLayout->addWidget(_pivot);

	QWidget* forgalJsonWidget = new QWidget(mainWidget);
	QVBoxLayout* forgalJsonLayout = new QVBoxLayout(forgalJsonWidget);
	QWidget* forgalJsonButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* forgalJsonButtonLayout = new QHBoxLayout(forgalJsonButtonWidget);
	ElaPushButton* forgalJsonUserModeButtom = new ElaPushButton(forgalJsonButtonWidget);
	forgalJsonUserModeButtom->setText("用户提示词");
	forgalJsonUserModeButtom->setEnabled(false);
	ElaPushButton* forgalJsonSystemModeButtom = new ElaPushButton(forgalJsonButtonWidget);
	forgalJsonSystemModeButtom->setText("系统提示词");
	forgalJsonSystemModeButtom->setEnabled(true);
	forgalJsonButtonLayout->addWidget(forgalJsonUserModeButtom);
	forgalJsonButtonLayout->addWidget(forgalJsonSystemModeButtom);
	forgalJsonButtonLayout->addStretch();
	forgalJsonLayout->addWidget(forgalJsonButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* forgalJsonStackedWidget = new QStackedWidget(forgalJsonWidget);
	// 用户提示词
	ElaPlainTextEdit* forgalJsonUserModeEdit = new ElaPlainTextEdit(forgalJsonStackedWidget);
	QFont plainTextFont = forgalJsonUserModeEdit->font();
	plainTextFont.setPixelSize(15);
	forgalJsonUserModeEdit->setFont(plainTextFont);
	forgalJsonUserModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["FORGALJSON_TRANS_PROMPT_EN"].value_or(std::string{}))
	);
	forgalJsonStackedWidget->addWidget(forgalJsonUserModeEdit);
	// 系统提示词
	ElaPlainTextEdit* forgalJsonSystemModeEdit = new ElaPlainTextEdit(forgalJsonStackedWidget);
	forgalJsonSystemModeEdit->setFont(plainTextFont);
	forgalJsonSystemModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["FORGALJSON_SYSTEM"].value_or(std::string{}))
	);
	forgalJsonStackedWidget->addWidget(forgalJsonSystemModeEdit);
	forgalJsonStackedWidget->setCurrentIndex(0);
	forgalJsonLayout->addWidget(forgalJsonStackedWidget, 1);
	connect(forgalJsonUserModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			forgalJsonStackedWidget->setCurrentIndex(0);
			forgalJsonUserModeButtom->setEnabled(false);
			forgalJsonSystemModeButtom->setEnabled(true);
		});
	connect(forgalJsonSystemModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			forgalJsonStackedWidget->setCurrentIndex(1);
			forgalJsonUserModeButtom->setEnabled(true);
			forgalJsonSystemModeButtom->setEnabled(false);
		});
	_pivot->appendPivot("ForGalJson");
	_stackedWidget->addWidget(forgalJsonWidget);

	// FORGALTSV
	QWidget* forgalTsvWidget = new QWidget(mainWidget);
	QVBoxLayout* forgalTsvLayout = new QVBoxLayout(forgalTsvWidget);
	QWidget* forgalTsvButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* forgalTsvButtonLayout = new QHBoxLayout(forgalTsvButtonWidget);
	ElaPushButton* forgalTsvUserModeButtom = new ElaPushButton(forgalTsvButtonWidget);
	forgalTsvUserModeButtom->setText("用户提示词");
	forgalTsvUserModeButtom->setEnabled(false);
	ElaPushButton* forgalTsvSystemModeButtom = new ElaPushButton(forgalTsvButtonWidget);
	forgalTsvSystemModeButtom->setText("系统提示词");
	forgalTsvSystemModeButtom->setEnabled(true);
	forgalTsvButtonLayout->addWidget(forgalTsvUserModeButtom);
	forgalTsvButtonLayout->addWidget(forgalTsvSystemModeButtom);
	forgalTsvButtonLayout->addStretch();
	forgalTsvLayout->addWidget(forgalTsvButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* forgalTsvStackedWidget = new QStackedWidget(forgalTsvWidget);
	// 用户提示词
	ElaPlainTextEdit* forgalTsvUserModeEdit = new ElaPlainTextEdit(forgalTsvStackedWidget);
	forgalTsvUserModeEdit->setFont(plainTextFont);
	forgalTsvUserModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["FORGALTSV_TRANS_PROMPT_EN"].value_or(std::string{}))
	);
	forgalTsvStackedWidget->addWidget(forgalTsvUserModeEdit);
	// 系统提示词
	ElaPlainTextEdit* forgalTsvSystemModeEdit = new ElaPlainTextEdit(forgalTsvStackedWidget);
	forgalTsvSystemModeEdit->setFont(plainTextFont);
	forgalTsvSystemModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["FORGALTSV_SYSTEM"].value_or(std::string{}))
	);
	forgalTsvStackedWidget->addWidget(forgalTsvSystemModeEdit);
	forgalTsvStackedWidget->setCurrentIndex(0);
	forgalTsvLayout->addWidget(forgalTsvStackedWidget, 1);
	connect(forgalTsvUserModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			forgalTsvStackedWidget->setCurrentIndex(0);
			forgalTsvUserModeButtom->setEnabled(false);
			forgalTsvSystemModeButtom->setEnabled(true);
		});
	connect(forgalTsvSystemModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			forgalTsvStackedWidget->setCurrentIndex(1);
			forgalTsvUserModeButtom->setEnabled(true);
			forgalTsvSystemModeButtom->setEnabled(false);
		});
	_pivot->appendPivot("ForGalTsv");
	_stackedWidget->addWidget(forgalTsvWidget);

	// FORNOVELTSV
	QWidget* forNovelTsvWidget = new QWidget(mainWidget);
	QVBoxLayout* forNovelTsvLayout = new QVBoxLayout(forNovelTsvWidget);
	QWidget* forNovelTsvButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* forNovelTsvButtonLayout = new QHBoxLayout(forNovelTsvButtonWidget);
	ElaPushButton* forNovelTsvUserModeButtom = new ElaPushButton(forNovelTsvButtonWidget);
	forNovelTsvUserModeButtom->setText("用户提示词");
	forNovelTsvUserModeButtom->setEnabled(false);
	ElaPushButton* forNovelTsvSystemModeButtom = new ElaPushButton(forNovelTsvButtonWidget);
	forNovelTsvSystemModeButtom->setText("系统提示词");
	forNovelTsvSystemModeButtom->setEnabled(true);
	forNovelTsvButtonLayout->addWidget(forNovelTsvUserModeButtom);
	forNovelTsvButtonLayout->addWidget(forNovelTsvSystemModeButtom);
	forNovelTsvButtonLayout->addStretch();
	forNovelTsvLayout->addWidget(forNovelTsvButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* forNovelTsvStackedWidget = new QStackedWidget(forNovelTsvWidget);
	// 用户提示词
	ElaPlainTextEdit* forNovelTsvUserModeEdit = new ElaPlainTextEdit(forNovelTsvStackedWidget);
	forNovelTsvUserModeEdit->setFont(plainTextFont);
	forNovelTsvUserModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["FORNOVELTSV_TRANS_PROMPT_EN"].value_or(std::string{}))
	);
	forNovelTsvStackedWidget->addWidget(forNovelTsvUserModeEdit);
	// 系统提示词
	ElaPlainTextEdit* forNovelTsvSystemModeEdit = new ElaPlainTextEdit(forNovelTsvStackedWidget);
	forNovelTsvSystemModeEdit->setFont(plainTextFont);
	forNovelTsvSystemModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["FORNOVELTSV_SYSTEM"].value_or(std::string{}))
	);
	forNovelTsvStackedWidget->addWidget(forNovelTsvSystemModeEdit);
	forNovelTsvStackedWidget->setCurrentIndex(0);
	forNovelTsvLayout->addWidget(forNovelTsvStackedWidget, 1);
	connect(forNovelTsvUserModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			forNovelTsvStackedWidget->setCurrentIndex(0);
			forNovelTsvUserModeButtom->setEnabled(false);
			forNovelTsvSystemModeButtom->setEnabled(true);
		});
	connect(forNovelTsvSystemModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			forNovelTsvStackedWidget->setCurrentIndex(1);
			forNovelTsvUserModeButtom->setEnabled(true);
			forNovelTsvSystemModeButtom->setEnabled(false);
		});
	_pivot->appendPivot("ForNovelTsv");
	_stackedWidget->addWidget(forNovelTsvWidget);

	// DEEPSEEK
	QWidget* deepSeekWidget = new QWidget(mainWidget);
	QVBoxLayout* deepSeekLayout = new QVBoxLayout(deepSeekWidget);
	QWidget* deepSeekButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* deepSeekButtonLayout = new QHBoxLayout(deepSeekButtonWidget);
	ElaPushButton* deepSeekUserModeButtom = new ElaPushButton(deepSeekButtonWidget);
	deepSeekUserModeButtom->setText("用户提示词");
	deepSeekUserModeButtom->setEnabled(false);
	ElaPushButton* deepSeekSystemModeButtom = new ElaPushButton(deepSeekButtonWidget);
	deepSeekSystemModeButtom->setText("系统提示词");
	deepSeekSystemModeButtom->setEnabled(true);
	deepSeekButtonLayout->addWidget(deepSeekUserModeButtom);
	deepSeekButtonLayout->addWidget(deepSeekSystemModeButtom);
	deepSeekButtonLayout->addStretch();
	deepSeekLayout->addWidget(deepSeekButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* deepSeekStackedWidget = new QStackedWidget(deepSeekWidget);
	// 用户提示词
	ElaPlainTextEdit* deepSeekUserModeEdit = new ElaPlainTextEdit(deepSeekStackedWidget);
	deepSeekUserModeEdit->setFont(plainTextFont);
	deepSeekUserModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["DEEPSEEKJSON_TRANS_PROMPT"].value_or(std::string{}))
	);
	deepSeekStackedWidget->addWidget(deepSeekUserModeEdit);
	// 系统提示词
	ElaPlainTextEdit* deepSeekSystemModeEdit = new ElaPlainTextEdit(deepSeekStackedWidget);
	deepSeekSystemModeEdit->setFont(plainTextFont);
	deepSeekSystemModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["DEEPSEEKJSON_SYSTEM_PROMPT"].value_or(std::string{}))
	);
	deepSeekStackedWidget->addWidget(deepSeekSystemModeEdit);
	deepSeekStackedWidget->setCurrentIndex(0);
	deepSeekLayout->addWidget(deepSeekStackedWidget, 1);
	connect(deepSeekUserModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			deepSeekStackedWidget->setCurrentIndex(0);
			deepSeekUserModeButtom->setEnabled(false);
			deepSeekSystemModeButtom->setEnabled(true);
		});
	connect(deepSeekSystemModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			deepSeekStackedWidget->setCurrentIndex(1);
			deepSeekUserModeButtom->setEnabled(true);
			deepSeekSystemModeButtom->setEnabled(false);
		});
	_pivot->appendPivot("DeepSeek");
	_stackedWidget->addWidget(deepSeekWidget);

	// SAKURA
	QWidget* sakuraWidget = new QWidget(mainWidget);
	QVBoxLayout* sakuraLayout = new QVBoxLayout(sakuraWidget);
	QWidget* sakuraButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* sakuraButtonLayout = new QHBoxLayout(sakuraButtonWidget);
	ElaPushButton* sakuraUserModeButtom = new ElaPushButton(sakuraButtonWidget);
	sakuraUserModeButtom->setText("用户提示词");
	sakuraUserModeButtom->setEnabled(false);
	ElaPushButton* sakuraSystemModeButtom = new ElaPushButton(sakuraButtonWidget);
	sakuraSystemModeButtom->setText("系统提示词");
	sakuraSystemModeButtom->setEnabled(true);
	sakuraButtonLayout->addWidget(sakuraUserModeButtom);
	sakuraButtonLayout->addWidget(sakuraSystemModeButtom);
	sakuraButtonLayout->addStretch();
	sakuraLayout->addWidget(sakuraButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* sakuraStackedWidget = new QStackedWidget(sakuraWidget);
	// 用户提示词
	ElaPlainTextEdit* sakuraUserModeEdit = new ElaPlainTextEdit(sakuraStackedWidget);
	sakuraUserModeEdit->setFont(plainTextFont);
	sakuraUserModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["SAKURA_TRANS_PROMPT"].value_or(std::string{}))
	);
	sakuraStackedWidget->addWidget(sakuraUserModeEdit);
	// 系统提示词
	ElaPlainTextEdit* sakuraSystemModeEdit = new ElaPlainTextEdit(sakuraStackedWidget);
	sakuraSystemModeEdit->setFont(plainTextFont);
	sakuraSystemModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["SAKURA_SYSTEM_PROMPT"].value_or(std::string{}))
	);
	sakuraStackedWidget->addWidget(sakuraSystemModeEdit);
	sakuraStackedWidget->setCurrentIndex(0);
	sakuraLayout->addWidget(sakuraStackedWidget, 1);
	connect(sakuraUserModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			sakuraStackedWidget->setCurrentIndex(0);
			sakuraUserModeButtom->setEnabled(false);
			sakuraSystemModeButtom->setEnabled(true);
		});
	connect(sakuraSystemModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			sakuraStackedWidget->setCurrentIndex(1);
			sakuraUserModeButtom->setEnabled(true);
			sakuraSystemModeButtom->setEnabled(false);
		});
	_pivot->appendPivot("Sakura");
	_stackedWidget->addWidget(sakuraWidget);

	// GENDIC
	QWidget* gendicWidget = new QWidget(mainWidget);
	QVBoxLayout* gendicLayout = new QVBoxLayout(gendicWidget);
	QWidget* gendicButtonWidget = new QWidget(mainWidget);
	QHBoxLayout* gendicButtonLayout = new QHBoxLayout(gendicButtonWidget);
	ElaPushButton* gendicUserModeButtom = new ElaPushButton(gendicButtonWidget);
	gendicUserModeButtom->setText("用户提示词");
	gendicUserModeButtom->setEnabled(false);
	ElaPushButton* gendicSystemModeButtom = new ElaPushButton(gendicButtonWidget);
	gendicSystemModeButtom->setText("系统提示词");
	gendicSystemModeButtom->setEnabled(true);
	gendicButtonLayout->addWidget(gendicUserModeButtom);
	gendicButtonLayout->addWidget(gendicSystemModeButtom);
	gendicButtonLayout->addStretch();
	gendicLayout->addWidget(gendicButtonWidget, 0, Qt::AlignTop);

	QStackedWidget* gendicStackedWidget = new QStackedWidget(gendicWidget);
	// 用户提示词
	ElaPlainTextEdit* gendicUserModeEdit = new ElaPlainTextEdit(gendicStackedWidget);
	gendicUserModeEdit->setFont(plainTextFont);
	gendicUserModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["GENDIC_PROMPT"].value_or(std::string{}))
	);
	gendicStackedWidget->addWidget(gendicUserModeEdit);
	// 系统提示词
	ElaPlainTextEdit* gendicSystemModeEdit = new ElaPlainTextEdit(gendicStackedWidget);
	gendicSystemModeEdit->setFont(plainTextFont);
	gendicSystemModeEdit->setPlainText(
		QString::fromStdString(_promptConfig["GENDIC_SYSTEM"].value_or(std::string{}))
	);
	gendicStackedWidget->addWidget(gendicSystemModeEdit);
	gendicStackedWidget->setCurrentIndex(0);
	gendicLayout->addWidget(gendicStackedWidget, 1);
	connect(gendicUserModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			gendicStackedWidget->setCurrentIndex(0);
			gendicUserModeButtom->setEnabled(false);
			gendicSystemModeButtom->setEnabled(true);
		});
	connect(gendicSystemModeButtom, &ElaPushButton::clicked, this, [=]()
		{
			gendicStackedWidget->setCurrentIndex(1);
			gendicUserModeButtom->setEnabled(true);
			gendicSystemModeButtom->setEnabled(false);
		});
	_pivot->appendPivot("GenDict");
	_stackedWidget->addWidget(gendicWidget);

	_applyFunc = [=]()
		{
			insertToml(_promptConfig, "FORGALJSON_SYSTEM", forgalJsonSystemModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "FORGALTSV_TRANS_PROMPT_EN", forgalTsvUserModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "FORGALTSV_SYSTEM", forgalTsvSystemModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "FORNOVELTSV_TRANS_PROMPT_EN", forNovelTsvUserModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "FORNOVELTSV_SYSTEM", forNovelTsvSystemModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "DEEPSEEKJSON_TRANS_PROMPT", deepSeekUserModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "DEEPSEEKJSON_SYSTEM_PROMPT", deepSeekSystemModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "SAKURA_TRANS_PROMPT", sakuraUserModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "SAKURA_SYSTEM_PROMPT", sakuraSystemModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "GENDIC_PROMPT", gendicUserModeEdit->toPlainText().toStdString());
			insertToml(_promptConfig, "GENDIC_SYSTEM", gendicSystemModeEdit->toPlainText().toStdString());
			std::ofstream ofs(_projectDir / L"Prompt.toml");
			ofs << _promptConfig;
			ofs.close();
		};

	mainLayout->addWidget(pivotScrollArea, 0, Qt::AlignTop);
	mainLayout->addWidget(_stackedWidget, 1);
	_pivot->setCurrentIndex(0);

	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, true, 0);
}
