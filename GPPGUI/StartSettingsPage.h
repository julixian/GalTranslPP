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
    double avg_speed;
    TimePoint last_time;
    double last_progress;
    bool is_first_update;

public:
    explicit ExponentialMovingAverageEstimator(double smoothing_factor = 0.1)
        : alpha(smoothing_factor), avg_speed(0.0), last_progress(0.0), is_first_update(true) {
    }

    Duration updateAndGetEta(double current_progress, double total_progress) {
        TimePoint current_time = Clock::now();

        if (is_first_update) {
            last_time = current_time;
            last_progress = current_progress;
            is_first_update = false;
            return Duration(std::numeric_limits<double>::infinity());
        }

        Duration delta_time = current_time - last_time;
        double delta_progress = current_progress - last_progress;

        if (delta_time.count() > 1e-9) { // 避免除以零
            double current_speed = delta_progress / delta_time.count();
            if (avg_speed == 0.0) { // 第一次有效计算
                avg_speed = current_speed;
            }
            else {
                avg_speed = (alpha * current_speed) + ((1.0 - alpha) * avg_speed);
            }
        }

        last_time = current_time;
        last_progress = current_progress;

        if (avg_speed <= 1e-9) {
            return Duration(std::numeric_limits<double>::infinity());
        }

        double remaining_work = total_progress - current_progress;
        return Duration(remaining_work / avg_speed);
    }

    void reset()
    {
        avg_speed = 0.0;
        last_progress = 0.0;
        is_first_update = true;
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
