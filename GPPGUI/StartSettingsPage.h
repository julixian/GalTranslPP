#ifndef STARTSETTINGSPAGE_H
#define STARTSETTINGSPAGE_H

#include "BasePage.h"
#include "EtaEstimator.hpp"
#include "TranslatorWorker.h"
#include <QThread>
#include <QSystemTrayIcon>
#include <filesystem>
#include <toml.hpp>

namespace fs = std::filesystem;

class ElaPushButton;
class ElaIconButton;
class ElaProgressBar;
class ElaNoWheelComboBox;
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
    explicit StartSettingsPage(fs::path& projectDir, toml::ordered_value& globalConfig, toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~StartSettingsPage() override;

	void apply2Config() override;

    void clearLog();


Q_SIGNALS:
    void startTranslatingSignal();  // 让 ProjectSettings 去保存配置
    void finishTranslatingSignal(const QString& transEngine, int exitCode); // 让 MainWindow 加点


private Q_SLOTS:
    void onStartTranslatingClicked();
    void onStopTranslatingClicked();
    void workFinished(int exitCode); // worker结束了的信号


private:
    static constexpr qsizetype MaxPendingLogBytes = 5 * 1024 * 1024;
    static constexpr int MaxLogLineCount = 10000;

    void ensureWorkerThread();
    void disposeWorkerThread();

    bool isLogScrollAtBottom() const;
    void setLogPaused(bool paused);
    void enqueuePendingLog(const QString& chunk);
    void flushPendingLogToView();
    void appendLogChunkToView(const QString& log);
    void resetLogBufferState(bool keepViewContent);


    void setupUi();
    fs::path& m_projectDir;
    toml::ordered_value& m_globalConfig;
    toml::ordered_value& m_projectConfig;

    QThread* m_workThread = nullptr;
    TranslatorWorker* m_worker = nullptr;

    ElaPushButton* m_startTranslateButton = nullptr;
    ElaPushButton* m_stopTranslateButton = nullptr;
    ElaIconButton* m_workbenchButton = nullptr;
    ElaProgressBar* m_progressBar = nullptr;

    ElaPlainTextEdit* m_logOutput = nullptr;
    QWidget* m_logPausedRow = nullptr;
    ElaText* m_logPausedHint = nullptr;
    ElaPushButton* m_resumeLogButton = nullptr;
    int m_secondsToResumeLog = 3;
    bool m_logPaused{};
    bool m_logResumeInProgress{};
    QString m_pendingLog;
    qsizetype m_pendingLogBytes{};
    bool m_pendingOverflowed{};
    bool m_timerStarted{};

    ElaNoWheelComboBox* m_filePluginComboBox = nullptr;

    QString m_workerTransEngine; // 当前工作的线程正在运行什么 transEngine
    ElaProgressRing* m_threadNumRing = nullptr;
    ElaText* m_speedLabel = nullptr;
    ElaLCDNumber* m_usedTimeLabel = nullptr;
    ElaLCDNumber* m_remainTimeLabel = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;
    EtaEstimator m_estimator;
    std::chrono::high_resolution_clock::time_point m_startTime;

    // 文件格式输出配置页
    NJCfgPage* m_njCfgPage = nullptr;
    EpubCfgPage* m_epubCfgPage = nullptr;
    PDFCfgPage* m_pdfCfgPage = nullptr;
    CustomFilePluginCfgPage* m_customFilePluginCfgPage = nullptr;

    // 翻译详情页
    TranslationWorkbenchPage* m_translationWorkbenchPage = nullptr;
};

#endif
