// 基于 mcpp-plugins v0.18.1 rules/qt.cppm（Apache-2.0，见 LICENSE）。
// 上游提交：6dc34901d0f12fc402f2c0e7de0e68acfb1cc9d6
// 本地改动：Windows 不声明 Qt/bin 为运行库搜索目录；Qt DLL 和动态插件由用户部署。
// 保留 moc/uic/rcc、Qt 链接及 Linguist 翻译处理。
module;
#include <cctype>

export module gpp.rules.qt;

import std;
import mcpp;
import mcpp.plugins;

export namespace gpp::rules::qt {

enum class moc_scan {
    project_headers,   // every header under the package root that declares a meta-object
    listed,            // only `options::moc_headers`
};

struct translations {
    // `.ts` files, beside those the project names in `[build] sources`.
    std::vector<std::string> ts;
    // Run `lupdate` over the sources before `lrelease`. It writes into the
    // `.ts` files, which are part of the source tree.
    bool update_sources = false;
    // `lupdate -tr-function-alias` values, e.g. `translate+=appTr`.
    std::vector<std::string> tr_function_alias;
    // The files `lupdate` reads. Empty takes the package's C++ files.
    std::vector<std::string> sources;
    // Where the `.qm` files are placed, relative to the program.
    std::string deploy_to = "translations";
    // Where `lrelease` writes them. Empty is `<out dir>/qt/translations`; a
    // project whose own release step copies them names a directory it knows.
    std::string out_dir;
    // Qt's own UI strings for these languages (`de`, `zh_CN`): the catalogs of
    // the modules the program links, from the SDK's `translations/`, combined
    // by `lconvert` into one `qt_<language>.qm` with no dependency left to
    // resolve, and placed under `deploy_to` -- the file windeployqt writes.
    std::vector<std::string> qt_languages;
};

struct options {
    // Qt modules, `Core` / `QtCore` / `Qt6Core` alike, in link order.
    std::vector<std::string> modules = { "Core" };
    // Modules whose private headers the package includes (`QtWidgets/private/…`).
    std::vector<std::string> private_modules;
    moc_scan moc = moc_scan::project_headers;
    std::vector<std::string> moc_headers;
    // `.ui` and `.qrc` files, beside those named in `[build] sources`.
    std::vector<std::string> forms;
    std::vector<std::string> resources;
    translations i18n;
    // The SDK. Empty consults `QT_ROOT_DIR`, then the `xim:qt` or
    // `xim:qt-base` payload the project declares, with `xim:qt-addons` as a
    // second prefix (see the header).
    std::string root;
    std::vector<std::string> extra_roots;
    std::string out_dir = std::string(mcpp::out_dir());
};

// ─── The SDK ───────────────────────────────────────────────────────────────

namespace detail {

inline std::string generic(const std::filesystem::path& p) {
    return p.lexically_normal().generic_string();
}

inline bool is_windows() { return std::string_view(mcpp::target_os()) == "windows"; }
inline bool is_macos()   { return std::string_view(mcpp::target_os()) == "macos"; }
inline bool host_windows() { return std::string(mcpp::host()).find("windows") != std::string::npos; }

inline std::filesystem::path absolute_from_root(const std::string& p) {
    std::filesystem::path path(p);
    if (path.is_relative()) path = std::filesystem::path(mcpp::manifest_dir()) / path;
    return path.lexically_normal();
}

inline void warn(const std::string& message) {
    std::cerr << message << '\n';
    std::string folded;
    bool space = false;
    for (char c : message) {
        if (c == '\n') { space = true; continue; }
        if (space) { if (c == ' ') continue; folded += ' '; space = false; }
        folded += c;
    }
    mcpp::warning(folded.c_str());
}

// `Core`, `QtCore` and `Qt6Core` name one module.
inline std::string module_name(std::string m) {
    if (m.starts_with("Qt6")) m = m.substr(3);
    else if (m.starts_with("Qt")) m = m.substr(2);
    return m;
}

// The translation catalog Qt ships a module's strings in: `qtbase` for the
// modules of qtbase, the repository's name for the others. A module with no
// catalog of its own has its strings in qtbase's.
inline std::string catalog_of(const std::string& module) {
    static const std::vector<std::pair<std::string, std::vector<std::string>>> table = {
        {"qtdeclarative", {"Qml", "Quick", "QuickControls2", "QuickWidgets", "QuickDialogs2"}},
        {"qtmultimedia",  {"Multimedia", "MultimediaWidgets"}},
        {"qtserialport",  {"SerialPort"}},
        {"qtwebsockets",  {"WebSockets"}},
        {"qtconnectivity", {"Bluetooth", "Nfc"}},
        {"qtlocation",    {"Location"}},
        {"qtwebengine",   {"WebEngineCore", "WebEngineWidgets", "WebEngineQuick"}},
        {"qt_help",       {"Help"}},
    };
    for (auto const& [catalog, modules] : table)
        for (auto const& m : modules)
            if (m == module) return catalog;
    return "qtbase";
}

// By index, as `mcpp.deps.vcpkg` lowers a triplet line: GCC 16 cannot inline a
// non-const std::string iterator in a module unit.
inline std::string upper(std::string s) {
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = char(std::toupper(static_cast<unsigned char>(s[i])));
    return s;
}

// A Qt tool: `bin/` on Windows, `libexec/` for the code generators elsewhere
// (Qt 6 moved `moc`, `uic` and `rcc` there), `bin/` for the Linguist tools.
inline std::string tool(std::span<const std::filesystem::path> roots, const char* name) {
    std::error_code ec;
    const std::string file = std::string(name) + (host_windows() ? ".exe" : "");
    for (auto const& r : roots)
        for (auto const* sub : { "bin", "libexec" })
            if (std::filesystem::is_regular_file(r / sub / file, ec)) return generic(r / sub / file);
    return {};
}

// Files under the package root with one of `exts`, skipping build output and
// version control, and the output directories of another build system
// (`mcpp::plugins::tree`, 0.18.1). Sorted, so the plan does not depend on
// directory order.
inline std::vector<std::filesystem::path> project_files(std::initializer_list<std::string_view> exts) {
    std::vector<std::filesystem::path> out;
    const std::filesystem::path root = mcpp::manifest_dir();
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        const auto name = it->path().filename().string();
        if (it->is_directory(ec)) {
            if (name == "target" || name == ".git" || name == "mcpp-generated" ||
                name == "vcpkg_installed" || name == "node_modules" ||
                mcpp::plugins::tree::build_output(it->path()))
                it.disable_recursion_pending();
            continue;
        }
        const auto ext = it->path().extension().string();
        for (auto e : exts) if (ext == e) { out.push_back(it->path()); break; }
    }
    std::ranges::sort(out);
    return out;
}

inline std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

inline bool declares_meta_object(const std::string& text) {
    for (auto m : { "Q_OBJECT", "Q_GADGET", "Q_NAMESPACE" }) {
        for (std::size_t at = text.find(m); at != std::string::npos; at = text.find(m, at + 1)) {
            const bool left  = at == 0 || !(std::isalnum(static_cast<unsigned char>(text[at - 1])) || text[at - 1] == '_');
            const std::size_t end = at + std::strlen(m);
            // `Q_OBJECT`, `Q_GADGET_EXPORT(…)`, `Q_NAMESPACE_EXPORT(…)`.
            const bool right = end >= text.size() || !(std::isalnum(static_cast<unsigned char>(text[end])))
                               || text.compare(end, 7, "_EXPORT") == 0;
            if (left && right) return true;
        }
    }
    return false;
}

// The device sources the project names, of one extension.
inline std::vector<std::string> device(std::string_view ext) {
    std::vector<std::string> out;
    const std::string all = mcpp::device_sources();
    std::size_t i = 0;
    while (i <= all.size()) {
        auto nl = all.find('\n', i);
        std::string one = all.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
        i = nl == std::string::npos ? all.size() + 1 : nl + 1;
        while (!one.empty() && (one.back() == ' ' || one.back() == '\r')) one.pop_back();
        if (one.size() > ext.size() && one.ends_with(ext)) out.push_back(one);
    }
    return out;
}

// The `<file>` entries of a `.qrc`, resolved against its directory: what `rcc`
// reads, so each is an input of the action that runs it.
inline std::vector<std::string> qrc_files(const std::filesystem::path& qrc) {
    std::vector<std::string> out;
    mcpp::plugins::xml::node doc;
    std::string error;
    if (!mcpp::plugins::xml::parse(read_file(qrc), doc, error)) return out;
    std::function<void(const mcpp::plugins::xml::node&)> walk = [&](const mcpp::plugins::xml::node& n) {
        if (n.name == "file") {
            // XML 解析器将元素中的文本保存为 name 为空的子节点。
            std::string text;
            for (auto const& child : n.children) {
                if (child.name.empty()) text += child.text;
            }
            text = mcpp::plugins::xml::trim_copy(text);
            if (!text.empty()) out.push_back(generic(qrc.parent_path() / text));
        }
        for (auto const& c : n.children) walk(c);
    };
    walk(doc);
    return out;
}

// The directory `include/Qt<Module>/<version>` private headers live in.
inline std::filesystem::path private_dir(const std::filesystem::path& root, const std::string& module) {
    std::error_code ec;
    const auto base = is_macos() ? root / "lib" / ("Qt" + module + ".framework") / "Headers"
                                 : root / "include" / ("Qt" + module);
    for (auto const& e : std::filesystem::directory_iterator(base, ec))
        if (e.is_directory(ec) && !e.path().filename().string().empty() &&
            std::isdigit(static_cast<unsigned char>(e.path().filename().string()[0])))
            return e.path();
    return {};
}

} // namespace detail

