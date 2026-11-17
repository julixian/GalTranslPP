#include "TaskbarController.h"

#include <QWidget>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <format>
#elif defined(__APPLE__)
#include <QGuiApplication>
#endif

#ifdef _WIN32
struct TaskbarController::Impl
{
    ITaskbarList3* taskbarList = nullptr;
    UINT taskbarCreatedMsg = 0;
};
#endif

TaskbarController::TaskbarController(QWidget* window)
    : m_window(window)
{
#ifdef _WIN32
    m_impl = new Impl;
    m_impl->taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarButtonCreated");
#endif
}

TaskbarController::~TaskbarController()
{
#ifdef _WIN32
    releaseTaskbarList();
    delete m_impl;
    m_impl = nullptr;
#endif
}

#ifdef _WIN32
bool TaskbarController::ensureTaskbarList()
{
    if (!m_impl || !m_window || !m_window->testAttribute(Qt::WA_WState_Created))
    {
        return false;
    }
    if (m_impl->taskbarList)
    {
        return true;
    }
    ITaskbarList3* taskbarList = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbarList))))
    {
        return false;
    }
    if (FAILED(taskbarList->HrInit()))
    {
        taskbarList->Release();
        return false;
    }
    m_impl->taskbarList = taskbarList;
    return true;
}

void TaskbarController::releaseTaskbarList()
{
    if (m_impl && m_impl->taskbarList)
    {
        m_impl->taskbarList->Release();
        m_impl->taskbarList = nullptr;
    }
}

void TaskbarController::applyProgress(bool force)
{
    if (!m_window || !m_window->testAttribute(Qt::WA_WState_Created))
    {
        return;
    }
    if (force && m_state == ProgressState::NoProgress)
    {
        return;
    }
    if (!ensureTaskbarList())
    {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (!hwnd)
    {
        return;
    }
    switch (m_state)
    {
    case ProgressState::NoProgress:
        m_impl->taskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
        break;
    case ProgressState::Indeterminate:
        m_impl->taskbarList->SetProgressState(hwnd, TBPF_INDETERMINATE);
        break;
    case ProgressState::Normal:
        m_impl->taskbarList->SetProgressState(hwnd, TBPF_NORMAL);
        m_impl->taskbarList->SetProgressValue(hwnd, m_completed, std::max(1ULL, m_total));
        break;
    }
}

void TaskbarController::applyBadge(bool force)
{
    if (!m_window || !m_window->testAttribute(Qt::WA_WState_Created))
    {
        return;
    }
    if (!force && m_badgeCount == m_lastAppliedBadgeCount)
    {
        return;
    }
    if (force && m_badgeCount <= 0 && m_lastAppliedBadgeCount <= 0)
    {
        return;
    }
    if (!ensureTaskbarList())
    {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (!hwnd)
    {
        return;
    }
    if (m_badgeCount <= 0)
    {
        if (SUCCEEDED(m_impl->taskbarList->SetOverlayIcon(hwnd, nullptr, L"")))
        {
            m_lastAppliedBadgeCount = 0;
        }
        return;
    }

    const int w = std::max(16, GetSystemMetrics(SM_CXSMICON));
    const int h = std::max(16, GetSystemMetrics(SM_CYSMICON));
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter painter(&img);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setBrush(QColor(0x20, 0x22, 0x25));
    painter.setPen(QPen(QColor(255, 255, 255, 65), 1.0));
    painter.drawEllipse(QRectF(0.5, 0.5, w - 1.0, h - 1.0));

    const QString label = m_badgeCount > 99 ? QStringLiteral("99+") : QString::number(m_badgeCount);
    QFont font(QStringLiteral("Segoe UI"));
    font.setBold(true);
    const qreal scale = label.size() == 1 ? 0.68 : label.size() == 2 ? 0.54 : 0.44;
    font.setPixelSize(std::max(7, qRound(h * scale)));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0.0, 0.0, static_cast<qreal>(w), static_cast<qreal>(h)), Qt::AlignCenter, label);
    painter.end();

    HICON hIcon = img.toHICON();
    if (!hIcon)
    {
        return;
    }
    const std::wstring desc = std::format(L"{}", m_badgeCount);
    if (SUCCEEDED(m_impl->taskbarList->SetOverlayIcon(hwnd, hIcon, desc.c_str())))
    {
        m_lastAppliedBadgeCount = m_badgeCount;
    }
    DestroyIcon(hIcon);
}
#endif

void TaskbarController::handleNativeEvent(void* message)
{
#ifdef _WIN32
    if (!m_impl || !message || m_impl->taskbarCreatedMsg == 0)
    {
        return;
    }
    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->message == m_impl->taskbarCreatedMsg)
    {
        releaseTaskbarList();
        if (ensureTaskbarList())
        {
            applyProgress(true);
            applyBadge(true);
        }
    }
#else
    (void)message;
#endif
}

void TaskbarController::setProgress(ProgressState state, unsigned long long completed, unsigned long long total)
{
    int nextPermille = -1;
    if (state == ProgressState::Normal)
    {
        if (total == 0)
        {
            state = ProgressState::Indeterminate;
            completed = 0;
        }
        else
        {
            completed = std::min(completed, total);
            nextPermille = static_cast<int>((completed * 1000ULL) / total);
        }
    }
    else
    {
        completed = 0;
        total = 0;
    }

    const bool boundary = state == ProgressState::Normal && (completed == 0 || completed == total) &&
        (completed != m_completed || total != m_total);
    if (!boundary && state == m_state && nextPermille == m_lastPermille)
    {
        return;
    }

    m_state = state;
    m_completed = completed;
    m_total = total;
    m_lastPermille = nextPermille;

#ifdef _WIN32
    applyProgress(false);
#endif
}

void TaskbarController::setBadgeNumber(int count)
{
    const int normalized = std::max(0, count);
    if (normalized == m_badgeCount && normalized == m_lastAppliedBadgeCount)
    {
        return;
    }
    m_badgeCount = normalized;
#ifdef _WIN32
    applyBadge(false);
#elif defined(__APPLE__)
    if (normalized != m_lastAppliedBadgeCount && qGuiApp)
    {
        qGuiApp->setBadgeNumber(normalized);
        m_lastAppliedBadgeCount = normalized;
    }
#endif
}
