#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

#include <fstream>
#include <array>
#include <filesystem>

inline void appendStartupTimingLog(const QString& message)
{
    static QMutex mutex;
    static std::ofstream logFile;
    static bool initialized = false;
    const QMutexLocker locker(&mutex);
    if (!initialized) {
        const std::array<std::filesystem::path, 3> candidates{
            std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()) / L"startup_timing.log",
            std::filesystem::path(QDir::currentPath().toStdWString()) / L"startup_timing.log",
            std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::TempLocation).toStdWString()) / L"GalTranslPP_startup_timing.log"
        };
        for (const std::filesystem::path& fileName : candidates) {
            logFile.open(fileName, std::ios::out | std::ios::app | std::ios::binary);
            if (logFile.is_open()) {
                logFile << "[StartupTiming] log file: " << fileName.string() << "\r\n";
                logFile.flush();
                qInfo().noquote() << QStringLiteral("[StartupTiming] 日志文件路径：%1")
                    .arg(QString::fromStdWString(fileName.wstring()));
                break;
            }
        }
        if (!logFile.is_open()) {
            qWarning() << "[StartupTiming] unable to create log file";
        }
        initialized = true;
    }
    if (logFile.is_open()) {
        logFile << message.toUtf8().constData() << "\r\n";
        logFile.flush();
    }
}
