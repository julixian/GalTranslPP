// StartSettingsPage.h

#ifndef STARTSETTINGSPAGE_H
#define STARTSETTINGSPAGE_H

#include <QThread>
#include <toml++/toml.hpp>
#include <filesystem>
#include <chrono>
#include "BasePage.h"
#include "TranslatorWorker.h"

namespace fs = std::filesystem;

class ElaPushButton;
class ElaProgressBar;
class ElaComboBox;
class ElaProgressRing;
class ElaLCDNumber;

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<double>; // 使用 double 类型的秒

class ExponentialMovingAverageEstimator {
private:
    double alpha;
    double avgSpeed;
    TimePoint lastTime;
    double lastProgress;
    bool isFirstUpdate;

public:
    explicit ExponentialMovingAverageEstimator(double smoothingFactor = 0.1)
        : alpha(smoothingFactor), avgSpeed(0.0), lastProgress(0.0), isFirstUpdate(true) {
    }

    Duration updateAndGetEta(double currentProgress, double totalProgress) {
        TimePoint currentTime = Clock::now();

        if (isFirstUpdate) {
            lastTime = currentTime;
            lastProgress = currentProgress;
            isFirstUpdate = false;
            return Duration(std::numeric_limits<double>::infinity());
        }

        Duration deltaTime = currentTime - lastTime;
        double deltaProgress = currentProgress - lastProgress;

        if (deltaTime.count() > 1e-9) { // 避免除以零
            double currentSpeed = deltaProgress / deltaTime.count();
            if (avgSpeed == 0.0) { // 第一次有效计算
                avgSpeed = currentSpeed;
            }
            else {
                avgSpeed = (alpha * currentSpeed) + ((1.0 - alpha) * avgSpeed);
            }
        }

        lastTime = currentTime;
        lastProgress = currentProgress;

        if (avgSpeed <= 1e-9) {
            return Duration(std::numeric_limits<double>::infinity());
        }

        double remainingWork = totalProgress - currentProgress;
        return Duration(remainingWork / avgSpeed);
    }

    void reset()
    {
        avgSpeed = 0.0;
        lastProgress = 0.0;
        isFirstUpdate = true;
    }
};


class StartSettingsPage : public BasePage
{
    Q_OBJECT

public:
    explicit StartSettingsPage(QWidget* mainWindow, fs::path& projectDir, toml::table& projectConfig, QWidget* parent = nullptr);
    ~StartSettingsPage() override;
    void apply2Config();

Q_SIGNALS:
    void startTranslating();  // 让projectSettings去保存配置
    void finishTranslating(const QString& transEngine, int exitCode); // 这两个向projectSettings页发送
    void startWork();
    void stopWork();  // 这两个向worker发送

private:

    fs::path& _projectDir;
    QThread* _workThread;
    TranslatorWorker* _worker;

    void _setupUI();
    toml::table& _projectConfig;
    QWidget* _mainWindow;

    ElaPushButton* _startTranslateButton;
    ElaPushButton* _stopTranslateButton;
    ElaProgressBar* _progressBar;

    ElaComboBox* _fileFormatComboBox;

    QString _transEngine;
    ElaProgressRing* _threadNumRing;
    ElaLCDNumber* _remainTimeLabel;
    ExponentialMovingAverageEstimator _estimator;

    std::chrono::high_resolution_clock::time_point _startTime;

private Q_SLOTS:

    void _onStartTranslatingClicked();
    void _onStopTranslatingClicked();

    void _workFinished(int exitCode); // worker结束了的信号

    void _onOutputSettingClicked();
};

#endif // STARTSETTINGSPAGE_H
