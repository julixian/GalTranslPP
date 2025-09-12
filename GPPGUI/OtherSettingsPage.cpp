#include "OtherSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
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
#include "ElaMessageBar.h"
#include "ElaToolTip.h"

import Tool;

OtherSettingsPage::OtherSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent) : 
	BasePage(parent), _projectConfig(projectConfig), _projectDir(projectDir)
{
	setWindowTitle("其它设置");
	setTitleVisible(false);
	_setupUI();
}

OtherSettingsPage::~OtherSettingsPage()
{

}

void OtherSettingsPage::apply2Config()
{
}

void OtherSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(5);

	// 项目路径
	ElaScrollPageArea* pathArea = new ElaScrollPageArea(mainWidget);
	pathArea->setFixedHeight(100);
	QHBoxLayout* pathLayout = new QHBoxLayout(pathArea);
	ElaText* pathLabel = new ElaText(pathArea);
	pathLabel->setText("项目路径");
	pathLabel->setTextPixelSize(16);
	pathLayout->addWidget(pathLabel);
	pathLayout->addStretch();
	ElaLineEdit* pathEdit = new ElaLineEdit(pathArea);
	pathEdit->setReadOnly(true);
	pathEdit->setText(QString(_projectDir.wstring()));
	pathEdit->setFixedWidth(400);
	pathLayout->addWidget(pathEdit);
	ElaPushButton* openButton = new ElaPushButton(pathArea);
	openButton->setText("打开文件夹");
	connect(openButton, &ElaPushButton::clicked, this, [=]()
		{
			QUrl dirUrl = QUrl::fromLocalFile(QString(_projectDir.wstring()));
			QDesktopServices::openUrl(dirUrl);
		});
	pathLayout->addWidget(openButton);
	ElaPushButton* moveButton = new ElaPushButton(pathArea);
	moveButton->setText("移动项目");
	connect(moveButton, &ElaPushButton::clicked, this, [=]()
		{
			if (_projectConfig["GUIConfig"]["isRunning"].value_or(false)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, "移动失败", "项目仍在运行中，无法移动", 3000);
				return;
			}

			QString newProjectParentPath = QFileDialog::getExistingDirectory(this, "请选择要移动到的文件夹", QDir::currentPath() + "/Projects");
			if (newProjectParentPath.isEmpty()) {
				return;
			}

			fs::path newProjectPath = fs::path(newProjectParentPath.toStdWString()) / _projectDir.filename();
			if (fs::exists(newProjectPath)) {
				ElaMessageBar::warning(ElaMessageBarType::TopRight, "移动失败", "目录下已有同名文件夹", 3000);
				return;
			}

			fs::rename(_projectDir, newProjectPath);
			_projectDir = newProjectPath;
			pathEdit->setText(QString(_projectDir.wstring()));
			ElaMessageBar::success(ElaMessageBarType::TopRight, "移动成功", QString(_projectDir.filename().wstring()) + " 项目已移动到新文件夹", 3000);
		});
	pathLayout->addWidget(moveButton);
	mainLayout->addWidget(pathArea);


	// 保存配置
	ElaScrollPageArea* saveArea = new ElaScrollPageArea(mainWidget);
	QHBoxLayout* saveLayout = new QHBoxLayout(saveArea);
	ElaText* saveLabel = new ElaText(saveArea);
	ElaToolTip* saveTip = new ElaToolTip(saveLabel);
	saveTip->setToolTip("开始翻译或关闭程序时会自动保存所有项目的配置，一般无需手动保存。");
	saveLabel->setText("保存配置");
	saveLabel->setTextPixelSize(16);
	saveLayout->addWidget(saveLabel);
	saveLayout->addStretch();
	ElaPushButton* saveButton = new ElaPushButton(saveArea);
	saveButton->setText("保存");
	connect(saveButton, &ElaPushButton::clicked, this, [=]()
		{
			Q_EMIT saveConfigSignal();
			ElaMessageBar::success(ElaMessageBarType::TopRight, "保存成功", QString(_projectDir.filename().wstring()) + " 项目配置已保存", 3000);
		});
	saveLayout->addWidget(saveButton);
	mainLayout->addWidget(saveArea);
	
	mainLayout->addStretch();
	addCentralWidget(mainWidget, true, true, 0);
}