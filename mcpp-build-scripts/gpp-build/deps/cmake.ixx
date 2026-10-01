// 基于 mcpp-plugins v0.18.1 deps/cmake.cppm（Apache-2.0）。
// 上游提交：6dc34901d0f12fc402f2c0e7de0e68acfb1cc9d6
// 本地改动：优先使用配置路径，否则从 PATH 查找工具；不声明或下载 xlings 工具包。
export module gpp.deps.cmake;

import std;
import mcpp;
import mcpp.plugins;
import mcpp.deps;
import mcpp.plugins.fs;
import mcpp.plugins.toolset;
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
    std::vector<mcpp::plugins::fs::deploy_entry> deploy;
    // Which toolset builds the subproject (0.17.0). The default `resolved` is
    // the toolset mcpp builds the program with, named by path, with the Ninja
    // generator and mcpp's own ninja. `detected` lets CMake find its own
    // toolset, as 0.16.0 did, until 2027-03-28. Compilers or a toolchain file
    // in `cache_args` decide instead. See docs/deps.md.
    mcpp::plugins::toolset::choice toolset;
    // Which generator (0.18.0). `default` is Ninja wherever the toolset is
    // named, a Visual Studio instance's included, as vcpkg builds its CMake
    // ports; `ninja` says the same, its 0.17.0 meaning. `visual_studio` keeps
    // CMake's Visual Studio generator on the instance mcpp resolved -- 0.17.0's
    // default -- for a subproject that needs MSBuild; with no instance it is
    // the default.
    enum class generator_kind { default_, ninja, visual_studio };
    generator_kind generator = generator_kind::default_;
    // The C runtime linkage on the MSVC ABI, "static" or "dynamic"
    // (`CMAKE_MSVC_RUNTIME_LIBRARY`). Empty follows the program's C++ runtime
    // contract (`mcpp::msvc_crt_linkage()`).
    std::string crt_linkage;
    // Where installations are kept for reuse (0.18.0): a directory, "off", or
    // empty for `MCPP_DEPS_CMAKE_CACHE`, and then the user's cache directory
    // (`%LOCALAPPDATA%/mcpp-plugins/deps-cmake`,
    // `$XDG_CACHE_HOME/mcpp-plugins/deps-cmake` or
    // `~/.cache/mcpp-plugins/deps-cmake`). Read when the action runs; the place
    // is no part of what is built. See docs/deps.md.
    std::string cache;
};

// The variable a `-D<name>[:<type>]=<value>` argument defines; empty for any
// other argument.
inline std::string_view defined_variable(std::string_view arg) {
    if (!arg.starts_with("-D")) return {};
    arg.remove_prefix(2);
    return arg.substr(0, arg.find_first_of(":="));
}

// Whether the configure arguments define `name`, in the joined form
// (`-D<name>=…`) or the spaced one (`-D <name>=…`).
inline bool defines(std::span<const std::string> args, std::string_view name) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view v = defined_variable(args[i]);
        if (args[i] == "-D" && i + 1 < args.size()) v = std::string_view(args[i + 1]).substr(0, args[i + 1].find_first_of(":="));
        if (!v.empty() && v == name) return true;
    }
    return false;
}

// Whether the configure arguments choose the compiler: `CMAKE_C_COMPILER`,
// `CMAKE_CXX_COMPILER` or `CMAKE_TOOLCHAIN_FILE` defined, or `--toolchain`
// (CMake 3.21). By the variable's name (0.18.0): `CMAKE_CXX_COMPILER_LAUNCHER`
// and `CMAKE_C_COMPILER_TARGET` choose nothing.
inline bool chooses_compiler(std::span<const std::string> args) {
    for (std::size_t i = 0; i < args.size(); ++i)
        if (args[i] == "--toolchain" || args[i].starts_with("--toolchain=")) return true;
    return defines(args, "CMAKE_C_COMPILER") || defines(args, "CMAKE_CXX_COMPILER")
        || defines(args, "CMAKE_TOOLCHAIN_FILE");
}

// Whether they choose the generator: `-G`, or `CMAKE_GENERATOR` defined.
// `CMAKE_GENERATOR_PLATFORM`, `_TOOLSET` and `_INSTANCE` refine a generator
// and choose none (0.18.0).
inline bool chooses_generator(std::span<const std::string> args) {
    for (std::size_t i = 0; i < args.size(); ++i)
        if (args[i].starts_with("-G")) return true;
    return defines(args, "CMAKE_GENERATOR");
}

