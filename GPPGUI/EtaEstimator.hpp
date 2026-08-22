#ifndef ETAESTIMATOR_HPP
#define ETAESTIMATOR_HPP

#include <chrono>
#include <deque>
#include <limits>
#include <utility>

class EtaEstimator {
private:
    static constexpr double kSpeedWindowSeconds = 120.0;

    struct ProgressEvent {
        std::chrono::steady_clock::time_point time;
        double amount;
    };

    std::deque<ProgressEvent> m_progressEvents;

    void trimProgressEvents(std::chrono::steady_clock::time_point now)
    {
        while (!m_progressEvents.empty() &&
            std::chrono::duration<double>(now - m_progressEvents.front().time).count() > kSpeedWindowSeconds) {
            m_progressEvents.pop_front();
        }
    }

public:
    std::pair<double, std::chrono::duration<double>> updateAndGetSpeedWithEta(double currentProgress, double totalProgress) {
        const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        trimProgressEvents(currentTime);

        double processedInWindow = 0.0;
        for (const ProgressEvent& event : m_progressEvents) {
            processedInWindow += event.amount;
        }

        const double speed = processedInWindow / kSpeedWindowSeconds;
        if (speed <= 1e-9) {
            return std::make_pair(0.0, std::chrono::duration<double>(std::numeric_limits<double>::infinity()));
        }

        const double remainingWork = totalProgress - currentProgress;
        return std::make_pair(speed, std::chrono::duration<double>(remainingWork / speed));
    }

    std::pair<double, std::chrono::duration<double>> recordProgressAndGetSpeedWithEta(
        double progressDelta, double currentProgress, double totalProgress) {
        const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        if (progressDelta > 0.0) {
            m_progressEvents.push_back({ currentTime, progressDelta });
        }
        trimProgressEvents(currentTime);
        return updateAndGetSpeedWithEta(currentProgress, totalProgress);
    }

    void reset()
    {
        m_progressEvents.clear();
    }
};

#endif
