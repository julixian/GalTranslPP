#pragma once

inline std::filesystem::path qt_config_path() {
    namespace fs = std::filesystem;
    const char* package_dir = std::getenv("MCPP_MANIFEST_DIR");
    if (!package_dir) return {};
    auto config = fs::path(package_dir) / "mcpp-build" / "qt-root.txt";
    if (!fs::exists(config)) config = fs::path(package_dir).parent_path() / "mcpp-build" / "qt-root.txt";
    return config;
}

inline std::filesystem::path qt_root_path() {
    namespace fs = std::filesystem;
    const auto config = qt_config_path();
    std::ifstream input(config);
    std::string root_text;
    if (!std::getline(input, root_text) || root_text.empty()) return {};
    return fs::path(root_text).lexically_normal();
}

inline int configure_qt(std::initializer_list<std::string_view> modules) {
    namespace fs = std::filesystem;
    const fs::path qt_root = qt_root_path();
    if (!fs::exists(qt_root / "include") || !fs::exists(qt_root / "lib")) {
        std::cerr << "Qt root is invalid; check mcpp-build/qt-root.txt\n";
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
