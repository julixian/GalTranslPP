// 基于 mcpp-plugins v0.15.1 deps/cmake.cppm（Apache-2.0）。
// 上游提交：b28cbc95500eda2bff46896b0a01310ed21b691b
// 本地改动：优先使用配置路径，否则从 PATH 查找工具；不声明或下载 xlings 工具包。
export module gpp.deps.cmake;

import std;
import mcpp;
import mcpp.plugins;
import mcpp.deps;
import gpp.deps.tools;

export namespace gpp::deps::cmake {

// Where the subproject installs each kind of file, relative to its prefix.
// A subproject whose `install()` rules nest them (`<prefix>/Widgets/include`)
// names the nesting here.
struct layout {
    std::string include = "include";
    std::string lib     = "lib";
    std::string bin     = "bin";
};

struct options {
    // The subproject's source directory, relative to the package root.
    std::string source;
    // Names the action and the prefix. Empty takes the source directory's name.
    std::string name;
    // `-D…`, `-G …` and any other argument for the configure step.
    std::vector<std::string> cache_args;
    // Prefixes the subproject's `find_package` searches (`CMAKE_PREFIX_PATH`),
    // e.g. `mcpp::rules::qt::root()`.
    std::vector<std::string> prefix_path;
    std::string config = "Release";
    layout dirs;
    // Library names in link order, as `mcpp.deps.vcpkg` takes them.
    std::vector<std::string> libraries;
    // Whether those libraries are shared: decides the file names off Windows
    // and whether the prefix's shared-library directory reaches `mcpp run`
    // and `mcpp pack`.
    bool shared = false;
    // CMake 可执行文件的绝对路径；留空时从 PATH 查找。
    std::string cmake;
    // Files of the prefix placed beside the program, as `mcpp.deps.vcpkg`
    // takes them (`{"bin/tool.cfg", "."}`).
    std::vector<mcpp::deps::deploy_entry> deploy;
};

// The prefix, by name (SPEC-007 R1.3).
struct prefix {
    std::string root, include, lib, bin;
    // The copies `options::deploy` produced, for a project's own layout.
    std::vector<mcpp::deps::deployed_file> deployed;
    explicit operator bool() const { return !root.empty(); }
};

inline std::string cmake_exe(const options& opt) {
    const auto executable = opt.cmake.empty()
        ? gpp::deps::tools::find_executable("cmake")
        : mcpp::deps::absolute_from_root(opt.cmake);
    if (!std::filesystem::is_regular_file(executable))
        throw std::runtime_error("CMake 可执行文件不存在：" + executable.generic_string());
    return mcpp::deps::generic(executable);
}

inline prefix use(const options& opt) {
    namespace fs = std::filesystem;
    constexpr std::string_view who = "gpp.deps.cmake";
    mcpp::fact("mcpp.plugins", std::string(mcpp::plugins::version).c_str());

    std::error_code ec;
    const fs::path source = mcpp::deps::absolute_from_root(opt.source);
    if (opt.source.empty() || !fs::is_regular_file(source / "CMakeLists.txt", ec)) {
        std::cerr << std::format(
            "{}: options::source must name a directory holding CMakeLists.txt; got '{}'.\n"
            "  A git submodule that was not checked out is an empty directory: "
            "`git submodule update --init`.\n", who, opt.source);
        return {};
    }
    const std::string name = opt.name.empty() ? source.filename().string() : opt.name;
    const fs::path base   = fs::path(mcpp::out_dir()) / "deps-cmake" / name;
    const fs::path build  = base / "build";
    const fs::path root   = base / "install";

    prefix p;
    p.root    = mcpp::deps::generic(root);
    p.include = mcpp::deps::generic(root / opt.dirs.include);
    p.lib     = mcpp::deps::generic(root / opt.dirs.lib);
    p.bin     = mcpp::deps::generic(root / opt.dirs.bin);

    const std::string cmake = cmake_exe(opt);
    {
        // ONE ACTION, THREE STEPS. `cmake -P` runs a script this program
        // writes: configure (every time -- over an existing cache CMake re-runs
        // only what changed, and the arguments may have changed, which is why
        // the action ran at all), then build and install.
        const std::string stamp  = mcpp::deps::generic(base / (name + ".stamp"));
        const std::string id     = "deps-cmake:" + name;
        const std::string desc   = "CMAKE " + name;
        const fs::path    script = base / (name + ".cmake");
        using mcpp::deps::bracket;
        std::vector<std::string> configure{
            "-S", mcpp::deps::generic(source), "-B", mcpp::deps::generic(build),
            "-DCMAKE_INSTALL_PREFIX=" + p.root, "-DCMAKE_BUILD_TYPE=" + opt.config };
        if (!opt.prefix_path.empty()) {
            std::string joined;
            for (auto const& d : opt.prefix_path) {
                if (!joined.empty()) joined += ';';
                joined += mcpp::deps::generic(mcpp::deps::absolute_from_root(d));
            }
            configure.push_back("-DCMAKE_PREFIX_PATH=" + joined);
        }
        const bool chosen = std::ranges::any_of(opt.cache_args, [](const std::string& x) {
            return x.contains("CMAKE_C_COMPILER") || x.contains("CMAKE_CXX_COMPILER")
                || x.contains("CMAKE_TOOLCHAIN_FILE");
        });
        if (const auto cc = mcpp::deps::program_compilers(); cc && !chosen) {
            configure.push_back("-DCMAKE_C_COMPILER=" + cc.c);
            configure.push_back("-DCMAKE_CXX_COMPILER=" + cc.cxx);
        }
        for (auto const& x : opt.cache_args) configure.push_back(x);
        std::string text = "# Written by gpp.deps.cmake: configure, build and install " + name + ".\n"
                           "execute_process(COMMAND ${CMAKE_COMMAND}";
        for (auto const& x : configure) text += "\n    " + bracket(x);
        text += "\n    RESULT_VARIABLE rc)\n"
                "if(NOT rc EQUAL 0)\n  message(FATAL_ERROR \"configure exited ${rc}\")\nendif()\n"
                "execute_process(COMMAND ${CMAKE_COMMAND} --build " + bracket(mcpp::deps::generic(build)) +
                " --config " + bracket(opt.config) + " --target install --parallel\n    RESULT_VARIABLE rc)\n"
                "if(NOT rc EQUAL 0)\n  message(FATAL_ERROR \"build and install exited ${rc}\")\nendif()\n";
        mcpp::deps::write_if_changed(script, text);
        const std::string scriptS = mcpp::deps::generic(script);
        mcpp::action a;
        a.id          = id.c_str();
        a.role        = mcpp::roles::prepare;
        a.description = desc.c_str();
        a.arg(cmake.c_str()).arg("-P").arg(scriptS.c_str());
        a.input(cmake.c_str());
        a.input(scriptS.c_str());
        for (auto const& f : mcpp::deps::files_under(source)) a.input(f.c_str());
        mcpp::deps::watch_tree(source);
        a.output(stamp.c_str());
        a.output_dir(p.root.c_str());
        a.submit();
        p.deployed = mcpp::deps::deploy_after("deps-cmake-" + name, stamp, fs::path(p.root), opt.deploy);
    }

    mcpp::include_dir(p.include.c_str());
    mcpp::deps::link_libraries(root / opt.dirs.lib, opt.libraries, opt.shared);
    if (opt.shared) mcpp::deps::runtime_directory(p.bin, p.lib);
    return p;
}

} // namespace gpp::deps::cmake
