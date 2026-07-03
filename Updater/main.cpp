#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QProcess>

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

void extractFileFromZip(const fs::path& zipPath, const fs::path& outputDir, const std::string& fileName) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractMatching(wide2Ascii(zipPath), fileName, wide2Ascii(outputDir));
}

void extractZipExclude(const fs::path& zipPath, const fs::path& outputDir, const std::set<std::string>& excludePrefixes) {
    const bit7z::Bit7zLibrary library{ "7z.dll" };
    std::vector<uint32_t> indices;

    bit7z::BitArchiveReader archive{ library, wide2Ascii(zipPath) };
    for (const auto& item : archive) {
        if (
            std::ranges::any_of(excludePrefixes, [&](const std::string& prefix) { return item.path().starts_with(prefix); })
            )
        {
            continue;
        }
        indices.push_back(item.index());
    }

    bit7z::BitFileExtractor extractor{ library, bit7z::BitFormat::Auto };
    extractor.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    extractor.extractItems(wide2Ascii(zipPath), indices, wide2Ascii(outputDir));
}

int main(int argc, char* argv[]) {

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");
#endif

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
        std::string orgQtVersion = parser.isSet("qtVersion") ? parser.value("qtVersion").toStdString() : "6.5.3";
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
            MessageBoxW(nullptr, L"Failed to extract update package.",
                L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
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
        QProcess::startDetached("new/Updater_new.exe", arguments, targetDir + "/new");
    }
    catch (const std::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, std::format(L"Failed to extract Updater_new.exe.\nError: {}",
            ascii2Wide(std::string_view{ e.what() })).c_str(),
            L"GalTransl++ Updater", MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    return 0;
}
