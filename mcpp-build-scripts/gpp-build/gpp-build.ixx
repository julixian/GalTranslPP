// gpp.build：供各项目的 build.mcpp 导入，配置依赖、Qt 代码生成与 Release 布局。
// Qt 插件负责编译及项目翻译生成；发布动作将 QM 复制到 Release。
export module gpp.build;

import std;
import mcpp;
import mcpp.deps;
import gpp.deps.vcpkg;
import gpp.deps.cmake;
import gpp.rules.qt;

export namespace gpp {

namespace fs = std::filesystem;

// ===== 本机工具路径配置 =====
// 在 R"(...)" 的括号内填写绝对路径，Windows 反斜杠无需转义。
// vcpkg：填写安装根目录（包含 vcpkg.exe 和 scripts），留空则从 PATH 查找。
// 示例：R"(D:\vcpkg)"
const fs::path vcpkg_root = R"()";
// CMake：填写可执行文件路径，留空则从 PATH 查找。
// 示例：R"(C:\Program Files\CMake\bin\cmake.exe)"
const fs::path cmake_executable = R"()";
// Qt：填写 SDK 根目录（包含 include、lib、bin），不可留空。
const fs::path qt_root = R"(D:\Qt\6.11.1\msvc2022_64)";
// ===== 配置结束 =====

fs::path workspace_directory() {
    // 通过宿主模块的位置定位工作区，不依赖调用它的项目目录。
    return fs::path(__FILE__).parent_path().parent_path().parent_path();
}
bool is_windows_target() { return std::string_view(mcpp::target_os()) == "windows"; }
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

fs::path qt_directory() {
    if (qt_root.empty() || !qt_root.is_absolute())
        throw std::runtime_error("请在 gpp-build.ixx 顶部的 qt_root 中填写 Qt SDK 的绝对路径");
    const auto directory = qt_root.lexically_normal();
    if (!fs::is_directory(directory / "include") || !fs::is_directory(directory / "lib"))
        throw std::runtime_error("Qt SDK 路径无效：" + directory.generic_string());
    return directory;
}

// Windows 系统库由各成员的 target.windows.build 声明。
// 这里处理按 profile 变化的 PE 链接选项，不把 /DEBUG 等传到 ELF/Mach-O 链接器。
void configure_executable_link_options() {
    if (!is_windows_target() || std::string_view(mcpp::target_env()) != "msvc") return;
    const bool uses_msvc_driver = std::string_view(mcpp::compiler()) == "msvc";
    auto add_linker_option = [uses_msvc_driver](const char* option) {
        mcpp::link_flag((uses_msvc_driver ? std::string(option) : "-Wl," + std::string(option)).c_str());
    };
    if (is_release_profile()) add_linker_option("/DEBUG");
    if (std::string_view(mcpp::profile()) == "release") {
        add_linker_option("/OPT:REF");
        add_linker_option("/OPT:ICF");
        add_linker_option("/INCREMENTAL:NO");
    }
}

constexpr const char* windows_triplet = "gpp-x64-windows-release";

gpp::deps::vcpkg::prefix configure_vcpkg(const char* link_target = nullptr) {
    gpp::deps::vcpkg::options options;
    options.vcpkg_root = vcpkg_root.generic_string();
    if (is_windows_target()) {
        if (std::string_view(mcpp::target_arch()) != "x86_64" ||
            std::string_view(mcpp::target_env()) != "msvc")
            throw std::runtime_error("GPP 自定义 triplet 当前只配置了 Windows x64 / MSVC ABI");
        options.triplet = windows_triplet;
        options.prefer_clang_cl = true;
        // 空标记文件控制 clang-cl 依赖的 LTO；MSVC 忽略它，创建或删除文件都会重新配置依赖。
        const auto lto_marker = workspace_directory() / "vcpkg-scripts" / ".vcpkg-use-lto";
        mcpp::rerun_if_changed(lto_marker.generic_string().c_str());
        options.lto = fs::is_regular_file(lto_marker);
    }
    options.manifest_root = workspace_directory().generic_string();
    options.install_root = (workspace_directory() / "vcpkg_installed").generic_string();
    const auto dependencies = gpp::deps::vcpkg::use(options);
    if (!dependencies) throw std::runtime_error("vcpkg 依赖配置失败");
    if (link_target && is_windows_target()) {
        // 安装完成后收集所有库，不再维护库名列表；只加入最终程序的链接输入。
        const fs::path collector = mcpp::dep_bin("gpp.vcpkg-link-libs", "vcpkg_link_libs");
        const auto librarian = std::string_view(mcpp::compiler()) == "msvc"
            ? fs::path(mcpp::abi_tool("ar"))
            : fs::path(mcpp::tool("cxx")).parent_path() / "llvm-lib.exe";
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
    if (!is_windows_target()) return; // 仓库自带的是 Windows Python 导入库。
    const auto python_library_directory = workspace_directory() / "3rdParty" / "pybind11" / "bin";
    for (const char* library_name : {"python3", "python312"})
        mcpp::link_flag((python_library_directory / (std::string(library_name) + ".lib")).generic_string().c_str());
}

gpp::rules::qt::options make_qt_options(std::vector<std::string> modules) {
    gpp::rules::qt::options qt_options;
    qt_options.root = qt_directory().generic_string();
    qt_options.modules = std::move(modules);
    qt_options.i18n.qt_languages = {}; // 不生成 Qt 自带的 qt_zh_CN.qm 等翻译。
    return qt_options;
}

// 在 compile(qt_options) 前配置；插件负责 lupdate/lrelease，返回供 Release 发布使用的 QM 路径。
fs::path configure_translation(gpp::rules::qt::options& qt_options, const char* translation_source_filename) {
    const fs::path project_directory = mcpp::manifest_dir();
    qt_options.i18n.ts = {translation_source_filename};
    qt_options.i18n.update_sources = true;
    qt_options.i18n.tr_function_alias = {"translate+=gppTr"};
    qt_options.i18n.out_dir = project_directory.generic_string();
    return project_directory / (fs::path(translation_source_filename).stem().string() + ".qm");
}

gpp::deps::cmake::prefix use_ela_widget_tools(const gpp::rules::qt::options& qt_options) {
    gpp::deps::cmake::options options;
    options.cmake = cmake_executable.generic_string();
    options.source = (workspace_directory() / "3rdParty" / "ElaWidgetTools").generic_string();
    options.name = "ElaWidgetTools";
    // target 被源码扫描和 rerun glob 排除；build-* 也符合 Ela 的 Git 忽略规则。
    options.cache = (fs::path(options.source) / "target" / "build-deps-cmake").generic_string();
    options.cache_args = {"-DQT_SDK_DIR=" + qt_options.root,
                          "-DELAWIDGETTOOLS_BUILD_EXAMPLE=OFF",
                          "-DELAWIDGETTOOLS_BUILD_STATIC_LIB=OFF"};
    options.dirs = {.include = "ElaWidgetTools/include", .lib = "ElaWidgetTools/lib",
                    .bin = "ElaWidgetTools/bin"};
    options.libraries = {"ElaWidgetTools"};
    options.shared = true;
    const auto dependencies = gpp::deps::cmake::use(options);
    if (!dependencies) throw std::runtime_error("ElaWidgetTools 构建配置失败");
    return dependencies;
}

// 自定义 Release 发布不读取可能尚未部署完成的 bin 目录。
// runtime_stage 从 vcpkg/Python/Ela 源目录收集非 Qt DLL；Qt 由用户另外部署。
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
        copy_action.output(output_file.c_str()).submit();
    }

