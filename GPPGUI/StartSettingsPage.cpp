#include "StartSettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QTimer>

#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaIconButton.h"
#include "ElaProgressRing.h"
#include "NoWheelComboBox.h"
#include "ElaMessageBar.h"
#include "ElaLCDNumber.h"
#include "ElaProgressBar.h"
#include "ElaToggleSwitch.h"

#include "NJCfgPage.h"
#include "EpubCfgPage.h"
#include "PDFCfgPage.h"
#include "CustomFilePluginCfgPage.h"
#include "TranslationWorkbenchPage.h"

import Tool;

StartSettingsPage::StartSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent) 
	: BasePage(parent), _projectConfig(projectConfig), _globalConfig(globalConfig), _projectDir(projectDir), _mainWindow(mainWindow)
{
	setWindowTitle(tr("启动设置"));
	setTitleVisible(false);

	_trayIcon = new QSystemTrayIcon(this);
	_trayIcon->setIcon(QIcon(":/GPPGUI/Resource/Image/julixian_s.ico"));
	connect(_trayIcon, &QSystemTrayIcon::messageClicked, this, [=]()
		{
			QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(_projectDir.wstring()));
			QDesktopServices::openUrl(dirUrl);
		});

	_setupUI();
}

StartSettingsPage::~StartSettingsPage()
{
	_trayIcon = nullptr;
	if (_worker && _workThread && _workThread->isRunning()) {
		_worker->stopTranslation();
	}
	_disposeWorkerThread();
}

void StartSettingsPage::apply2Config()
{
	_njCfgPage->apply2Config();
	_epubCfgPage->apply2Config();
	_pdfCfgPage->apply2Config();
	_customFilePluginCfgPage->apply2Config();
	if (_applyFunc) {
		_applyFunc();
	}
}

void StartSettingsPage::clearLog() {
	_resetLogBufferState(false);
}

bool StartSettingsPage::_isLogScrollAtBottom() const
{
	const QScrollBar* scrollBar = _logOutput->verticalScrollBar();
	return scrollBar->value() >= scrollBar->maximum() - 4;
}

void StartSettingsPage::_ensureWorkerThread()
{
	if (_workThread) {
		return;
	}

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
			_usedTimeLabel->display("00:00:00");
			_remainTimeLabel->display("--:--");
			_estimator.reset();
			if (_translationWorkbenchPage && _transEngine != "Rebuild") {
				_translationWorkbenchPage->updateStage(tr("翻译中"), QString());
			}
		});
	connect(_worker, &TranslatorWorker::writeLogSignal, this, [this](const QString& log)
		{
			if (_isLogScrollAtBottom() && !_logPaused && !_logResumeInProgress && _pendingLog.isEmpty() && !_pendingOverflowed) {
				_appendLogChunkToView(log);
				return;
			}
			if (!_isLogScrollAtBottom()) {
				_setLogPaused(true);
			}
			_enqueuePendingLog(log);
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
			const int previousProgress = _progressBar->value();
			_progressBar->setValue(previousProgress + ticks);
			const int progressDelta = _progressBar->value() - previousProgress;
			const auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - _startTime);
			_usedTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", elapsedSeconds)
			));
			if (progressDelta <= 0) {
				return;
			}
			const auto etaWithSpeed = _estimator.recordProgressAndGetSpeedWithEta(progressDelta, _progressBar->value(), _progressBar->maximum());
			const double& speed = etaWithSpeed.first;
			const Duration& eta = etaWithSpeed.second;
			if (_speedLabel) {
				_speedLabel->setText(QString::fromStdString(
					std::format("{:.2f} lines/s", speed)
				));
			}
			if (eta.count() == std::numeric_limits<double>::infinity() || std::isnan(eta.count())) {
				_remainTimeLabel->display("--:--");
				return;
			}
			_remainTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", eta)
			));
		});
	connect(_worker, &TranslatorWorker::runtimeFilesResetSignal, this, [=](const QVector<GuiRuntimeFileProgress>& files)
		{
			_translationWorkbenchPage->resetRuntimeFiles(files);
		});
	connect(_worker, &TranslatorWorker::runtimeFileProgressSignal, this, [=](const GuiRuntimeFileProgress& file)
		{
			_translationWorkbenchPage->updateRuntimeFile(file);
		});
	connect(_worker, &TranslatorWorker::runtimeFileProgressBatchSignal, this, [=](const QVector<GuiRuntimeFileProgress>& files)
		{
			_translationWorkbenchPage->updateRuntimeFiles(files);
		});
	connect(_worker, &TranslatorWorker::runtimeSuccessSignal, this, [=](const GuiRuntimeSuccessEvent& event)
		{
			_translationWorkbenchPage->appendSuccess(event);
		});
	connect(_worker, &TranslatorWorker::runtimeSuccessBatchSignal, this, [=](const QVector<GuiRuntimeSuccessEvent>& events)
		{
			_translationWorkbenchPage->appendSuccesses(events);
		});
	connect(_worker, &TranslatorWorker::runtimeErrorSignal, this, [=](const GuiRuntimeErrorEvent& event)
		{
			_translationWorkbenchPage->appendError(event);
		});
	connect(_worker, &TranslatorWorker::runtimeErrorBatchSignal, this, [=](const QVector<GuiRuntimeErrorEvent>& events)
		{
			_translationWorkbenchPage->appendErrors(events);
		});
	connect(_worker, &TranslatorWorker::runtimeStageChangedSignal, this, [=](const QString& stage, const QString& currentFile)
		{
			_translationWorkbenchPage->updateStage(stage, currentFile);
		});
	_workThread->start();
}

