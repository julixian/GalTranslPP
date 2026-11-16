#pragma once

class QWidget;

class TaskbarController
{
public:
    enum class ProgressState
    {
        NoProgress,
        Indeterminate,
        Normal
    };

    explicit TaskbarController(QWidget* window);
    ~TaskbarController();

    TaskbarController(const TaskbarController&) = delete;
    TaskbarController& operator=(const TaskbarController&) = delete;

    void handleNativeEvent(void* message);
    void setProgress(ProgressState state, unsigned long long completed = 0, unsigned long long total = 0);
    void setBadgeNumber(int count);

private:
    QWidget* m_window = nullptr;
    ProgressState m_state = ProgressState::NoProgress;
    unsigned long long m_completed = 0;
    unsigned long long m_total = 0;
    int m_lastPermille = -1;
    int m_badgeCount = 0;
    int m_lastAppliedBadgeCount = 0;

#ifdef _WIN32
    struct Impl;
    Impl* m_impl = nullptr;
    bool ensureTaskbarList();
    void releaseTaskbarList();
    void applyProgress(bool force);
    void applyBadge(bool force);
#endif
};
