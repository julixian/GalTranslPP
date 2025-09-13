#include <Windows.h>
#include <spdlog/spdlog.h>

import std;
import Tool;
import TerminalController;
namespace fs = std::filesystem;

#pragma comment(lib, "../lib/GalTranslPP.lib")

int main(int argc, char* argv[])
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    fs::path currentProjectPath;

    while (true) {
        std::cout << "\n======================================================\n";
        std::cout << "GalTransl++ CLI - test v" << GPPVERSION << "\n";
        std::cout << "======================================================\n";

        std::cout << "请输入项目文件夹或Config.toml的路径。";
        if (!currentProjectPath.empty()) {
            std::cout << " (直接按 Enter 可再次处理: " << wide2Ascii(currentProjectPath.wstring()) << ")";
        }
        std::cout << "\n> ";

        std::string inputPathStr;
        std::getline(std::cin, inputPathStr);

        // --- 处理用户输入 ---
        if (inputPathStr.empty()) {
            // 用户直接按了回车
            if (currentProjectPath.empty()) {
                // 如果之前没有处理过任何项目，提示用户必须输入
                spdlog::warn("首次运行，请输入一个有效的项目路径。");
                continue; // 回到循环开头，再次提示
            }
            // 如果之前有项目，就使用上一次的路径
            spdlog::info("再次处理项目: {}", wide2Ascii(currentProjectPath.wstring()));
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
            spdlog::error("路径 '{}' 不存在或不是一个有效的文件夹，请重新输入。", wide2Ascii(currentProjectPath.wstring()));
            currentProjectPath.clear(); // 清空无效路径，以便下次循环提示用户重新输入
            continue;
        }

        try {
            spdlog::info("开始处理项目: {}", wide2Ascii(currentProjectPath.wstring()));

            {
                std::unique_ptr<ITranslator> translator = createTranslator(currentProjectPath, std::make_shared<TerminalController>());
                if (!translator) {
                    spdlog::error("创建翻译器实例失败，请检查项目配置。");
                    continue;
                }

                translator->run();
            }

            std::cout << std::endl;
            spdlog::info("项目 '{}' 处理完成！", wide2Ascii(currentProjectPath.wstring()));

        }
        catch (const std::invalid_argument& e) {
            spdlog::critical("[参数错误] {}", e.what());
        }
        catch (const std::runtime_error& e) {
            spdlog::critical("[运行时错误] {}", e.what());
        }
        catch (const std::exception& e) {
            spdlog::critical("[标准库错误] {}", e.what());
        }
        catch (...) {
            spdlog::critical("发生未知错误。");
        }
    }

    return 0;
}
