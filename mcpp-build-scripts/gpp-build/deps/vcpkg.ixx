// 基于 mcpp-plugins v0.15.1 deps/vcpkg.cppm（Apache-2.0）。
// 上游提交：b28cbc95500eda2bff46896b0a01310ed21b691b
// 本地改动：优先使用配置路径，否则从 PATH 查找工具；不声明或下载 xlings 工具包。
export module gpp.deps.vcpkg;

import std;
import mcpp;
import mcpp.plugins;
import mcpp.deps;
import gpp.deps.tools;

export namespace gpp::deps::vcpkg {

struct options {
    // The vcpkg triplet. Empty derives it from the target: `x64-windows`,
    // `arm64-windows`, `x64-mingw-dynamic`, `x64-linux`, `arm64-linux`,
    // `x64-osx`, `arm64-osx`; on Linux under a libc++ toolchain, the generated
    // `x64-linux-libcxx` or `arm64-linux-libcxx`, whose ports build with mcpp's
    // clang. A custom triplet is named here and found through the manifest's
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
    // and `mcpp pack` carries them. See `mcpp::deps::deploy_after`.
    std::vector<mcpp::deps::deploy_entry> deploy;
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
    std::vector<mcpp::deps::deployed_file> deployed;
    explicit operator bool() const { return !root.empty(); }
};

// ─── The triplet ───────────────────────────────────────────────────────────

inline std::string default_triplet() {
    const std::string os = mcpp::target_os(), arch = mcpp::target_arch(), env = mcpp::target_env();
    const std::string a = arch == "x86_64" ? "x64"
                        : arch == "aarch64" ? "arm64"
                        : (arch == "i686" || arch == "x86") ? "x86" : arch;
    if (os == "windows") return env == "gnu" ? a + "-mingw-dynamic" : a + "-windows";
    if (os == "macos")   return a + "-osx";
    if (os == "linux")   return a + "-linux";
    return {};
}

// The `overlay-triplets` a `vcpkg-configuration.json` beside the manifest
// names, resolved against the file's directory as vcpkg resolves them.
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

// Whether the triplet links libraries as shared objects. Read from the
// triplet file itself -- `set(VCPKG_LIBRARY_LINKAGE dynamic)` outside any
// `if()`, which is where a per-port exception lives -- and otherwise from
// vcpkg's convention: dynamic on Windows, static elsewhere, a `-dynamic`
// suffix for the community triplets that say so in their name.
inline bool shared_linkage(const std::string& triplet,
                           std::span<const std::filesystem::path> search) {
    std::error_code ec;
    for (auto const& dir : search) {
        const auto file = dir / (triplet + ".cmake");
        if (!std::filesystem::is_regular_file(file, ec)) continue;
        mcpp::rerun_if_changed(mcpp::deps::generic(file).c_str());
        std::ifstream in(file);
        std::string line;
        int depth = 0;
        while (std::getline(in, line)) {
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
            else if (depth == 0 && s.starts_with("set(vcpkg_library_linkage")) {
                return s.find("dynamic") != std::string::npos;
            }
        }
        break;
    }
    if (triplet.ends_with("-dynamic")) return true;
    if (triplet.ends_with("-static") || triplet.ends_with("-static-md")) return false;
    return mcpp::deps::is_windows();
}

// ─── The member ────────────────────────────────────────────────────────────

inline prefix use(const options& opt = {}) {
    namespace fs = std::filesystem;
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

    // THE C++ LIBRARY. Where the host compiler's C++ library is not the
    // program's -- libc++ on Linux -- the default triplet is a generated one,
    // `<triplet>-libcxx`, whose ports build with mcpp's own clang
    // (`mcpp::deps::program_compilers`). A triplet the project names is used
    // as it stands.
    const mcpp::deps::compilers cc = opt.triplet.empty() ? mcpp::deps::program_compilers()
                                                         : mcpp::deps::compilers{};
    const std::string triplet = !opt.triplet.empty() ? opt.triplet
                              : cc ? default_triplet() + "-libcxx" : default_triplet();
    if (triplet.empty() || triplet == "-libcxx") {
        std::cerr << std::format("{}: no default vcpkg triplet for the target '{}'; name one with "
                                 "options::triplet.\n", who, std::string(mcpp::target()));
        return {};
    }

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

    // ── the tool ──
    const bool host_windows = std::string_view(mcpp::host()).find("windows") != std::string_view::npos;
    const fs::path exe = opt.vcpkg_root.empty()
        ? gpp::deps::tools::find_executable("vcpkg")
        : mcpp::deps::absolute_from_root(opt.vcpkg_root) / (host_windows ? "vcpkg.exe" : "vcpkg");
    const fs::path tool_root = opt.vcpkg_root.empty()
        ? mcpp::deps::find_upward(exe.parent_path(), ".vcpkg-root")
        : mcpp::deps::absolute_from_root(opt.vcpkg_root);
    if (tool_root.empty() || !fs::is_regular_file(tool_root / "scripts" / "buildsystems" / "vcpkg.cmake"))
        throw std::runtime_error("vcpkg 未对应到完整安装目录（需要 scripts/buildsystems/vcpkg.cmake，"
                                 "从 PATH 查找时还需要 .vcpkg-root）；请检查顶部配置或 PATH：" + exe.generic_string());
    const std::string vcpkgRoot = mcpp::deps::generic(tool_root);

    // ── the overlays: the manifest's own, then the project's extras ──
    std::vector<fs::path> overlayTriplets = manifest_overlays(manifestRoot, "overlay-triplets");
    for (auto const& d : opt.overlay_triplets) overlayTriplets.push_back(mcpp::deps::absolute_from_root(d));
    const std::vector<fs::path> overlayPorts = manifest_overlays(manifestRoot, "overlay-ports");

    // The generated triplet and the toolchain file it chains: vcpkg's own
    // Linux toolchain, with the compilers set first.
    fs::path generated;
    if (cc && !vcpkgRoot.empty()) {
        generated = fs::path(mcpp::out_dir()) / "deps-vcpkg" / "triplets";
        const std::string arch = triplet.substr(0, triplet.find('-'));
        const fs::path chain = generated / (triplet + ".toolchain.cmake");
        mcpp::deps::write_if_changed(generated / (triplet + ".cmake"), std::format(
            "# Written by gpp.deps.vcpkg: the ports build with the program's compiler,\n"
            "# so their C++ standard library is the program's.\n"
            "set(VCPKG_TARGET_ARCHITECTURE {})\n"
            "set(VCPKG_CRT_LINKAGE dynamic)\n"
            "set(VCPKG_LIBRARY_LINKAGE static)\n"
            "set(VCPKG_CMAKE_SYSTEM_NAME Linux)\n"
            "set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE \"{}\")\n",
            arch, mcpp::deps::generic(chain)));
        mcpp::deps::write_if_changed(chain, std::format(
            "set(CMAKE_C_COMPILER \"{}\")\n"
            "set(CMAKE_CXX_COMPILER \"{}\")\n"
            "include(\"{}\")\n",
            cc.c, cc.cxx, mcpp::deps::generic(fs::path(vcpkgRoot) / "scripts" / "toolchains" / "linux.cmake")));
    }

    std::vector<fs::path> tripletSearch = overlayTriplets;
    if (!generated.empty()) tripletSearch.insert(tripletSearch.begin(), generated);
    if (!vcpkgRoot.empty()) {
        tripletSearch.push_back(fs::path(vcpkgRoot) / "triplets");
        tripletSearch.push_back(fs::path(vcpkgRoot) / "triplets" / "community");
    }
    const bool shared = shared_linkage(triplet, tripletSearch);

    // ── the installation, as an edge ──
    const fs::path manifestFile = manifestRoot / "vcpkg.json";
    const fs::path configFile   = manifestRoot / "vcpkg-configuration.json";
    mcpp::rerun_if_changed(mcpp::deps::generic(configFile).c_str());
    if (exe.empty() || !fs::is_regular_file(exe, ec)) {
        throw std::runtime_error("vcpkg 可执行文件不存在：" + exe.generic_string());
    } else {
        // THE ACTION IS vcpkg ITSELF. Everything an installation needs is an
        // argument: `--vcpkg-root` pairs the tool with the scripts it was
        // released with, whatever `VCPKG_ROOT` the shell has; vcpkg locks the
        // installation root itself (`<root>/vcpkg/vcpkg-running.lock`), and
        // `--x-wait-for-lock` makes a second installation of the same root --
        // two workspace members, run concurrently -- wait for the first
        // instead of failing (measured on GalTranslPP under 0.15.0); its build
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
        a.input(exeS.c_str());
        a.input(mcpp::deps::generic(manifestFile).c_str());
        if (fs::is_regular_file(configFile, ec)) a.input(mcpp::deps::generic(configFile).c_str());
        // An overlay's files are inputs: a changed patch or triplet is a
        // different installation.
        for (auto const& d : overlayTriplets) {
            for (auto const& f : mcpp::deps::files_under(d)) a.input(f.c_str());
            mcpp::deps::watch_tree(d);
        }
        for (auto const& d : overlayPorts) {
            for (auto const& f : mcpp::deps::files_under(d)) a.input(f.c_str());
            mcpp::deps::watch_tree(d);
        }
        a.output(stamp.c_str());
        a.output_dir(p.root.c_str());
        a.submit();
        p.deployed = mcpp::deps::deploy_after("deps-vcpkg-" + triplet, stamp, root, opt.deploy);
    }

    // ── the prefix, into the build ──
    mcpp::include_dir(p.include.c_str());
    mcpp::deps::link_libraries(root / "lib", opt.libraries, shared);
    if (shared) mcpp::deps::runtime_directory(p.bin, p.lib);
    return p;
}

} // namespace gpp::deps::vcpkg