    void copy_runtime_libraries(std::string_view member, const fs::path& destination_directory,
                                std::string_view destination_name) {
        const std::string runtime_stage_executable = mcpp::dep_bin("gpp.runtime-stage", "runtime_stage");
        if (runtime_stage_executable.empty()) throw std::runtime_error("未声明 runtime_stage 宿主工具");
        const auto manifest_file = (release_directory / ".mcpp-runtime" /
            (std::string(member) + "-" + std::string(destination_name) + ".txt")).generic_string();
        const auto dependency_file = manifest_file + ".d";
        const auto action_id = "release-runtime-" + std::to_string(next_action_number++);
        mcpp::action copy_action;
        copy_action.id = action_id.c_str();
        copy_action.role = mcpp::roles::artifact;
        copy_action.depfile = dependency_file.c_str();
        copy_action.arg(runtime_stage_executable.c_str())
            .arg("--exe").arg(executable_file.c_str())
            .arg("--dest").arg(destination_directory.generic_string().c_str())
            .arg("--manifest").arg(manifest_file.c_str())
            .arg("--depfile").arg(dependency_file.c_str())
            .input(executable_file.c_str())
            .input(runtime_stage_executable.c_str())
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
        copy_action.submit();
    }

    void copy_opencc_share(std::string_view member, std::string_view destination_name,
                           const fs::path& release_destination_directory) {
        const std::string runtime_stage_executable = mcpp::dep_bin("gpp.runtime-stage", "runtime_stage");
        if (runtime_stage_executable.empty()) throw std::runtime_error("未声明 runtime_stage 宿主工具");
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
        copy_action.arg(runtime_stage_executable.c_str()).arg("--copy-tree")
            .arg("--source").arg(source_directory.generic_string().c_str())
            .arg("--dest").arg(destination_directory.generic_string().c_str())
            .arg("--manifest").arg(manifest_path.c_str())
            .arg("--depfile").arg(dependency_path.c_str())
            .input(executable_file.c_str())
            .input(vcpkg_install_stamp.generic_string().c_str())
            .input(runtime_stage_executable.c_str())
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
        if (!is_windows_target() || !is_release_profile()) return;
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
    try {
        if (is_release_profile()) mcpp::define("NDEBUG");
        return configure();
    }
    catch (const std::exception& error) {
        std::cerr << "GPP build: " << error.what() << '\n';
        return 1;
    }
}

} // namespace gpp