void StartSettingsPage::_disposeWorkerThread()
{
	if (!_workThread) {
		return;
	}
	_workThread->quit();
	_workThread->wait();
	_workThread->deleteLater();
	_workThread = nullptr;
	_worker = nullptr;
}

void StartSettingsPage::_setLogPaused(bool paused)
{
	if (_logPaused == paused) {
		return;
	}
	_logPaused = paused;
	if (_logPausedRow) {
		_logPausedRow->setVisible(_logPaused);
	}
}

void StartSettingsPage::_enqueuePendingLog(const QString& chunk)
{
	const qsizetype chunkBytes = chunk.size();
	if (_pendingLogBytes + chunkBytes > MAX_PENDING_LOG_BYTES && !_pendingLog.contains("```\n问题概览:")) {
		_pendingLog.clear();
		_pendingLogBytes = 0;
		_pendingOverflowed = true;
	}
	_pendingLog += chunk;
	_pendingLogBytes += chunkBytes;
}

void StartSettingsPage::_flushPendingLogToView()
{
	if (!_pendingLog.isEmpty()) {
		_appendLogChunkToView(_pendingLog);
		_pendingLog.clear();
	}
	if (_pendingOverflowed) {
		_appendLogChunkToView(tr("[GUI] 日志窗口缓存超过 5MB，有旧缓存被丢弃。完整日志请查看项目 logs/*.log。") + "\n");
		_pendingOverflowed = false;
	}
	_pendingLogBytes = 0;
}

