#define PYBIND11_HEADERS
#include "../GalTranslPP/GPPMacros.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkProxyFactory>
#include <QSharedMemory>
#include <QTranslator>

#include "ElaApplication.h"
#include "mainwindow.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include <toml.hpp>

#pragma comment(lib, "GPPVersion.lib")
#pragma comment(lib, "GalTranslPP.lib")
#pragma comment(lib, "ElaWidgetTools.lib")

import Tool;
import PythonManager;
namespace fs = std::filesystem;
namespace py = pybind11;

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
                gppTr("GPPGUI.main", "Updater 更新错误").toStdWString().c_str(),
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
                        gppTr("GPPGUI.main", "无法创建共享内存段，程序即将退出。").toStdWString().c_str(),
                        gppTr("GPPGUI.main", "错误").toStdWString().c_str(),
                        MB_ICONERROR);
#endif
                    return 1; // 创建失败，退出
                }
            }

            const std::string pyEnvPathStr = toml::find_or(globalConfig, "pyEnvPath", "BaseConfig/Python-3.12.10-embed-amd64");
            const fs::path pyEnvPath = ascii2Wide(pyEnvPathStr);
            startUpPythonEnv(pyEnvPath, release);
        }
        catch (...) { }

        

        eApp->init();
        MainWindow w;
        w.show();
        if (checkUpdate) {
            w.checkUpdate();
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
                            QByteArray data = clientConnection->readAll();
                            if (data == "activate") {
                                if (w.isMinimized()) {
                                    w.setWindowState(w.windowState() & ~Qt::WindowMinimized);
                                }
                                Qt::WindowFlags flags = w.windowFlags();
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
                        gppTr("GPPGUI.main", "无法启动本地服务，程序即将退出。").toStdWString().c_str(),
                        gppTr("GPPGUI.main", "错误").toStdWString().c_str(),
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
                gppTr("GPPGUI.main", "缓存删除错误").toStdWString().c_str(),
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
            gppTr("GPPGUI.main", "TOML 错误").toStdWString().c_str(),
            MB_ICONERROR);
#endif
        return 1;
    }
    catch (const std::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(
            nullptr,
            ascii2Wide(std::string_view(e.what())).c_str(),
            gppTr("GPPGUI.main", "标准错误").toStdWString().c_str(),
            MB_ICONERROR);
#endif
        return 1;
    }
    catch (...) {
#ifdef Q_OS_WIN
        MessageBoxW(
            nullptr,
            gppTr("GPPGUI.main", "遇到了未知的错误，程序即将退出。").toStdWString().c_str(),
            gppTr("GPPGUI.main", "错误").toStdWString().c_str(),
            MB_ICONERROR);
#endif
        return 1;
    }

    return 2;
}
