#include "StartSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QFileDialog>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaComboBox.h"
#include "ElaMessageBar.h"
#include "ElaProgressBar.h"

#include "NJCfgDialog.h"
#include "EpubCfgDialog.h"

import Tool;

StartSettingsPage::StartSettingsPage(fs::path& projectDir, toml::table& projectConfig, QWidget* parent) : 
	BasePage(parent), _projectConfig(projectConfig), _projectDir(projectDir)
{
	setWindowTitle("启动设置");
	setTitleVisible(false);

	_setupUI();
}

StartSettingsPage::~StartSettingsPage()
{
	if (_workThread && _workThread->isRunning()) {
		_worker->stopTranslation();
		_workThread->quit();
		_workThread->wait();
	}
}

void StartSettingsPage::apply2Config()
{

}

void StartSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

	QWidget* topWidget = new QWidget(mainWidget);
	QHBoxLayout* topLayout = new QHBoxLayout(topWidget);

	// 日志输出
	ElaPlainTextEdit* logOutput = new ElaPlainTextEdit(topWidget);
	logOutput->setReadOnly(true);
	QFont font = logOutput->font();
	font.setPixelSize(14);
	logOutput->setFont(font);
	logOutput->setPlaceholderText("日志输出");
	topLayout->addWidget(logOutput);


	ElaScrollPageArea* buttonArea = new ElaScrollPageArea(mainWidget);
	buttonArea->setFixedWidth(200);
	buttonArea->setFixedHeight(620);
	QVBoxLayout* buttonLayout = new QVBoxLayout(buttonArea);

	// 文件格式
	std::string filePlugin = _projectConfig["plugins"]["filePlugin"].value_or("NormalJson");
	QString filePluginStr = QString::fromStdString(filePlugin);
	ElaText* fileFormatLabel = new ElaText(buttonArea);
	fileFormatLabel->setTextPixelSize(16);
	fileFormatLabel->setText("文件格式:");
	buttonLayout->addWidget(fileFormatLabel);
	ElaComboBox* fileFormatComboBox = new ElaComboBox(buttonArea);
	fileFormatComboBox->addItem("NormalJson");
	fileFormatComboBox->addItem("Epub");
	if (!filePluginStr.isEmpty()) {
		int index = fileFormatComboBox->findText(filePluginStr);
		if (index != -1) {
			fileFormatComboBox->setCurrentIndex(index);
		}
	}
	connect(fileFormatComboBox, &ElaComboBox::currentTextChanged, this, [=](const QString& text)
		{
			insertToml(_projectConfig, "plugins.filePlugin", text.toStdString());
		});
	buttonLayout->addWidget(fileFormatComboBox);

	// 针对文件格式的输出设置
	ElaPushButton* outputSetting = new ElaPushButton(buttonArea);
	outputSetting->setText("文件输出设置");
	buttonLayout->addWidget(outputSetting);
	buttonLayout->addStretch();
	connect(outputSetting, &ElaPushButton::clicked, this, [=](bool checked)
		{
			QString fileFormat = fileFormatComboBox->currentText();
			if (fileFormat == "NormalJson") {
				NJCfgDialog cfgDialog(_projectConfig, this);
				cfgDialog.exec();
			}
			else if (fileFormat == "Epub") {
				EpubCfgDialog cfgDialog(_projectConfig, this);
				cfgDialog.exec();
			}
		});

	// 翻译模式
	std::string transEngine = _projectConfig["plugins"]["transEngine"].value_or("ForGalJson");
	QString transEngineStr = QString::fromStdString(transEngine);
	ElaText* translateModeLabel = new ElaText(buttonArea);
	translateModeLabel->setTextPixelSize(16);
	translateModeLabel->setText("翻译模式:");
	buttonLayout->addWidget(translateModeLabel);
	ElaComboBox* translateMode = new ElaComboBox(buttonArea);
	translateMode->addItem("ForGalJson");
	translateMode->addItem("ForGalTsv");
	translateMode->addItem("ForNovelTsv");
	translateMode->addItem("DeepseekJson");
	translateMode->addItem("Sakura");
	translateMode->addItem("DumpName");
	translateMode->addItem("GenDict");
	translateMode->addItem("Rebuild");
	if (!transEngineStr.isEmpty()) {
		int index = translateMode->findText(transEngineStr);
		if (index != -1) {
			translateMode->setCurrentIndex(index);
		}
	}
	connect(translateMode, &ElaComboBox::currentTextChanged, this, [=](const QString& text)
		{
			insertToml(_projectConfig, "plugins.transEngine", text.toStdString());
		});
	buttonLayout->addWidget(translateMode);

	// 开始翻译
	_startTranslateButton = new ElaPushButton(buttonArea);
	_startTranslateButton->setText("开始翻译");
	connect(_startTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStartTranslatingClicked);
	buttonLayout->addWidget(_startTranslateButton);

	// 停止翻译
	_stopTranslateButton = new ElaPushButton(buttonArea);
	_stopTranslateButton->setText("停止翻译");
	_stopTranslateButton->setEnabled(false);
	connect(_stopTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStopTranslatingClicked);
	buttonLayout->addWidget(_stopTranslateButton);
	topLayout->addWidget(buttonArea);

	mainLayout->addWidget(topWidget);


	// 进度条
	_progressBar = new ElaProgressBar(mainWidget);
	_progressBar->setRange(0, 100);
	_progressBar->setValue(0);
	mainLayout->addWidget(_progressBar);


	// 翻译线程
	_workThread = new QThread(this);
	_worker = new TranslatorWorker(_projectDir);
	_worker->moveToThread(_workThread);
	connect(_workThread, &QThread::finished, _worker, &TranslatorWorker::deleteLater);
	connect(this, &StartSettingsPage::startWork, _worker, &TranslatorWorker::doTranslation);
	connect(_worker, &TranslatorWorker::translationFinished, this, &StartSettingsPage::_workFinished);

	connect(_worker, &TranslatorWorker::makeBarSignal, this, [=](int totalSentences, int totalThreads)
		{
			_progressBar->setRange(0, totalSentences);
			_progressBar->setValue(0);
			_progressBar->setFormat("%v/%m lines [%p%]");
		});
	connect(_worker, &TranslatorWorker::writeLogSignal, this, [=](QString log)
		{
			logOutput->moveCursor(QTextCursor::End);
			logOutput->insertPlainText(log);
			logOutput->moveCursor(QTextCursor::End);
		});
	connect(_worker, &TranslatorWorker::addThreadNumSignal, this, [=]()
		{

		});
	connect(_worker, &TranslatorWorker::reduceThreadNumSignal, this, [=]()
		{

		});
	connect(_worker, &TranslatorWorker::updateBarSignal, this, [=](int ticks)
		{
			_progressBar->setValue(_progressBar->value() + ticks);
		});

	addCentralWidget(mainWidget);
	_workThread->start();
}

