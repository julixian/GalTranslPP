#include "../GalTranslPP/GPPMacros.hpp"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTranslator>

#ifdef _WIN32
#include <Windows.h>
#endif

import GPPVersion;
import PythonManager;
import TerminalController;
import TerminalPreview;
import toml11;
import Tool;

using namespace gpp;

namespace fs = std::filesystem;
namespace py = pybind11;

NAMESPACE_BEGIN(gpp::cli)

fs::path projectDirectory(const std::string& input)
{
    auto pathText = input;
    if (pathText.size() >= 2 && pathText.front() == '"' && pathText.back() == '"') {
        pathText = pathText.substr(1, pathText.size() - 2);
    }
    fs::path path = ascii2Wide(pathText);
    if (fs::is_regular_file(path)) {
        path = path.parent_path();
    }
    if (!fs::is_directory(path)) {
        throw std::invalid_argument(gppTr("GPPCLI.GPPCLI", "路径 '%1' 不存在或不是一个有效的文件夹，请重新输入")
                .arg(wide2Ascii(path))
                .toStdString());
    }
    return fs::canonical(path);
}

bool confirmProject(const fs::path& projectDir, bool plain)
{
    if (!plain) {
        return showProjectPreview(projectDir);
    }
    printProjectPreview(projectDir);
    std::cout << gppTr("GPPCLI.GPPCLI", "按 Enter 开始翻译，输入 q 返回：").toStdString();
    std::string answer;
    if (!std::getline(std::cin, answer) || !answer.empty()) {
        return false;
    }
    const auto config = uparseToml(projectDir / L"Config.toml");
    if (ProblemOverviewTracker::hasUnimportedChanges(projectDir, transCacheDirName, config)) {
        std::cout << gppTr("GPPCLI.GPPCLI", "输入 y 继续翻译，其它输入返回：").toStdString();
        return std::getline(std::cin, answer) && (answer == "y" || answer == "Y");
    }
    return true;
}

void importOverview(const fs::path& projectDir, const fs::path& file)
{
    const auto result = importProblemOverview(projectDir, file);
    ProblemOverviewTracker::recordLastWriteTime(projectDir, file);
    std::cout << gppTr("ProblemOverview", "成功导入 %1 个句子至 transl_cache").arg(result.imported).toStdString()
              << '\n';
    for (const auto& problem : result.problems) {
        std::cerr << problem << '\n';
    }
}

NAMESPACE_END(gpp::cli)

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");
    initializeProcessJob();