void StartSettingsPage::_appendLogChunkToView(const QString& log)
{
	if (log.isEmpty()) {
		return;
	}

	_logOutput->setUpdatesEnabled(false);
	QScrollBar* scrollBar = _logOutput->verticalScrollBar();

	{
		scrollBar->blockSignals(true);
		QTextCursor tempCursor(_logOutput->document());
		tempCursor.movePosition(QTextCursor::End);
		tempCursor.setCharFormat(QTextCharFormat());

		auto processLogFunc = [&](const QString& l)
			{
				QStringList lines = l.split('\n');
				for (int i = 0; i < lines.size(); ++i) {
					QString& line = lines[i];
					line = line.trimmed();
					if (i == lines.size() - 1 && line.isEmpty()) break;
					QTextCharFormat fmt;
					if (line.contains(" error]")) {
						fmt.setForeground(Qt::red);
					}
					else if (line.contains(" critical]")) {
						fmt.setForeground(Qt::darkRed);
						fmt.setFontWeight(QFont::Bold);
					}
					else if (line.contains(" warning]")) {
						fmt.setForeground(QColor(255, 140, 0));
					}
					else if (line.contains(" debug]")) {
						fmt.setForeground(QColor(Qt::darkBlue));
					}
					else if (line.contains(" trace]")) {
						fmt.setForeground(QColor(Qt::darkGreen));
					}
					else {
						fmt.setForeground(QColor(Qt::black));
					}
					tempCursor.setCharFormat(fmt);
					tempCursor.insertText(line);
					if (i < lines.size() - 1) {
						tempCursor.insertText("\n");
					}
				}
			};

		if (log.contains("```\n问题概览:")) {
			QString logCopy = log;
			int index = log.indexOf("```\n问题概览:");
			QString pre = logCopy.left(index);
			logCopy = logCopy.mid(index);
			index = logCopy.indexOf("问题概览结束\n```");
			QString overview = logCopy.left(index + 10);
			logCopy = logCopy.mid(index + 10);
			QString post = std::move(logCopy);
			processLogFunc(pre);
			QTextCharFormat format;
			format.setForeground(QColor(255, 0, 0));
			tempCursor.setCharFormat(format);
			tempCursor.insertText(overview);
			processLogFunc(post);
		}
		else {
			if (log.length() > 4096 || log.count('\n') > 30) {
				tempCursor.insertText(log);
			}
			else {
				processLogFunc(log);
			}
		}

		int currentLineCount = _logOutput->document()->lineCount();
		if (currentLineCount > MAX_LOG_LINE_COUNT) {
			const int toRemoveLineCount = currentLineCount - MAX_LOG_LINE_COUNT;
			QTextCursor deleteCursor(_logOutput->document());
			deleteCursor.movePosition(QTextCursor::Start);
			deleteCursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, toRemoveLineCount);
			deleteCursor.removeSelectedText();
		}

		scrollBar->blockSignals(false);
	}

	scrollBar->setValue(scrollBar->maximum());
	_logOutput->setUpdatesEnabled(true);
}

void StartSettingsPage::_resetLogBufferState(bool keepViewContent)
{
	_pendingLog.clear();
	_pendingLogBytes = 0;
	_pendingOverflowed = false;
	_logResumeInProgress = false;
	_setLogPaused(false);
	if (!keepViewContent && _logOutput) {
		_logOutput->clear();
	}
}