void StartSettingsPage::_onStartTranslatingClicked()
{
	Q_EMIT startTranslating();

	_startTranslateButton->setEnabled(false);
	_progressBar->setValue(0);

	Q_EMIT startWork();
	_transEngine = QString::fromStdString(_projectConfig["plugins"]["transEngine"].value_or(std::string{}));
	_stopTranslateButton->setEnabled(true);
}

void StartSettingsPage::_onStopTranslatingClicked()
{
	_stopTranslateButton->setEnabled(false);
	_worker->stopTranslation();
	ElaMessageBar::information(ElaMessageBarType::BottomRight, "停止中", "正在等待最后一批翻译完成，请稍候...", 3000);
}

void StartSettingsPage::_workFinished(int exitCode)
{
	switch (exitCode)
	{
	case -2:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, "翻译失败", "项目 " + QString(_projectDir.filename().wstring()) + " 翻译任务失败，请检查日志输出。", 3000);
		break;
	case -1:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, "翻译失败", "项目 " + QString(_projectDir.filename().wstring()) + " 连工厂函数都失败了，玩毛啊", 3000);
		break;
	case 0:
		ElaMessageBar::success(ElaMessageBarType::BottomRight, "翻译完成", "请查收项目 " + QString(_projectDir.filename().wstring()) + " 的翻译结果。", 3000);
		break;
	case 1:
		ElaMessageBar::information(ElaMessageBarType::BottomRight, "停止成功", "项目 " + QString(_projectDir.filename().wstring()) + " 的翻译任务已终止", 3000);
		break;
	default:
		break;
	}

	Q_EMIT finishTranslating(_transEngine, exitCode);
	_startTranslateButton->setEnabled(true);
	_stopTranslateButton->setEnabled(false);
}
