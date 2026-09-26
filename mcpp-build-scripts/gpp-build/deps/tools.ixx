// 从构建进程的 PATH 选择独立安装的工具，并把绝对路径交给安装 action。
export module gpp.deps.tools;

import std;
import mcpp;

export namespace gpp::deps::tools {
namespace fs = std::filesystem;

fs::path find_executable(std::string_view name) {
    mcpp::rerun_if_env_changed("PATH");
    const char* environment_path = std::getenv("PATH");
    const std::string search_path = environment_path ? environment_path : "";
    const bool host_windows = std::string_view(mcpp::host()).find("windows") != std::string_view::npos;
    const char separator = host_windows ? ';' : ':';
    const std::string filename = std::string(name) + (host_windows ? ".exe" : "");
    std::size_t start = 0;
    while (start <= search_path.size()) {
        const auto end = search_path.find(separator, start);
        std::string directory = search_path.substr(start, end == std::string::npos ? end : end - start);
        if (directory.size() >= 2 && directory.front() == '"' && directory.back() == '"')
            directory = directory.substr(1, directory.size() - 2);
        const fs::path candidate = (directory.empty() ? fs::current_path() : fs::path(directory)) / filename;
        std::error_code error;
        const auto status = fs::status(candidate, error);
        constexpr auto execute_permissions = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
        if (!error && fs::is_regular_file(status) &&
            (host_windows || (status.permissions() & execute_permissions) != fs::perms::none)) {
            const auto executable = fs::canonical(candidate, error);
            if (!error) {
                mcpp::rerun_if_changed(executable.generic_string().c_str());
                return executable;
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    throw std::runtime_error("PATH 中找不到 " + filename + "；请先独立安装并将其目录加入 PATH");
}
} // namespace gpp::deps::tools
