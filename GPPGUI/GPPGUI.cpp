#define PYBIND11_HEADERS
#include "../GalTranslPP/GPPMacros.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkProxyFactory>
#include <QSharedMemory>
#include <QTimer>
#include <QTranslator>

#include "ElaApplication.h"
#include "mainwindow.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#include <DbgHelp.h>
#include <Strsafe.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

#include <toml.hpp>

#pragma comment(lib, "GPPVersion.lib")
#pragma comment(lib, "GalTranslPP.lib")
#pragma comment(lib, "ElaWidgetTools.lib")

import Tool;
import PythonManager;
namespace fs = std::filesystem;
namespace py = pybind11;

#ifdef Q_OS_WIN
LONG WINAPI writeMiniDump(EXCEPTION_POINTERS* exceptionPointers) noexcept
{
    wchar_t dumpDir[MAX_PATH]{};
    DWORD pathLength = GetModuleFileNameW(nullptr, dumpDir, MAX_PATH);
    if (pathLength == 0 || pathLength >= MAX_PATH) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    while (pathLength > 0 && dumpDir[pathLength - 1] != L'\\' && dumpDir[pathLength - 1] != L'/') {
        --pathLength;
    }
    if (pathLength == 0) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    dumpDir[pathLength - 1] = L'\0';

    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t dumpPath[MAX_PATH]{};
    if (FAILED(StringCchPrintfW(
        dumpPath,
        MAX_PATH,
        L"%s\\GalTranslPP_GUI_%04u%02u%02u_%02u%02u%02u_%lu.dmp",
        dumpDir,
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        GetCurrentProcessId())))
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    const HANDLE dumpFile = CreateFileW(
        dumpPath,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (dumpFile == INVALID_HANDLE_VALUE) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{
        .ThreadId = GetCurrentThreadId(),
        .ExceptionPointers = exceptionPointers,
        .ClientPointers = FALSE
    };
    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        dumpFile,
        MiniDumpNormal,
        exceptionPointers ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);
    CloseHandle(dumpFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void waitForProcessToExit(qint64 pid) {
#ifdef Q_OS_WIN
    const HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess != nullptr) {
        WaitForSingleObject(hProcess, INFINITE);
        CloseHandle(hProcess);
    }
#else

#endif
}

int main(int argc, char* argv[])
{
    // 公共初始化
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(writeMiniDump);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");
#endif

    // 使用一个唯一的key
    const QString uniqueKey = "{957C27D8-37BB-4F54-9EBE-D0F5C701CBBF}";
    // 使用 QSharedMemory 防止多实例运行
    QSharedMemory sharedMemory(uniqueKey);

    QTranslator baseTranslator;
    QTranslator coreTranslator;
    QTranslator guiTranslator;
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("GalTransl++ GUI");
    QDir::setCurrent(QApplication::applicationDirPath());
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ QStringList{"pid"}, "pid", "pid" });
    parser.process(app);
    if (parser.isSet("pid")) {
        const qint64 pid = parser.value("pid").toLongLong();
        waitForProcessToExit(pid);
    }

    if (fs::exists(L"Updater_new.exe")) {
        try {
            fs::rename(L"Updater_new.exe", L"Updater.exe");
            fs::remove_all(L"new");
        }
        catch (const fs::filesystem_error& e) {
#ifdef Q_OS_WIN
            MessageBoxW(
                nullptr,
                ascii2Wide(std::string_view(e.what())).c_str(),
                gppTr("GPPGUI.GPPGUI", "Updater 更新错误").toStdWString().c_str(),
                MB_ICONERROR);
#endif
        }
    }
    
    try {
        // 依赖配置的初始化
        bool checkUpdate = true;
        bool allowMultiInstance = false;
        QLocalServer server;  // 创建 QLocalServer，用于接收来自新实例的消息
        std::unique_ptr<py::gil_scoped_release> release;

        try {
            const toml::value globalConfig = toml::uparse(globalConfigPath);
            checkUpdate = toml::find_or(globalConfig, "autoCheckUpdate", true);
            const std::string language = toml::find_or(globalConfig, "language", "zh_CN");
            if (language == "zh_CN") {
                if (baseTranslator.load("qt_zh_CN.qm", "translations")) {
                    app.installTranslator(&baseTranslator);
                }
            }
            else {
                if (baseTranslator.load(QString("qt_%1.qm").arg(language), "translations")) {
                    app.installTranslator(&baseTranslator);
                }
                if (coreTranslator.load(QString("qt_gpp_%1.qm").arg(language), "translations")) {
                    app.installTranslator(&coreTranslator);
                }
                if (guiTranslator.load(QString("qt_gppgui_%1.qm").arg(language), "translations")) {
                    app.installTranslator(&guiTranslator);
                }
            }
            allowMultiInstance = toml::find_or(globalConfig, "allowMultiInstance", false);
            if (!allowMultiInstance) {
                // 尝试附加到共享内存。
                // 如果成功，说明已有实例在运行。
                if (sharedMemory.attach()) {
                    // --- 这是第二个实例（客户端）的逻辑 ---
                    // 创建一个本地套接字，用于和第一个实例通信
                    QLocalSocket socket;
                    socket.connectToServer(uniqueKey); // 使用相同的key作为服务器名
                    // 等待连接成功（最多等待500毫秒）
                    if (socket.waitForConnected(500)) {
                        // 发送一个简单的消息，告诉服务器激活窗口
                        socket.write("activate");
                        socket.waitForBytesWritten(500);
                        socket.disconnectFromServer();
                    }
                    // 客户端使命完成，退出
                    return 0;
                }
                // --- 如果程序运行到这里，说明这是第一个实例（服务器） ---
                // 尝试创建共享内存段
                if (!sharedMemory.create(1)) {
#ifdef Q_OS_WIN
                    MessageBoxW(
                        nullptr,
                        gppTr("GPPGUI.GPPGUI", "无法创建共享内存段，程序即将退出。").toStdWString().c_str(),
                        gppTr("GPPGUI.GPPGUI", "错误").toStdWString().c_str(),
                        MB_ICONERROR);
#endif
                    return 1; // 创建失败，退出
                }
            }

            const std::string pythonEnvPathStr = toml::find_or(globalConfig, "pythonEnvPath", "BaseConfig/Python-3.12.10-embed-amd64");
            const fs::path pythonEnvPath = ascii2Wide(pythonEnvPathStr);
            startUpPythonEnv(pythonEnvPath, release);
        }
        catch (...) { }

        

        eApp->init();
        const int fontId = QFontDatabase::addApplicationFont(":/GPPGUI/Resource/fonts/MonaspaceNeon-Regular.otf");
        QFont uiFont = app.font();
        if (fontId >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            if (!families.empty()) {
                uiFont.setFamilies({
                    families.front(),
                    "Microsoft YaHei UI",
                    "Microsoft YaHei",
                    "Segoe UI"
                    });
            }
        }
        app.setFont(uiFont);
        MainWindow w;
        w.show();
        if (checkUpdate) {
            QTimer::singleShot(2000, &w, &MainWindow::checkUpdate);
        }



        if (!allowMultiInstance) {
            // 当有新连接时，触发 newConnection 信号
            QObject::connect(&server, &QLocalServer::newConnection, [&]()
                {
                    // 获取连接
                    QLocalSocket* clientConnection = server.nextPendingConnection();
                    QObject::connect(clientConnection, &QLocalSocket::disconnected,
                        clientConnection, &QLocalSocket::deleteLater);
                    // 当接收到数据时
                    QObject::connect(clientConnection, &QLocalSocket::readyRead, [&]()
                        {
                            const QByteArray data = clientConnection->readAll();
                            if (data == "activate") {
                                if (w.isMinimized()) {
                                    w.setWindowState(w.windowState() & ~Qt::WindowMinimized);
                                }
                                const Qt::WindowFlags flags = w.windowFlags();
                                w.setWindowFlags(flags | Qt::WindowStaysOnTopHint);
                                w.show();
                                w.activateWindow();
                                w.moveToCenter();
                                w.setWindowFlags(flags);
                                w.show();
                            }
                        });
                });
            // 开始监听。如果监听失败，可能是之前的实例崩溃但没释放server name。
            // 我们需要确保在程序退出时正确关闭服务器。
            if (!server.listen(uniqueKey)) {
                // 如果监听失败，可能是因为上次程序异常退出导致 server name 未被释放
                // 尝试移除旧的 server name
                QLocalServer::removeServer(uniqueKey);
                // 再次尝试监听
                if (!server.listen(uniqueKey)) {
#ifdef Q_OS_WIN
                    MessageBoxW(
                        nullptr,
                        gppTr("GPPGUI.GPPGUI", "无法启动本地服务，程序即将退出。").toStdWString().c_str(),
                        gppTr("GPPGUI.GPPGUI", "错误").toStdWString().c_str(),
                        MB_ICONERROR);
#endif
                    return 1;
                }
            }
        }

        const int result = app.exec();
        shutDownPythonEnv(release);

        // 程序退出前，确保服务器关闭
        if (!allowMultiInstance) {
            server.close();
        }
        try {
            fs::remove_all(L"cache");
        }
        catch (const fs::filesystem_error& e) {
#ifdef Q_OS_WIN
            MessageBoxW(
                nullptr,
                ascii2Wide(std::string_view(e.what())).c_str(),
                gppTr("GPPGUI.GPPGUI", "缓存删除错误").toStdWString().c_str(),
                MB_ICONERROR);
#endif
        }
        return result;
    }
    catch (const toml::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(
            nullptr,
            ascii2Wide(std::string_view(e.what())).c_str(),
            gppTr("GPPGUI.GPPGUI", "TOML 错误").toStdWString().c_str(),
            MB_ICONERROR);
#endif
        return 1;
    }
    catch (const std::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(
            nullptr,
            ascii2Wide(std::string_view(e.what())).c_str(),
            gppTr("GPPGUI.GPPGUI", "标准错误").toStdWString().c_str(),
            MB_ICONERROR);
#endif
        return 1;
    }
    catch (...) {
#ifdef Q_OS_WIN
        MessageBoxW(
            nullptr,
            gppTr("GPPGUI.GPPGUI", "遇到了未知的错误，程序即将退出。").toStdWString().c_str(),
            gppTr("GPPGUI.GPPGUI", "错误").toStdWString().c_str(),
            MB_ICONERROR);
#endif
        return 1;
    }

    return 2;
}
