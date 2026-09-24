#pragma once

// Shared build-graph actions for the three executable workspace members.
// This header is included only by build.mcpp programs (which import std and mcpp).

#include "mcpp_translations.hpp"

struct executable_actions {
    using path = std::filesystem::path;

    path project = path(mcpp::manifest_dir());
    path workspace = project.parent_path();
    path release = workspace / "Release";
    path qt = qt_root_path();
    path vcpkg = workspace / "vcpkg_installed" / "gpp-x64-windows-release";
    std::string target;
    std::string target_file;
    unsigned next_action = 0;

    explicit executable_actions(std::string name)
        : target(std::move(name)), target_file("${mcpp.target_file:" + target + "}") {
        mcpp::rerun_if_changed(qt_config_path().string().c_str());
        // Private release directories are optional, as in the VS post-build events.
        mcpp::rerun_if_changed(release.string().c_str());
    }

    bool ready() const {
        if (qt.empty() || !std::filesystem::exists(qt / "bin" / "lrelease.exe")) {
            std::cerr << "Qt tools missing; check mcpp-build/qt-root.txt\n";
            return false;
        }
        if (!std::filesystem::is_directory(vcpkg / "bin")) {
            std::cerr << "vcpkg runtime bin missing; install project dependencies with the gpp-x64-windows-release triplet\n";
            return false;
        }
        return true;
    }

    void copy(std::string source, const path& destination) {
        const auto output = destination.lexically_normal().string();
        const auto id = "stage-" + std::to_string(next_action++);
        mcpp::action action;
        action.id = id.c_str();
        action.role = "artifact";
        action.arg("${mcpp.self}").arg("stage").arg("--verify").arg("content")
              .arg("--output").arg(output.c_str()).arg(source.c_str())
              .input(target_file.c_str()).input(source.c_str())
              .output(output.c_str()).submit();
    }

    void copy_file(const path& source, const path& destination) {
        copy(source.lexically_normal().string(), destination);
    }

    void copy_if_present(const path& source, const path& destination) {
        if (std::filesystem::is_regular_file(source)) copy_file(source, destination);
    }

