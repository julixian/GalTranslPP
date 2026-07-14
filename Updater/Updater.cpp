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

QString gppTr(const char* context, const char* source) {
    return QCoreApplication::translate(context, source);
}

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

#if __cpp_lib_string_view < 202403L
template<class CharT, class Traits, class Alloc>
[[nodiscard]] std::basic_string<CharT, Traits, Alloc>
operator+(const std::basic_string<CharT, Traits, Alloc>& lhs,
    std::basic_string_view<CharT, Traits> rhs) {
    std::basic_string<CharT, Traits, Alloc> r(lhs.get_allocator());
    r.reserve(lhs.size() + rhs.size());
    return r.append(lhs).append(rhs);
}

template<class CharT, class Traits, class Alloc>
[[nodiscard]] std::basic_string<CharT, Traits, Alloc>
operator+(std::basic_string_view<CharT, Traits> lhs,
    const std::basic_string<CharT, Traits, Alloc>& rhs) {
    std::basic_string<CharT, Traits, Alloc> r(rhs.get_allocator());
    r.reserve(lhs.size() + rhs.size());
    return r.append(lhs).append(rhs);
}
#endif

std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
template<typename T>
    requires(std::is_same_v<std::remove_cvref_t<T>, fs::path>)
std::string wide2Ascii(T&& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
#ifdef _WIN32
    return wide2Ascii(path.native(), codePage, usedDefaultChar);
#else
    return wide2Ascii(path.wstring(), codePage, usedDefaultChar);
#endif
}
std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_UTF8);

#ifdef _WIN32
std::string wide2Ascii(std::wstring_view wide, UINT codePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(),
        ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::wstring ascii2Wide(std::string_view ascii, UINT codePage) {
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(),
        nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(),
        wide.data(), len);
    return wide;
}
#endif