void StartSettingsPage::_setupUI()
{
	QWidget* mainWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
	mainLayout->setContentsMargins(20, 15, 15, 0);

	QWidget* topWidget = new QWidget(mainWidget);
	QHBoxLayout* topLayout = new QHBoxLayout(topWidget);

	// 日志输出
	QWidget* logAreaWidget = new QWidget(topWidget);
	QVBoxLayout* logAreaLayout = new QVBoxLayout(logAreaWidget);
	logAreaLayout->setContentsMargins(0, 0, 0, 0);
	logAreaLayout->setSpacing(4);

	_logOutput = new ElaPlainTextEdit(logAreaWidget);
	_logOutput->setReadOnly(true);
	QFont font = _logOutput->font();
	font.setPixelSize(14);
	_logOutput->setFont(font);
	_logOutput->setPlaceholderText(tr("日志输出"));
	logAreaLayout->addWidget(_logOutput);

	_logPausedRow = new QWidget(logAreaWidget);
	QHBoxLayout* logPausedRowLayout = new QHBoxLayout(_logPausedRow);
	logPausedRowLayout->setContentsMargins(0, 0, 0, 0);
	logPausedRowLayout->setSpacing(8);

	_logPausedHint = new ElaText(_logPausedRow);
	_logPausedHint->setTextPixelSize(12);
	_logPausedHint->setText(tr("日志输出已暂停，点击右侧按钮\n回到底部并补发缓存"));
	logPausedRowLayout->addWidget(_logPausedHint);
	logPausedRowLayout->addStretch();

	_resumeLogButton = new ElaPushButton(_logPausedRow);
	_resumeLogButton->setText(tr("回到底部并继续输出"));
	logPausedRowLayout->addWidget(_resumeLogButton);
	_logPausedRow->setVisible(false);
	logAreaLayout->addWidget(_logPausedRow);
	topLayout->addWidget(logAreaWidget);

	connect(_resumeLogButton, &ElaPushButton::clicked, this, [=]()
		{
			QScrollBar* scrollBar = _logOutput->verticalScrollBar();
			_logResumeInProgress = true;
			{
				QSignalBlocker blocker(scrollBar);
				scrollBar->setValue(scrollBar->maximum());
				_flushPendingLogToView();
				scrollBar->setValue(scrollBar->maximum());
			}
			_setLogPaused(false);
			_logResumeInProgress = false;
		});

	QTimer* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [=]()
		{
			if (!_logPausedRow->isVisible() || !_isLogScrollAtBottom()) {
				timer->stop();
				_timerStarted = false;
				return;
			}
			if (--(_secondsToResumeLog) > 0) {
				_resumeLogButton->setText(tr("继续输出") + "(" + QString::number(_secondsToResumeLog) + ")");
			}
			else {
				timer->stop();
				_resumeLogButton->click();
				_timerStarted = false;
			}
		});
	_logOutput->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	QScrollBar* logScrollBar = _logOutput->verticalScrollBar();
	connect(logScrollBar, &QScrollBar::valueChanged, this, [=](int)
		{
			if (_logResumeInProgress) {
				return;
			}
			if (!_isLogScrollAtBottom()) {
				if (_logPaused || !_startTranslateButton->isEnabled()) {
					_resumeLogButton->setText(tr("回到底部并继续输出"));
					_secondsToResumeLog = 3;
					_setLogPaused(true);
				}
			}
			else if (_logPausedRow->isVisible()) {
				if (!_timerStarted) { // 不想用 isActive，怕又出问题
					_resumeLogButton->setText(tr("继续输出") + "(" + QString::number(_secondsToResumeLog) + ")");
					timer->start(1000);
					_timerStarted = true;
				}
			}
		});


	ElaScrollPageArea* buttonArea = new ElaScrollPageArea(mainWidget);
	buttonArea->setFixedWidth(200);
	buttonArea->setMaximumHeight(600);
	QVBoxLayout* buttonLayout = new QVBoxLayout(buttonArea);

	// 文件格式
	std::string filePlugin = toml::find_or(_projectConfig, "plugins", "filePlugin", "NormalJson");
	QString filePluginStr = QString::fromStdString(filePlugin);
	ElaText* fileFormatLabel = new ElaText(buttonArea);
	fileFormatLabel->setTextPixelSize(16);
	fileFormatLabel->setText(tr("文件格式:"));
	buttonLayout->addWidget(fileFormatLabel);
	_fileFormatComboBox = new NoWheelComboBox(buttonArea);
	_fileFormatComboBox->addItem("NormalJson");
	_fileFormatComboBox->addItem("Epub");
	_fileFormatComboBox->addItem("PDF");
	_fileFormatComboBox->addItem("Custom");
	if (!filePluginStr.isEmpty()) {
		if (filePluginStr.toLower().endsWith(".lua") || filePluginStr.toLower().endsWith(".py")) {
			_fileFormatComboBox->setCurrentIndex(3);
		}
		else {
			int index = _fileFormatComboBox->findText(filePluginStr);
			if (index >= 0) {
				_fileFormatComboBox->setCurrentIndex(index);
			}
		}
	}
	buttonLayout->addWidget(_fileFormatComboBox);

	// 针对文件格式的输出设置
	ElaPushButton* outputSetting = new ElaPushButton(buttonArea);
	outputSetting->setText(tr("文件处理器设置"));
	buttonLayout->addWidget(outputSetting);
	connect(outputSetting, &ElaPushButton::clicked, this, &StartSettingsPage::_onOutputSettingClicked);

	// 线程数
	ElaText* threadNumLabel = new ElaText(buttonArea);
	threadNumLabel->setTextPixelSize(16);
	threadNumLabel->setText(tr("工作线程数:"));
	buttonLayout->addWidget(threadNumLabel);
	QWidget* threadNumWidget = new QWidget(buttonArea);
	QHBoxLayout* threadNumLayout = new QHBoxLayout(threadNumWidget);
	_threadNumRing = new ElaProgressRing(buttonArea);
	threadNumLayout->addWidget(_threadNumRing);
	_speedLabel = new ElaText(buttonArea);
	_speedLabel->setTextPixelSize(12);
	_speedLabel->setText("0 lines/s");
	threadNumLayout->addWidget(_speedLabel);
	buttonLayout->addWidget(threadNumWidget);

	// 已用时间
	ElaText* usedTimeLabelText = new ElaText(buttonArea);
	usedTimeLabelText->setTextPixelSize(14);
	usedTimeLabelText->setText(tr("已用时间:"));
	buttonLayout->addWidget(usedTimeLabelText);
	_usedTimeLabel = new ElaLCDNumber(buttonArea);
	_usedTimeLabel->display("00:00:00");
	buttonLayout->addWidget(_usedTimeLabel);

	// 剩余时间
	ElaText* remainTimeLabelText = new ElaText(buttonArea);
	remainTimeLabelText->setTextPixelSize(14);
	remainTimeLabelText->setText(tr("剩余时间:"));
	buttonLayout->addWidget(remainTimeLabelText);
	_remainTimeLabel = new ElaLCDNumber(buttonArea);
	_remainTimeLabel->display("00:00:00");
	buttonLayout->addWidget(_remainTimeLabel);

	// 翻译模式
	std::string transEngine = toml::find_or(_projectConfig, "plugins", "transEngine", "ForGalJson");
	QString transEngineStr = QString::fromStdString(transEngine);
	ElaText* translateModeLabel = new ElaText(buttonArea);
	translateModeLabel->setTextPixelSize(16);
	translateModeLabel->setText(tr("翻译模式:"));
	buttonLayout->addWidget(translateModeLabel);
	ElaComboBox* translateMode = new NoWheelComboBox(buttonArea);
	// 不要被鼠标滚轮的滚动改变选项
	translateMode->addItem("ForGalTsv");
	translateMode->addItem("ForNovelTsv");
	translateMode->addItem("ForGalJson");
	translateMode->addItem("Sakura");
	translateMode->addItem("DumpName");
	translateMode->addItem("NameTrans");
	translateMode->addItem("GenDict");
	translateMode->addItem("Rebuild");
	translateMode->addItem("ShowNormal");
	if (!transEngineStr.isEmpty()) {
		int index = translateMode->findText(transEngineStr);
		if (index >= 0) {
			translateMode->setCurrentIndex(index);
		}
	}
	buttonLayout->addWidget(translateMode);

	const bool agentEnabled = toml::find_or(_projectConfig, "agent", "enabled", false);
	ElaText* agentModeLabel = new ElaText(buttonArea);
	agentModeLabel->setTextPixelSize(14);
	agentModeLabel->setText(tr("实验性: Agent 模式"));
	buttonLayout->addWidget(agentModeLabel);
	ElaToggleSwitch* agentModeToggle = new ElaToggleSwitch(buttonArea);
	agentModeToggle->setIsToggled(agentEnabled);
	buttonLayout->addWidget(agentModeToggle);
	auto refreshAgentControls = [=]()
		{
			const QString currentMode = translateMode->currentText();
			const bool supportsAgent = currentMode == "ForGalTsv" || currentMode == "ForNovelTsv" || currentMode == "GenDict";
			agentModeToggle->setEnabled(supportsAgent);
			if (!supportsAgent) {
				agentModeToggle->setIsToggled(false);
			}
		};
	connect(translateMode, &ElaComboBox::currentTextChanged, this, [=](const QString&) { refreshAgentControls(); });
	connect(agentModeToggle, &ElaToggleSwitch::toggled, this, [=](bool) { refreshAgentControls(); });
	refreshAgentControls();

	// 开始翻译
	_startTranslateButton = new ElaPushButton(buttonArea);
	_startTranslateButton->setText(tr("开始翻译"));
	connect(_startTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStartTranslatingClicked);
	connect(_startTranslateButton, &ElaPushButton::clicked, this, [=]()
		{
			const bool scrollAtBottom = _isLogScrollAtBottom();
			if (!_logOutput->toPlainText().isEmpty()) {
				QTextCursor tempCursor(_logOutput->document());
				tempCursor.movePosition(QTextCursor::End);
				tempCursor.insertText("\n\n\n\n\n");
			}
			if (scrollAtBottom) {
				QScrollBar* scrollBar = _logOutput->verticalScrollBar();
				scrollBar->setValue(scrollBar->maximum());
			}
		});
	buttonLayout->addWidget(_startTranslateButton);

	// 停止翻译
	_stopTranslateButton = new ElaPushButton(buttonArea);
	_stopTranslateButton->setText(tr("停止翻译"));
	_stopTranslateButton->setEnabled(false);
	connect(_stopTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::_onStopTranslatingClicked);
	buttonLayout->addWidget(_stopTranslateButton);
	topLayout->addWidget(buttonArea);

	mainLayout->addWidget(topWidget);


	// 进度条
	QWidget* progressRow = new QWidget(mainWidget);
	QHBoxLayout* progressLayout = new QHBoxLayout(progressRow);
	progressLayout->setContentsMargins(0, 0, 0, 0);
	progressLayout->setSpacing(8);
	_progressBar = new ElaProgressBar(mainWidget);
	_progressBar->setRange(0, 100);
	_progressBar->setValue(0);
	progressLayout->addWidget(_progressBar, 1);
	_workbenchButton = new ElaIconButton(ElaIconType::ChartSimple, 16, 35, 35, progressRow);
	_workbenchButton->setToolTip(tr("详情"));
	connect(_workbenchButton, &QPushButton::clicked, this, [=]()
		{
			this->navigation(5);
		});
	progressLayout->addWidget(_workbenchButton);
	mainLayout->addWidget(progressRow);

	_translationWorkbenchPage = new TranslationWorkbenchPage(this);

	// 这个的 isVerticalGrabGesture 保持为 true 主要是方便随便拉一下看进度条而不必非要转鼠标滚轮或者侧边滚动条
	addCentralWidget(mainWidget, true, true, 0);

	_applyFunc = [=]()
		{
			if (_fileFormatComboBox->currentText() != "Custom") {
				insertToml(_projectConfig, "plugins.filePlugin", _fileFormatComboBox->currentText().toStdString());
			}
			else {
				const std::string customFilePluginStr = toml::find_or(_projectConfig, "plugins", "customFilePlugin", "Lua/MySampleFilePlugin.lua");
				const fs::path customFilePluginPath = ascii2Wide(customFilePluginStr);
				if (
					!isSameExtension(customFilePluginPath, L".lua") &&
					!isSameExtension(customFilePluginPath, L".py")
					) 
				{
					ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("文件格式错误"), tr("自定义文件插件的格式必须是 .lua 或 .py 格式。"), 3000);
				}
				insertToml(_projectConfig, "plugins.filePlugin", customFilePluginStr);
			}
			insertToml(_projectConfig, "plugins.transEngine", translateMode->currentText().toStdString());
			insertToml(_projectConfig, "agent.enabled", agentModeToggle->getIsToggled());
		};

	// 顺序和_onOutputSettingClicked里的索引一致
	_njCfgPage = new NJCfgPage(_projectConfig, this);
	addCentralWidget(_njCfgPage, true, false, 0);
	_epubCfgPage = new EpubCfgPage(_projectConfig, this);
	addCentralWidget(_epubCfgPage, true, false, 0);
	_pdfCfgPage = new PDFCfgPage(_projectConfig, this);
	addCentralWidget(_pdfCfgPage, true, false, 0);
	_customFilePluginCfgPage = new CustomFilePluginCfgPage(_projectDir, _globalConfig, _projectConfig, this);
	addCentralWidget(_customFilePluginCfgPage, true, false, 0);
	addCentralWidget(_translationWorkbenchPage, true, false, 0);
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
	else if (fileFormat == "PDF") {
		this->navigation(3);
	}
	else if (fileFormat == "Custom") {
		this->navigation(4);
	}
}


