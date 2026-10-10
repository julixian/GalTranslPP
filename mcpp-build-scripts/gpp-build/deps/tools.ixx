export module gpp.deps.tools;

import std;
import mcpp;

export namespace gpp::deps::tools::win {

// 工具由用户安装；这里只从 PATH 查找并解析符号链接。
std::filesystem::path find_executable(std::string_view name) {
    namespace fs = std::filesystem;
    mcpp::rerun_if_env_changed("PATH");
    const char* environment = std::getenv("PATH");
    const std::string path = environment ? environment : "";
    const auto filename = std::string(name) + ".exe";
    for (std::size_t begin = 0; begin <= path.size();) {
        const auto end = path.find(';', begin);
        auto directory = path.substr(begin, end == std::string::npos ? end : end - begin);
        if (directory.size() >= 2 && directory.front() == '"' && directory.back() == '"')
            directory = directory.substr(1, directory.size() - 2);
        const auto candidate = fs::path(directory) / filename;
        if (fs::is_regular_file(candidate)) {
            const auto executable = fs::canonical(candidate);
            mcpp::rerun_if_changed(executable.generic_string().c_str());
            return executable;
        }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    throw std::runtime_error("PATH 中找不到 " + filename);
}
} // namespace gpp::deps::tools::win
