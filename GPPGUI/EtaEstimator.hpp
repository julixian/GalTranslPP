#ifndef ETAESTIMATOR_HPP
#define ETAESTIMATOR_HPP

#include <chrono>
#include <deque>
#include <limits>
#include <utility>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<double>; // 使用 double 类型的秒

class EtaEstimator {
private:
    static constexpr double SpeedWindowSeconds = 60.0;

    struct ProgressEvent {
        TimePoint time;
        double amount;
    };

    std::deque<ProgressEvent> progressEvents;

    void trim(TimePoint now)
    {
        while (!progressEvents.empty() && Duration(now - progressEvents.front().time).count() > SpeedWindowSeconds) {
            progressEvents.pop_front();
        }
    }

public:
    std::pair<double, Duration> updateAndGetSpeedWithEta(double currentProgress, double totalProgress) {
        TimePoint currentTime = Clock::now();
        trim(currentTime);

        double processedInWindow = 0.0;
        for (const ProgressEvent& event : progressEvents) {
            processedInWindow += event.amount;
        }

        const double speed = processedInWindow / SpeedWindowSeconds;
        if (speed <= 1e-9) {
            return std::make_pair(0.0, Duration(std::numeric_limits<double>::infinity()));
        }

        double remainingWork = totalProgress - currentProgress;
        return std::make_pair(speed, Duration(remainingWork / speed));
    }

    std::pair<double, Duration> recordProgressAndGetSpeedWithEta(double progressDelta, double currentProgress, double totalProgress) {
        TimePoint currentTime = Clock::now();
        if (progressDelta > 0.0) {
            progressEvents.push_back({ currentTime, progressDelta });
        }
        trim(currentTime);
        return updateAndGetSpeedWithEta(currentProgress, totalProgress);
    }

    void reset()
    {
        progressEvents.clear();
    }
};

#endif // ETAESTIMATOR_HPP
