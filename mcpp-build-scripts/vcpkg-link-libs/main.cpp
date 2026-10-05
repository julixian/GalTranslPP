#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

import std;

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    try {
        fs::path librarian, libraryDirectory, output;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (++i == argc) throw std::runtime_error("缺少参数值");
            const auto value = fs::u8path(argv[i]);
            if (option == "--librarian") librarian = value;
            else if (option == "--lib-dir") libraryDirectory = value;
            else if (option == "--output") output = value;
            else throw std::runtime_error("未知参数：" + std::string(option));
        }
        if (librarian.empty() || libraryDirectory.empty() || output.empty())
            throw std::runtime_error("需要 --librarian、--lib-dir 和 --output");
        std::vector<fs::path> libraries;
        for (const auto& entry : fs::directory_iterator(libraryDirectory))
            if (entry.is_regular_file() && entry.path().extension() == ".lib")
                libraries.push_back(entry.path());
        std::ranges::sort(libraries);
        if (libraries.empty()) throw std::runtime_error("安装目录中没有链接库");

        fs::create_directories(output.parent_path());
        auto responsePath = output;
        responsePath.replace_extension(".rsp");
        {
            // 响应文件避免命令行长度限制；UTF-8 BOM 同时适用于 llvm-lib 和 MSVC lib。
            std::ofstream response(responsePath, std::ios::binary);
            response.exceptions(std::ios::failbit | std::ios::badbit);
            response << "\xEF\xBB\xBF";
            auto writePath = [&](const fs::path& path) {
                const auto text = path.generic_u8string();
                response.write(reinterpret_cast<const char*>(text.data()), text.size());
            };
            response << "\"/OUT:";
            writePath(output);
            response << "\"\n";
            for (const auto& library : libraries) {
                response << '\"';
                writePath(library);
                response << "\"\n";
            }
        }
        // 汇集静态库与导入库，链接器仍按需提取成员，不使用 /WHOLEARCHIVE。
        auto command = L"\"" + librarian.wstring() + L"\" @\"" + responsePath.wstring() + L"\"";
        STARTUPINFOW startup{sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(librarian.c_str(), command.data(), nullptr, nullptr, FALSE,
                            0, nullptr, nullptr, &startup, &process))
            throw std::runtime_error(std::format("启动归档工具失败：{}", GetLastError()));
        CloseHandle(process.hThread);
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hProcess);
        return static_cast<int>(exitCode);
    } catch (const std::exception& error) {
        std::cerr << "vcpkg-link-libs: " << error.what() << '\n';
        return 1;
    }
}
