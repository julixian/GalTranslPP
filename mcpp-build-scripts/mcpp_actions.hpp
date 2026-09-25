#pragma once

// Shared build-graph actions for the three executable workspace members.
// This header is included only by build.mcpp programs (which import std and mcpp).

#include "mcpp_translations.hpp"

struct executable_actions {
    using path = std::filesystem::path;

    path project = path(mcpp::manifest_dir());
    path workspace = project.parent_path();
    path release = workspace /
        (std::string_view(mcpp::profile()) == "fast-release" ? "FastRelease" : "Release");
    path qt = qt_root_path();
    path vcpkg = workspace / "vcpkg_installed" / "gpp-x64-windows-release";
    std::string target;
    std::string target_file;
    unsigned next_action = 0;

    explicit executable_actions(std::string name)
        : target(std::move(name)), target_file("${mcpp.target_file:" + target + "}") {
        // Private release directories are optional, as in the VS post-build events.
        mcpp::rerun_if_changed(release.string().c_str());
    }

    bool ready() const {
        if (qt.empty() || !std::filesystem::exists(qt / "bin" / "lrelease.exe")) {
            std::cerr << "Qt tools missing; check mcpp-build-scripts/qt-root.txt\n";
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

    bool stage_runtime_files(std::string_view member, const path& destination) {
        const char* tool = mcpp::dep_bin("gpp.runtime-stage", "runtime_stage");
        if (!tool || !*tool) {
            std::cerr << "runtime-stage host tool is unavailable\n";
            return false;
        }
        const auto manifest = destination /
            (".mcpp-runtime-" + std::string(member) + ".txt");
        const auto exe = target_file;
        const auto output = manifest.lexically_normal().string();
        const auto dest = destination.lexically_normal().string();
        const auto id = "runtime-stage-" + std::to_string(next_action++);
        mcpp::action action;
        action.id = id.c_str();
        action.role = "artifact";
        action.arg(tool).arg("--exe").arg(exe.c_str())
              .arg("--manifest").arg(output.c_str())
              .arg("--dest").arg(dest.c_str())
              .input(exe.c_str()).input(tool).output(output.c_str());
        const std::vector<path> search_dirs{
            vcpkg / "bin",
            workspace / "3rdParty" / "pybind11" / "bin",
            workspace / "3rdParty" / "ElaWidgetTools" / "Install" /
                "ElaWidgetTools" / "bin",
            workspace / "3rdParty"
        };
        for (const auto& dir : search_dirs) {
            if (!std::filesystem::is_directory(dir)) continue;
            const auto dir_arg = dir.lexically_normal().string();
            action.arg("--search").arg(dir_arg.c_str());
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".dll") continue;
                const auto file = entry.path().lexically_normal().string();
                action.input(file.c_str());
            }
        }
        action.arg("--seed").arg("7z.dll");
        if (member != "Updater") {
            action.arg("--seed").arg("python3.dll");
            action.arg("--seed").arg("python312.dll");
        }
        action.submit();
        return true;
    }

    void copy_translation_files(std::string_view member, const path& qm,
                                const path& destination) {
        const auto translations = destination / "translations";
        copy_file(qm, translations / qm.filename());
        if (member != "Updater") {
            const auto core_qm = workspace / "GalTranslPP" / "qt_gpp_en.qm";
            copy_file(core_qm, translations / core_qm.filename());
        }
    }

    path prepare_translations(std::string_view member) const {
        const auto ts = project / (member == "GPPCLI" ? "qt_gppcli_en.ts" :
                                   member == "GPPGUI" ? "qt_gppgui_en.ts" :
                                                        "qt_gppupdater_en.ts");
        return qt_translation(project, ts,
                              path(mcpp::out_dir()) / (ts.stem().string() + ".qm"));
    }

    bool publish_release(std::string_view member, const path& own_qm) {
        if (own_qm.empty()) return false;
        const auto profile = std::string_view(mcpp::profile());
        if (profile != "release" && profile != "fast-release") return true;
        if (!ready()) return false;
        const bool cli = member == "GPPCLI";
        const bool gui = member == "GPPGUI";
        const auto base = release / (cli ? "GPPCLI" : "GPPGUI");
        const auto mirror = release / (cli ? "GPPCLI_PRIVATE" : "GPPGUI_PRIVATE");
        const bool private_exists = std::filesystem::is_directory(mirror);
        std::vector<path> destinations{base};
        if (gui) destinations.push_back(release / "GUICORE");
        if ((cli || gui) && private_exists) {
            destinations.push_back(mirror);
        }

        copy(target_file, base / (target + ".exe"));
        if (gui) copy(target_file, release / "GUICORE" / (target + ".exe"));
        if (member == "Updater") {
            copy(target_file, release / "GUICORE" / "Updater_new.exe");
            if (private_exists) copy(target_file, mirror / "Updater_new.exe");
        } else if (private_exists) {
            copy(target_file, mirror / (target + ".exe"));
        }

        for (const auto& dir : destinations)
            if (!stage_runtime_files(member, dir)) return false;

        for (const auto& dir : destinations) copy_translation_files(member, own_qm, dir);
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
        for (std::filesystem::recursive_directory_iterator it(project), end;
             it != end; ++it) {
            if (it.depth() == 0 && it->is_directory() &&
                (it->path().filename() == "target" ||
                 it->path().filename() == "mcpp-generated")) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file() || it->path().extension() != ".h") continue;
            mcpp::rerun_if_changed(it->path().string().c_str());
            std::ifstream input(it->path());
            const std::string content(std::istreambuf_iterator<char>{input}, {});
            if (content.find("Q_OBJECT") != std::string::npos ||
                content.find("Q_GADGET") != std::string::npos ||
                content.find("Q_NAMESPACE") != std::string::npos)
                headers.push_back(it->path());
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
