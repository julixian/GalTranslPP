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

StartSettingsPage::StartSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent)
	: BasePage(parent), m_projectDir(projectDir), m_globalConfig(globalConfig), m_projectConfig(projectConfig)
{
	setWindowTitle(tr("启动设置"));
	setTitleVisible(false);

	m_trayIcon = new QSystemTrayIcon(this);
	m_trayIcon->setIcon(QIcon(":/GPPGUI/Resource/Image/julixian_s.ico"));
	connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, [=]()
		{
			QUrl dirUrl = QUrl::fromLocalFile(QString::fromStdWString(m_projectDir.wstring()));
			QDesktopServices::openUrl(dirUrl);
		});

	setupUi();
}

StartSettingsPage::~StartSettingsPage()
{
	m_trayIcon = nullptr;
	if (m_worker && m_workThread && m_workThread->isRunning()) {
		m_worker->stopTranslation();
	}
	disposeWorkerThread();
}

void StartSettingsPage::apply2Config()
{
	m_njCfgPage->apply2Config();
	m_epubCfgPage->apply2Config();
	m_pdfCfgPage->apply2Config();
	m_customFilePluginCfgPage->apply2Config();
	if (m_applyFunc) {
		m_applyFunc();
	}
}

void StartSettingsPage::clearLog() {
	resetLogBufferState(false);
}

bool StartSettingsPage::isLogScrollAtBottom() const
{
	const QScrollBar* scrollBar = m_logOutput->verticalScrollBar();
	return scrollBar->value() >= scrollBar->maximum() - 4;
}

void StartSettingsPage::ensureWorkerThread()
{
	if (m_workThread) {
		return;
	}

	m_workThread = new QThread(this);
	m_worker = new TranslatorWorker(m_projectDir);
	m_worker->moveToThread(m_workThread);
	connect(m_workThread, &QThread::finished, m_worker, &TranslatorWorker::deleteLater);
	connect(this, &StartSettingsPage::startWorkSignal, m_worker, &TranslatorWorker::doTranslation);
	connect(m_worker, &TranslatorWorker::translationFinishedSignal, this, &StartSettingsPage::workFinished);

	connect(m_worker, &TranslatorWorker::makeBarSignal, this, [=](int totalSentences, int totalThreads)
		{
			m_progressBar->setRange(0, totalSentences);
			m_progressBar->setValue(0);
			m_threadNumRing->setRange(0, totalThreads);
			m_threadNumRing->setValue(0);
			m_progressBar->setFormat("%v/%m lines [%p%]");
			m_startTime = std::chrono::high_resolution_clock::now();
			m_usedTimeLabel->display("00:00:00");
			m_remainTimeLabel->display("--:--");
			m_estimator.reset();
			if (m_translationWorkbenchPage && m_transEngine != "Rebuild") {
				m_translationWorkbenchPage->updateStage(tr("翻译中"), QString());
			}
		});
	connect(m_worker, &TranslatorWorker::writeLogSignal, this, [this](const QString& log)
		{
			if (isLogScrollAtBottom() && !m_logPaused && !m_logResumeInProgress && m_pendingLog.isEmpty() && !m_pendingOverflowed) {
				appendLogChunkToView(log);
				return;
			}
			if (!isLogScrollAtBottom()) {
				setLogPaused(true);
			}
			enqueuePendingLog(log);
		});
	connect(m_worker, &TranslatorWorker::addThreadNumSignal, this, [=]()
		{
			m_threadNumRing->setValue(m_threadNumRing->getValue() + 1);
		});
	connect(m_worker, &TranslatorWorker::reduceThreadNumSignal, this, [=]()
		{
			m_threadNumRing->setValue(m_threadNumRing->getValue() - 1);
		});
	connect(m_worker, &TranslatorWorker::updateBarSignal, this, [=](int ticks)
		{
			const int previousProgress = m_progressBar->value();
			m_progressBar->setValue(previousProgress + ticks);
			const int progressDelta = m_progressBar->value() - previousProgress;
			const auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_startTime);
			m_usedTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", elapsedSeconds)
			));
			if (progressDelta <= 0) {
				return;
			}
			const auto etaWithSpeed = m_estimator.recordProgressAndGetSpeedWithEta(progressDelta, m_progressBar->value(), m_progressBar->maximum());
			const double& speed = etaWithSpeed.first;
			const Duration& eta = etaWithSpeed.second;
			if (m_speedLabel) {
				m_speedLabel->setText(QString::fromStdString(
					std::format("{:.2f} lines/s", speed)
				));
			}
			if (eta.count() == std::numeric_limits<double>::infinity() || std::isnan(eta.count())) {
				m_remainTimeLabel->display("--:--");
				return;
			}
			m_remainTimeLabel->display(QString::fromStdString(
				std::format("{:%T}", eta)
			));
		});
	connect(m_worker, &TranslatorWorker::runtimeFilesResetSignal, this, [=](const QVector<GuiRuntimeFileProgress>& files)
		{
			m_translationWorkbenchPage->resetRuntimeFiles(files);
		});
	connect(m_worker, &TranslatorWorker::runtimeFileProgressBatchSignal, this, [=](const QVector<GuiRuntimeFileProgress>& files)
		{
			m_translationWorkbenchPage->updateRuntimeFiles(files);
		});
	connect(m_worker, &TranslatorWorker::runtimeSuccessBatchSignal, this, [=](const QVector<GuiRuntimeSuccessEvent>& events)
		{
			m_translationWorkbenchPage->appendSuccesses(events);
		});
	connect(m_worker, &TranslatorWorker::runtimeErrorBatchSignal, this, [=](const QVector<GuiRuntimeErrorEvent>& events)
		{
			m_translationWorkbenchPage->appendErrors(events);
		});
	connect(m_worker, &TranslatorWorker::runtimeStageChangedSignal, this, [=](const QString& stage, const QString& currentFile)
		{
			m_translationWorkbenchPage->updateStage(stage, currentFile);
		});
	m_workThread->start();
}

