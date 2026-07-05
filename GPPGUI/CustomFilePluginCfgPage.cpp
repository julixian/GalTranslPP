#include "CustomFilePluginCfgPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDesktopServices>

#include "ElaScrollPageArea.h"
#include "ElaComboBox.h"
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"

import Tool;

CustomFilePluginCfgPage::CustomFilePluginCfgPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent)
	: BasePage(parent), m_projectDir(projectDir), m_globalConfig(globalConfig), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("自定义文件处理插件配置"));
	setContentsMargins(30, 15, 15, 0);

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 创建自定义文件处理插件路径设置
	const std::string customFilePluginPath = toml::find_or(m_projectConfig, "plugins", "customFilePlugin", "Lua/MySampleFilePlugin.lua");
	ElaScrollPageArea* filePluginArea = new ElaScrollPageArea(this);
	QHBoxLayout* filePluginLayout = new QHBoxLayout(filePluginArea);
	ElaText* filePluginText = new ElaText(tr("自定义文件处理插件路径"), 16, filePluginArea);
	filePluginText->setWordWrap(false);
	filePluginLayout->addWidget(filePluginText);
	ElaLineEdit* filePluginEdit = new ElaLineEdit(filePluginArea);
	filePluginEdit->setText(QString::fromStdString(customFilePluginPath));
	filePluginLayout->addWidget(filePluginEdit);
	ElaPushButton* filePluginBtn = new ElaPushButton(tr("浏览"), filePluginArea);
	connect(filePluginBtn, &ElaPushButton::clicked, this, [=]()
		{
			QString path = QFileDialog::getOpenFileName(window(), tr("选择自定义文件处理插件"),
				QString::fromStdString(toml::find_or(m_globalConfig, "lastPluginPath", "./")),
				"custom script (*.lua *.py)");
			if (!path.isEmpty())
			{
				filePluginEdit->setText(path);
				insertToml(m_globalConfig, "lastPluginPath", path.toStdString());
			}
		});
	filePluginLayout->addWidget(filePluginBtn);
	mainLayout->addWidget(filePluginArea);

	// 继承的基类 # NormalJson, Epub, PDF
	const std::string inherit = toml::find_or(m_projectConfig, "plugins", "baseClassName", "NormalJson");
	ElaScrollPageArea* inheritArea = new ElaScrollPageArea(this);
	QHBoxLayout* inheritLayout = new QHBoxLayout(inheritArea);
	ElaText* inheritText = new ElaText(tr("基类继承"), 16, inheritArea);
	inheritLayout->addWidget(inheritText);
	ElaComboBox* inheritCombo = new ElaComboBox(inheritArea);
	inheritCombo->setFixedWidth(150);
	inheritCombo->addItem("NormalJson");
	inheritCombo->addItem("Epub");
	inheritCombo->addItem("PDF");
	if (!inherit.empty()) {
		QString inheritStr = QString::fromStdString(inherit);
		int index = inheritCombo->findText(inheritStr);
		if (index >= 0) {
			inheritCombo->setCurrentIndex(index);
		}
	}
	inheritLayout->addWidget(inheritCombo);
	mainLayout->addWidget(inheritArea);

	m_applyFunc = [=]()
		{
			insertToml(m_projectConfig, "plugins.customFilePlugin", filePluginEdit->text().toStdString());
			insertToml(m_projectConfig, "plugins.baseClassName", inheritCombo->currentText().toStdString());
		};

	mainLayout->addStretch();
	centerWidget->setWindowTitle(tr("自定义文件处理插件配置"));
	addCentralWidget(centerWidget, true, false, 0);
}

CustomFilePluginCfgPage::~CustomFilePluginCfgPage()
{

}