    void copy_matching(const path& dir, const path& destination,
                       std::string_view prefix = {}) {
        if (!std::filesystem::is_directory(dir)) return;
        std::vector<path> files;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".dll") continue;
            if (!prefix.empty() && !entry.path().filename().string().starts_with(prefix)) continue;
            files.push_back(entry.path());
        }
        std::ranges::sort(files);
        for (const auto& source : files) copy_file(source, destination / source.filename());
    }

    void copy_plugins(const path& destination) {
        for (const char* name : {"generic", "iconengines", "imageformats",
                                  "networkinformation", "platforms", "styles", "tls"}) {
            const auto dir = qt / "plugins" / name;
            if (!std::filesystem::is_directory(dir)) continue;
            std::vector<path> files;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
                if (entry.is_regular_file()) files.push_back(entry.path());
            std::ranges::sort(files);
            for (const auto& source : files)
                copy_file(source, destination / name / source.lexically_relative(dir));
        }
    }

    bool publish(std::string_view member) {
        const auto ts = project / (member == "GPPCLI" ? "qt_gppcli_en.ts" :
                                   member == "GPPGUI" ? "qt_gppgui_en.ts" :
                                                        "qt_gppupdater_en.ts");
        const auto own_qm = qt_translation(project, ts,
                                           path(mcpp::out_dir()) / (ts.stem().string() + ".qm"));
        if (own_qm.empty()) return false;
        if (std::string_view(mcpp::profile()) != "release") return true;
        if (!ready()) return false;
        const bool cli = member == "GPPCLI";
        const bool gui = member == "GPPGUI";
        const auto base = release / (cli ? "GPPCLI" : "GPPGUI");
        const auto mirror = release / (cli ? "GPPCLI_PRIVATE" : "GPPGUI_PRIVATE");
        const bool private_exists = std::filesystem::is_directory(mirror);
        std::vector<path> runtime_destinations{base};
        std::vector<path> translations{base};
        if (gui) {
            runtime_destinations.push_back(release / "GUICORE");
            translations.push_back(release / "GUICORE");
        }
        if ((cli || gui) && private_exists) {
            runtime_destinations.push_back(mirror);
            translations.push_back(mirror);
        }

        copy(target_file, base / (target + ".exe"));
        if (gui) copy(target_file, release / "GUICORE" / (target + ".exe"));
        if (member == "Updater") {
            copy(target_file, release / "GUICORE" / "Updater_new.exe");
            if (private_exists) copy(target_file, mirror / "Updater_new.exe");
        } else if (private_exists) {
            copy(target_file, mirror / (target + ".exe"));
        }

        for (const auto& dir : runtime_destinations) {
            copy_matching(vcpkg / "bin", dir);
            copy_matching(workspace / "3rdParty" / "pybind11" / "bin", dir, "python");
            copy_if_present(workspace / "3rdParty" / "7z.dll", dir / "7z.dll");
            const auto qt_dlls = cli
                ? std::vector<std::string>{"Qt6Core.dll"}
                : member == "Updater"
                    ? std::vector<std::string>{"Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll"}
                    : std::vector<std::string>{"Qt6Core.dll", "Qt6Gui.dll", "Qt6Network.dll",
                                               "Qt6Widgets.dll", "Qt6Svg.dll", "opengl32sw.dll"};
            for (const auto& dll : qt_dlls)
                copy_if_present(qt / "bin" / dll, dir / dll);
            if (gui) copy_if_present(
                workspace / "3rdParty" / "ElaWidgetTools" / "Install" /
                "ElaWidgetTools" / "bin" / "ElaWidgetTools.dll",
                dir / "ElaWidgetTools.dll");
        }
        if (!cli) copy_plugins(base);

        for (const auto& dir : translations) {
            copy_file(own_qm, dir / "translations" / own_qm.filename());
            if (cli || gui) {
                const auto core_qm = workspace / "GalTranslPP" / "qt_gpp_en.qm";
                copy_file(core_qm, dir / "translations" / core_qm.filename());
            }
        }
        return true;
    }

    bool generate_gui_sources() {
        const auto moc = qt / "bin" / "moc.exe";
        const auto rcc = qt / "bin" / "rcc.exe";
        if (!std::filesystem::is_regular_file(moc) || !std::filesystem::is_regular_file(rcc)) {
            std::cerr << "Qt moc/rcc missing under " << qt << '\n';
            return false;
        }
        mcpp::rerun_if_changed_glob("**/*.h");
        mcpp::rerun_if_changed_glob("Resource/**");
        std::vector<path> headers;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(project)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".h") continue;
            const auto relative = entry.path().lexically_relative(project);
            if (relative.empty() || *relative.begin() == "target" ||
                *relative.begin() == "mcpp-generated") continue;
            mcpp::rerun_if_changed(entry.path().string().c_str());
            std::ifstream input(entry.path());
            const std::string content(std::istreambuf_iterator<char>{input}, {});
            if (content.find("Q_OBJECT") != std::string::npos ||
                content.find("Q_GADGET") != std::string::npos ||
                content.find("Q_NAMESPACE") != std::string::npos)
                headers.push_back(entry.path());
        }
        std::ranges::sort(headers);
        for (const auto& header : headers) {
            const auto output = path(mcpp::out_dir()) / ("moc_" + header.stem().string() + ".cpp");
            const auto in = header.string();
            const auto out = output.string();
            const auto tool = moc.string();
            const auto id = "moc-" + header.stem().string();
            mcpp::action action;
            action.id = id.c_str();
            action.role = "source";
            action.arg(tool.c_str()).arg(in.c_str()).arg("-o").arg(out.c_str())
                  .input(in.c_str()).output(out.c_str()).submit();
        }
        const auto qrc = project / "GPPGUI.qrc";
        const auto output = path(mcpp::out_dir()) / "qrc_GPPGUI.cpp";
        const auto in = qrc.string();
        const auto out = output.string();
        const auto tool = rcc.string();
        mcpp::action action;
        action.id = "rcc-GPPGUI";
        action.role = "source";
        action.arg(tool.c_str()).arg(in.c_str()).arg("-o").arg(out.c_str())
              .input(in.c_str()).output(out.c_str());
        for (const auto& entry : std::filesystem::recursive_directory_iterator(project / "Resource")) {
            if (entry.is_regular_file()) {
                const auto resource = entry.path().string();
                action.input(resource.c_str());
            }
        }
        action.submit();
        return true;
    }
};
