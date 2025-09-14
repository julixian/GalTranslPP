#include "TLFCfgPage.h"

#include <QVBoxLayout>
#include <QFormLayout>

#include "ElaScrollPageArea.h"
#include "ElaSpinBox.h"
#include "ElaComboBox.h"
#include "ElaToggleSwitch.h"
#include "ElaText.h"

import Tool;

void TLFCfgPage::apply2Config()
{

}

TLFCfgPage::TLFCfgPage(toml::table& projectConfig, QWidget* parent) : BasePage(parent), _projectConfig(projectConfig)
{
	setWindowTitle("换行修复设置");

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	// 换行模式
	QString fixMode = QString::fromStdString(_projectConfig["plugins"]["TextLinebreakFix"]["换行模式"].value_or(std::string{}));
	ElaScrollPageArea* fixModeArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* fixModeLayout = new QHBoxLayout(fixModeArea);
	ElaText* fixModeText = new ElaText("换行模式", fixModeArea);
	fixModeText->setTextPixelSize(16);
	fixModeLayout->addWidget(fixModeText);
	fixModeLayout->addStretch();
	ElaComboBox* fixModeComboBox = new ElaComboBox(fixModeArea);
	fixModeComboBox->addItem("优先标点");
	fixModeComboBox->addItem("保持位置");
	fixModeComboBox->addItem("固定字数");
	fixModeComboBox->addItem("平均");
	if (!fixMode.isEmpty()) {
		int index = fixModeComboBox->findText(fixMode);
		if (index >= 0) {
			fixModeComboBox->setCurrentIndex(index);
		}
	}
	insertToml(_projectConfig, "plugins.TextLinebreakFix.换行模式", fixMode.toStdString());
	connect(fixModeComboBox, &ElaComboBox::currentTextChanged, this, [=](const QString& text)
		{
			insertToml(_projectConfig, "pulgins.TextLinebreakFix.换行模式", text.toStdString());
		});
	fixModeLayout->addWidget(fixModeComboBox);

	// 分段字数阈值
	int threshold = _projectConfig["plugins"]["TextLinebreakFix"]["分段字数阈值"].value_or(21);
	ElaScrollPageArea* segmentThresholdArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* segmentThresholdLayout = new QHBoxLayout(segmentThresholdArea);
	ElaText* segmentThresholdText = new ElaText("分段字数阈值", segmentThresholdArea);
	segmentThresholdText->setTextPixelSize(16);
	segmentThresholdLayout->addWidget(segmentThresholdText);
	segmentThresholdLayout->addStretch();
	ElaSpinBox* segmentThresholdSpinBox = new ElaSpinBox(segmentThresholdArea);
	segmentThresholdSpinBox->setRange(1, 999);
	segmentThresholdSpinBox->setValue(threshold);
	insertToml(_projectConfig, "plugins.TextLinebreakFix.分段字数阈值", threshold);
	connect(segmentThresholdSpinBox, &ElaSpinBox::valueChanged, this, [=](int value)
		{
			insertToml(_projectConfig, "plugins.TextLinebreakFix.分段字数阈值", value);
		});
	segmentThresholdLayout->addWidget(segmentThresholdSpinBox);

	// 强制修复
	bool forceFix = _projectConfig.at_path("plugins.TextLinebreakFix.强制修复").value_or(false);
	ElaScrollPageArea* forceFixArea = new ElaScrollPageArea(centerWidget);
	QHBoxLayout* forceFixLayout = new QHBoxLayout(forceFixArea);
	ElaText* forceFixText = new ElaText("强制修复", forceFixArea);
	forceFixText->setTextPixelSize(16);
	forceFixLayout->addWidget(forceFixText);
	forceFixLayout->addStretch();
	ElaToggleSwitch* forceFixToggleSwitch = new ElaToggleSwitch(forceFixArea);
	forceFixToggleSwitch->setIsToggled(forceFix);
	insertToml(_projectConfig, "plugins.TextLinebreakFix.强制修复", forceFix);
	connect(forceFixToggleSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked)
		{
			insertToml(_projectConfig, "plugins.TextLinebreakFix.强制修复", checked);
		});
	forceFixLayout->addWidget(forceFixToggleSwitch);

	mainLayout->addWidget(fixModeArea);
	mainLayout->addWidget(segmentThresholdArea);
	mainLayout->addWidget(forceFixArea);
	mainLayout->addStretch();
	centerWidget->setWindowTitle("换行修复设置");
	addCentralWidget(centerWidget);
}

TLFCfgPage::~TLFCfgPage()
{

}