// The prefix, by name (SPEC-007 R1.3).
struct prefix {
    std::string root, include, lib, bin;
    // The copies `options::deploy` produced, for a project's own layout.
    std::vector<mcpp::plugins::fs::deployed_file> deployed;
    // How the toolset reached CMake: "instance", "chain" or "detected".
    std::string mechanism;
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

namespace detail {

// The script of a keyed installation: `steps` (configure, build, install) runs
// only when the cache holds no installation with the key. The key is
// computed when the action runs, from `recipe` (the plugin's version, the
// toolset and the keyed arguments), CMake's version, the environment CMake
// reads its compilers and flags from, and the SHA-256 of each file `listed`
// under the source directory. Every step of the cache that fails leaves the
// work to CMake: a copy that fails, an entry that lost a file, a directory
// that cannot be written.
inline std::string cached_steps(const std::string& name, const std::string& source,
                                const std::string& build, const std::string& prefix,
                                const std::string& recipe, const std::string& listed,
                                const std::string& cache, const std::string& steps) {
    std::string text = R"cmake(
# AN INSTALLATION IS BUILT ONCE (mcpp.plugins 0.18.0). An entry of the cache
# whose key is this installation's is copied into the prefix; otherwise CMake
# builds it, and it is kept unless one of its text files names a directory of
# this subproject, which would not hold where the entry is copied to.
set(source @SOURCE@)
set(build @BUILD@)
set(prefix @PREFIX@)
set(recipe @RECIPE@)
set(listed @LISTED@)
set(cache @CACHE@)

function(mcpp_install)
@STEPS@endfunction()

if(cache STREQUAL "")
  set(cache "$ENV{MCPP_DEPS_CMAKE_CACHE}")
endif()
if(cache STREQUAL "")
  if(CMAKE_HOST_WIN32)
    if(NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
      set(cache "$ENV{LOCALAPPDATA}/mcpp-plugins/deps-cmake")
    elseif(NOT "$ENV{APPDATA}" STREQUAL "")
      set(cache "$ENV{APPDATA}/mcpp-plugins/deps-cmake")
    endif()
  elseif(NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
    set(cache "$ENV{XDG_CACHE_HOME}/mcpp-plugins/deps-cmake")
  elseif(NOT "$ENV{HOME}" STREQUAL "")
    set(cache "$ENV{HOME}/.cache/mcpp-plugins/deps-cmake")
  endif()
endif()

if(cache STREQUAL "" OR cache STREQUAL "off" OR CMAKE_VERSION VERSION_LESS 3.21)
  mcpp_install()
else()
  file(TO_CMAKE_PATH "${cache}" cache)

  # THE KEY.
  set(sums "")
  string(REPLACE "\n" ";" files "${listed}")
  foreach(f IN LISTS files)
    if(NOT f STREQUAL "")
      file(SHA256 "${source}/${f}" sum)
      string(APPEND sums "${sum} ${f}\n")
    endif()
  endforeach()
  string(SHA256 sums "${sums}")
  set(stated "${recipe}cmake ${CMAKE_VERSION}\n")
  foreach(v IN ITEMS CC CXX CFLAGS CXXFLAGS LDFLAGS RC RCFLAGS CMAKE_TOOLCHAIN_FILE
                     CMAKE_GENERATOR CMAKE_GENERATOR_PLATFORM CMAKE_GENERATOR_TOOLSET)
    if(DEFINED ENV{${v}})
      string(APPEND stated "env ${v}=$ENV{${v}}\n")
    endif()
  endforeach()
  string(APPEND stated "sources ${sums}\n")
  string(SHA256 key "${stated}")
  string(SUBSTRING "${key}" 0 16 key)
  set(entry "${cache}/@NAME@/${key}")

  # A KEPT INSTALLATION. Its files take this build's time, so what compiled
  # against another installation compiles again.
  set(taken FALSE)
  if(EXISTS "${entry}/files.txt")
    file(REMOVE_RECURSE "${prefix}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${entry}/install" "${prefix}"
                    RESULT_VARIABLE rc)
    if(rc EQUAL 0)
      set(taken TRUE)
      file(STRINGS "${entry}/files.txt" kept ENCODING UTF-8)
      foreach(f IN LISTS kept)
        if(NOT EXISTS "${prefix}/${f}")
          set(taken FALSE)
          break()
        endif()
        file(TOUCH_NOCREATE "${prefix}/${f}")
      endforeach()
    endif()
    if(taken)
      message(STATUS "gpp.deps.cmake: @NAME@ taken from ${entry}")
    else()
      message(STATUS "gpp.deps.cmake: ${entry} is incomplete; CMake builds @NAME@")
    endif()
  endif()

  if(NOT taken)
    # From an empty prefix, so what is kept is what this build installed.
    file(REMOVE_RECURSE "${prefix}")
    mcpp_install()

    # ONLY AN INSTALLATION THAT CAN MOVE IS KEPT.
    set(forms "${source}" "${build}" "${prefix}")
    foreach(d IN ITEMS "${source}" "${build}" "${prefix}")
      string(REPLACE "/" "\\" native "${d}")
      list(APPEND forms "${native}")
    endforeach()
    if(CMAKE_HOST_WIN32)
      string(TOLOWER "${forms}" forms)
    endif()
    set(text_files .cmake .pc .la .prl .pri .json .txt .h .hh .hpp .hxx .inl .ipp)
    file(GLOB_RECURSE installed LIST_DIRECTORIES false RELATIVE "${prefix}" "${prefix}/*")
    set(why "")
    foreach(f IN LISTS installed)
      get_filename_component(ext "${f}" LAST_EXT)
      string(TOLOWER "${ext}" ext)
      list(FIND text_files "${ext}" at)
      if(NOT at EQUAL -1)
        file(READ "${prefix}/${f}" content)
        if(CMAKE_HOST_WIN32)
          string(TOLOWER "${content}" content)
        endif()
        foreach(form IN LISTS forms)
          string(FIND "${content}" "${form}" at)
          if(NOT at EQUAL -1)
            set(why "${f} names ${form}")
            break()
          endif()
        endforeach()
        if(NOT why STREQUAL "")
          break()
        endif()
      endif()
    endforeach()

    if(NOT why STREQUAL "")
      message(STATUS "gpp.deps.cmake: @NAME@ is not kept: its installation cannot move (${why})")
    else()
      string(RANDOM LENGTH 8 tag)
      set(staging "${entry}.${tag}.partial")
      execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${prefix}" "${staging}/install"
                      RESULT_VARIABLE rc)
      if(rc EQUAL 0)
        string(REPLACE ";" "\n" names "${installed}")
        file(WRITE "${staging}/files.txt" "${names}\n")
        file(WRITE "${staging}/entry.txt" "${stated}")
        # A rename is whole or nothing: a reader finds files.txt only in a
        # complete entry. A directory is never renamed onto one that holds
        # files, so of two builds that keep one key the second discards its
        # copy. (`NO_REPLACE` is refused for a directory.)
        file(RENAME "${staging}" "${entry}" RESULT rc)
      endif()
      if(rc EQUAL 0)
        message(STATUS "gpp.deps.cmake: @NAME@ kept as ${entry}")
      else()
        file(REMOVE_RECURSE "${staging}")
      endif()
    endif()
  endif()
endif()
)cmake";
    auto fill = [&text](std::string_view token, const std::string& value) {
        for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
            text.replace(at, token.size(), value);
    };
    using mcpp::deps::bracket;
    fill("@STEPS@", steps);
    fill("@SOURCE@", bracket(source));
    fill("@BUILD@", bracket(build));
    fill("@PREFIX@", bracket(prefix));
    fill("@RECIPE@", bracket("\n" + recipe));
    fill("@LISTED@", bracket("\n" + listed));
    fill("@CACHE@", bracket(cache));
    fill("@NAME@", name);
    return text;
}

} // namespace detail

inline prefix use(const options& opt) {
    namespace fs = std::filesystem;
    namespace ts = mcpp::plugins::toolset;
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

    // THE TOOLSET. Compilers or a toolchain file among the cache arguments
    // are the project's decision, and so is a generator it names -- each by
    // the argument that states it, not by a substring of another (0.18.0).
    const bool chosen          = chooses_compiler(opt.cache_args);
    const bool generatorChosen = chooses_generator(opt.cache_args);
    const bool visualStudio    = opt.generator == options::generator_kind::visual_studio;
    auto tools = visualStudio ? ts::resolve(opt.toolset) : ts::resolve_named(opt.toolset);
    if (!tools) {
        std::cerr << std::format("{}: {}\n", who, tools.error());
        return {};
    }
    const std::string crt = opt.crt_linkage.empty() ? tools->crt : opt.crt_linkage;
    if (!crt.empty() && crt != "static" && crt != "dynamic") {
        std::cerr << std::format("{}: options::crt_linkage is '{}'; it is \"static\" or \"dynamic\".\n",
                                 who, crt);
        return {};
    }
    const std::string ninja = mcpp::ninja_program();
    const bool named    = !chosen && tools->how == ts::mechanism::chain && !tools->cxx.empty();
    const bool useNinja = named && !generatorChosen && !ninja.empty();
    const bool instance = !chosen && !generatorChosen && tools->how == ts::mechanism::instance;

    // One build directory per toolset statement: CMake refuses a cache made
    // with another generator or instance. What 0.16.0 configured stays in
    // `build/`.
    std::string key;
    if (useNinja) key = "ninja;" + tools->identity + ";" + tools->cxx;
    else if (named) key = "named;" + tools->identity + ";" + tools->cxx;
    else if (instance) key = "instance;" + tools->instance_dir + ";" + tools->toolset_version;
    const std::string name = opt.name.empty() ? source.filename().string() : opt.name;
    const fs::path base   = fs::path(mcpp::out_dir()) / "deps-cmake" / name;
    const fs::path build  = base / (key.empty() ? std::string("build") : "build-" + mcpp::deps::short_name(key));
    const fs::path root   = base / "install";

    prefix p;
    p.root      = mcpp::deps::generic(root);
    p.include   = mcpp::deps::generic(root / opt.dirs.include);
    p.lib       = mcpp::deps::generic(root / opt.dirs.lib);
    p.bin       = mcpp::deps::generic(root / opt.dirs.bin);
    p.mechanism = std::string(ts::name(named ? ts::mechanism::chain
                                       : instance ? ts::mechanism::instance : ts::mechanism::detected));

    const std::string cmake = cmake_exe(opt);
    {
        // ONE ACTION, THREE STEPS. `cmake -P` runs a script this program
        // writes: configure (every time -- over an existing cache CMake re-runs
        // only what changed, and the arguments may have changed, which is why
        // the action ran at all), then build and install -- or, when the cache
        // holds the installation, a copy of it.
        const std::string stamp  = mcpp::deps::generic(base / (name + ".stamp"));
        const std::string id     = "deps-cmake:" + name;
        const std::string desc   = "CMAKE " + name;
        const fs::path    script = base / (name + ".cmake");
        using mcpp::deps::bracket;
        // Each configure argument, and the same argument as the cache key
        // states it: with no path of this machine. The subproject's three
        // directories are placeholders; a tool is its file name, the toolset
        // identity stating its version; the Visual Studio instance is the
        // toolset it provides.
        std::vector<std::string> configure, keyed;
        auto arg  = [&](std::string a, std::string k) { configure.push_back(std::move(a)); keyed.push_back(std::move(k)); };
        auto same = [&](const std::string& a) { arg(a, a); };
        auto tool = [&](const char* variable, const std::string& path) {
            arg(std::string("-D") + variable + "=" + path,
                std::string("-D") + variable + "=<" + fs::path(path).filename().string() + ">");
        };
        const std::string sourceS = mcpp::deps::generic(source), buildS = mcpp::deps::generic(build);
        same("-S"); arg(sourceS, "<source>");
        same("-B"); arg(buildS, "<build>");
        arg("-DCMAKE_INSTALL_PREFIX=" + p.root, "-DCMAKE_INSTALL_PREFIX=<prefix>");
        same("-DCMAKE_BUILD_TYPE=" + opt.config);
        if (!opt.prefix_path.empty()) {
            std::string joined;
            for (auto const& d : opt.prefix_path) {
                if (!joined.empty()) joined += ';';
                joined += mcpp::deps::generic(mcpp::deps::absolute_from_root(d));
            }
            same("-DCMAKE_PREFIX_PATH=" + joined);
        }
        if (useNinja) {
            same("-G");
            same("Ninja");
            tool("CMAKE_MAKE_PROGRAM", ts::forward(ninja));
        }
        if (named) {
            tool("CMAKE_C_COMPILER", tools->cc);
            tool("CMAKE_CXX_COMPILER", tools->cxx);
            if (tools->msvc_abi) {
                if (!tools->rc.empty()) tool("CMAKE_RC_COMPILER", tools->rc);
                if (!tools->mt.empty()) tool("CMAKE_MT", tools->mt);
                if (!tools->ld.empty()) tool("CMAKE_LINKER", tools->ld);
            }
        }
        if (instance) {
            // CMake's documented selection of a Visual Studio instance and of
            // a toolset version within it.
            arg("-DCMAKE_GENERATOR_INSTANCE=" + tools->instance_dir, "-DCMAKE_GENERATOR_INSTANCE=<instance>");
            if (!tools->toolset_version.empty() && tools->toolset_version != tools->instance_default) {
                same("-T");
                same("version=" + tools->toolset_version);
            }
        }
        // The C runtime follows the program's (CMake 3.15+, policy CMP0091).
        if (tools->msvc_abi && !crt.empty() && !defines(opt.cache_args, "CMAKE_MSVC_RUNTIME_LIBRARY")) {
            same("-DCMAKE_POLICY_DEFAULT_CMP0091=NEW");
            same(std::string("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>")
                 + (crt == "dynamic" ? "DLL" : ""));
        }
        for (auto const& x : opt.cache_args) same(x);

        std::string steps = "execute_process(COMMAND ${CMAKE_COMMAND}";
        for (auto const& x : configure) steps += "\n    " + bracket(x);
        steps += "\n    RESULT_VARIABLE rc)\n"
                 "if(NOT rc EQUAL 0)\n  message(FATAL_ERROR \"configure exited ${rc}\")\nendif()\n"
                 "execute_process(COMMAND ${CMAKE_COMMAND} --build " + bracket(buildS) +
                 " --config " + bracket(opt.config) + " --target install --parallel\n    RESULT_VARIABLE rc)\n"
                 "if(NOT rc EQUAL 0)\n  message(FATAL_ERROR \"build and install exited ${rc}\")\nendif()\n";
        std::string text = "# Written by gpp.deps.cmake: configure, build and install " + name + ".\n"
                           "# toolset: " + ts::describe(*tools) + "\n";
        const auto files = mcpp::deps::files_under(source);
        // A key states the compilers through the toolset's identity. On the
        // MSVC ABI that is the MSVC toolset, which a row compiler (clang-cl,
        // `compiler::row`) is not, so such an installation is not kept.
        const bool identified = instance || (named && (!tools->msvc_abi || opt.toolset.cc == ts::compiler::abi_native));
        if (!identified || opt.cache == "off") {
            text += steps;
        } else {
            std::string recipe = std::format("mcpp.plugins {}\ntoolset {} ({})\nconfig {}\n",
                                             mcpp::plugins::version, tools->identity, ts::name(tools->how),
                                             opt.config);
            for (auto const& k : keyed) recipe += "arg " + k + "\n";
            std::string listed;
            for (auto const& f : files) listed += fs::path(f).lexically_relative(source).generic_string() + "\n";
            const std::string cache = opt.cache.empty() ? std::string()
                                                        : mcpp::deps::generic(mcpp::deps::absolute_from_root(opt.cache));
            text += detail::cached_steps(name, sourceS, buildS, p.root, recipe, listed, cache, steps);
        }
        mcpp::plugins::fs::write_if_changed(script, text);
        const std::string scriptS = mcpp::deps::generic(script);
        mcpp::action a;
        a.id          = id.c_str();
        a.role        = mcpp::roles::prepare;
        a.description = desc.c_str();
        a.arg(cmake.c_str()).arg("-P").arg(scriptS.c_str());
        // Named tools run with the environment the engine runs them with, and
        // their directories first on PATH.
        if (named) {
            for (auto const& [k, v] : tools->env) a.env(k.c_str(), v.c_str());
            if (!tools->path_dirs.empty()) {
                const char sep = ts::path_separator();
                std::string path;
                for (auto const& d : tools->path_dirs) {
                    if (!path.empty()) path += sep;
                    std::string n = d;
                    if (ts::host_is_windows())
                        for (std::size_t i = 0; i < n.size(); ++i) if (n[i] == '/') n[i] = '\\';
                    path += n;
                }
                const char* cur = std::getenv("PATH");
                if (cur && *cur) { path += sep; path += cur; }
                a.env("PATH", path.c_str());
            }
        }
        a.input(cmake.c_str());
        a.input(scriptS.c_str());
        for (auto const& f : files) a.input(f.c_str());
        mcpp::deps::watch_tree(source);
        a.output(stamp.c_str());
        a.output_dir(p.root.c_str());
        a.submit();
        p.deployed = mcpp::plugins::fs::deploy_after("deps-cmake-" + name, stamp, fs::path(p.root), opt.deploy);
    }

    mcpp::include_dir(p.include.c_str());
    mcpp::deps::link_libraries(root / opt.dirs.lib, opt.libraries, opt.shared);
    if (opt.shared) mcpp::deps::runtime_directory(p.bin, p.lib);
    return p;
}

} // namespace gpp::deps::cmake