// 底下的可以不用看

void StartSettingsPage::_onStartTranslatingClicked()
{
	_resetLogBufferState(true);
	if (_translationWorkbenchPage) {
		_translationWorkbenchPage->clearRuntime();
	}
	Q_EMIT startTranslating();
	_transEngine = QString::fromStdString(toml::find_or(_projectConfig, "plugins", "transEngine", ""));

	_startTime = std::chrono::high_resolution_clock::now();
	_usedTimeLabel->display("00:00:00");
	_startTranslateButton->setEnabled(false);
	_progressBar->setValue(0);
	_ensureWorkerThread();

	Q_EMIT startWork();
	_stopTranslateButton->setEnabled(true);
}

void StartSettingsPage::_onStopTranslatingClicked()
{
	_stopTranslateButton->setEnabled(false);
	if (_worker) {
		_worker->stopTranslation();
	}
	ElaMessageBar::information(ElaMessageBarType::BottomRight, tr("停止中"), tr("正在等待最后一批翻译完成，请稍候..."), 3000);
}

void StartSettingsPage::_workFinished(int exitCode)
{
	_threadNumRing->setValue(0);
	_remainTimeLabel->display("00:00:00");
	_trayIcon->show();

	switch (exitCode)
	{
	case -2:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("翻译失败"), tr("项目 ") + 
			QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的翻译任务失败，请检查日志输出。"), 3000);

		// 显示通知消息
		_trayIcon->showMessage(
			tr("翻译失败"),                  // 标题
				tr("项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的翻译任务失败，请检查日志输出。"),      // 内容
			QSystemTrayIcon::Critical, // 图标类型 (Information, Warning, Critical)
			5000                          // 显示时长 (毫秒)
		);
		break;
	case -1:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("翻译失败"), tr("项目 ") + 
			QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 连工厂函数都失败了，玩毛啊"), 3000);
		break;
	case 0:
		if (_transEngine == "DumpName" || _transEngine == "GenDict") {
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("生成完成"), tr("项目 ") + 
				QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的生成任务已完成。"), 3000);
			_trayIcon->showMessage(
				tr("生成完成"),                  // 标题
					tr("项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的生成任务已完成。"),      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		else if (_transEngine == "ShowNormal") {
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("生成完成"), 
				tr("请在 show_normal 文件夹中查收项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的预处理结果。"), 3000);
			_trayIcon->showMessage(
				tr("生成完成"),                  // 标题
					tr("请在 show_normal 文件夹中查收项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的预处理结果。"),      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		else {
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("翻译完成"), 
				tr("请在 gt_output 文件夹中查收项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + " 的翻译结果。", 3000);
			_trayIcon->showMessage(
				tr("翻译完成"),                  // 标题
					tr("请在 gt_output 文件夹中查收项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的翻译结果。"),      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		break;
	case 1:
		_trayIcon->showMessage(
			tr("翻译停止"),                  // 标题
			tr("项目 ") + QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的翻译任务停止成功。"),      // 内容
			QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
			5000                          // 显示时长 (毫秒)
		);
		ElaMessageBar::information(ElaMessageBarType::BottomRight, tr("停止成功"), tr("项目 ") + 
			QString::fromStdWString(_projectDir.filename().wstring()) + tr(" 的翻译任务已终止"), 3000);
		break;
	default:
		break;
	}
	std::thread([this]()
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));
			if (_trayIcon) {
				_trayIcon->hide();
			}
		}).detach();

	Q_EMIT finishTranslatingSignal(_transEngine, exitCode);
	_startTranslateButton->setEnabled(true);
	_stopTranslateButton->setEnabled(false);
	_disposeWorkerThread();
}
