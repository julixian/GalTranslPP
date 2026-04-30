
#include <toml.hpp>
#include <QCommandLineParser>
#include <QApplication>
#include <QProcess>
#include <QDir>


#pragma comment(lib, "GalTranslPP.lib")

#ifdef Q_OS_WIN
#include <windows.h>
#pragma comment(lib, "User32.lib")
#endif


import Tool;
namespace fs = std::filesystem;

void waitForProcessToExit(qint64 pid) {
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess != nullptr) {
        WaitForSingleObject(hProcess, INFINITE);
        CloseHandle(hProcess);
    }
#else

#endif
}



int main(int argc, char* argv[]) {

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("GalTransl++ Updater");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ {"p", "pid"}, "Process ID of the main application.", "pid" });
    parser.addOption({ {"s", "source"}, "Source path of the update package.", "source" });
    parser.addOption({ {"t", "target"}, "Target directory for installation.", "target" });
    parser.addOption({ {"r", "restart"}, "Restart the main application after update is installed.", "restart" });
    parser.addOption({ {"n", "newActionFlag"}, "Flag to indicate a new action.", "newActionFlag" });
    parser.addOption({ QStringList{"gppVersion"}, "Version of GalTranslPP.", "gppVersion" });
    parser.addOption({ QStringList{"pythonVersion"}, "Version of Python.", "pythonVersion" });
    parser.addOption({ QStringList{"promptVersion"}, "Version of Prompt.", "promptVersion" });
    parser.addOption({ QStringList{"dictVersion"}, "Version of dictionary.", "dictVersion" });
    parser.addOption({ QStringList{"qtVersion"}, "Version of QT.", "qtVersion" });
    parser.addOption({ QStringList{"icuVersion"}, "Version of ICU.", "icuVersion" });
    parser.process(a);

    qint64 pid = parser.value("pid").toLongLong();
    QString sourceZip = parser.value("source");
    QString targetDir = parser.value("target");

    if (pid == 0 || sourceZip.isEmpty() || targetDir.isEmpty()) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, L"Invalid arguments provided.", L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    if (parser.isSet("newActionFlag")) {
        std::string orgGppVersion = parser.isSet("gppVersion") ? parser.value("gppVersion").toStdString() : "1.0.0";
        std::string orgPythonVersion = parser.isSet("pythonVersion") ? parser.value("pythonVersion").toStdString() : "1.0.0";
        std::string orgPromptVersion = parser.isSet("promptVersion") ? parser.value("promptVersion").toStdString() : "1.0.0";
        std::string orgDictVersion = parser.isSet("dictVersion") ? parser.value("dictVersion").toStdString() : "1.0.0";
        std::string orgQtVersion = parser.isSet("qtVersion") ? parser.value("qtVersion").toStdString() : "6.9.2";
        std::string orgIcuVersion = parser.isSet("icuVersion") ? parser.value("icuVersion").toStdString() : "7.4.0";

        waitForProcessToExit(pid);

        try {
            bool isCompatible;

            std::set<std::string> excludePreFixes =
            {
                "BaseConfig/pyScripts", "BaseConfig/Prompt.toml", 
                "BaseConfig/Dict", 
            };
            
            if (cmpVer(PYTHONVERSION, orgPythonVersion, isCompatible)) {
                excludePreFixes.erase("BaseConfig/pyScripts");
            }
            if (cmpVer(PROMPTVERSION, orgPromptVersion, isCompatible)) {
                if (!isCompatible) {
#ifdef Q_OS_WIN
                    MessageBoxW(nullptr, L"由于提示词解析方式发生不兼容变更，本次更新将强制覆盖原默认提示词。\n"
                        L"你可以先行备份，然后点击确定以继续更新。", L"GalTransl++ Updater", MB_OK | MB_TOPMOST);
#endif
                    excludePreFixes.erase("BaseConfig/Prompt.toml");
                }
                else {
#ifdef Q_OS_WIN
                    int ret = MessageBoxW(nullptr, L"检测到新版本的 Prompt，是否更新 Prompt (会覆盖当前的默认提示词)？", L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                    if (ret == IDYES) {
                        excludePreFixes.erase("BaseConfig/Prompt.toml");
                    }
#endif
                }
            }
            if (cmpVer(DICTVERSION, orgDictVersion, isCompatible)) {
#ifdef Q_OS_WIN
                int ret = MessageBoxW(nullptr, L"检测到新版本的 GPT 字典，是否更新字典（会覆盖当前的默认字典）？", L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                if (ret == IDYES) {
                    excludePreFixes.erase("BaseConfig/Dict");
                }
#endif
            }
            if (cmpVer(ICUVERSION, orgIcuVersion, isCompatible)) {
                try {
                    const std::vector<std::string> orgVerStrVec = splitString(orgIcuVersion, '.');
                    const std::wstring orgVerStr = ascii2Wide(orgVerStrVec.at(0) + orgVerStrVec.at(1));
                    const std::vector<std::wstring> icuDlls = { L"icudt", L"icuin", L"icuuc" };
                    for (const auto& icuDll : icuDlls) {
#ifdef Q_OS_WIN
                        const fs::path dllPath = L"../" + icuDll + orgVerStr + L".dll";
#endif
                        if (fs::exists(dllPath)) {
                            fs::remove(dllPath);
                        }
                    }
                }
                catch(...) { }
            }

            {
                const std::vector<std::string> transformedExcludePreFixes = excludePreFixes | std::views::transform([](const auto& s)
                    {
                        return replaceStr(s, "/", "\\");
                    }) | std::ranges::to<std::vector>();
                excludePreFixes.insert_range(transformedExcludePreFixes);
            }
            extractZipExclude(sourceZip.toStdWString(), targetDir.toStdWString(), excludePreFixes);

#ifdef Q_OS_WIN
            MessageBoxW(nullptr, L"GalTransl++ 更新成功", L"成功", MB_OK | MB_TOPMOST);
#endif
            fs::remove(sourceZip.toStdWString());
            if (parser.isSet("restart")) {
                QStringList args;
                args << "--pid" << QString::number(QApplication::applicationPid());
                QProcess::startDetached(parser.value("restart"), args, targetDir);
            }
        }
        catch (const std::exception&) {
#ifdef Q_OS_WIN
            MessageBoxW(nullptr, L"Failed to extract update package.", L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
            return -1;
        }
        return 0;
    }

    // 1. 等待主程序退出
    waitForProcessToExit(pid);

    // 2. 解压并覆盖文件
    try {
        fs::create_directories(targetDir.toStdWString() + L"/new");
        extractFileFromZip(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new", "Updater_new.exe");
        extractFileFromZip(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new", "Qt6Core.dll");
        extractFileFromZip(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new", "7z.dll");
        QStringList arguments;
        arguments << "--newActionFlag" << QString::number(QApplication::applicationPid());
        arguments << "--pid" << QString::number(QApplication::applicationPid());
        arguments << "--source" << sourceZip << "--target" << targetDir;
        arguments << "--gppVersion" << QString::fromStdString(GPPVERSION);
        arguments << "--pythonVersion" << QString::fromStdString(PYTHONVERSION);
        arguments << "--promptVersion" << QString::fromStdString(PROMPTVERSION);
        arguments << "--dictVersion" << QString::fromStdString(DICTVERSION);
        arguments << "--qtVersion" << QString::fromStdString(QTVERSION);
        arguments << "--icuVersion" << QString::fromStdString(ICUVERSION);
        if (parser.isSet("restart")) {
            arguments << "--restart" << parser.value("restart");
        }
        QProcess::startDetached("new/Updater_new.exe", arguments, QString(targetDir.toStdWString() + L"/new"));
    }
    catch (const std::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, std::format(L"Failed to extract Updater_new.exe.\nError: {}", ascii2Wide(std::string_view(e.what()))).c_str(),
            L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    return 0;
}
