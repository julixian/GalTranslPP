#define PYBIND11_HEADERS
#include "../GalTranslPP/GPPMacros.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QTranslator>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <toml.hpp>

import GPPVersion;
import Tool;
import PythonManager;
import TerminalController;

namespace fs = std::filesystem;
namespace py = pybind11;

#pragma comment(lib, "GPPVersion.lib")
#pragma comment(lib, "GalTranslPP.lib")

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");
#endif

    QTranslator baseTranslator;
    QTranslator coreTranslator;
    QTranslator cliTranslator;
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("GalTransl++ CLI");
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    ascii2Wide("222");
    std::unique_ptr<py::gil_scoped_release> release;

    try {
        const auto globalConfig = toml::uparse(globalConfigPath);
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
            if (cliTranslator.load(QString("qt_gppcli_%1.qm").arg(language), "translations")) {
                app.installTranslator(&cliTranslator);
            }
        }
        const std::string pythonEnvPathStr = toml::find_or(globalConfig, "pythonEnvPath", "BaseConfig/Python-3.12.10-embed-amd64");
        try {
            const fs::path pythonEnvPath = ascii2Wide(pythonEnvPathStr);
            if (!startUpPythonEnv(pythonEnvPath, release)) {
                spdlog::warn(gppTr("GPPCLI.main", "未设置 Python 环境，将无法使用需要 Python 环境的模块")
                    .toStdString());
            }
        }
        catch(...) {
            spdlog::error(gppTr("GPPCLI.main", "Python 环境配置失败").toStdString());
        }
    }
    catch (...) {
        spdlog::critical(gppTr("GPPCLI.main", "无法读取全局配置，请检查 BaseConfig/GlobalConfig.toml 是否存在")
            .toStdString());
        return 1;
    }

    try {
        if (fs::exists(L"cache")) {
            spdlog::warn(gppTr("GPPCLI.main", "检测到异常退出，请注意备份相关翻译缓存").toStdString());
        }
        else {
            fs::create_directories(L"cache");
        }
    }
    catch (const fs::filesystem_error& e) {
        spdlog::error(gppTr("GPPCLI.main", "缓存检测或创建错误: %1")
            .arg(e.what())
            .toStdString());
    }

    fs::path currentProjectPath;
    while (true) {
        // 先清理所有可能残留的输入和屏幕
        std::cin.clear();
        std::cout << "\n======================================================\n";
        std::cout << std::format("GalTransl++ CLI v{}", GPPVERSION) << "\n";
        std::cout << "Press Ctrl+Z to exit.\n";
        std::cout << "======================================================\n";

        std::cout << gppTr("GPPCLI.main", "请输入项目文件夹或 Config.toml 的路径。").toStdString();
        if (!currentProjectPath.empty()) {
            std::cout << gppTr("GPPCLI.main", " (直接按 Enter 可再次处理: %1)")
                .arg(wide2Ascii(currentProjectPath.wstring()))
                .toStdString();
        }
        std::cout << "\n> ";

        std::string inputPathStr;
        if (release) {
            py::gil_scoped_acquire acquire;
            auto builtins = py::module_::import("builtins");
            try {
                inputPathStr = builtins.attr("input")("").cast<std::string>();
            }
            catch (...) {
                break;
            }
        }
        else {
            if (!std::getline(std::cin, inputPathStr)) {
                break; // 输入结束，退出程序
            }
        }

        // --- 处理用户输入 ---
        if (inputPathStr.empty()) {
            // 用户直接按了回车
            if (currentProjectPath.empty()) {
                // 如果之前没有处理过任何项目，提示用户必须输入
                spdlog::warn(gppTr("GPPCLI.main", "首次运行，请输入一个有效的项目路径")
                    .toStdString());
                continue; // 回到循环开头，再次提示
            }
            // 如果之前有项目，就使用上一次的路径
            spdlog::info(gppTr("GPPCLI.main", "再次处理项目: %1")
                .arg(wide2Ascii(currentProjectPath.wstring()))
                .toStdString());
        }
        else {
            // 用户输入了新路径
            // 去除可能存在的引号
            if (inputPathStr.starts_with('"') && inputPathStr.ends_with('"')) {
                inputPathStr = inputPathStr.substr(1, inputPathStr.length() - 2);
            }
            currentProjectPath = ascii2Wide(inputPathStr);
            if (fs::is_regular_file(currentProjectPath) && currentProjectPath.has_parent_path()) {
                currentProjectPath = currentProjectPath.parent_path(); // 去掉文件名，只留目录
            }
        }

        // 检查路径是否存在且为目录
        if (!fs::exists(currentProjectPath) || !fs::is_directory(currentProjectPath)) {
            spdlog::error(gppTr("GPPCLI.main", "路径 '%1' 不存在或不是一个有效的文件夹，请重新输入")
                .arg(wide2Ascii(currentProjectPath.wstring()))
                .toStdString());
            currentProjectPath.clear(); // 清空无效路径，以便下次循环提示用户重新输入
            continue;
        }

        try {
            spdlog::info(gppTr("GPPCLI.main", "开始处理项目: %1")
                .arg(wide2Ascii(currentProjectPath.wstring()))
                .toStdString());

            {
                std::unique_ptr<ITranslator> translator = createTranslator(currentProjectPath, std::make_shared<TerminalController>());
                if (!translator) {
                    spdlog::error(gppTr("GPPCLI.main", "创建翻译器实例失败，请检查项目配置")
                        .toStdString());
                    continue;
                }

                translator->run();
            }

            std::cout << std::endl;
            spdlog::info(gppTr("GPPCLI.main", "项目 '%1' 处理完成！")
                .arg(wide2Ascii(currentProjectPath.wstring()))
                .toStdString());

        }
        catch (const std::invalid_argument& e) {
            spdlog::critical(gppTr("GPPCLI.main", "[参数错误] %1")
                .arg(e.what())
                .toStdString());
        }
        catch (const std::runtime_error& e) {
            spdlog::critical(gppTr("GPPCLI.main", "[运行时错误] %1")
                .arg(e.what())
                .toStdString());
        }
        catch (const std::exception& e) {
            spdlog::critical(gppTr("GPPCLI.main", "[标准库错误] %1")
                .arg(e.what())
                .toStdString());
        }
        catch (...) {
            spdlog::critical(gppTr("GPPCLI.main", "发生未知错误").toStdString());
        }
    }

    shutDownPythonEnv(release);

    try {
        fs::remove(L"cache");
    }
    catch (const fs::filesystem_error& e) {
        spdlog::warn(gppTr("GPPCLI.main", "缓存删除错误: %1")
            .arg(e.what())
            .toStdString());
    }

    spdlog::info(gppTr("GPPCLI.main", "程序退出").toStdString());
    return 0;
}