void StartSettingsPage::disposeWorkerThread()
{
	if (!m_workThread) {
		return;
	}
	m_workThread->quit();
	m_workThread->wait();
	m_workThread->deleteLater();
	m_workThread = nullptr;
	m_worker = nullptr;
}

void StartSettingsPage::setLogPaused(bool paused)
{
	if (m_logPaused == paused) {
		return;
	}
	m_logPaused = paused;
	if (m_logPausedRow) {
		m_logPausedRow->setVisible(m_logPaused);
	}
}

void StartSettingsPage::enqueuePendingLog(const QString& chunk)
{
	const qsizetype chunkBytes = chunk.size();
	if (m_pendingLogBytes + chunkBytes > MaxPendingLogBytes && !m_pendingLog.contains("```\n问题概览:")) {
		m_pendingLog.clear();
		m_pendingLogBytes = 0;
		m_pendingOverflowed = true;
	}
	m_pendingLog += chunk;
	m_pendingLogBytes += chunkBytes;
}

void StartSettingsPage::flushPendingLogToView()
{
	if (!m_pendingLog.isEmpty()) {
		appendLogChunkToView(m_pendingLog);
		m_pendingLog.clear();
	}
	if (m_pendingOverflowed) {
		appendLogChunkToView(tr("[GUI] 日志窗口缓存超过 5MB，有旧缓存被丢弃。完整日志请查看项目 logs/*.log。\n"));
		m_pendingOverflowed = false;
	}
	m_pendingLogBytes = 0;
}

void StartSettingsPage::appendLogChunkToView(const QString& log)
{
	if (log.isEmpty()) {
		return;
	}

	m_logOutput->setUpdatesEnabled(false);
	QScrollBar* scrollBar = m_logOutput->verticalScrollBar();

	{
		scrollBar->blockSignals(true);
		QTextCursor tempCursor(m_logOutput->document());
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

		int currentLineCount = m_logOutput->document()->lineCount();
		if (currentLineCount > MaxLogLineCount) {
			const int toRemoveLineCount = currentLineCount - MaxLogLineCount;
			QTextCursor deleteCursor(m_logOutput->document());
			deleteCursor.movePosition(QTextCursor::Start);
			deleteCursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, toRemoveLineCount);
			deleteCursor.removeSelectedText();
		}

		scrollBar->blockSignals(false);
	}

	scrollBar->setValue(scrollBar->maximum());
	m_logOutput->setUpdatesEnabled(true);
}