std::optional<int> str2Int(std::string_view str) {
    int value = 0;
    // 注意: from_chars 不会跳过前导空格
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

template<typename T>
std::vector<T> splitStringImpl(auto&& str, auto&& delimiter)
{
    std::vector<T> result;
    for (auto&& subStrView : str | std::views::split(delimiter)) {
        result.emplace_back(subStrView.begin(), subStrView.end());
    }
    return result;
}
std::vector<std::string_view> splitStringView(std::string_view str, char delimiter) {
    return splitStringImpl<std::string_view>(str, delimiter);
}
std::vector<std::string_view> splitStringView(std::string_view str, std::string_view delimiter) {
    return splitStringImpl<std::string_view>(str, delimiter);
}
std::vector<std::string> splitString(std::string_view str, char delimiter) {
    return splitStringImpl<std::string>(str, delimiter);
}
std::vector<std::string> splitString(std::string_view str, std::string_view delimiter) {
    return splitStringImpl<std::string>(str, delimiter);
}

std::string replaceStr(const std::string& str, std::string_view org, std::string_view rep) {
    return boost::replace_all_copy(str, org, rep);
}

int compareVersion(std::string_view latestVer, std::string_view currentVer)
{
    auto parseVersion = [](std::string_view version) -> std::optional<std::array<int, 3>>
        {
            if (const size_t versionPrefixPos = version.find_last_of('v'); versionPrefixPos != std::string_view::npos) {
                version.remove_prefix(versionPrefixPos + 1);
            }
            const std::vector<std::string_view> parts = splitStringView(version, '.');
            if (parts.size() != 3) {
                return std::nullopt;
            }
            std::array<int, 3> result{};
            for (size_t i = 0; i < result.size(); ++i) {
                const std::optional<int> part = str2Int(parts[i]);
                if (!part.has_value()) {
                    return std::nullopt;
                }
                result[i] = part.value();
            }
            return result;
        };

    const std::optional<std::array<int, 3>> latestVersion = parseVersion(latestVer);
    const std::optional<std::array<int, 3>> currentVersion = parseVersion(currentVer);
    if (!latestVersion.has_value() || !currentVersion.has_value()) {
        return -2;
    }

    if (latestVersion->at(0) > currentVersion->at(0)) {
        return 2;
    }
    if (latestVersion->at(0) < currentVersion->at(0)) {
        return -1;
    }

    for (size_t i = 1; i < latestVersion->size(); ++i) {
        if (latestVersion->at(i) > currentVersion->at(i)) {
            return 1;
        }
        if (latestVersion->at(i) < currentVersion->at(i)) {
            return -1;
        }
    }
    return 0;
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
    parser.addOption({ QStringList{"pid"}, "pid", "pid" });
    parser.addOption({ QStringList{"source"}, "source", "source" });
    parser.addOption({ QStringList{"target"}, "target", "target" });
    parser.addOption({ QStringList{"restart"}, "restart", "restart" });
    parser.addOption({ QStringList{"newActionFlag"}, "newActionFlag", "newActionFlag" });
    parser.addOption({ QStringList{"gppVersion"}, "gppVersion", "gppVersion" });
    parser.addOption({ QStringList{"pythonVersion"}, "pythonVersion", "pythonVersion" });
    parser.addOption({ QStringList{"promptVersion"}, "promptVersion", "promptVersion" });
    parser.addOption({ QStringList{"dictVersion"}, "dictVersion", "dictVersion" });
    parser.addOption({ QStringList{"qtVersion"}, "qtVersion", "qtVersion" });
    parser.addOption({ QStringList{"icuVersion"}, "icuVersion", "icuVersion" });
    parser.process(app);

    const qint64 pid = parser.value("pid").toLongLong();
    const QString sourceZip = parser.value("source");
    const QString targetDir = parser.value("target");

    if (pid != 0) {
        waitForProcessToExit(pid);
    }

    try {
        const fs::path globalConfigPath = parser.isSet("newActionFlag")
            ? L"../BaseConfig/GlobalConfig.toml"
            : L"BaseConfig/GlobalConfig.toml";
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
        MessageBoxW(nullptr, gppTr("Updater.Updater", "非法参数").toStdWString().c_str(),
            gppTr("Updater.Updater", "GalTransl++ Updater").toStdWString().c_str(),
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
        const std::string orgQtVersion = parser.isSet("qtVersion") ? parser.value("qtVersion").toStdString() : "6.11.1";
        const std::string orgIcuVersion = parser.isSet("icuVersion") ? parser.value("icuVersion").toStdString() : "7.8.0";

        waitForProcessToExit(pid);

        try {
            std::set<std::string> excludePreFixes =
            {
                "BaseConfig/PythonScripts", "BaseConfig/Prompt.toml", 
                "BaseConfig/Dicts", 
            };
            
            if (compareVersion(PYTHONVERSION, orgPythonVersion) > 0) {
                excludePreFixes.erase("BaseConfig/PythonScripts");
            }
            const int promptVersionCompare = compareVersion(PROMPTVERSION, orgPromptVersion);
            if (promptVersionCompare > 0) {
                if (promptVersionCompare == 2) {
#ifdef Q_OS_WIN
                    MessageBoxW(nullptr, gppTr("Updater.Updater",
                        "由于提示词解析方式发生不兼容变更，本次更新将强制覆盖原默认提示词。\n"
                        "你可以先行备份，然后点击确定以继续更新。").toStdWString().c_str(),
                        L"GalTransl++ Updater", MB_OK | MB_TOPMOST);
#endif
                    excludePreFixes.erase("BaseConfig/Prompt.toml");
                }
                else {
#ifdef Q_OS_WIN
                    const int ret = MessageBoxW(nullptr, gppTr("Updater.Updater",
                        "检测到新版本的 Prompt，是否更新 Prompt (会覆盖当前的默认提示词)？").toStdWString().c_str(),
                        L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                    if (ret == IDYES) {
                        excludePreFixes.erase("BaseConfig/Prompt.toml");
                    }
#endif
                }
            }
            if (compareVersion(DICTVERSION, orgDictVersion) > 0) {
#ifdef Q_OS_WIN
                const int ret = MessageBoxW(nullptr, gppTr("Updater.Updater",
                    "检测到新版本的 GPT 字典，是否更新字典（会覆盖当前的默认字典）？").toStdWString().c_str(),
                    L"GalTransl++ Updater", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                if (ret == IDYES) {
                    excludePreFixes.erase("BaseConfig/Dicts");
                }
#endif
            }
            if (compareVersion(ICUVERSION, orgIcuVersion) > 0) {
                try {
                    const std::vector<std::string> orgVerStrVec = splitString(orgIcuVersion, '.');
                    const std::wstring orgVerStr = ascii2Wide(orgVerStrVec.at(0) + orgVerStrVec.at(1));
                    for (const auto& icuDll : { L"icudt", L"icuin", L"icuuc" }) {
#ifdef Q_OS_WIN
                        const fs::path dllPath = std::wstring(L"../") + icuDll + orgVerStr + L".dll";
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
            MessageBoxW(nullptr, gppTr("Updater.Updater",
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
            MessageBoxW(nullptr, gppTr("Updater.Updater",
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
        arguments << "--gppVersion" << QString::fromUtf8(GPPVERSION);
        arguments << "--pythonVersion" << QString::fromUtf8(PYTHONVERSION);
        arguments << "--promptVersion" << QString::fromUtf8(PROMPTVERSION);
        arguments << "--dictVersion" << QString::fromUtf8(DICTVERSION);
        arguments << "--qtVersion" << QString::fromUtf8(QTVERSION);
        arguments << "--icuVersion" << QString::fromUtf8(ICUVERSION);
        if (parser.isSet("restart")) {
            arguments << "--restart" << parser.value("restart");
        }
        QProcess::startDetached("new/Updater_new.exe", arguments, targetDir + "/new");
    }
    catch (const std::exception& e) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr, gppTr("Updater.Updater", "提取 Updater_new.exe 失败。\n错误: %1")
            .arg(e.what()).toStdWString().c_str(),
            gppTr("Updater.Updater", "GalTransl++ Updater").toStdWString().c_str(),
            MB_ICONERROR | MB_TOPMOST);
#endif
        return -1;
    }

    return 0;
}