// The SDKs this build uses: `options::root` and `extra_roots`, or the xim
// payloads. Empty when none is present.
// Where the SDK came from: the level that named it, and its prefixes.
struct sdk_source {
    std::string level;   // "options", "QT_ROOT_DIR", "xlings", or empty
    std::vector<std::filesystem::path> roots;
};

inline sdk_source locate(const options& opt = {}) {
    namespace fs = std::filesystem;
    sdk_source out;
    std::error_code ec;
    auto add = [&](const fs::path& p) {
        if (!p.empty() && fs::is_directory(p, ec)) out.roots.push_back(p);
    };
    auto extras = [&] {
        for (auto const& r : opt.extra_roots) add(detail::absolute_from_root(r));
    };
    // 1. The build program.
    if (!opt.root.empty()) {
        out.level = "options";
        add(detail::absolute_from_root(opt.root));
        extras();
        return out;
    }
    // 2. The machine.
    mcpp::rerun_if_env_changed("QT_ROOT_DIR");
    if (const char* env = std::getenv("QT_ROOT_DIR"); env && *env) {
        out.level = "QT_ROOT_DIR";
        add(fs::path(env));
        extras();
        return out;
    }
    // 3. A declared payload. `xim:qt` is the full base and `xim:qt-base` its
    // qtbase + qttools subset; a project that declares both uses the full one.
    const std::string full = mcpp::xpkg_dir("xim", "qt");
    const std::string base = full.empty() ? std::string(mcpp::xpkg_dir("xim", "qt-base")) : full;
    if (!base.empty()) {
        out.level = "xlings";
        add(fs::path(base));
        if (const std::string addons = mcpp::xpkg_dir("xim", "qt-addons"); !addons.empty())
            add(fs::path(addons));
    }
    extras();
    return out;
}

