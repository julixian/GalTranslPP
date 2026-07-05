#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QProcess>
#include <QTranslator>

#ifdef Q_OS_WIN
#include <windows.h>
#pragma comment(lib, "User32.lib")
#endif

#define BIT7Z_AUTO_FORMAT
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfileextractor.hpp>

#include <boost/algorithm/string.hpp>
#include <toml.hpp>

import GPPVersion;

namespace fs = std::filesystem;

#pragma comment(lib, "GPPVersion.lib")

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

QString gppTr(const char* context, const char* source) {
    return QCoreApplication::translate(context, source);
}

std::string wide2Ascii(const std::wstring& wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
std::string wide2Ascii(const wchar_t* wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
    return wide2Ascii(std::wstring_view(wide), codePage, usedDefaultChar);
}
template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
std::string wide2Ascii(T&& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
#ifdef _WIN32
    return wide2Ascii(path.native(), codePage, usedDefaultChar);
#else
    return wide2Ascii(path.wstring(), codePage, usedDefaultChar);
#endif
}
std::wstring ascii2Wide(const std::string& ascii, UINT codePage = CP_UTF8);
std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_UTF8);

#ifdef _WIN32
std::string wide2Ascii(const std::wstring& wide, UINT codePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::string wide2Ascii(std::wstring_view wide, UINT codePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT codePage) {
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), wide.data(), len);
    return wide;
}

std::wstring ascii2Wide(std::string_view ascii, UINT codePage) {
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), wide.data(), len);
    return wide;
}
#endif

std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep) {
    return boost::replace_all_copy(str, org, rep);
}

std::optional<int> str2Int(std::string_view sv) {
    int value = 0;
    // 注意：from_chars 不会跳过前导空格！如果需要，得自己 trim 一下
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    // ec == std::errc() 表示解析动作成功
    // ptr == sv.data() + sv.size() 表示整个字符串都被消耗完了（没有剩余垃圾字符）
    if (ec == std::errc() && ptr == sv.data() + sv.size()) {
        return value;
    }
    return std::nullopt;
}

auto splitStringImpl(auto&& str, auto&& delimiter) -> decltype(auto)
{
    std::vector<std::remove_cvref_t<decltype(str)>> result;
    for (auto&& subStrView : str | std::views::split(delimiter)) {
        result.emplace_back(subStrView.begin(), subStrView.end());
    }
    return result;
}
std::vector<std::string> splitString(const std::string& str, char delimiter) { return splitStringImpl(str, delimiter); }
std::vector<std::string> splitString(const std::string& str, std::string_view delimiter) { return splitStringImpl(str, delimiter); }
std::vector<std::string_view> splitStringView(std::string_view strv, char delimiter) { return splitStringImpl(strv, delimiter); }
std::vector<std::string_view> splitStringView(std::string_view strv, std::string_view delimiter) { return splitStringImpl(strv, delimiter); }

bool cmpVer(const std::string& latestVer, const std::string& currentVer, bool& isCompatible)
{
    bool isCurrentVerPre = false;
    auto removePostfix = [&](std::string v) -> std::string
        {
            while (true) {
                if (
                    !v.empty() &&
                    (v.back() < '0' || v.back() > '9')
                    ) {
                    v.pop_back();
                    isCurrentVerPre = true;
                }
                else {
                    break;
                }
            }
            return v;
        };

    const std::string fixedCurrentVer = removePostfix(currentVer);
    const std::string v1s = latestVer.find_last_of("v") == std::string::npos ? latestVer : latestVer.substr(latestVer.find_last_of("v") + 1);
    const std::string v2s = fixedCurrentVer.find_last_of("v") == std::string::npos ? fixedCurrentVer : fixedCurrentVer.substr(fixedCurrentVer.find_last_of("v") + 1);

    const std::vector<std::string> latestVerParts = splitString(v1s, '.');
    const std::vector<std::string> currentVerParts = splitString(v2s, '.');

    const size_t len = std::max(latestVerParts.size(), currentVerParts.size());

    for (size_t i = 0; i < len; i++) {
        const int latestVerPart = i < latestVerParts.size() ? str2Int(latestVerParts[i]).value_or(0) : 0;
        const int currentVerPart = i < currentVerParts.size() ? str2Int(currentVerParts[i]).value_or(0) : 0;
        if (i == 0) {
            isCompatible = latestVerPart <= currentVerPart;
        }

        if (latestVerPart > currentVerPart) {
            return true;
        }
        else if (latestVerPart < currentVerPart) {
            return false;
        }
    }

    if (isCurrentVerPre) {
        return true;
    }

    return false;
}

void extractZipInclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& includePrefixes) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractIf(wide2Ascii(zipPath), wide2Ascii(outputDir), [&](const bit7z::BitArchiveItem& item)
        {
            if (const std::string genericPath = replaceStr(item.path(), "\\", "/");
                std::ranges::any_of(includePrefixes, [&](const std::string& prefix)
                    {
                        return genericPath.starts_with(prefix);
                    })
                )
            {
                return bit7z::FilterResult::ProcessItem;
            }
            return bit7z::FilterResult::SkipItem;
        });
}

void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractIf(wide2Ascii(zipPath), wide2Ascii(outputDir), [&](const bit7z::BitArchiveItem& item)
        {
            if (const std::string genericPath = replaceStr(item.path(), "\\", "/");
                std::ranges::any_of(excludePrefixes, [&](const std::string& prefix)
	                {
		                return genericPath.starts_with(prefix);
	                })
                )
            {
                return bit7z::FilterResult::SkipItem;
            }
            return bit7z::FilterResult::ProcessItem;
        });
}



int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");
#endif

    QTranslator baseTranslator;
    QTranslator updaterTranslator;
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("GalTransl++ Updater");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ QStringList{"pid"}, "Process ID of the main application.", "pid" });
    parser.addOption({ QStringList{"source"}, "Source path of the update package.", "source" });
    parser.addOption({ QStringList{"target"}, "Target directory for installation.", "target" });
    parser.addOption({ QStringList{"restart"}, "Restart the main application after update is installed.", "restart" });
    parser.addOption({ QStringList{"newActionFlag"}, "Flag to indicate a new action.", "newActionFlag" });
    parser.addOption({ QStringList{"gppVersion"}, "Version of GalTranslPP.", "gppVersion" });
    parser.addOption({ QStringList{"pythonVersion"}, "Version of Python.", "pythonVersion" });
    parser.addOption({ QStringList{"promptVersion"}, "Version of Prompt.", "promptVersion" });
    parser.addOption({ QStringList{"dictVersion"}, "Version of dictionary.", "dictVersion" });
    parser.addOption({ QStringList{"qtVersion"}, "Version of QT.", "qtVersion" });
    parser.addOption({ QStringList{"icuVersion"}, "Version of ICU.", "icuVersion" });
    parser.process(app);

    const qint64 pid = parser.value("pid").toLongLong();
    const QString sourceZip = parser.value("source");
    const QString targetDir = parser.value("target");

    if (pid != 0) {
        waitForProcessToExit(pid);
    }

    try {
        const fs::path globalConfigPath = parser.isSet("newActionFlag")
            ? L"../BaseConfig/globalConfig.toml"
            : L"BaseConfig/globalConfig.toml";
        std::ifstream ifs(globalConfigPath, std::ios::binary);
        const auto configData = toml::parse(ifs, wide2Ascii(globalConfigPath));
        const std::string language = toml::find_or(configData, "language", "zh_CN");
        if (language == "zh_CN") {
            if (baseTranslator.load("qt_zh_CN.qm", "translations")) {
                app.installTranslator(&baseTranslator);
            }
        }
        else {
            if (baseTranslator.load(QString("qt_%1.qm").arg(language), "translations")) {
                app.installTranslator(&baseTranslator);
            }
            if (updaterTranslator.load(QString("qt_gppupdater_%1.qm").arg(language), "translations")) {
                app.installTranslator(&updaterTranslator);
            }
        }
    }
    catch (...) { }

    if (pid == 0 || sourceZip.isEmpty() || targetDir.isEmpty()) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, gppTr("Updater.main", "非法参数").toStdWString().c_str(),
            gppTr("Updater.main", "GalTransl++ Updater 内部错误").toStdWString().c_str(),
            MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }


    // Updater 启动时路径
    if (parser.isSet("newActionFlag")) {
        const std::string orgGppVersion = parser.isSet("gppVersion") ? parser.value("gppVersion").toStdString() : "1.0.0";
        const std::string orgPythonVersion = parser.isSet("pythonVersion") ? parser.value("pythonVersion").toStdString() : "1.0.0";
        const std::string orgPromptVersion = parser.isSet("promptVersion") ? parser.value("promptVersion").toStdString() : "1.0.0";
        const std::string orgDictVersion = parser.isSet("dictVersion") ? parser.value("dictVersion").toStdString() : "1.0.0";
        const std::string orgQtVersion = parser.isSet("qtVersion") ? parser.value("qtVersion").toStdString() : "6.5.3";
        const std::string orgIcuVersion = parser.isSet("icuVersion") ? parser.value("icuVersion").toStdString() : "7.4.0";

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
                    MessageBoxW(nullptr, gppTr("Updater.main",
                        "由于提示词解析方式发生不兼容变更，本次更新将强制覆盖原默认提示词。\n"
                        "你可以先行备份，然后点击确定以继续更新。").toStdWString().c_str(),
                        L"GalTransl++ Updater", MB_OK | MB_TOPMOST);
#endif
                    excludePreFixes.erase("BaseConfig/Prompt.toml");
                }
                else {
#ifdef Q_OS_WIN
                    const int ret = MessageBoxW(nullptr, gppTr("Updater.main",
                        "检测到新版本的 Prompt，是否更新 Prompt (会覆盖当前的默认提示词)？").toStdWString().c_str(),
                        L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                    if (ret == IDYES) {
                        excludePreFixes.erase("BaseConfig/Prompt.toml");
                    }
#endif
                }
            }
            if (cmpVer(DICTVERSION, orgDictVersion, isCompatible)) {
#ifdef Q_OS_WIN
                const int ret = MessageBoxW(nullptr, gppTr("Updater.main",
                    "检测到新版本的 GPT 字典，是否更新字典（会覆盖当前的默认字典）？").toStdWString().c_str(),
                    L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
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

            extractZipExclude(sourceZip.toStdWString(), targetDir.toStdWString(), excludePreFixes);

#ifdef Q_OS_WIN
            MessageBoxW(nullptr, gppTr("Updater.main",
                "GalTransl++ 更新成功").toStdWString().c_str(),
                L"GalTransl++ Updater", MB_OK | MB_TOPMOST);
#endif
            fs::remove(sourceZip.toStdWString());
            if (parser.isSet("restart")) {
                QStringList args;
                args << "--pid" << QString::number(QApplication::applicationPid());
                QProcess::startDetached(parser.value("restart"), args, targetDir);
            }
        }
        catch (const std::exception& e) {
#ifdef Q_OS_WIN
            MessageBoxW(nullptr, gppTr("Updater.main",
                "GalTransl++ 更新包解压失败。\n错误: %1").arg(e.what()).toStdWString().c_str(),
                L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
            return -1;
        }
        return 0;
    }


    // 主程序启动时路径
    try {
        extractZipInclude(sourceZip.toStdWString(), targetDir.toStdWString() + L"/new",
            { "Updater_new.exe", "Qt6Core.dll", "7z.dll", "translations" });
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
        QProcess::startDetached("new/Updater_new.exe", arguments, targetDir + "/new");
    }
    catch (const std::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, gppTr("Updater.main", "提取 Updater_new.exe 失败。\n错误: %1")
            .arg(e.what()).toStdWString().c_str(),
            gppTr("Updater.main", "GalTransl++ Updater 内部错误").toStdWString().c_str(),
            MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    return 0;
}