void StartSettingsPage::resetLogBufferState(bool keepViewContent)
{
	m_pendingLog.clear();
	m_pendingLogBytes = 0;
	m_pendingOverflowed = false;
	m_logResumeInProgress = false;
	setLogPaused(false);
	if (!keepViewContent && m_logOutput) {
		m_logOutput->clear();
	}
}

void StartSettingsPage::setupUi()
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

	m_logOutput = new ElaPlainTextEdit(logAreaWidget);
	m_logOutput->setReadOnly(true);
	QFont font = m_logOutput->font();
	font.setPixelSize(14);
	m_logOutput->setFont(font);
	m_logOutput->setPlaceholderText(tr("日志输出"));
	logAreaLayout->addWidget(m_logOutput);

	m_logPausedRow = new QWidget(logAreaWidget);
	QHBoxLayout* logPausedRowLayout = new QHBoxLayout(m_logPausedRow);
	logPausedRowLayout->setContentsMargins(0, 0, 0, 0);
	logPausedRowLayout->setSpacing(8);

	m_logPausedHint = new ElaText(m_logPausedRow);
	m_logPausedHint->setTextPixelSize(12);
	m_logPausedHint->setText(tr("日志输出已暂停，点击右侧按钮\n回到底部并补发缓存"));
	logPausedRowLayout->addWidget(m_logPausedHint);
	logPausedRowLayout->addStretch();

	m_resumeLogButton = new ElaPushButton(m_logPausedRow);
	m_resumeLogButton->setText(tr("回到底部并继续输出"));
	logPausedRowLayout->addWidget(m_resumeLogButton);
	m_logPausedRow->setVisible(false);
	logAreaLayout->addWidget(m_logPausedRow);
	topLayout->addWidget(logAreaWidget);

	connect(m_resumeLogButton, &ElaPushButton::clicked, this, [=]()
		{
			QScrollBar* scrollBar = m_logOutput->verticalScrollBar();
			m_logResumeInProgress = true;
			{
				QSignalBlocker blocker(scrollBar);
				scrollBar->setValue(scrollBar->maximum());
				flushPendingLogToView();
				scrollBar->setValue(scrollBar->maximum());
			}
			setLogPaused(false);
			m_logResumeInProgress = false;
		});

	QTimer* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [=]()
		{
			if (!m_logPausedRow->isVisible() || !isLogScrollAtBottom()) {
				timer->stop();
				m_timerStarted = false;
				return;
			}
			if (--(m_secondsToResumeLog) > 0) {
				m_resumeLogButton->setText(tr("继续输出(%1)").arg(QString::number(m_secondsToResumeLog)));
			}
			else {
				timer->stop();
				m_resumeLogButton->click();
				m_timerStarted = false;
			}
		});
	m_logOutput->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	QScrollBar* logScrollBar = m_logOutput->verticalScrollBar();
	connect(logScrollBar, &QScrollBar::valueChanged, this, [=](int)
		{
			if (m_logResumeInProgress) {
				return;
			}
			if (!isLogScrollAtBottom()) {
				if (m_logPaused || !m_startTranslateButton->isEnabled()) {
					m_resumeLogButton->setText(tr("回到底部并继续输出"));
					m_secondsToResumeLog = 3;
					setLogPaused(true);
				}
			}
			else if (m_logPausedRow->isVisible()) {
				if (!m_timerStarted) { // 不想用 isActive，怕又出问题
					m_resumeLogButton->setText(tr("继续输出(%1)").arg(QString::number(m_secondsToResumeLog)));
					timer->start(1000);
					m_timerStarted = true;
				}
			}
		});


	ElaScrollPageArea* buttonArea = new ElaScrollPageArea(mainWidget);
	buttonArea->setFixedWidth(200);
	buttonArea->setMaximumHeight(600);
	QVBoxLayout* buttonLayout = new QVBoxLayout(buttonArea);

	// 文件格式
	const std::string filePlugin = toml::find_or(m_projectConfig, "plugins", "filePlugin", "NormalJson");
	QString filePluginStr = QString::fromStdString(filePlugin);
	ElaText* fileFormatLabel = new ElaText(buttonArea);
	fileFormatLabel->setTextPixelSize(16);
	fileFormatLabel->setText(tr("文件格式:"));
	buttonLayout->addWidget(fileFormatLabel);
	m_fileFormatComboBox = new NoWheelComboBox(buttonArea);
	m_fileFormatComboBox->addItem("NormalJson");
	m_fileFormatComboBox->addItem("Epub");
	m_fileFormatComboBox->addItem("PDF");
	m_fileFormatComboBox->addItem("Custom");
	if (!filePluginStr.isEmpty()) {
		if (filePluginStr.toLower().endsWith(".lua") || filePluginStr.toLower().endsWith(".py")) {
			m_fileFormatComboBox->setCurrentIndex(3);
		}
		else {
			int index = m_fileFormatComboBox->findText(filePluginStr);
			if (index >= 0) {
				m_fileFormatComboBox->setCurrentIndex(index);
			}
		}
	}
	buttonLayout->addWidget(m_fileFormatComboBox);

	// 针对文件格式的输出设置
	ElaPushButton* outputSetting = new ElaPushButton(buttonArea);
	outputSetting->setText(tr("文件处理器设置"));
	buttonLayout->addWidget(outputSetting);
	connect(outputSetting, &ElaPushButton::clicked, this, &StartSettingsPage::onOutputSettingClicked);

	// 线程数
	ElaText* threadNumLabel = new ElaText(buttonArea);
	threadNumLabel->setTextPixelSize(16);
	threadNumLabel->setText(tr("工作线程数:"));
	buttonLayout->addWidget(threadNumLabel);
	QWidget* threadNumWidget = new QWidget(buttonArea);
	QHBoxLayout* threadNumLayout = new QHBoxLayout(threadNumWidget);
	m_threadNumRing = new ElaProgressRing(buttonArea);
	threadNumLayout->addWidget(m_threadNumRing);
	m_speedLabel = new ElaText(buttonArea);
	m_speedLabel->setTextPixelSize(12);
	m_speedLabel->setText("0 lines/s");
	threadNumLayout->addWidget(m_speedLabel);
	buttonLayout->addWidget(threadNumWidget);

	// 已用时间
	ElaText* usedTimeLabelText = new ElaText(buttonArea);
	usedTimeLabelText->setTextPixelSize(14);
	usedTimeLabelText->setText(tr("已用时间:"));
	buttonLayout->addWidget(usedTimeLabelText);
	m_usedTimeLabel = new ElaLCDNumber(buttonArea);
	m_usedTimeLabel->display("00:00:00");
	buttonLayout->addWidget(m_usedTimeLabel);

	// 剩余时间
	ElaText* remainTimeLabelText = new ElaText(buttonArea);
	remainTimeLabelText->setTextPixelSize(14);
	remainTimeLabelText->setText(tr("剩余时间:"));
	buttonLayout->addWidget(remainTimeLabelText);
	m_remainTimeLabel = new ElaLCDNumber(buttonArea);
	m_remainTimeLabel->display("00:00:00");
	buttonLayout->addWidget(m_remainTimeLabel);

	// 翻译模式
	const std::string transEngine = toml::find_or(m_projectConfig, "plugins", "transEngine", "ForGalJson");
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

	const bool agentEnabled = toml::find_or(m_projectConfig, "agent", "enabled", false);
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
	m_startTranslateButton = new ElaPushButton(buttonArea);
	m_startTranslateButton->setText(tr("开始翻译"));
	connect(m_startTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::onStartTranslatingClicked);
	connect(m_startTranslateButton, &ElaPushButton::clicked, this, [=]()
		{
			const bool scrollAtBottom = isLogScrollAtBottom();
			if (!m_logOutput->toPlainText().isEmpty()) {
				QTextCursor tempCursor(m_logOutput->document());
				tempCursor.movePosition(QTextCursor::End);
				tempCursor.insertText("\n\n\n\n\n");
			}
			if (scrollAtBottom) {
				QScrollBar* scrollBar = m_logOutput->verticalScrollBar();
				scrollBar->setValue(scrollBar->maximum());
			}
		});
	buttonLayout->addWidget(m_startTranslateButton);

	// 停止翻译
	m_stopTranslateButton = new ElaPushButton(buttonArea);
	m_stopTranslateButton->setText(tr("停止翻译"));
	m_stopTranslateButton->setEnabled(false);
	connect(m_stopTranslateButton, &ElaPushButton::clicked, this, &StartSettingsPage::onStopTranslatingClicked);
	buttonLayout->addWidget(m_stopTranslateButton);
	topLayout->addWidget(buttonArea);

	mainLayout->addWidget(topWidget);


	// 进度条
	QWidget* progressRow = new QWidget(mainWidget);
	QHBoxLayout* progressLayout = new QHBoxLayout(progressRow);
	progressLayout->setContentsMargins(0, 0, 0, 0);
	progressLayout->setSpacing(8);
	m_progressBar = new ElaProgressBar(mainWidget);
	m_progressBar->setRange(0, 100);
	m_progressBar->setValue(0);
	progressLayout->addWidget(m_progressBar, 1);
	m_workbenchButton = new ElaIconButton(ElaIconType::ChartSimple, 16, 35, 35, progressRow);
	m_workbenchButton->setToolTip(tr("详情"));
	connect(m_workbenchButton, &QPushButton::clicked, this, [=]()
		{
			this->navigation(5);
		});
	progressLayout->addWidget(m_workbenchButton);
	mainLayout->addWidget(progressRow);

	m_translationWorkbenchPage = new TranslationWorkbenchPage(this);

	// 这个的 isVerticalGrabGesture 保持为 true 主要是方便随便拉一下看进度条而不必非要转鼠标滚轮或者侧边滚动条
	addCentralWidget(mainWidget, true, true, 0);

	m_applyFunc = [=]()
		{
			if (m_fileFormatComboBox->currentText() != "Custom") {
				insertToml(m_projectConfig, "plugins.filePlugin", m_fileFormatComboBox->currentText().toStdString());
			}
			else {
				const std::string customFilePluginStr = toml::find_or(m_projectConfig, "plugins", "customFilePlugin", "Lua/MySampleFilePlugin.lua");
				const fs::path customFilePluginPath = ascii2Wide(customFilePluginStr);
				if (
					!isSameExtension(customFilePluginPath, L".lua") &&
					!isSameExtension(customFilePluginPath, L".py")
					)
				{
					ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("文件格式错误"), tr("自定义文件插件的格式必须是 .lua 或 .py 格式。"), 3000);
				}
				insertToml(m_projectConfig, "plugins.filePlugin", customFilePluginStr);
			}
			insertToml(m_projectConfig, "plugins.transEngine", translateMode->currentText().toStdString());
			insertToml(m_projectConfig, "agent.enabled", agentModeToggle->getIsToggled());
		};

	// 顺序和_onOutputSettingClicked里的索引一致
	m_njCfgPage = new NJCfgPage(m_projectConfig, this);
	addCentralWidget(m_njCfgPage, true, false, 0);
	m_epubCfgPage = new EpubCfgPage(m_projectConfig, this);
	addCentralWidget(m_epubCfgPage, true, false, 0);
	m_pdfCfgPage = new PDFCfgPage(m_projectConfig, this);
	addCentralWidget(m_pdfCfgPage, true, false, 0);
	m_customFilePluginCfgPage = new CustomFilePluginCfgPage(m_projectDir, m_globalConfig, m_projectConfig, this);
	addCentralWidget(m_customFilePluginCfgPage, true, false, 0);
	addCentralWidget(m_translationWorkbenchPage, true, false, 0);
}