inline std::vector<std::filesystem::path> roots(const options& opt = {}) {
    return locate(opt).roots;
}

// The first SDK, for a project that hands it to something else (a CMake
// subproject's `CMAKE_PREFIX_PATH`). Empty when none is present.
inline std::string root(const options& opt = {}) {
    auto r = roots(opt);
    return r.empty() ? std::string() : detail::generic(r.front());
}

// ─── What the program loads ────────────────────────────────────────────────

// 非 Windows 平台保留 Qt 的运行库搜索路径；Windows 的 Qt DLL 由用户部署。
inline void runtime_directories(std::span<const std::filesystem::path> roots) {
    if (detail::is_windows()) return;
    for (auto const& r : roots) {
        const std::string dir = detail::generic(r / "lib");
        mcpp::runtime_search_dir(dir.c_str());
    }
}

// ─── The rule ──────────────────────────────────────────────────────────────

inline bool compile(options opt = {}) {
    namespace fs = std::filesystem;
    using detail::generic;
    constexpr std::string_view who = "gpp.rules.qt";
    mcpp::fact("mcpp.plugins", std::string(mcpp::plugins::version).c_str());

    const sdk_source source = locate(opt);
    const auto& sdks = source.roots;
    if (sdks.empty()) {
        // Reached by a build program that asked for Qt: since mcpp 2026.9.27.1
        // (mcpp#715) a package that enables `rules-qt` only to import this
        // module, and has no `.ui`, `.qrc` or `.ts` of its own, gets no
        // synthesised program at all.
        detail::warn(std::format(
            "{}: no Qt SDK{}. Nothing Qt-specific is planned. Name one with options::root or "
            "QT_ROOT_DIR, or declare a payload, which `mcpp build` provisions:\n"
            "    [target.'cfg(any(windows, linux, macos))'.xlings.workspace]\n"
            "    \"xim:qt-base\" = \"6.11.1\"",
            who, source.level.empty() ? std::string()
                                      : " at the directory " + source.level + " names"));
        return true;
    }
    mcpp::fact("rules-qt.sdk", std::format("{}: {}", source.level, generic(sdks.front())).c_str());
    // Qt's official Linux binaries are built with GCC against libstdc++, and
    // their interface carries `std::` types; a libc++ program does not link them.
    if (!detail::is_windows() && !detail::is_macos()
        && std::string_view(mcpp::cxx_stdlib()) == "libc++" && source.level == "xlings")
        detail::warn(std::format(
            "{}: the Qt SDK is Qt's official Linux build, whose C++ standard library is "
            "libstdc++, and this toolchain's is libc++; build with a gcc toolchain "
            "(`--toolchain gcc@<version>`), or name a Qt built with libc++.", who));
    const fs::path main = sdks.front();
    const fs::path gen  = fs::path(opt.out_dir) / "qt";
    std::error_code ec;
    fs::create_directories(gen, ec);

    // ── the modules ──
    std::vector<std::string> modules;
    for (auto const& m : opt.modules) {
        const auto name = detail::module_name(m);
        if (std::ranges::find(modules, name) == modules.end()) modules.push_back(name);
    }
    std::vector<std::string> missing;
    for (auto const& r : sdks) {
        if (detail::is_macos()) {
            const std::string f = "-F" + generic(r / "lib");
            mcpp::cxxflag(f.c_str());
        } else {
            mcpp::include_dir(generic(r / "include").c_str());
        }
    }
    for (auto const& m : modules) {
        bool found = false;
        for (auto const& r : sdks) {
            fs::path lib, headers;
            if (detail::is_windows()) {
                lib = r / "lib" / ("Qt6" + m + ".lib");
                headers = r / "include" / ("Qt" + m);
            } else if (detail::is_macos()) {
                lib = r / "lib" / ("Qt" + m + ".framework") / ("Qt" + m);
                headers = r / "lib" / ("Qt" + m + ".framework") / "Headers";
            } else {
                lib = r / "lib" / ("libQt6" + m + ".so");
                headers = r / "include" / ("Qt" + m);
            }
            if (!fs::exists(lib, ec)) continue;
            mcpp::include_dir(generic(headers).c_str());
            mcpp::link_flag(generic(lib).c_str());
            const std::string def = "QT_" + detail::upper(m) + "_LIB";
            mcpp::define(def.c_str());
            found = true;
            break;
        }
        if (!found) missing.push_back(m);
    }
    for (auto const& m : opt.private_modules) {
        const auto name = detail::module_name(m);
        for (auto const& r : sdks) {
            const auto dir = detail::private_dir(r, name);
            if (dir.empty()) continue;
            mcpp::include_dir(generic(dir).c_str());
            mcpp::include_dir(generic(dir / ("Qt" + name)).c_str());
            break;
        }
    }
    if (!missing.empty()) {
        // An incomplete SDK is a state of the machine: said, and the rest of
        // the configuration stated, so the link is where it fails (R1.2).
        std::string list;
        for (auto const& m : missing) list += " " + m;
        detail::warn(std::format(
            "{}: module(s){} not found in {}. The base package carries Core, Gui, Widgets, "
            "Network, Svg, Qml, Quick and the other qtbase/qtdeclarative modules; the "
            "additional libraries (Multimedia, Charts, WebSockets, ...) are `xim:qt-addons`, "
            "which the project declares beside `xim:qt`.",
            who, list, generic(main)));
    }

    // ── the compiler and the loader ──
    if (std::string_view(mcpp::compiler()) == "msvc") {
        // Qt 6 refuses a `__cplusplus` that does not state the standard, which
        // is cl.exe's default.
        mcpp::cxxflag("/Zc:__cplusplus");
        mcpp::cxxflag("/permissive-");
    }
    if (!detail::is_windows() && !detail::is_macos()) {
        // Qt's Linux libraries are built with `-reduce-relocations`, and its
        // headers refuse position-dependent code: `-fPIC`, as Qt's own CMake
        // package requires of every consumer.
        mcpp::cxxflag("-fPIC");
    }
    runtime_directories(sdks);

    // ── moc ──
    const std::string moc = detail::tool(sdks, "moc");
    std::vector<fs::path> headers;
    if (opt.moc == moc_scan::listed) {
        for (auto const& h : opt.moc_headers) headers.push_back(detail::absolute_from_root(h));
    } else {
        for (auto const* g : { "**/*.h", "**/*.hpp", "**/*.hxx" }) mcpp::rerun_if_changed_glob(g);
        for (auto const& h : detail::project_files({ ".h", ".hpp", ".hxx" })) {
            mcpp::rerun_if_changed(generic(h).c_str());
            if (detail::declares_meta_object(detail::read_file(h))) headers.push_back(h);
        }
    }
    // A source that includes its own `<stem>.moc` declares a meta-object in the
    // source itself; its moc output is a header that source includes.
    std::vector<fs::path> inlineMoc;
    if (opt.moc == moc_scan::project_headers) {
        for (auto const* g : { "**/*.cpp", "**/*.cc", "**/*.cxx" }) mcpp::rerun_if_changed_glob(g);
        for (auto const& c : detail::project_files({ ".cpp", ".cc", ".cxx" })) {
            const auto text = detail::read_file(c);
            if (text.find("\"" + c.stem().string() + ".moc\"") != std::string::npos &&
                detail::declares_meta_object(text))
                inlineMoc.push_back(c);
        }
    }
    if ((!headers.empty() || !inlineMoc.empty()) && moc.empty()) {
        detail::warn(std::format("{}: `moc` not found under {} (bin/ or libexec/); no meta-object "
                                 "code is generated.", who, generic(main)));
        headers.clear();
        inlineMoc.clear();
    }
    // EVERY GENERATED NAME HAS ONE SOURCE. Each generator names its output
    // after the input's stem, in one directory, so two inputs with one stem in
    // different directories would be two actions writing one file; refused
    // naming both, for moc, uic, rcc and lrelease alike.
    std::map<std::string, fs::path> claimed;
    auto claim = [&](const std::string& outName, const fs::path& in) -> bool {
        auto [it, fresh] = claimed.try_emplace(outName, in);
        if (fresh) return true;
        std::cerr << std::format("{}: two files produce `{}`: {} and {}. Generated files are "
                                 "named after the input's stem; rename one.\n",
                                 who, outName, generic(it->second), generic(in));
        return false;
    };
    auto mocOne = [&](const fs::path& in, const std::string& outName) -> bool {
        if (!claim(outName, in)) return false;
        const std::string out = generic(gen / outName);
        const std::string dep = out + ".d";
        const std::string src = generic(in);
        const std::string id  = "qt:moc:" + outName;
        const std::string desc = "MOC " + in.filename().string();
        mcpp::action a;
        a.id = id.c_str();
        a.role = mcpp::roles::source;
        a.description = desc.c_str();
        a.depfile = dep.c_str();
        a.arg(moc.c_str()).arg(src.c_str()).arg("-o").arg(out.c_str())
         .arg("--output-dep-file").arg("--dep-file-path").arg(dep.c_str())
         .input(src.c_str()).output(out.c_str()).submit();
        return true;
    };
    for (auto const& h : headers)
        if (!mocOne(h, "moc_" + h.stem().string() + ".cpp")) return false;
    for (auto const& c : inlineMoc)
        if (!mocOne(c, c.stem().string() + ".moc")) return false;

    // ── uic ──
    std::vector<std::string> forms = detail::device(".ui");
    for (auto const& f : opt.forms) forms.push_back(f);
    if (!forms.empty()) {
        const std::string uic = detail::tool(sdks, "uic");
        if (uic.empty()) {
            detail::warn(std::format("{}: `uic` not found under {}; no form is generated.", who, generic(main)));
            forms.clear();
        }
        for (auto const& f : forms) {
            if (!claim("ui_" + fs::path(f).stem().string() + ".h", detail::absolute_from_root(f))) return false;
            const std::string in  = generic(detail::absolute_from_root(f));
            const std::string out = generic(gen / ("ui_" + fs::path(f).stem().string() + ".h"));
            const std::string id  = "qt:uic:" + fs::path(f).stem().string();
            const std::string desc = "UIC " + fs::path(f).filename().string();
            mcpp::action a;
            a.id = id.c_str();
            a.role = mcpp::roles::source;
            a.description = desc.c_str();
            a.arg(uic.c_str()).arg(in.c_str()).arg("-o").arg(out.c_str())
             .input(in.c_str()).output(out.c_str()).submit();
        }
    }
    if (!forms.empty() || !inlineMoc.empty()) mcpp::include_dir(generic(gen).c_str());

    // ── rcc ──
    std::vector<std::string> resources = detail::device(".qrc");
    for (auto const& r : opt.resources) resources.push_back(r);
    if (!resources.empty()) {
        const std::string rcc = detail::tool(sdks, "rcc");
        if (rcc.empty()) {
            detail::warn(std::format("{}: `rcc` not found under {}; no resource is compiled.", who, generic(main)));
            resources.clear();
        }
        for (auto const& r : resources) {
            const fs::path qrc = detail::absolute_from_root(r);
            const std::string stem = qrc.stem().string();
            if (!claim("qrc_" + stem + ".cpp", qrc)) return false;
            const std::string in  = generic(qrc);
            const std::string out = generic(gen / ("qrc_" + stem + ".cpp"));
            const std::string id  = "qt:rcc:" + stem;
            const std::string desc = "RCC " + qrc.filename().string();
            mcpp::rerun_if_changed(in.c_str());
            mcpp::action a;
            a.id = id.c_str();
            a.role = mcpp::roles::source;
            a.description = desc.c_str();
            a.arg(rcc.c_str()).arg("--name").arg(stem.c_str()).arg(in.c_str()).arg("-o").arg(out.c_str())
             .input(in.c_str());
            for (auto const& f : detail::qrc_files(qrc)) a.input(f.c_str());
            a.output(out.c_str()).submit();
        }
    }

    // ── translations ──
    std::vector<std::string> ts = detail::device(".ts");
    for (auto const& t : opt.i18n.ts) ts.push_back(t);
    if (!ts.empty()) {
        const std::string lrelease = detail::tool(sdks, "lrelease");
        const std::string lupdate  = detail::tool(sdks, "lupdate");
        if (lrelease.empty() || (opt.i18n.update_sources && lupdate.empty())) {
            detail::warn(std::format("{}: `{}` not found under {}; it is part of qttools, and no "
                                     "translation is released.",
                                     who, lrelease.empty() ? "lrelease" : "lupdate", generic(main)));
            ts.clear();
        }
        std::vector<std::string> sources;
        if (opt.i18n.update_sources) {
            if (!opt.i18n.sources.empty()) {
                for (auto const& s : opt.i18n.sources) sources.push_back(generic(detail::absolute_from_root(s)));
            } else {
                for (auto const* g : { "**/*.cpp", "**/*.h", "**/*.hpp", "**/*.ixx", "**/*.cppm" })
                    mcpp::rerun_if_changed_glob(g);
                for (auto const& f : detail::project_files({ ".cpp", ".h", ".hpp", ".ixx", ".cppm" }))
                    sources.push_back(generic(f));
            }
        }
        for (auto const& t : ts) {
            const fs::path file = detail::absolute_from_root(t);
            const std::string stem = file.stem().string();
            if (!claim(stem + ".qm", file)) return false;
            const std::string in = generic(file);
            const fs::path qmDir = opt.i18n.out_dir.empty() ? gen / "translations"
                                                            : detail::absolute_from_root(opt.i18n.out_dir);
            const std::string qm = generic(qmDir / (stem + ".qm"));
            if (opt.i18n.update_sources) {
                const std::string id = "qt:lupdate:" + stem;
                const std::string desc = "LUPDATE " + file.filename().string();
                mcpp::action u;
                u.id = id.c_str();
                // The file lupdate writes is named before it runs, so the action
                // names it as its output (SPEC-007 R3.2) and needs neither a
                // stamp nor a `prepare` directory. `lrelease` takes the same file
                // as its input, which orders the two.
                u.role = mcpp::roles::source;
                u.description = desc.c_str();
                u.arg(lupdate.c_str()).arg("-silent").arg("-extensions").arg("cpp,h,hpp,ixx,cppm");
                for (auto const& a : opt.i18n.tr_function_alias) u.arg("-tr-function-alias").arg(a.c_str());
                for (auto const& s : sources) u.arg(s.c_str()).input(s.c_str());
                u.arg("-ts").arg(in.c_str()).output(in.c_str()).submit();
            }
            const std::string id = "qt:lrelease:" + stem;
            const std::string desc = "LRELEASE " + file.filename().string();
            mcpp::action r;
            r.id = id.c_str();
            r.role = mcpp::roles::source;
            r.description = desc.c_str();
            r.arg(lrelease.c_str()).arg("-silent").arg(in.c_str()).arg("-qm").arg(qm.c_str()).input(in.c_str());
            r.output(qm.c_str()).submit();
            mcpp::deploy(qm.c_str(), opt.i18n.deploy_to.c_str());
        }
    }

    // ── Qt's own translations ──
    if (!opt.i18n.qt_languages.empty()) {
        const std::string lconvert = detail::tool(sdks, "lconvert");
        const fs::path qmDir = opt.i18n.out_dir.empty() ? gen / "translations"
                                                        : detail::absolute_from_root(opt.i18n.out_dir);
        std::vector<std::string> catalogs = {"qtbase"};
        for (auto const& m : opt.modules) {
            const std::string c = detail::catalog_of(detail::module_name(m));
            if (std::ranges::find(catalogs, c) == catalogs.end()) catalogs.push_back(c);
        }
        if (lconvert.empty()) {
            detail::warn(std::format("{}: `lconvert` not found under {}; it is part of qttools, and "
                                     "Qt's own translations are not placed.", who, generic(main)));
        } else {
            for (auto const& lang : opt.i18n.qt_languages) {
                std::vector<std::string> inputs;
                for (auto const& c : catalogs)
                    for (auto const& r : sdks)
                        if (const auto qm = r / "translations" / (c + "_" + lang + ".qm"); fs::is_regular_file(qm, ec)) {
                            inputs.push_back(generic(qm));
                            break;
                        }
                if (inputs.empty()) {
                    detail::warn(std::format("{}: the SDK has no Qt translation for '{}' (no "
                                             "translations/qtbase_{}.qm under {}).", who, lang, lang, generic(main)));
                    continue;
                }
                const std::string name = "qt_" + lang + ".qm";
                if (!claim(name, fs::path(inputs.front()))) return false;
                const std::string out  = generic(qmDir / name);
                const std::string id   = "qt:lconvert:" + lang;
                const std::string desc = "LCONVERT " + name;
                mcpp::action a;
                a.id = id.c_str();
                a.role = mcpp::roles::source;
                a.description = desc.c_str();
                a.arg(lconvert.c_str()).arg("-o").arg(out.c_str());
                for (auto const& in : inputs) a.arg("-i").arg(in.c_str()).input(in.c_str());
                a.output(out.c_str()).submit();
                mcpp::deploy(out.c_str(), opt.i18n.deploy_to.c_str());
            }
        }
    }

    return true;
}

} // namespace gpp::rules::qt
