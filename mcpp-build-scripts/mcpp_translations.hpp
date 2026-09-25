#pragma once

// Qt's VS integration updates TS and releases QM before compiling each project.
// A blocking check gives lupdate a stamp; the QM source action depends on it.
inline std::filesystem::path qt_translation(const std::filesystem::path& project,
                                            const std::filesystem::path& ts,
                                            const std::filesystem::path& qm) {
    namespace fs = std::filesystem;
    const auto qt = qt_root_path();
    const auto update = (qt / "bin" / "lupdate.exe").string();
    const auto release = (qt / "bin" / "lrelease.exe").string();
    const auto ts_name = ts.stem().string();
    const auto ts_path = ts.string();
    const auto qm_path = qm.string();
    const auto stamp = (fs::path(mcpp::out_dir()) / (ts_name + ".lupdate.stamp")).string();
    if (!fs::is_regular_file(update) || !fs::is_regular_file(release)) {
        std::println(stderr, "Qt Linguist tools missing under {}", qt.string());
        return {};
    }

    mcpp::rerun_if_changed_glob("**/*.cpp");
    mcpp::rerun_if_changed_glob("**/*.h");
    mcpp::rerun_if_changed_glob("**/*.hpp");
    mcpp::rerun_if_changed_glob("**/*.ixx");
    std::vector<fs::path> sources;
    for (fs::recursive_directory_iterator it(project), end; it != end; ++it) {
        if (it.depth() == 0 && it->is_directory() &&
            (it->path().filename() == "target" ||
             it->path().filename() == "mcpp-generated")) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file()) continue;
        const auto ext = it->path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".ixx")
            sources.push_back(it->path());
    }
    std::ranges::sort(sources);

    const auto update_id = "lupdate-" + ts_name;
    mcpp::action update_action;
    update_action.id = update_id.c_str();
    update_action.role = "check";
    update_action.blocking = true;
    update_action.arg(update.c_str()).arg("-silent")
                 .arg("-extensions").arg("cpp,h,hpp,ixx")
                 .arg("-tr-function-alias").arg("translate+=gppTr");
    for (const auto& source : sources) {
        const auto input = source.string();
        update_action.arg(input.c_str()).input(input.c_str());
    }
    update_action.arg("-ts").arg(ts_path.c_str()).output(stamp.c_str()).submit();

    const auto release_id = "lrelease-" + ts_name;
    mcpp::action release_action;
    release_action.id = release_id.c_str();
    release_action.role = "source";
    release_action.arg(release.c_str()).arg(ts_path.c_str()).arg("-qm").arg(qm_path.c_str())
                  .input(stamp.c_str()).input(ts_path.c_str())
                  .output(qm_path.c_str()).submit();
    return qm;
}
