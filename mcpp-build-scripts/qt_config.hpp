#pragma once

// mcpp caches build.mcpp separately from its included headers. Track the shared
// build logic so editing a helper recompiles the programs that include it.
inline void track_build_scripts() {
    const auto directory = std::filesystem::path(mcpp::manifest_dir()).parent_path() /
                           "mcpp-build-scripts";
    for (const char* name : {"qt_config.hpp", "mcpp_actions.hpp",
                             "mcpp_translations.hpp", "vcpkg_link.hpp"}) {
        const auto file = (directory / name).generic_string();
        mcpp::rerun_if_changed(file.c_str());
    }
}

inline std::filesystem::path qt_config_path() {
    namespace fs = std::filesystem;
    return fs::path(mcpp::manifest_dir()).parent_path() /
           "mcpp-build-scripts" / "qt-root.txt";
}

inline std::filesystem::path qt_root_path() {
    namespace fs = std::filesystem;
    const auto config = qt_config_path();
    std::ifstream input(config);
    std::string root_text;
    if (!std::getline(input, root_text) || root_text.empty()) return {};
    if (!root_text.empty() && root_text.back() == '\r') root_text.pop_back();
    return fs::path(root_text).lexically_normal();
}

inline int configure_qt(std::initializer_list<std::string_view> modules) {
    namespace fs = std::filesystem;
    const fs::path qt_root = qt_root_path();
    if (!fs::is_directory(qt_root / "include") || !fs::is_directory(qt_root / "lib")) {
        std::cerr << "Qt root is invalid; check mcpp-build-scripts/qt-root.txt\n";
        return 1;
    }
    const auto config = qt_config_path();
    mcpp::rerun_if_changed(config.generic_string().c_str());
    mcpp::include_dir((qt_root / "include").generic_string().c_str());
    mcpp::link_search((qt_root / "lib").generic_string().c_str());
    for (const auto module : modules) {
        const auto name = std::string(module);
        const auto include_name = name.starts_with("Qt6") ? "Qt" + name.substr(3) : name;
        mcpp::include_dir((qt_root / "include" / include_name).generic_string().c_str());
        mcpp::link_lib(name.c_str());
    }
    return 0;
}
