#pragma once

// Resolve vendored libraries from this workspace before passing them to the
// linker. An exact file path cannot resolve to a same-named system library.
inline bool link_vcpkg_libraries(std::initializer_list<std::string_view> names) {
    namespace fs = std::filesystem;
    const auto lib_dir = fs::path(mcpp::manifest_dir()).parent_path() /
                         "vcpkg_installed" / "gpp-x64-windows-release" / "lib";
    for (const auto name : names) {
        const auto lib = lib_dir / (std::string(name) + ".lib");
        if (!fs::is_regular_file(lib)) {
            std::println(stderr, "Missing vcpkg library: {}", lib.string());
            return false;
        }
        const auto path = lib.generic_string();
        mcpp::link_flag(path.c_str());
    }
    return true;
}