void StartSettingsPage::onOutputSettingClicked()
{
	QString fileFormat = m_fileFormatComboBox->currentText();
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

void StartSettingsPage::onStartTranslatingClicked()
{
	resetLogBufferState(true);
	if (m_translationWorkbenchPage) {
		m_translationWorkbenchPage->clearRuntime();
	}
	Q_EMIT startTranslatingSignal();
	m_transEngine = QString::fromStdString(toml::find_or(m_projectConfig, "plugins", "transEngine", ""));

	m_startTime = std::chrono::high_resolution_clock::now();
	m_usedTimeLabel->display("00:00:00");
	m_startTranslateButton->setEnabled(false);
	m_progressBar->setValue(0);
	ensureWorkerThread();

	Q_EMIT startWorkSignal();
	m_stopTranslateButton->setEnabled(true);
}

void StartSettingsPage::onStopTranslatingClicked()
{
	m_stopTranslateButton->setEnabled(false);
	if (m_worker) {
		m_worker->stopTranslation();
	}
	ElaMessageBar::information(ElaMessageBarType::BottomRight, tr("停止中"), tr("正在等待最后一批翻译完成，请稍候..."), 3000);
}

void StartSettingsPage::workFinished(int exitCode)
{
	m_threadNumRing->setValue(0);
	m_remainTimeLabel->display("00:00:00");
	m_trayIcon->show();
	const QString projectName = QString::fromStdWString(m_projectDir.filename().wstring());

	switch (exitCode)
	{
	case -2:
	{
		const QString message = tr("项目 %1 的翻译任务失败，请检查日志输出。").arg(projectName);
		ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("翻译失败"), message, 3000);

		// 显示通知消息
		m_trayIcon->showMessage(
			tr("翻译失败"),                  // 标题
			message,      // 内容
			QSystemTrayIcon::Critical, // 图标类型 (Information, Warning, Critical)
			5000                          // 显示时长 (毫秒)
		);
		break;
	}
	case -1:
		ElaMessageBar::error(ElaMessageBarType::BottomRight, tr("翻译失败"),
			tr("项目 %1 连工厂函数都失败了，玩毛啊").arg(projectName), 3000);
		break;
	case 0:
		if (m_transEngine == "DumpName" || m_transEngine == "GenDict") {
			const QString message = tr("项目 %1 的生成任务已完成。").arg(projectName);
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("生成完成"), message, 3000);
			m_trayIcon->showMessage(
				tr("生成完成"),                  // 标题
				message,      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		else if (m_transEngine == "ShowNormal") {
			const QString message = tr("请在 show_normal 文件夹中查收项目 %1 的预处理结果。").arg(projectName);
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("生成完成"), message, 3000);
			m_trayIcon->showMessage(
				tr("生成完成"),                  // 标题
				message,      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		else {
			const QString message = tr("请在 gt_output 文件夹中查收项目 %1 的翻译结果。").arg(projectName);
			ElaMessageBar::success(ElaMessageBarType::BottomRight, tr("翻译完成"), message, 3000);
			m_trayIcon->showMessage(
				tr("翻译完成"),                  // 标题
				message,      // 内容
				QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
				5000                          // 显示时长 (毫秒)
			);
		}
		break;
	case 1:
	{
		const QString trayMessage = tr("项目 %1 的翻译任务停止成功。").arg(projectName);
		m_trayIcon->showMessage(
			tr("翻译停止"),                  // 标题
			trayMessage,      // 内容
			QSystemTrayIcon::Information, // 图标类型 (Information, Warning, Critical)
			5000                          // 显示时长 (毫秒)
		);
		ElaMessageBar::information(ElaMessageBarType::BottomRight, tr("停止成功"),
			tr("项目 %1 的翻译任务已终止").arg(projectName), 3000);
		break;
	}
	default:
		break;
	}
	std::thread([this]()
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));
			if (m_trayIcon) {
				m_trayIcon->hide();
			}
		}).detach();

	Q_EMIT finishTranslatingSignal(m_transEngine, exitCode);
	m_startTranslateButton->setEnabled(true);
	m_stopTranslateButton->setEnabled(false);
	disposeWorkerThread();
}
