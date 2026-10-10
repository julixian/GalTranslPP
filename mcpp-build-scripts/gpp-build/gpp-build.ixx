export module gpp.build;

import std;
import mcpp;
import mcpp.deps;
import mcpp.plugins.fs;
import gpp.deps.vcpkg;
import gpp.rules.qt;

export namespace gpp {

namespace fs = std::filesystem;
namespace win {

// ===== 本机工具路径配置 =====
// 在 R"(...)" 的括号内填写绝对路径，Windows 反斜杠无需转义。
// vcpkg：填写安装根目录（包含 vcpkg.exe 和 scripts），留空则从 PATH 查找。
// 示例：R"(D:\vcpkg)"
const fs::path vcpkg_root = R"()";
// Qt：填写 SDK 根目录（包含 include、lib、bin），不可留空。
const fs::path qt_root = R"(D:\Qt\6.11.1\msvc2022_64)";
// ===== 配置结束 =====

fs::path workspace_directory() {
    // 通过宿主模块的位置定位工作区，不依赖调用它的项目目录。
    return fs::path(__FILE__).parent_path().parent_path().parent_path();
}
bool is_release_profile() {
    const std::string_view profile = mcpp::profile();
    return profile == "release" || profile == "fast-release";
}

// 配置文件第一行允许 UTF-8 BOM、CRLF 和首尾空白。
std::string read_first_line(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    std::string text;
    if (!std::getline(input, text)) return {};
    if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// Windows 系统库在 mcpp.toml 中声明；这里只处理随 profile 变化的链接选项。
void configure_executable_link_options() {
    const bool uses_msvc_driver = std::string_view(mcpp::compiler()) == "msvc";
    auto add_linker_option = [uses_msvc_driver](const char* option) {
        mcpp::link_flag((uses_msvc_driver ? std::string(option) : "-Wl," + std::string(option)).c_str());
    };
    // 禁用增量模式，避免未变更的导入库保留旧时间戳，导致 Ninja 每次重新链接。
    add_linker_option("/INCREMENTAL:NO");
    if (is_release_profile()) add_linker_option("/DEBUG");
    if (std::string_view(mcpp::profile()) == "release") {
        add_linker_option("/OPT:REF");
        add_linker_option("/OPT:ICF");
    }
}

constexpr const char* windows_triplet = "gpp-x64-windows-release";

gpp::deps::vcpkg::prefix configure_vcpkg(const char* link_target = nullptr) {
    gpp::deps::vcpkg::options options;
    options.vcpkg_root = vcpkg_root;
    options.triplet = windows_triplet;
    // 空标记文件只控制 clang-cl 依赖的 LTO。
    const auto lto_marker = workspace_directory() / "vcpkg-scripts" / ".vcpkg-use-lto";
    mcpp::rerun_if_changed(lto_marker.generic_string().c_str());
    options.lto = fs::is_regular_file(lto_marker);
    options.manifest_root = workspace_directory();
    const auto dependencies = gpp::deps::vcpkg::use(options);
    mcpp::include_dir(dependencies.include.c_str());
    if (link_target) {
        // 安装完成后收集所有库，不再维护库名列表；只加入最终程序的链接输入。
        const fs::path collector = mcpp::dep_bin("gpp.vcpkg-link-libs", "vcpkg-link-libs");
        const fs::path librarian = dependencies.librarian;
        const auto output = fs::path(mcpp::out_dir()) / "vcpkg-libs.lib";
        mcpp::action action;
        action.id = "vcpkg-link-libs";
        action.role = mcpp::roles::object;
        action.target(link_target);
        action.arg(collector.generic_string().c_str())
            .arg("--librarian").arg(librarian.generic_string().c_str())
            .arg("--lib-dir").arg(dependencies.lib.c_str())
            .arg("--output").arg(output.generic_string().c_str())
            .input(collector.generic_string().c_str()).input(librarian.generic_string().c_str())
            .input(dependencies.install_stamp.c_str())
            .output(output.generic_string().c_str()).submit();
    }
    return dependencies;
}

void link_python_libraries() {
    const auto python_library_directory = workspace_directory() / "3rdParty" / "pybind11" / "bin";
    for (const char* library_name : {"python3", "python312"})
        mcpp::link_flag((python_library_directory / (std::string(library_name) + ".lib")).generic_string().c_str());
}

gpp::rules::qt::options make_qt_options(std::vector<std::string> modules) {
    gpp::rules::qt::options qt_options;
    qt_options.root = qt_root;
    qt_options.modules = std::move(modules);
    return qt_options;
}

// 在 qt::use(qt_options) 前配置；插件负责 lupdate/lrelease，返回供 Release 发布使用的 QM 路径。
fs::path configure_translation(gpp::rules::qt::options& qt_options, const char* translation_source_filename) {
    const fs::path project_directory = mcpp::manifest_dir();
    qt_options.i18n.ts = {translation_source_filename};
    qt_options.i18n.update_sources = true;
    qt_options.i18n.tr_function_alias = {"translate+=gppTr"};
    qt_options.i18n.out_dir = project_directory;
    return project_directory / (fs::path(translation_source_filename).stem().string() + ".qm");
}

fs::path use_ela_widget_tools(const gpp::rules::qt::options& qt_options) {
    const auto source = workspace_directory() / "3rdParty" / "ElaWidgetTools";
    const auto install = source / "target" / "mcpp-install" / mcpp::target() / mcpp::profile();
    const auto source_directory = source.generic_string();
    const auto install_directory = install.generic_string();
    const auto stamp = (fs::path(mcpp::out_dir()) / "ela-build.stamp").generic_string();
    // Ela 独立于 GPP 的依赖图，在自己的目录下构建；Qt、编译器和 profile 沿用本次 GPP 构建。
    mcpp::action build;
    build.id = "ela-mcpp-build";
    build.role = mcpp::roles::prepare;
    build.description = "mcpp build ElaWidgetTools";
    build.arg("${mcpp.self}").arg("build")
        .arg("-p").arg("ElaWidgetTools")
        .arg("--toolchain").arg(("path:" + std::string(mcpp::toolchain_dir())).c_str())
        .arg("--profile").arg(mcpp::profile())
        .cwd(source_directory.c_str()).env("QT_ROOT_DIR", qt_options.root.generic_string().c_str())
        .env("ELAWIDGETTOOLS_INSTALL_DIR", install_directory.c_str())
        .output(stamp.c_str()).output_dir(install_directory.c_str());
    mcpp::deps::watch_tree(source / "ElaWidgetTools");
    for (const auto& file : mcpp::deps::files_under(source / "ElaWidgetTools")) build.input(file.c_str());
    for (const char* file : {"mcpp.toml", "mcpp-qt.hpp"}) {
        const auto path = (source / file).generic_string();
        mcpp::rerun_if_changed(path.c_str());
        if (fs::is_regular_file(path)) build.input(path.c_str());
    }
    // 子构建会重写 mcpp.lock，不能直接作为 Ninja 输入，否则动作会反复失效。
    // 监视原锁文件，只在内容变化时更新快照，让依赖变更仍能触发 Ela 构建。
    const auto lock = source / "mcpp.lock";
    mcpp::rerun_if_changed(lock.generic_string().c_str());
    std::ifstream lock_input(lock, std::ios::binary);
    const std::string lock_content{std::istreambuf_iterator<char>(lock_input), {}};
    const auto lock_snapshot = fs::path(mcpp::out_dir()) / "ela-dependencies.lock";
    fs::create_directories(lock_snapshot.parent_path());
    mcpp::plugins::fs::write_if_changed(lock_snapshot, lock_content);
    build.input(lock_snapshot.generic_string().c_str());
    build.submit();
    mcpp::include_dir((source / "ElaWidgetTools").generic_string().c_str());
    const std::vector<std::string> libraries = {"ElaWidgetTools"};
    mcpp::deps::link_libraries(install, libraries, true);
    return install;
}

// 自定义 Release 发布不读取可能尚未部署完成的 bin 目录。
// runtime-stage 从 vcpkg/Python/Ela 源目录收集非 Qt DLL；Qt 由用户另外部署。
struct release_publisher {
    fs::path release_directory = workspace_directory() / "Release";
    fs::path vcpkg_installation_directory;
    fs::path vcpkg_install_stamp;
    std::vector<fs::path> additional_runtime_directories;
    std::string target_name;
    std::string executable_file;
    unsigned next_action_number = 0;

    release_publisher(std::string executable_target, const gpp::deps::vcpkg::prefix& vcpkg)
        : vcpkg_installation_directory(vcpkg.root), vcpkg_install_stamp(vcpkg.install_stamp),
          target_name(std::move(executable_target)),
          executable_file("${mcpp.target_file:" + target_name + "}") {}

    fs::path private_release_directory(std::string_view member) const {
        const auto configuration_file = release_directory /
            (member == "GPPCLI" ? "GPPCLI_PRIVATE.txt" : "GPPGUI_PRIVATE.txt");
        mcpp::rerun_if_changed(configuration_file.generic_string().c_str());
        if (!fs::is_regular_file(configuration_file)) return {};
        const auto configured_path = read_first_line(configuration_file);
        if (configured_path.empty()) return {};
        fs::path destination_directory(configured_path);
        if (destination_directory.is_relative()) destination_directory = release_directory / destination_directory;
        destination_directory = destination_directory.lexically_normal();
        if (!fs::is_directory(destination_directory)) {
            mcpp::warning(("PRIVATE 发布路径不存在：" + destination_directory.generic_string()).c_str());
            return {};
        }
        return destination_directory;
    }

    void copy_file(const std::string& source_file, const fs::path& destination_file,
                   bool track_source = true) {
        const auto output_file = destination_file.lexically_normal().generic_string();
        const auto action_id = "release-stage-" + std::to_string(next_action_number++);
        mcpp::action copy_action;
        copy_action.id = action_id.c_str();
        copy_action.role = mcpp::roles::artifact;
        copy_action.arg("${mcpp.self}").arg("stage")
            .arg("--verify").arg("content")
            .arg("--output").arg(output_file.c_str())
            .arg(source_file.c_str())
            .input(executable_file.c_str());
        if (track_source) copy_action.input(source_file.c_str());
        // Release 由多个 profile 共用；每次检查内容，不能依赖目标文件的新旧判断当前配置。
        // 此输出有意不生成，让 Ninja 调度发布检查；stage 在内容相同时不重写实际产物。
        const auto check = (fs::path(mcpp::out_dir()) / (action_id + ".check")).generic_string();
        copy_action.output(output_file.c_str()).output(check.c_str()).submit();
    }

    void copy_runtime_libraries(std::string_view member, const fs::path& destination_directory,
                                std::string_view destination_name) {
        const std::string runtimeStageExecutable = mcpp::dep_bin("gpp.runtime-stage", "runtime-stage");
        if (runtimeStageExecutable.empty()) throw std::runtime_error("未声明 runtime-stage 宿主工具");
        const auto manifest_file = (release_directory / ".mcpp-runtime" /
            (std::string(member) + "-" + std::string(destination_name) + ".txt")).generic_string();
        const auto dependency_file = manifest_file + ".d";
        const auto action_id = "release-runtime-" + std::to_string(next_action_number++);
        mcpp::action copy_action;
        copy_action.id = action_id.c_str();
        copy_action.role = mcpp::roles::artifact;
        copy_action.depfile = dependency_file.c_str();
        copy_action.arg(runtimeStageExecutable.c_str())
            .arg("--exe").arg(executable_file.c_str())
            .arg("--dest").arg(destination_directory.generic_string().c_str())
            .arg("--manifest").arg(manifest_file.c_str())
            .arg("--depfile").arg(dependency_file.c_str())
            .input(executable_file.c_str())
            .input(runtimeStageExecutable.c_str())
            .output(manifest_file.c_str());
        std::vector<fs::path> runtime_search_directories = {
            vcpkg_installation_directory / "bin",
            workspace_directory() / "3rdParty" / "pybind11" / "bin",
            workspace_directory() / "3rdParty"
        };
        runtime_search_directories.insert(runtime_search_directories.end(),
            additional_runtime_directories.begin(), additional_runtime_directories.end());
        for (const auto& search_directory : runtime_search_directories) {
            // Ela 首次构建时尚未安装，搜索目录也必须传入；exe 的链接已依赖 prepare。
            copy_action.arg("--search").arg(search_directory.generic_string().c_str());
            if (!fs::is_directory(search_directory)) continue;
            for (const auto& entry : fs::directory_iterator(search_directory))
                if (entry.is_regular_file() && entry.path().extension() == ".dll")
                    copy_action.input(entry.path().generic_string().c_str());
        }
        // 这些 DLL 可能由程序动态加载，无法只靠 EXE 的导入表找到。
        copy_action.arg("--include-dll").arg("7z.dll");
        if (member != "Updater") {
            copy_action.arg("--include-dll").arg("python3.dll");
            copy_action.arg("--include-dll").arg("python312.dll");
        }
        // 运行库同样可能被其他 profile 覆盖，发布时重新核对，内容相同则不复制。
        const auto check = (fs::path(mcpp::out_dir()) / (action_id + ".check")).generic_string();
        copy_action.output(check.c_str()).submit();
    }

    void copy_opencc_share(std::string_view member, std::string_view destination_name,
                           const fs::path& release_destination_directory) {
        const std::string runtimeStageExecutable = mcpp::dep_bin("gpp.runtime-stage", "runtime-stage");
        if (runtimeStageExecutable.empty()) throw std::runtime_error("未声明 runtime-stage 宿主工具");
        const fs::path source_directory = vcpkg_installation_directory / "share" / "opencc";
        const fs::path destination_directory = release_destination_directory / "BaseConfig" / "opencc";
        // Windows 下 Ninja 不根据目录时间戳检测新文件，由构建脚本监视文件集合。
        mcpp::rerun_if_changed_glob((source_directory / "**/*").generic_string().c_str());
        std::string manifest_name = "OpenCC-" + std::string(member);
        if (destination_name != member) manifest_name += "-" + std::string(destination_name);
        const fs::path manifest_file = release_directory / ".mcpp-runtime" / (manifest_name + ".txt");
        const fs::path dependency_file = manifest_file.generic_string() + ".d";
        const std::string manifest_path = manifest_file.generic_string();
        const std::string dependency_path = dependency_file.generic_string();
        const auto action_id = "release-opencc-" + std::to_string(next_action_number++);
        mcpp::action copy_action;
        copy_action.id = action_id.c_str();
        copy_action.role = mcpp::roles::artifact;
        copy_action.depfile = dependency_path.c_str();
        copy_action.arg(runtimeStageExecutable.c_str()).arg("--copy-tree")
            .arg("--source").arg(source_directory.generic_string().c_str())
            .arg("--dest").arg(destination_directory.generic_string().c_str())
            .arg("--manifest").arg(manifest_path.c_str())
            .arg("--depfile").arg(dependency_path.c_str())
            .input(executable_file.c_str())
            .input(vcpkg_install_stamp.generic_string().c_str())
            .input(runtimeStageExecutable.c_str())
            .output(manifest_path.c_str())
            .output(destination_directory.generic_string().c_str());
        // 新增文件在重新配置后直接成为输入，内容更新和目标文件缺失另由 depfile 追踪。
        if (fs::is_directory(source_directory)) {
            std::vector<fs::path> files;
            for (const auto& entry : fs::recursive_directory_iterator(source_directory))
                if (entry.is_regular_file()) files.push_back(entry.path());
            std::ranges::sort(files);
            for (const auto& file : files) copy_action.input(file.generic_string().c_str());
        }
        copy_action.submit();
    }

    void publish_release(std::string_view member, const fs::path& translation_file) {
        if (!is_release_profile()) return;
        const bool is_cli = member == "GPPCLI";
        const bool is_gui = member == "GPPGUI";
        const auto package_release_directory = release_directory / (is_cli ? "GPPCLI" : "GPPGUI");
        const auto private_directory = private_release_directory(member);
        std::vector<std::pair<fs::path, std::string>> destinations{
            {package_release_directory, is_cli ? "GPPCLI" : "GPPGUI"}
        };
        if (is_gui || member == "Updater")
            destinations.emplace_back(release_directory / "GUICORE", "GUICORE");
        if (!private_directory.empty())
            destinations.emplace_back(private_directory, is_cli ? "GPPCLI_PRIVATE" : "GPPGUI_PRIVATE");
        if (is_cli || is_gui) {
            // PDB 是链接副产物；以 EXE 为依赖，避免把未声明的 PDB 当成 Ninja 输入。
            copy_file("${mcpp.bin_dir}/" + target_name + ".pdb",
                      release_directory / ".pdb" / (target_name + ".pdb"), false);
            // 安装完成后递归复制 share/opencc；GUI 的 GUICORE 也需要完整配置。
            copy_opencc_share(member, member, package_release_directory);
            if (is_gui)
                copy_opencc_share(member, "GUICORE", release_directory / "GUICORE");
        }
        for (const auto& [destination_directory, destination_name] : destinations) {
            const auto executable_filename = member == "Updater" && destination_name != "GPPGUI"
                ? "Updater_new.exe" : target_name + ".exe";
            copy_file(executable_file, destination_directory / executable_filename);
            copy_runtime_libraries(member, destination_directory, destination_name);
            copy_file(translation_file.generic_string(),
                      destination_directory / "translations" / translation_file.filename());
            if (member != "Updater")
                copy_file((workspace_directory() / "GalTranslPP" / "qt_gpp_en.qm").generic_string(),
                          destination_directory / "translations" / "qt_gpp_en.qm");
        }
    }
};

template<class Function>
int run_build_script(Function configure) {
    if (is_release_profile()) mcpp::define("NDEBUG");
    return configure();
}
} // namespace win

// 只在入口选择平台；win 内部可以直接使用 Windows 路径、库和发布规则。
template<class Function>
int run_build_script(Function configure) {
    try {
        if (std::string_view(mcpp::host()).contains("windows") &&
            std::string_view(mcpp::target_os()) == "windows" &&
            std::string_view(mcpp::target_env()) == "msvc" &&
            std::string_view(mcpp::target_arch()) == "x86_64")
            return win::run_build_script(std::move(configure));
        throw std::runtime_error("当前项目只配置了 Windows 本机构建");
    }
    catch (const std::exception& error) {
        std::cerr << "GalTranslPP build: " << error.what() << '\n';
        return 1;
    }
}
} // namespace gpp
