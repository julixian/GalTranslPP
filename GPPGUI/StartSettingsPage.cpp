#include "StartSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QFileDialog>
#include <QScrollBar>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaToolTip.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaProgressRing.h"
#include "ElaComboBox.h"
#include "ElaMessageBar.h"
#include "ElaLCDNumber.h"
#include "ElaProgressBar.h"

#include "NJCfgPage.h"
#include "EpubCfgPage.h"

import Tool;

StartSettingsPage::StartSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::table& projectConfig, QWidget* parent) : 
	BasePage(parent), _projectConfig(projectConfig), _projectDir(projectDir), _mainWindow(mainWindow)
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
	_njCfgPage->apply2Config();
	_epubCfgPage->apply2Config();
	if (_applyFunc) {
		_applyFunc();
	}
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
	logOutput->setMaximumBlockCount(3000);
	logOutput->setPlaceholderText("日志输出");
	topLayout->addWidget(logOutput);


	ElaScrollPageArea* buttonArea = new ElaScrollPageArea(mainWidget);
	buttonArea->setFixedWidth(200);
	buttonArea->setMaximumHeight(600);
	QVBoxLayout* buttonLayout = new QVBoxLayout(buttonArea);

	// 文件格式
	std::string filePlugin = _projectConfig["plugins"]["filePlugin"].value_or("NormalJson");
	QString filePluginStr = QString::fromStdString(filePlugin);
	ElaText* fileFormatLabel = new ElaText(buttonArea);
	fileFormatLabel->setTextPixelSize(16);
	fileFormatLabel->setText("文件格式:");
	buttonLayout->addWidget(fileFormatLabel);
	_fileFormatComboBox = new ElaComboBox(buttonArea);
	_fileFormatComboBox->addItem("NormalJson");
	_fileFormatComboBox->addItem("Epub");
	if (!filePluginStr.isEmpty()) {
		int index = _fileFormatComboBox->findText(filePluginStr);
		if (index >= 0) {
			_fileFormatComboBox->setCurrentIndex(index);
		}
	}
	buttonLayout->addWidget(_fileFormatComboBox);

	// 针对文件格式的输出设置
	ElaPushButton* outputSetting = new ElaPushButton(buttonArea);
	outputSetting->setText("文件输出设置");
	buttonLayout->addWidget(outputSetting);
	connect(outputSetting, &ElaPushButton::clicked, this, &StartSettingsPage::_onOutputSettingClicked);

	// 线程数
	ElaText* threadNumLabel = new ElaText(buttonArea);
	threadNumLabel->setTextPixelSize(16);
	threadNumLabel->setText("工作线程数:");
	buttonLayout->addWidget(threadNumLabel);
	QWidget* threadNumWidget = new QWidget(buttonArea);
	QHBoxLayout* threadNumLayout = new QHBoxLayout(threadNumWidget);
	_threadNumRing = new ElaProgressRing(buttonArea);
	threadNumLayout->addWidget(_threadNumRing);
	ElaText* speedLabel = new ElaText(buttonArea);
	speedLabel->setTextPixelSize(12);
	speedLabel->setText("0 lines/s");
	threadNumLayout->addWidget(speedLabel);
	buttonLayout->addWidget(threadNumWidget);

	// 已用时间
	ElaText* usedTimeLabelText = new ElaText(buttonArea);
	usedTimeLabelText->setTextPixelSize(14);
	usedTimeLabelText->setText("已用时间:");
	buttonLayout->addWidget(usedTimeLabelText);
	ElaLCDNumber* usedTimeLabel = new ElaLCDNumber(buttonArea);
	usedTimeLabel->display("00:00:00");
	buttonLayout->addWidget(usedTimeLabel);

	// 剩余时间
	ElaText* remainTimeLabelText = new ElaText(buttonArea);
	remainTimeLabelText->setTextPixelSize(14);
	remainTimeLabelText->setText("剩余时间:");
	buttonLayout->addWidget(remainTimeLabelText);
	_remainTimeLabel = new ElaLCDNumber(buttonArea);
	_remainTimeLabel->display("00:00:00");
	buttonLayout->addWidget(_remainTimeLabel);

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
		if (index >= 0) {
			translateMode->setCurrentIndex(index);
		}
	}
	buttonLayout->addWidget(translateMode);

	// 开始翻译
	_startTranslateButton = new ElaPushButton(buttonArea);
	_startTranslateButton->setText("开始翻译");
	connect(_startTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStartTranslatingClicked);
	connect(_startTranslateButton, &ElaPushButton::clicked, this, [=]()
		{
			QScrollBar* scrollBar = logOutput->verticalScrollBar();
			bool scrollIsAtBottom = (scrollBar->value() == scrollBar->maximum());
			QTextCursor tempCursor(logOutput->document());
			tempCursor.movePosition(QTextCursor::End);
			tempCursor.insertText("\n\n");
			if (scrollIsAtBottom) {
				scrollBar->setValue(scrollBar->maximum());
			}
		});
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
			_threadNumRing->setRange(0, totalThreads);
			_threadNumRing->setValue(0);
			_progressBar->setFormat("%v/%m lines [%p%]");
			_startTime = std::chrono::high_resolution_clock::now();
			usedTimeLabel->display("00:00:00");
			_remainTimeLabel->display("--:--");
			_estimator.reset();
		});
	connect(_worker, &TranslatorWorker::writeLogSignal, this, [=](QString log)
		{
			// 1. 滚动条判断
			QScrollBar* scrollBar = logOutput->verticalScrollBar();
			bool scrollIsAtBottom = (scrollBar->value() == scrollBar->maximum());

			scrollBar->blockSignals(true);

			// 2. 使用一个临时的“影子”光标在后台进行操作
			QTextCursor tempCursor(logOutput->document());
			tempCursor.movePosition(QTextCursor::End); // 移动到文档末尾

			if (log.contains("```\n问题概览:")) {
				QString pre, overview, post;
				int index = log.indexOf("```\n问题概览:");
				pre = log.left(index);
				log = log.mid(index);
				index = log.indexOf("问题概览结束\n```");
				overview = log.left(index + 10);
				log = log.mid(index + 10);
				post = log;
				tempCursor.insertText(pre);
				tempCursor.movePosition(QTextCursor::End);
				QTextCharFormat format;
				format.setForeground(QColor(255, 0, 0));
				tempCursor.setCharFormat(format);
				tempCursor.insertText(overview);
				tempCursor.movePosition(QTextCursor::End);
				tempCursor.setCharFormat(QTextCharFormat());
				tempCursor.insertText(post);
			}
			else {
				tempCursor.insertText(log); // 在末尾插入文本
			}
			scrollBar->blockSignals(false);
			// 3. 智能滚动
			if (scrollIsAtBottom) {
				scrollBar->setValue(scrollBar->maximum());
			}
		});
	connect(_worker, &TranslatorWorker::addThreadNumSignal, this, [=]()
		{
			_threadNumRing->setValue(_threadNumRing->getValue() + 1);
		});
	connect(_worker, &TranslatorWorker::reduceThreadNumSignal, this, [=]()
		{
			_threadNumRing->setValue(_threadNumRing->getValue() - 1);
		});
	connect(_worker, &TranslatorWorker::updateBarSignal, this, [=](int ticks)
		{
			_progressBar->setValue(_progressBar->value() + ticks);
			std::chrono::seconds elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>
				(std::chrono::high_resolution_clock::now() - _startTime);
			usedTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", elapsedSeconds)
			));
			if (ticks <= 0) {
				return;
			}
			auto etaWithSpeed = _estimator.updateAndGetSpeedWithEta(_progressBar->value(), _progressBar->maximum());
			const double& speed = etaWithSpeed.first;
			const Duration& eta = etaWithSpeed.second;
			speedLabel->setText(QString::fromStdString(
				std::format("{:.2f} lines/s", speed == 0.0 ? (double)_progressBar->maximum() / (elapsedSeconds.count() + 1) : speed)
			));
			if (eta.count() == std::numeric_limits<double>::infinity() || std::isnan(eta.count())) {
				_remainTimeLabel->display("--:--");
				return;
			}
			_remainTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", eta)
			));
		});

	_workThread->start();
	addCentralWidget(mainWidget);

	_applyFunc = [=]()
		{
			insertToml(_projectConfig, "plugins.filePlugin", _fileFormatComboBox->currentText().toStdString());
			insertToml(_projectConfig, "plugins.transEngine", translateMode->currentText().toStdString());
		};

	// 顺序和_onOutputSettingClicked里的索引一致
	_njCfgPage = new NJCfgPage(_projectConfig, this);
	addCentralWidget(_njCfgPage, true, true, 0);
	_epubCfgPage = new EpubCfgPage(_projectConfig, this);
	addCentralWidget(_epubCfgPage, true, true, 0);
}

void StartSettingsPage::_onOutputSettingClicked()
{
	QString fileFormat = _fileFormatComboBox->currentText();
	if (fileFormat == "NormalJson") {
		this->navigation(1);
	}
	else if (fileFormat == "Epub") {
		this->navigation(2);
	}
}


// 底下的可以不用看

void StartSettingsPage::_onStartTranslatingClicked()
{
	Q_EMIT startTranslating();

	_startTranslateButton->setEnabled(false);
	_progressBar->setValue(0);
	insertToml(_projectConfig, "GUIConfig.inRunning", true);

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
	_threadNumRing->setValue(0);
	_remainTimeLabel->display("00:00:00");
	insertToml(_projectConfig, "GUIConfig.inRunning", false);
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
