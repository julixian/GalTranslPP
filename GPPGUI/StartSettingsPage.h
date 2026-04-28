// StartSettingsPage.h

#ifndef STARTSETTINGSPAGE_H
#define STARTSETTINGSPAGE_H

#include <QThread>
#include <toml.hpp>
#include <filesystem>
#include <QSystemTrayIcon>
#include "BasePage.h"
#include "TranslatorWorker.h"
#include "ExponentialMovingAverageEstimator.hpp"

namespace fs = std::filesystem;

class ElaPushButton;
class ElaIconButton;
class ElaProgressBar;
class ElaComboBox;
class ElaPlainTextEdit;
class ElaProgressRing;
class ElaLCDNumber;
class ElaText;
class NJCfgPage;
class EpubCfgPage;
class PDFCfgPage;
class CustomFilePluginCfgPage;
class TranslationWorkbenchPage;

class StartSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit StartSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~StartSettingsPage() override;

    virtual void apply2Config() override;

    void clearLog();

Q_SIGNALS:
    void startTranslating();  // 让projectSettings去保存配置
    void finishTranslatingSignal(const QString& transEngine, int exitCode); // 这两个向projectSettings页发送
    void startWork();
    void stopWork();  // 这两个向worker发送

private:
    static constexpr qsizetype MAX_PENDING_LOG_BYTES = 5 * 1024 * 1024;
    static constexpr int MAX_LOG_LINE_COUNT = 10000;

    bool _isLogScrollAtBottom() const;
    void _ensureWorkerThread();
    void _disposeWorkerThread();
    void _setLogPaused(bool paused);
    void _enqueuePendingLog(const QString& chunk);
    void _flushPendingLogToView();
    void _appendLogChunkToView(const QString& log);
    void _resetLogBufferState(bool keepViewContent);

private:

    fs::path& _projectDir;
    QThread* _workThread{nullptr};
    TranslatorWorker* _worker{nullptr};

    void _setupUI();
    toml::ordered_value& _projectConfig;
    toml::ordered_value& _globalConfig;
    QWidget* _mainWindow;

    ElaPushButton* _startTranslateButton;
    ElaPushButton* _stopTranslateButton;
    ElaIconButton* _workbenchButton{nullptr};
    ElaProgressBar* _progressBar;

    ElaPlainTextEdit* _logOutput;
    QWidget* _logPausedRow{nullptr};
    ElaText* _logPausedHint{nullptr};
    ElaPushButton* _resumeLogButton{nullptr};
    int _secondsToResumeLog{3};
    bool _logPaused{false};
    bool _logResumeInProgress{false};
    QString _pendingLog;
    qsizetype _pendingLogBytes{0};
    bool _pendingOverflowed{false};
    bool _timerStarted{false};

    ElaComboBox* _fileFormatComboBox;

    QString _transEngine;
    ElaProgressRing* _threadNumRing;
    ElaText* _speedLabel{nullptr};
    ElaLCDNumber* _usedTimeLabel;
    ElaLCDNumber* _remainTimeLabel;
    QSystemTrayIcon* _trayIcon;
    ExponentialMovingAverageEstimator _estimator;
    std::chrono::high_resolution_clock::time_point _startTime;

private Q_SLOTS:

    void _onStartTranslatingClicked();
    void _onStopTranslatingClicked();
    void _workFinished(int exitCode); // worker结束了的信号
    void _onOutputSettingClicked();

private:
    // 文件格式输出配置页
    NJCfgPage* _njCfgPage;
    EpubCfgPage* _epubCfgPage;
    PDFCfgPage* _pdfCfgPage;
    CustomFilePluginCfgPage* _customFilePluginCfgPage;
    TranslationWorkbenchPage* _translationWorkbenchPage{nullptr};
};

#endif // STARTSETTINGSPAGE_H
