// 基于 mcpp-plugins v0.18.1 deps/vcpkg.cppm（Apache-2.0）。
// 上游提交：6dc34901d0f12fc402f2c0e7de0e68acfb1cc9d6
// 本地改动：优先使用配置路径，否则从 PATH 查找工具；不声明或下载 xlings 工具包。
export module gpp.deps.vcpkg;

import std;
import mcpp;
import mcpp.plugins;
import mcpp.deps;
import mcpp.plugins.fs;
import mcpp.plugins.toolset;
import gpp.deps.tools;

export namespace gpp::deps::vcpkg {

struct options {
    // The vcpkg triplet. Empty derives it from the target: `x64-windows`
    // (`x64-windows-static` for a statically linked C runtime),
    // `arm64-windows`, `x64-mingw-dynamic`, `x64-linux`, `arm64-linux`,
    // `x64-osx`, `arm64-osx`. Under the `chain` mechanism the installation uses
    // a triplet derived from this one, which names the resolved tools. A
    // custom triplet is named here and found through the manifest's
    // `overlay-triplets` like any other.
    std::string triplet;
    // Library names in link order: `fmt` denotes `lib/fmt.lib` on Windows and
    // `lib/libfmt.a` or `lib/libfmt.so` elsewhere; a name with an extension
    // (`libzstd.so`) is a file name under `lib/`.
    std::vector<std::string> libraries;
    // The directory holding `vcpkg.json`. Empty searches upward from the
    // package root, so the members of a workspace find the manifest at its
    // root.
    std::string manifest_root;
    // Where vcpkg installs. Empty is vcpkg's own default,
    // `<manifest root>/vcpkg_installed`. Each triplet is its own vcpkg
    // installation, `<install root>/<triplet>`, whose prefix is
    // `<install root>/<triplet>/<triplet>`: vcpkg's manifest mode removes from
    // an installation the packages of every triplet but the one it installs,
    // so two triplets sharing one -- a target switched, a toolchain whose C++
    // library differs -- would each remove the other's prefix.
    std::string install_root;
    // Further overlay-triplet directories, beside the manifest's own.
    std::vector<std::string> overlay_triplets;
    // Arguments appended to `vcpkg install` (`--x-feature=…`, `--allow-unsupported`).
    std::vector<std::string> install_args;
    // vcpkg 安装根目录；留空时从 PATH 中 vcpkg 的真实路径定位。
    std::string vcpkg_root;
    // Files of the prefix the program reads at run time, placed beside it
    // (`{"share/opencc/t2s.json", "BaseConfig/opencc"}`): `mcpp run` finds them
    // and `mcpp pack` carries them. See `mcpp::plugins::fs::deploy_after`.
    std::vector<mcpp::plugins::fs::deploy_entry> deploy;
    // Which toolset builds the ports (0.17.0). The default `resolved` is the
    // toolset mcpp builds the program with: a Visual Studio instance is
    // selected with `VCPKG_VISUAL_STUDIO_PATH` (every port kind builds, and
    // with the instance vcpkg would choose anyway nothing is rebuilt); any
    // other toolset is named in a derived triplet `<base>-mcpp-<hash>`
    // (CMake and make ports build; an MSBuild port needs Visual Studio and is
    // refused by name). `detected` lets vcpkg find its own toolset, as 0.16.0
    // did, until 2027-03-28. See docs/deps.md.
    mcpp::plugins::toolset::choice toolset;
    // Windows 优先使用当前 mcpp 编译器旁的 clang-cl；不存在时沿用 MSVC 工具集。
    bool prefer_clang_cl = false;
    // 仅 clang-cl 开启依赖的 Release LTO；MSVC 回退时忽略，避免生成无法由 Clang 链接的 /GL 产物。
    bool lto = false;
    // The C runtime linkage of the ports on the MSVC ABI, "static" or
    // "dynamic". Empty follows the program's C++ runtime contract
    // (`mcpp::msvc_crt_linkage()`): a `self-contained` program links it
    // statically, so the default triplet becomes `<arch>-windows-static`. A
    // triplet named in `triplet` whose linkage contradicts it is refused.
    std::string crt_linkage;
};

// The prefix, by name: the installation fills it during the build, and the
// build program refers to it without looking inside (SPEC-007 R1.3).
struct prefix {
    std::string root;       // <install root>/<triplet>/<triplet>
    std::string include;    // root/include
    std::string lib;        // root/lib
    std::string bin;        // root/bin
    std::string share;      // root/share
    std::string triplet;
    std::string install_stamp; // vcpkg prepare action 的输出；发布文件必须等安装完成。
    // The copies `options::deploy` produced, for a project's own layout.
    std::vector<mcpp::plugins::fs::deployed_file> deployed;
    // How the toolset reached vcpkg: "instance", "chain" or "detected".
    std::string mechanism;
    explicit operator bool() const { return !root.empty(); }
};

// ─── The triplet ───────────────────────────────────────────────────────────

// The target's standard triplet. `crt` is the C runtime linkage on the MSVC
// ABI: "static" selects `<arch>-windows-static`, whose C runtime and libraries
// are both static.
inline std::string default_triplet(std::string_view crt = {}) {
    const std::string os = mcpp::target_os(), arch = mcpp::target_arch(), env = mcpp::target_env();
    const std::string a = arch == "x86_64" ? "x64"
                        : arch == "aarch64" ? "arm64"
                        : (arch == "i686" || arch == "x86") ? "x86" : arch;
    if (os == "windows") {
        if (env == "gnu") return a + "-mingw-dynamic";
        return crt == "static" ? a + "-windows-static" : a + "-windows";
    }
    if (os == "macos")   return a + "-osx";
    if (os == "linux")   return a + "-linux";
    return {};
}

// 解析配置中的 overlay-ports / overlay-triplets；相对路径按配置文件所在目录解析。
inline std::vector<std::filesystem::path> manifest_overlays(const std::filesystem::path& manifest_root,
                                                            std::string_view key) {
    std::vector<std::filesystem::path> out;
    const auto file = manifest_root / "vcpkg-configuration.json";
    std::ifstream in(file, std::ios::binary);
    if (!in) return out;
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    mcpp::plugins::json::value doc;
    if (!mcpp::plugins::json::parse_json(text, doc)) return out;
    if (auto const* list = doc.get(key)) {
        for (auto const& item : list->items) {
            std::filesystem::path p(item.text);
            if (p.is_relative()) p = manifest_root / p;
            out.push_back(p.lexically_normal());
        }
    }
    return out;
}

// The file that defines `triplet`, searched in vcpkg's order: overlays first.
inline std::filesystem::path triplet_file(const std::string& triplet,
                                          std::span<const std::filesystem::path> search) {
    std::error_code ec;
    for (auto const& dir : search) {
        const auto file = dir / (triplet + ".cmake");
        if (std::filesystem::is_regular_file(file, ec)) return file;
    }
    return {};
}

inline std::string read_text(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The value a triplet file gives `variable` outside any `if()` -- where a
// per-port exception lives -- the last such `set()` winning, as in CMake.
// Empty when the file does not set it there.
inline std::string top_level_setting(const std::string& text, std::string_view variable) {
    std::string want = "set(";
    for (std::size_t i = 0; i < variable.size(); ++i)
        want += char(std::tolower(static_cast<unsigned char>(variable[i])));
    std::string value;
    int depth = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        // By index: under GCC 16 a range-for over a non-const std::string in
        // a module unit fails with "inlining failed in call to always_inline
        // ... function body not available" (measured on this file).
        std::string s(line.size(), ' ');
        for (std::size_t i = 0; i < line.size(); ++i)
            s[i] = char(std::tolower(static_cast<unsigned char>(line[i])));
        const auto first = s.find_first_not_of(" \t");
        if (first == std::string::npos || s[first] == '#') continue;
        s = s.substr(first);
        if (s.starts_with("if(") || s.starts_with("if ("))           ++depth;
        else if (s.starts_with("endif(") || s.starts_with("endif (")) { if (depth) --depth; }
        else if (depth == 0 && s.starts_with(want)) {
            std::string rest = s.substr(want.size());
            const auto close = rest.find(')');
            rest = rest.substr(0, close);
            std::string v;
            for (std::size_t i = 0; i < rest.size(); ++i)
                if (rest[i] != ' ' && rest[i] != '\t' && rest[i] != '"') v += rest[i];
            value = v;
        }
    }
    return value;
}

// Whether the triplet links libraries as shared objects: its own
// `VCPKG_LIBRARY_LINKAGE`, and otherwise vcpkg's convention: dynamic on
// Windows, static elsewhere, a `-dynamic` suffix for the community triplets
// that say so in their name.
inline bool shared_linkage(const std::string& triplet,
                           std::span<const std::filesystem::path> search) {
    if (const auto file = triplet_file(triplet, search); !file.empty()) {
        mcpp::rerun_if_changed(mcpp::deps::generic(file).c_str());
        const auto v = top_level_setting(read_text(file), "VCPKG_LIBRARY_LINKAGE");
        if (!v.empty()) return v == "dynamic";
    }
    if (triplet.ends_with("-dynamic")) return true;
    if (triplet.ends_with("-static") || triplet.ends_with("-static-md")) return false;
    return mcpp::deps::is_windows();
}

// The vcpkg toolchain a chain-loaded toolchain file includes after naming the
// tools, so that ports keep vcpkg's standard flags for the target system.
inline std::string_view vcpkg_system_toolchain() {
    const std::string_view os = mcpp::target_os();
    if (os == "windows") return std::string_view(mcpp::target_env()) == "gnu" ? "mingw" : "windows";
    if (os == "macos")   return "osx";
    return "linux";
}

// The chain-loaded toolchain file. It holds no path: every tool is read from
// the installation's environment (`MCPP_VCPKG_*`, passed through untracked),
// so vcpkg's ABI hash sees the toolset's identity and its compilers and never
// where they are. A Windows path goes through `file(TO_CMAKE_PATH)`, since a
// backslash in a CMake string is an escape.
inline std::string chain_toolchain_text(bool clang_cl) {
    std::string t =
        "# Written by gpp.deps.vcpkg: the tools mcpp resolved, read from the\n"
        "# environment of the installation, then vcpkg's own toolchain for the\n"
        "# target system.\n";
    for (auto const& [var, env] : { std::pair<std::string_view, std::string_view>
                                        {"CMAKE_C_COMPILER",   "MCPP_VCPKG_CC"},
                                        {"CMAKE_CXX_COMPILER", "MCPP_VCPKG_CXX"},
                                        {"CMAKE_LINKER",       "MCPP_VCPKG_LD"},
                                        {"CMAKE_AR",           "MCPP_VCPKG_AR"},
                                        {"CMAKE_ASM_MASM_COMPILER", "MCPP_VCPKG_ASM_MASM"},
                                        {"CMAKE_RC_COMPILER",  "MCPP_VCPKG_RC"},
                                        {"CMAKE_MT",           "MCPP_VCPKG_MT"} }) {
        t += std::format("if(DEFINED ENV{{{0}}} AND NOT \"$ENV{{{0}}}\" STREQUAL \"\")\n"
                         "  file(TO_CMAKE_PATH \"$ENV{{{0}}}\" z_mcpp_tool)\n"
                         "  set({1} \"${{z_mcpp_tool}}\")\n"
                         "endif()\n", env, var);
    }
    t += "if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_SYSROOT)\n"
         "  # A compiler other than Apple's has no default SDK.\n"
         "  execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE z_mcpp_sdk\n"
         "                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)\n"
         "  if(z_mcpp_sdk)\n    set(CMAKE_OSX_SYSROOT \"${z_mcpp_sdk}\")\n  endif()\n"
         "endif()\n"
         "file(TO_CMAKE_PATH \"$ENV{MCPP_VCPKG_ROOT}\" z_mcpp_vcpkg_root)\n";
    t += std::format("include(\"${{z_mcpp_vcpkg_root}}/scripts/toolchains/{}.cmake\")\n",
                     vcpkg_system_toolchain());
    if (clang_cl) {
        // CMake 的 LLVM RC 预处理步骤只识别大写 /C，避免把代码页参数转交给 clang-cl。
        t += "string(REPLACE \"/c65001\" \"/C65001\" CMAKE_RC_FLAGS \"${CMAKE_RC_FLAGS}\")\n";
    }
    return t;
}

// What an MSBuild port meets under `chain`: vcpkg runs MSBuild with
// `/p:PlatformToolset=external` and fails without naming the cause ("msbuild:
// no such file or directory" when no Visual Studio exists). The MSBuild
// helpers read `VCPKG_PLATFORM_TOOLSET` to build that argument; the watch
// stops the port there and says why. The helper is recognised in the watch's
// CALL STACK, not its current file: a read inside a function reports the
// caller's file (the portfile), and the stack names the file that defines the
// function (measured with CMake 4.4). The pattern names the helper's own
// directory and files, so a project whose path contains "msbuild" is not
// refused.
inline std::string msbuild_refusal_text(std::string_view identity) {
    return std::format(
        "# gpp.deps.vcpkg: a port that builds with MSBuild needs a Visual Studio\n"
        "# instance, and this toolset comes from none.\n"
        "if(PORT AND NOT DEFINED Z_MCPP_MSBUILD_WATCH)\n"
        "  set(Z_MCPP_MSBUILD_WATCH 1)\n"
        "  function(z_mcpp_msbuild_watch variable access value current_file stack)\n"
        "    if(access STREQUAL \"READ_ACCESS\" AND stack MATCHES "
        "\"/vcpkg-msbuild/|vcpkg_install_msbuild\\\\.cmake|vcpkg_build_msbuild\\\\.cmake\")\n"
        "      message(FATAL_ERROR \"gpp.deps.vcpkg: the port '${{PORT}}' builds with MSBuild, which "
        "needs a Visual Studio instance; the toolset mcpp resolved ({}) comes from none. Build with "
        "the toolchain msvc@system on a machine with Visual Studio, or set the deps-vcpkg option "
        "toolset.toolset to detected for this project.\")\n"
        "    endif()\n"
        "  endfunction()\n"
        "  variable_watch(VCPKG_PLATFORM_TOOLSET z_mcpp_msbuild_watch)\n"
        "endif()\n", identity);
}

// ─── The member ────────────────────────────────────────────────────────────

inline prefix use(const options& opt = {}) {
    namespace fs = std::filesystem;
    namespace ts = mcpp::plugins::toolset;
    constexpr std::string_view who = "gpp.deps.vcpkg";
    mcpp::fact("mcpp.plugins", std::string(mcpp::plugins::version).c_str());

    // THE MANIFEST. Its absence is a mistake in the project, not a state of
    // the machine, so it is the one refusal here.
    fs::path manifestRoot = opt.manifest_root.empty()
        ? mcpp::deps::find_upward(mcpp::manifest_dir(), "vcpkg.json")
        : mcpp::deps::absolute_from_root(opt.manifest_root);
    std::error_code ec;
    if (manifestRoot.empty() || !fs::is_regular_file(manifestRoot / "vcpkg.json", ec)) {
        std::cerr << std::format(
            "{}: no vcpkg.json at or above {}.\n"
            "  This member installs the libraries a vcpkg manifest names; write one\n"
            "  (`vcpkg new --application` writes a minimal one), or name its directory\n"
            "  with options::manifest_root.\n",
            who, opt.manifest_root.empty() ? std::string(mcpp::manifest_dir()) : opt.manifest_root);
        return {};
    }

    // THE TOOLSET, and how it reaches vcpkg (mcpp.plugins.toolset).
    auto tools = ts::resolve(opt.toolset);
    bool clang_cl = false;
    if (opt.prefer_clang_cl && ts::msvc_abi() && std::string_view(mcpp::compiler()) == "clang") {
        // 只取 mcpp 当前选中的 LLVM，不从 PATH 混入另一版本的 clang-cl。
        const fs::path llvm_bin = fs::path(mcpp::tool("cxx")).parent_path();
        const auto compiler = llvm_bin / "clang-cl.exe";
        mcpp::rerun_if_changed(compiler.generic_string().c_str());
        if (fs::is_regular_file(compiler, ec)) {
            tools = ts::resolve({ts::source::resolved, ts::compiler::row});
            if (tools) {
                tools->cc = tools->cxx = compiler.generic_string();
                tools->ld = (llvm_bin / "lld-link.exe").generic_string();
                tools->ar = (llvm_bin / "llvm-lib.exe").generic_string();
                // LLVM RC 接受 /fo 等 Windows 参数，并支持 CMake 的 Clang 资源依赖扫描。
                tools->rc = (llvm_bin / "llvm-rc.exe").generic_string();
                tools->mt = mcpp::abi_tool("mt");
                // MASM、nmake 等辅助工具仍来自同一 MSVC 工具集，C/C++ 编译器保持 clang-cl。
                const auto native_bin = fs::path(mcpp::abi_tool("cxx")).parent_path().generic_string();
                if (!native_bin.empty()) tools->path_dirs.push_back(native_bin);
                // 驱动所在目录也参与 triplet 身份，切换 LLVM 时隔离安装和缓存。
                tools->identity += "; clang-cl " + compiler.generic_string();
                clang_cl = true;
            }
        }
    }
    if (!tools) {
        std::cerr << std::format("{}: {}\n", who, tools.error());
        return {};
    }

    // THE C RUNTIME. On the MSVC ABI a port's C runtime must be the program's:
    // a static library built against the other one fails the link
    // (`/failifmismatch`), and a DLL built against it puts a second C++
    // runtime into the process without a word.
    const std::string crt = opt.crt_linkage.empty() ? tools->crt : opt.crt_linkage;
    if (!crt.empty() && crt != "static" && crt != "dynamic") {
        std::cerr << std::format("{}: options::crt_linkage is '{}'; it is \"static\" or \"dynamic\".\n",
                                 who, crt);
        return {};
    }

    // ── the tool ──
    const fs::path exe = opt.vcpkg_root.empty()
        ? gpp::deps::tools::find_executable("vcpkg")
        : mcpp::deps::absolute_from_root(opt.vcpkg_root) / (ts::host_is_windows() ? "vcpkg.exe" : "vcpkg");
    const fs::path tool_root = opt.vcpkg_root.empty()
        ? mcpp::deps::find_upward(exe.parent_path(), ".vcpkg-root")
        : mcpp::deps::absolute_from_root(opt.vcpkg_root);
    if (tool_root.empty() || !fs::is_regular_file(tool_root / "scripts/buildsystems/vcpkg.cmake", ec))
        throw std::runtime_error(
            "vcpkg 未对应到完整安装目录（需要 scripts/buildsystems/vcpkg.cmake，从 PATH 查找时还需要 .vcpkg-root）；"
            "请检查顶部配置或 PATH：" + exe.generic_string());
    const std::string vcpkgRoot = mcpp::deps::generic(tool_root);

    // ── the overlays: the manifest's own, then the project's extras ──
    std::vector<fs::path> overlayTriplets = manifest_overlays(manifestRoot, "overlay-triplets");
    for (auto const& d : opt.overlay_triplets) overlayTriplets.push_back(mcpp::deps::absolute_from_root(d));
    const std::vector<fs::path> overlayPorts = manifest_overlays(manifestRoot, "overlay-ports");
    std::vector<fs::path> tripletSearch = overlayTriplets;
    if (!vcpkgRoot.empty()) {
        tripletSearch.push_back(fs::path(vcpkgRoot) / "triplets");
        tripletSearch.push_back(fs::path(vcpkgRoot) / "triplets" / "community");
    }

    // ── the base triplet: the project's, or the target's standard one ──
    const std::string base = !opt.triplet.empty() ? opt.triplet : default_triplet(tools->msvc_abi ? crt : "");
    if (base.empty()) {
        std::cerr << std::format("{}: no default vcpkg triplet for the target '{}'; name one with "
                                 "options::triplet.\n", who, std::string(mcpp::target()));
        return {};
    }
    const fs::path baseFile = triplet_file(base, tripletSearch);
    const std::string baseText = baseFile.empty() ? std::string() : read_text(baseFile);
    if (!baseFile.empty()) mcpp::rerun_if_changed(mcpp::deps::generic(baseFile).c_str());

    // Two explicit statements that cannot both hold are an error: the
    // project's triplet and the program's C runtime.
    if (tools->msvc_abi && !crt.empty() && !opt.triplet.empty() && !baseFile.empty()) {
        std::string tripletCrt = top_level_setting(baseText, "VCPKG_CRT_LINKAGE");
        if (tripletCrt.empty()) tripletCrt = "dynamic";
        if (tripletCrt != crt) {
            std::cerr << std::format(
                "{}: the triplet '{}' links the C runtime {} (VCPKG_CRT_LINKAGE), and the program "
                "links it {} ({}).\n"
                "  A static library built against the other C runtime fails the link, and a DLL "
                "built against it\n"
                "  puts a second C++ runtime into the process.\n"
                "  hint: name a triplet whose VCPKG_CRT_LINKAGE is {} ({}), or set "
                "options::crt_linkage to state the linkage the ports use.\n",
                who, base, tripletCrt, crt,
                opt.crt_linkage.empty() ? std::format("the program's C++ runtime contract, {}",
                                                      std::string(mcpp::cxx_runtime()))
                                        : std::string("options::crt_linkage"),
                crt, crt == "static" ? default_triplet("static") : default_triplet("dynamic"));
            return {};
        }
    }

    // ── the triplet vcpkg installs with ──
    //
    // `detected`, and `instance` with the instance's own toolset version, use
    // the base triplet as it stands. `chain`, and `instance` with another
    // version, use a DERIVED triplet `<base>-mcpp-<hash>`: the base inlined
    // (vcpkg hashes a triplet file's content, not a file it includes), then
    // the resolved toolset. Its name changes exactly when its content does.
    // A project triplet that chain-loads its own toolchain has decided the
    // tools, and is used as it stands.
    const bool projectChains = !top_level_setting(baseText, "VCPKG_CHAINLOAD_TOOLCHAIN_FILE").empty();
    const bool chain = tools->how == ts::mechanism::chain && !projectChains;
    const bool use_lto = opt.lto && clang_cl;
    const bool pinVersion = tools->how == ts::mechanism::instance && !tools->toolset_version.empty()
                         && !tools->instance_default.empty()
                         && tools->toolset_version != tools->instance_default;
    std::string triplet = base;
    fs::path generated;
    std::vector<std::pair<std::string, std::string>> actionEnv;
    std::vector<std::string> untracked;
    if (chain || pinVersion || use_lto) {
        generated = fs::path(mcpp::out_dir()) / "deps-vcpkg" / "triplets";
        std::string text = std::format(
            "# Written by gpp.deps.vcpkg: the triplet '{}', then the toolset mcpp resolved.\n"
            "# toolset: {}\n", base, ts::describe(*tools));
        text += baseText;
        if (!text.ends_with('\n')) text += '\n';
        text += "\n# ── gpp.deps.vcpkg ──\n";
        if (tools->msvc_abi && !crt.empty()) text += std::format("set(VCPKG_CRT_LINKAGE {})\n", crt);
        if (pinVersion)
            text += std::format("set(VCPKG_PLATFORM_TOOLSET_VERSION {})\n", tools->toolset_version);
        if (use_lto) {
            // clang-cl 生成 LLVM bitcode；Make 等直接调用编译器链接的流程也需明确选择 LLD。
            text += "string(APPEND VCPKG_C_FLAGS_RELEASE \" -flto=full -fuse-ld=lld\")\n"
                    "string(APPEND VCPKG_CXX_FLAGS_RELEASE \" -flto=full -fuse-ld=lld\")\n";
        }
        if (chain) {
            const std::string chainFile = std::format("mcpp-chain-{}.cmake", vcpkg_system_toolchain());
            mcpp::plugins::fs::write_if_changed(generated / chainFile, chain_toolchain_text(clang_cl));
            text += std::format("set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE \"${{CMAKE_CURRENT_LIST_DIR}}/{}\")\n", chainFile);
            auto put = [&](std::string name, const std::string& value) {
                if (value.empty()) return;
                actionEnv.emplace_back(name, value);
                untracked.push_back(std::move(name));
            };
            const bool win = ts::host_is_windows();
            auto native = [&](std::string p) {
                if (win) for (std::size_t i = 0; i < p.size(); ++i) if (p[i] == '/') p[i] = '\\';
                return p;
            };
            put("MCPP_VCPKG_CC",   native(tools->cc));
            put("MCPP_VCPKG_CXX",  native(tools->cxx));
            put("MCPP_VCPKG_LD",   native(tools->ld));
            put("MCPP_VCPKG_AR",   native(tools->ar));
            if (clang_cl) put("MCPP_VCPKG_ASM_MASM", native(mcpp::abi_tool("as")));
            put("MCPP_VCPKG_RC",   native(tools->rc));
            put("MCPP_VCPKG_MT",   native(tools->mt));
            put("MCPP_VCPKG_ROOT", native(vcpkgRoot));
            for (auto const& [k, v] : tools->env) put(k, v);
            std::string list;
            for (auto const& u : untracked) list += " " + u;
            text += std::format("set(VCPKG_ENV_PASSTHROUGH_UNTRACKED ${{VCPKG_ENV_PASSTHROUGH_UNTRACKED}}{})\n", list);
            // PATH: make-based ports find `link.exe` there, and vcpkg's clean
            // environment keeps the caller's PATH only when told to. The tools'
            // directories go first, then the system's own.
            if (tools->msvc_abi && win) {
                std::string path;
                for (auto const& d : tools->path_dirs) { if (!path.empty()) path += ';'; path += native(d); }
                const char* sr = std::getenv("SystemRoot");
                const std::string root = sr && *sr ? sr : "C:\\Windows";
                for (auto const& d : { root + "\\system32", root, root + "\\System32\\Wbem",
                                       root + "\\System32\\WindowsPowerShell\\v1.0" })
                    path += ";" + d;
                actionEnv.emplace_back("PATH", path);
                const char* keep = std::getenv("VCPKG_KEEP_ENV_VARS");
                actionEnv.emplace_back("VCPKG_KEEP_ENV_VARS",
                                       keep && *keep ? std::string(keep) + ";PATH" : std::string("PATH"));
                text += msbuild_refusal_text(tools->identity);
            }
        }
        triplet = std::format("{}-mcpp-{}", base, mcpp::deps::short_name(text));
        mcpp::plugins::fs::write_if_changed(generated / (triplet + ".cmake"), text);
    }
    if (tools->how == ts::mechanism::instance) {
        // The instance mcpp resolved, selected the way vcpkg documents. When
        // it is the instance vcpkg would choose anyway, the ABI hash is
        // unchanged and nothing is rebuilt.
        std::string dir = tools->instance_dir;
        for (std::size_t i = 0; i < dir.size(); ++i) if (dir[i] == '/') dir[i] = '\\';
        actionEnv.emplace_back("VCPKG_VISUAL_STUDIO_PATH", dir);
    }
    if (!generated.empty()) tripletSearch.insert(tripletSearch.begin(), generated);

    const fs::path installRoot = opt.install_root.empty()
        ? manifestRoot / "vcpkg_installed" : mcpp::deps::absolute_from_root(opt.install_root);
    const fs::path tripletRoot = installRoot / triplet;
    const fs::path root = tripletRoot / triplet;

    prefix p;
    p.root      = mcpp::deps::generic(root);
    p.include   = mcpp::deps::generic(root / "include");
    p.lib       = mcpp::deps::generic(root / "lib");
    p.bin       = mcpp::deps::generic(root / "bin");
    p.share     = mcpp::deps::generic(root / "share");
    p.triplet   = triplet;
    p.install_stamp = mcpp::deps::generic(
        fs::path(mcpp::out_dir()) / "deps-vcpkg" / (triplet + ".stamp"));
    // A project triplet that chain-loads its own toolchain is reported as
    // `detected`: this member names no tool for it.
    p.mechanism = std::string(ts::name(chain ? ts::mechanism::chain
                                       : tools->how == ts::mechanism::instance ? ts::mechanism::instance
                                       : ts::mechanism::detected));

    const bool shared = shared_linkage(triplet, tripletSearch);

    // ── the installation, as an edge ──
    const fs::path manifestFile = manifestRoot / "vcpkg.json";
    const fs::path configFile   = manifestRoot / "vcpkg-configuration.json";
    // 配置内容或文件存在性改变时重新规划，更新依赖、overlay 和安装 action。
    mcpp::rerun_if_changed(mcpp::deps::generic(manifestFile).c_str());
    mcpp::rerun_if_changed(mcpp::deps::generic(configFile).c_str());
    if (!fs::is_regular_file(exe, ec))
        throw std::runtime_error("vcpkg 可执行文件不存在：" + exe.generic_string());
    {
        // THE ACTION IS vcpkg ITSELF. Everything an installation needs is an
        // argument: `--vcpkg-root` pairs the tool with the scripts it was
        // released with, whatever `VCPKG_ROOT` the shell has; vcpkg locks the
        // installation root itself (`<root>/vcpkg/vcpkg-running.lock`), and
        // `--x-wait-for-lock` makes a second installation of the same root --
        // two workspace members, run concurrently -- wait for the first
        // instead of failing (measured on the validation project under 0.15.0); its build
        // and package trees go to a short directory under vcpkg's per-user
        // directory, because a port's build nests deep and Windows tools still
        // enforce MAX_PATH.
        const std::string& stamp = p.install_stamp;
        const std::string id    = "deps-vcpkg:install:" + triplet;
        const std::string desc  = "VCPKG install " + triplet;
        const std::string exeS  = mcpp::deps::generic(exe);
        const fs::path user     = mcpp::deps::vcpkg_user_dir();
        const fs::path work     = user / "mcpp" / mcpp::deps::short_name(mcpp::deps::generic(tripletRoot));
        mcpp::rerun_if_env_changed("VCPKG_DOWNLOADS");
        mcpp::rerun_if_env_changed("VCPKG_KEEP_ENV_VARS");
        const char* downloads = std::getenv("VCPKG_DOWNLOADS");
        mcpp::action a;
        a.id          = id.c_str();
        a.role        = mcpp::roles::prepare;
        a.description = desc.c_str();
        a.arg(exeS.c_str()).arg("install")
         .arg(("--vcpkg-root=" + vcpkgRoot).c_str())
         .arg("--disable-metrics")
         .arg("--x-wait-for-lock")
         .arg("--triplet").arg(triplet.c_str())
         .arg(("--x-manifest-root=" + mcpp::deps::generic(manifestRoot)).c_str())
         .arg(("--x-install-root=" + mcpp::deps::generic(tripletRoot)).c_str())
         .arg(("--x-buildtrees-root=" + mcpp::deps::generic(work / "bt")).c_str())
         .arg(("--x-packages-root=" + mcpp::deps::generic(work / "pk")).c_str())
         .arg("--clean-buildtrees-after-build")
         .arg("--clean-packages-after-build");
        // The host triplet is the derived one on the MSVC ABI: vcpkg would
        // otherwise build its host ports with a standard triplet and look
        // for Visual Studio, which the managed toolset does not come from.
        if (chain && tools->msvc_abi && std::string_view(mcpp::host()) == std::string_view(mcpp::target()))
            a.arg(("--host-triplet=" + triplet).c_str());
        // Downloads are shared by every project on the machine; a user who has
        // moved them already (`VCPKG_DOWNLOADS`) keeps that.
        if (!downloads || !*downloads)
            a.arg(("--downloads-root=" + mcpp::deps::generic(user / "downloads")).c_str());
        // `arg()` and `input()` copy what they are given, so the temporaries
        // below need not outlive the call.
        for (auto const& d : opt.overlay_triplets)
            a.arg(("--overlay-triplets=" + mcpp::deps::generic(mcpp::deps::absolute_from_root(d))).c_str());
        if (!generated.empty()) {
            a.arg(("--overlay-triplets=" + mcpp::deps::generic(generated)).c_str());
            for (auto const& f : mcpp::deps::files_under(generated)) a.input(f.c_str());
        }
        for (auto const& x : opt.install_args) a.arg(x.c_str());
        for (auto const& [k, v] : actionEnv) a.env(k.c_str(), v.c_str());
        // 工具更新也需要重跑安装 action，由 vcpkg 的编译器 ABI 跟踪决定是否重建。
        if (clang_cl) {
            for (const auto* tool : {&tools->cxx, &tools->ld, &tools->ar}) {
                mcpp::rerun_if_changed(tool->c_str());
                a.input(tool->c_str());
            }
        }
        a.input(exeS.c_str());
        a.input(mcpp::deps::generic(manifestFile).c_str());
        if (fs::is_regular_file(configFile, ec)) a.input(mcpp::deps::generic(configFile).c_str());
        // 按配置中的实际 overlay 路径监视文件集合；已有文件的内容另行参与重跑和安装检测。
        const auto watch_overlay = [&](const fs::path& directory) {
            const auto relative = directory.lexically_relative(fs::path(mcpp::manifest_dir()));
            const auto pattern = ((relative.empty() ? directory : relative) / "**").generic_string();
            mcpp::rerun_if_changed_glob(pattern.c_str());
            for (auto const& f : mcpp::deps::files_under(directory)) {
                mcpp::rerun_if_changed(f.c_str());
                a.input(f.c_str());
            }
        };
        for (auto const& directory : overlayTriplets) watch_overlay(directory);
        for (auto const& directory : overlayPorts) watch_overlay(directory);
        a.output(stamp.c_str());
        a.output_dir(p.root.c_str());
        a.submit();
        p.deployed = mcpp::plugins::fs::deploy_after("deps-vcpkg-" + triplet, stamp, root, opt.deploy);
    }

    // ── the prefix, into the build ──
    mcpp::include_dir(p.include.c_str());
    mcpp::deps::link_libraries(root / "lib", opt.libraries, shared);
    if (shared) mcpp::deps::runtime_directory(p.bin, p.lib);
    return p;
}

} // namespace gpp::deps::vcpkg
