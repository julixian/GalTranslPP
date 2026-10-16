#pragma once

#include <QCoreApplication>
#include <QThread>
#include <QString>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <signal.h>
#include <errno.h>
#endif

enum class GppMessageBoxType { Information, Error, Question };

// 启动和更新阶段还没有主窗口，使用系统原生对话框。
inline bool gppMessageBox(const QString& message, const QString& title,
                         GppMessageBoxType type = GppMessageBoxType::Error)
{
#ifdef _WIN32
    const UINT flags = type == GppMessageBoxType::Question ? MB_YESNO | MB_ICONQUESTION
        : type == GppMessageBoxType::Error ? MB_OK | MB_ICONERROR : MB_OK;
    const int result = MessageBoxW(nullptr, (const wchar_t*)message.utf16(),
        (const wchar_t*)title.utf16(), flags | MB_SETFOREGROUND);
    return result == (type == GppMessageBoxType::Question ? IDYES : IDOK);
#elif defined(__APPLE__)
    const auto text = CFStringCreateWithCharacters(nullptr, (const UniChar*)message.utf16(), message.size());
    const auto heading = CFStringCreateWithCharacters(nullptr, (const UniChar*)title.utf16(), title.size());
    const bool question = type == GppMessageBoxType::Question;
    CFOptionFlags response = 0;
    const auto level = type == GppMessageBoxType::Error ? kCFUserNotificationStopAlertLevel
        : kCFUserNotificationNoteAlertLevel;
    const auto status = CFUserNotificationDisplayAlert(0, level, nullptr, nullptr, nullptr,
        heading, text, question ? CFSTR("Yes") : nullptr, question ? CFSTR("No") : nullptr,
        nullptr, &response);
    CFRelease(text);
    CFRelease(heading);
    return status == 0 && (response & 3) == kCFUserNotificationDefaultResponse;
#endif
}

inline void gppWaitForProcess(qint64 pid)
{
    if (pid <= 0) return;
#ifdef _WIN32
    const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    if (process) {
        WaitForSingleObject(process, INFINITE);
        CloseHandle(process);
    }
#elif defined(__APPLE__)
    // 更新器与 GUI 不是父子等待关系，不能用 waitpid 等待对方。
    while (kill((pid_t)pid, 0) == 0 || errno == EPERM) QThread::msleep(50);
#endif
}

#ifdef _WIN32
inline constexpr auto gppUpdatePackage = "GUICORE.7z";
inline constexpr auto gppUpdaterExecutable = "Updater.exe";
inline constexpr auto gppNewUpdaterExecutable = "Updater_new.exe";
inline constexpr auto gppArchiveLibrary = "7z.dll";
#elif defined(__APPLE__)
inline constexpr auto gppUpdatePackage = "GUICORE_MACOS_ARM64.7z";
inline constexpr auto gppUpdaterExecutable = "Updater";
inline constexpr auto gppNewUpdaterExecutable = "Updater_new";
inline constexpr auto gppArchiveLibrary = "7z.so";
#endif