#endif
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("GalTransl++ CLI");
    QCoreApplication::setApplicationVersion(QString::fromUtf8(GPPVERSION));
    QTranslator baseTranslator;
    QTranslator coreTranslator;
    QTranslator cliTranslator;
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QCommandLineParser parser;
    parser.setApplicationDescription("GalTransl++ CLI");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("project", "Project folder or Config.toml", "[project]");
    const QCommandLineOption yesOption({ "y", "yes" }, "Run without the configuration preview");
    const QCommandLineOption previewOption("preview", "Print the configuration preview and exit");
    const QCommandLineOption plainOption("no-tui", "Use a plain-text preview instead of the TUI");
    const QCommandLineOption importOption("import-problems", "Import a ProblemOverview file and exit", "file");
    parser.addOptions({ yesOption, previewOption, plainOption, importOption });
    parser.process(app);
    const auto projects = parser.positionalArguments();
    if (projects.size() > 1 || ((parser.isSet(previewOption) || parser.isSet(importOption)) && projects.isEmpty()) ||
        (parser.isSet(previewOption) && parser.isSet(importOption)))
    {
        parser.showHelp(1);
    }

    toml::value globalConfig;
    try {
        globalConfig = uparseToml(globalConfigPath);
        const auto language = toml::find_or(globalConfig, "language", "zh_CN");
        if (baseTranslator.load(QString("qt_%1.qm").arg(language), "translations")) {
            app.installTranslator(&baseTranslator);
        }
        if (language != "zh_CN") {
            if (coreTranslator.load(QString("qt_gpp_%1.qm").arg(language), "translations")) {
                app.installTranslator(&coreTranslator);
            }
            if (cliTranslator.load(QString("qt_gppcli_%1.qm").arg(language), "translations")) {
                app.installTranslator(&cliTranslator);
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::critical(
            gppTr("GPPCLI.GPPCLI", "无法读取全局配置，请检查 BaseConfig/GlobalConfig.toml 是否存在").toStdString());
        spdlog::error(e.what());
        return 1;
    }

    // 预览、帮助和问题清单导入不初始化 Python，也不创建运行时缓存。
    std::unique_ptr<py::gil_scoped_release> release;
    bool runtimeStarted = false;
    const auto runProject = [&](const fs::path& projectDir)
        {
            if (parser.isSet(yesOption)) {
                const auto config = uparseToml(projectDir / L"Config.toml");
                if (ProblemOverviewTracker::hasUnimportedChanges(projectDir, transCacheDirName, config)) {
                    spdlog::warn(gppTr("TerminalPreview", "检测到 %1 被修改但未导入。")
                            .arg(wide2Ascii(ProblemOverviewTracker::overviewPath(projectDir, config).filename()))
                            .toStdString());
                }
            }
            if (!runtimeStarted) {
                runtimeStarted = true;
                try {
                    const auto environment =
                        ascii2Wide(toml::find_or(globalConfig, "pythonEnvPath", "BaseConfig/Python-3.12.10-embed-amd64"));
                    if (!startUpPythonEnv(environment, release)) {
                        spdlog::warn(
                            gppTr("GPPCLI.GPPCLI", "未设置 Python 环境，将无法使用需要 Python 环境的模块").toStdString());
                    }
                }
                catch (...) {
                    spdlog::error(gppTr("GPPCLI.GPPCLI", "Python 环境配置失败").toStdString());
                }
                if (fs::exists(L"cache")) {
                    spdlog::warn(gppTr("GPPCLI.GPPCLI", "检测到异常退出，请注意备份相关翻译缓存").toStdString());
                }
                else {
                    fs::create_directories(L"cache");
                }
            }
            spdlog::info(gppTr("GPPCLI.GPPCLI", "开始处理项目: %1").arg(wide2Ascii(projectDir)).toStdString());
            {
                const auto controller = std::make_shared<TerminalController>();
                const auto translator = createTranslator(projectDir, controller);
                if (!translator) {
                    throw std::runtime_error(gppTr("GPPCLI.GPPCLI", "创建翻译器实例失败，请检查项目配置").toStdString());
                }
                translator->run();
            }
            ProblemOverviewTracker::recordLastWriteTime(projectDir);
            spdlog::info(gppTr("GPPCLI.GPPCLI", "项目 '%1' 处理完成！").arg(wide2Ascii(projectDir)).toStdString());
        };

    int exitCode = 0;
    fs::path currentProject;
    const bool interactive = hasInteractiveTerminal();
    const bool singleProject = !projects.isEmpty();
    while (true) {
        try {
            if (singleProject) {
                currentProject = cli::projectDirectory(projects.front().toStdString());
            }
            else {
                std::cout << "\nGalTransl++ CLI v" << GPPVERSION << '\n';
#ifdef _WIN32
                std::cout << gppTr("GPPCLI.GPPCLI", "按 Ctrl+Z 回车退出。").toStdString() << '\n';
#elif defined(__APPLE__)
                std::cout << gppTr("GPPCLI.GPPCLI", "按 Ctrl+D 退出。").toStdString() << '\n';
#endif
                std::cout << gppTr("GPPCLI.GPPCLI", "请输入项目文件夹或 Config.toml 的路径。").toStdString();
                if (!currentProject.empty()) {
                    std::cout << gppTr("GPPCLI.GPPCLI", " (直接按 Enter 可再次处理: %1)")
                                     .arg(wide2Ascii(currentProject))
                                     .toStdString();
                }
                std::cout << "\n> ";
                std::string input;
                if (!std::getline(std::cin, input)) {
                    break;
                }
                if (!input.empty()) {
                    currentProject = cli::projectDirectory(input);
                }
                if (currentProject.empty()) {
                    continue;
                }
            }
            if (parser.isSet(importOption)) {
                fs::path file = ascii2Wide(parser.value(importOption).toStdString());
                cli::importOverview(currentProject, file);
            }
            else if (parser.isSet(previewOption)) {
                printProjectPreview(currentProject);
            }
            else {
                if (!parser.isSet(yesOption)) {
                    if (!interactive) {
                        throw std::invalid_argument(
                            gppTr("GPPCLI.GPPCLI", "非交互终端请使用 --yes 执行翻译，或使用 --preview 查看配置")
                                .toStdString());
                    }
                    if (!cli::confirmProject(currentProject, parser.isSet(plainOption))) {
                        if (singleProject) {
                            break;
                        }
                        continue;
                    }
                }
                runProject(currentProject);
            }
        }
        catch (const std::exception& e) {
            spdlog::error(e.what());
            exitCode = 1;
        }
        catch (...) {
            spdlog::critical(gppTr("GPPCLI.GPPCLI", "发生未知错误").toStdString());
            exitCode = 1;
        }
        if (singleProject || !interactive) {
            break;
        }
    }
    if (runtimeStarted) {
        shutDownPythonEnv(release);
        try {
            fs::remove_all(L"cache");
        }
        catch (const fs::filesystem_error& e) {
            spdlog::warn(gppTr("GPPCLI.GPPCLI", "缓存删除错误: %1").arg(e.what()).toStdString());
        }
    }
    return exitCode;
}
