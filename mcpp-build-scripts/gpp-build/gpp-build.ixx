// gpp.build：供各项目的 build.mcpp 导入，配置依赖、Qt 代码生成、程序旁的数据文件与 Release 打包格式。
// 依赖安装、Qt 代码生成与部署由 mcpp:plugins 提供（deps-vcpkg、deps-cmake、deps-archive、rules-qt），
// 这里只描述本项目特有的部分：链接的库、BaseConfig 与发布目录的布局。
export module gpp.build;

import std;
import mcpp;
import mcpp.plugins.fs;
import mcpp.deps.vcpkg;
import mcpp.deps.cmake;
import mcpp.deps.archive;
import mcpp.rules.qt;

export namespace gpp {

namespace fs = std::filesystem;

// ===== 本机工具路径配置（可选） =====
// 均可留空：留空时使用 xlings 提供的版本，无需手动安装。
// 在 R"(...)" 的括号内填写绝对路径，Windows 反斜杠无需转义。
// vcpkg：安装根目录（包含 vcpkg.exe 和 scripts），留空使用 xim:vcpkg。
// 示例：R"(D:\vcpkg)"
const fs::path vcpkg_root = R"()";
// CMake：可执行文件路径，留空使用 xim:cmake。
// 示例：R"(C:\Program Files\CMake\bin\cmake.exe)"
const fs::path cmake_executable = R"()";
// Qt：SDK 根目录（包含 include、lib、bin），留空依次使用环境变量 QT_ROOT_DIR 与根 mcpp.toml 声明的 xim:qt-base。
// 示例：R"(D:\Qt\6.11.1\msvc2022_64)"
const fs::path qt_root = R"()";
// ===== 配置结束 =====

fs::path workspace_directory() { return fs::path(mcpp::manifest_dir()).parent_path(); }
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

// 安装根目录 vcpkg.json 中的库；link_libraries 为本成员链接的库，deploy 为放到程序旁的前缀文件。
mcpp::deps::vcpkg::prefix configure_vcpkg(std::vector<std::string> link_libraries = {},
                                          std::vector<mcpp::plugins::fs::deploy_entry> deploy = {}) {
    mcpp::deps::vcpkg::options options;
    options.vcpkg_root = vcpkg_root.generic_string();
    if (is_windows_target()) {
        if (std::string_view(mcpp::target_arch()) != "x86_64" ||
            std::string_view(mcpp::target_env()) != "msvc")
            throw std::runtime_error("GPP 自定义 triplet 当前只配置了 Windows x64 / MSVC ABI");
        options.triplet = windows_triplet;
    }
    options.libraries = std::move(link_libraries);
    options.deploy = std::move(deploy);
    auto dependencies = mcpp::deps::vcpkg::use(options);
    if (!dependencies) throw std::runtime_error("vcpkg 依赖配置失败");
    return dependencies;
}

std::vector<std::string> core_link_libraries() {
    return {"7zip", "bit7z64", "abseil_dll", "cld3", "cpr", "gumbo",
            "icuio", "icuin", "icuuc", "icudt", "libprotobuf-lite", "fmt",
            "lua", "mecab", "opencc", "pcre2-8", "spdlog", "tree-sitter", "zip", "z"};
}

// OpenCC 的全部配置与词典，程序从 BaseConfig/opencc 读取（与上游 3.1.2 一致，复制整个 share/opencc）。
// 文件名须在安装前写明，这里是 vcpkg 基线中 opencc 1.4.1 安装的全部文件（其 data/CMakeLists.txt 的
// CONFIG_FILES 与 DICTS）。t2s.json 本身读取四个词典：CJK_Compatibility_Ideographs、TSPhrases、
// TSCharactersExt、TSCharacters。CI 比较这份清单与安装目录，基线变化时由此发现。
std::vector<mcpp::plugins::fs::deploy_entry> opencc_files() {
    constexpr const char* configs[] = {
        "hk2s", "hk2sp", "hk2t", "jp2t", "s2hk", "s2hkp", "s2t", "s2tw", "s2twp",
        "t2hk", "t2jp", "t2s", "t2tw", "tw2s", "tw2sp", "tw2t"};
    constexpr const char* dictionaries[] = {
        "CJK_Compatibility_Ideographs", "STCharacters", "STPhrases", "TSCharacters", "TSPhrases",
        "TWPhrases", "TWPhrasesRev", "TWVariantsPhrases", "TWVariants", "TWVariantsRevPhrases",
        "HKVariantsPhrases", "HKVariants", "HKVariantsRevPhrases", "HKPhrases", "HKPhrasesRev",
        "JPShinjitaiCharacters", "JPShinjitaiPhrases", "TSCharactersExt",
        "STPhrases_GeneratedFromRegionalPhrases", "TWVariantsRev", "HKVariantsRev",
        "JPShinjitaiCharactersRev"};
    std::vector<mcpp::plugins::fs::deploy_entry> files;
    for (const char* name : configs) files.push_back({"share/opencc/" + std::string(name) + ".json", "BaseConfig/opencc"});
    for (const char* name : dictionaries) files.push_back({"share/opencc/" + std::string(name) + ".ocd2", "BaseConfig/opencc"});
    return files;
}


// 仓库自带的是 Windows Python 的导入库与 DLL；DLL 目录作为运行时搜索目录，由 mcpp 放到程序旁。
void link_python_libraries() {
    if (!is_windows_target()) return;
    const auto python_library_directory = workspace_directory() / "3rdParty" / "pybind11" / "bin";
    for (const char* library_name : {"python3", "python312"})
        mcpp::link_flag((python_library_directory / (std::string(library_name) + ".lib")).generic_string().c_str());
    mcpp::runtime_search_dir(python_library_directory.generic_string().c_str());
}

// bit7z 在运行时加载 7z.dll，来自根 mcpp.toml 声明的 xim:7zip。
void deploy_seven_zip() {
    if (!is_windows_target()) return;
    const fs::path directory = mcpp::xpkg_dir("xim", "7zip");
    if (directory.empty()) throw std::runtime_error("未找到 xim:7zip，请检查根 mcpp.toml 的 xlings.workspace");
    mcpp::deploy((directory / "7z.dll").generic_string().c_str(), ".");
}

// 把 directory 下的文件按路径顺序放到程序旁的 destination 目录；skip 以相对路径判断，返回 true 的文件或子目录不放置。
template<class Skip>
void deploy_directory(const fs::path& directory, const fs::path& destination, Skip skip) {
    std::vector<fs::path> files;
    std::error_code error;
    for (auto entry = fs::recursive_directory_iterator(directory, error);
         entry != fs::recursive_directory_iterator(); entry.increment(error)) {
        if (error) break;
        const auto relative = entry->path().lexically_relative(directory);
        if (entry->is_directory(error)) {
            if (skip(relative)) entry.disable_recursion_pending();
        } else if (entry->is_regular_file(error) && !skip(relative)) {
            files.push_back(relative);
        }
    }
    std::ranges::sort(files);
    for (const auto& relative : files) {
        const auto parent = relative.has_parent_path() ? destination / relative.parent_path() : destination;
        mcpp::deploy((directory / relative).generic_string().c_str(), parent.generic_string().c_str());
    }
}

// ===== 程序旁的数据文件：`mcpp run` 直接使用，`mcpp pack` 一并打包 =====

constexpr std::string_view python_environment = "Python-3.12.10-embed-amd64";

// 由 core 调用，放到每个链接 core 的程序旁：Example/BaseConfig、从其中压缩包解出的嵌入式 Python，
// 以及 vcpkg 安装的 OpenCC 数据（由 configure_vcpkg 的 deploy 放置）。
void deploy_base_config() {
    const auto base_config_directory = workspace_directory() / "Example" / "BaseConfig";
    const fs::path python_directory(python_environment);
    const fs::path python_archive = std::string(python_environment) + ".zip";
    mcpp::rerun_if_changed_glob("../Example/BaseConfig/**");
    // 压缩包由下面的动作解出；旧版手动步骤可能在源码目录留下的 opencc 与 Python 目录不再使用。
    deploy_directory(base_config_directory, "BaseConfig", [&](const fs::path& relative) {
        return relative == python_archive || relative == python_directory || relative == "opencc";
    });

    mcpp::deps::archive::options python;
    python.archive = (base_config_directory / python_archive).generic_string();
    python.to = "BaseConfig";
    python.cmake = cmake_executable.generic_string();
    if (!mcpp::deps::archive::unpack(python)) throw std::runtime_error("嵌入式 Python 解压配置失败");
}

// 由 GPPCLI 调用：示例项目放到程序旁。
void deploy_sample_project() {
    mcpp::rerun_if_changed_glob("../Example/SampleProject/**");
    deploy_directory(workspace_directory() / "Example" / "SampleProject", "SampleProject",
                     [](const fs::path&) { return false; });
}

// ===== Qt =====

mcpp::rules::qt::options make_qt_options(std::vector<std::string> modules) {
    if (!qt_root.empty() && !qt_root.is_absolute())
        throw std::runtime_error("gpp-build.ixx 中的 qt_root 须为绝对路径");
    mcpp::rules::qt::options qt_options;
    qt_options.root = qt_root.lexically_normal().generic_string();
    qt_options.modules = std::move(modules);
    qt_options.deploy_plugins = {}; // 需要 Qt 插件的程序自行列出，例如 GUI 的 platforms/styles/imageformats。
    return qt_options;
}

// 在 compile(qt_options) 前配置；插件负责 lupdate/lrelease，QM 文件放到程序旁的 translations。
void configure_translation(mcpp::rules::qt::options& qt_options, const char* translation_source_filename) {
    qt_options.i18n.ts = {translation_source_filename};
    qt_options.i18n.update_sources = true;
    qt_options.i18n.tr_function_alias = {"translate+=gppTr"};
}

mcpp::deps::cmake::prefix use_ela_widget_tools(const mcpp::rules::qt::options& qt_options) {
    mcpp::deps::cmake::options options;
    options.cmake = cmake_executable.generic_string();
    options.source = (workspace_directory() / "3rdParty" / "ElaWidgetTools").generic_string();
    options.name = "ElaWidgetTools";
    options.cache_args = {"-DQT_SDK_DIR=" + mcpp::rules::qt::root(qt_options),
                          "-DELAWIDGETTOOLS_BUILD_EXAMPLE=OFF",
                          "-DELAWIDGETTOOLS_BUILD_STATIC_LIB=OFF"};
    options.dirs = {.include = "ElaWidgetTools/include", .lib = "ElaWidgetTools/lib",
                    .bin = "ElaWidgetTools/bin"};
    options.libraries = {"ElaWidgetTools"};
    options.shared = true;
    auto dependencies = mcpp::deps::cmake::use(options);
    if (!dependencies) throw std::runtime_error("ElaWidgetTools 构建配置失败");
    return dependencies;
}

// ===== `mcpp pack --format release`：Release 发布目录 =====
//
// mcpp pack 暂存的目录已含程序、按导入表收集的 DLL，以及放到程序旁的全部文件（Qt 插件与翻译、7z.dll、
// BaseConfig 等）。这里只按原有布局把它复制到 Release：
//   GPPCLI           CLI 的完整目录
//   GPPGUI           GUI 的完整目录，含 Updater.exe
//   GUICORE          GUI 目录去掉全局配置、MeCab 与 Python，Updater 为 Updater_new.exe（OpenCC 随更新包提供，3.1.2 起）
//   *_PRIVATE        Release/GPPCLI_PRIVATE.txt 或 GPPGUI_PRIVATE.txt 第一行指定的目录，不含 BaseConfig 与 SampleProject
//   .pdb             程序的 PDB

constexpr const char* release_pack_format = "release";

// 文件在某个布局中的相对路径；返回空表示该布局不含此文件。
using release_layout = std::function<fs::path(const fs::path&)>;

fs::path private_release_directory(const fs::path& release_directory, std::string_view member) {
    const auto configuration_file = release_directory / (std::string(member) + "_PRIVATE.txt");
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

// 以 `mcpp stage` 复制一个文件的发布动作；input 为动作的依赖，默认即源文件。
void stage_file(const std::string& action_id, const std::string& source_file, const fs::path& destination_file,
                const std::string& input = {}) {
    const auto output_file = destination_file.lexically_normal().generic_string();
    mcpp::action copy_action;
    copy_action.id = action_id.c_str();
    copy_action.role = mcpp::roles::artifact;
    copy_action.arg("${mcpp.self}").arg("stage")
        .arg("--verify").arg("content")
        .arg("--output").arg(output_file.c_str())
        .arg(source_file.c_str())
        .input(input.empty() ? source_file.c_str() : input.c_str())
        .output(output_file.c_str()).submit();
}

// 以一个 `mcpp stage --list` 动作把 placements（暂存目录中的相对路径，发布目录中的目标）复制到一个发布目录。
// 每个文件的语义与 stage_file 相同：内容相同则不重写。清单写暂存目录的绝对路径，动作的输入写 ${mcpp.stage_dir}，
// 两者都来自本次 pack 的暂存目录。
void stage_files(const std::string& action_id, const fs::path& stage_directory,
                 const std::vector<std::pair<fs::path, fs::path>>& placements) {
    if (placements.empty()) return;
    const auto list_file = (fs::path(mcpp::out_dir()) / "release" / (action_id + ".list")).generic_string();
    std::string list;
    for (const auto& [file, destination_file] : placements)
        list += (stage_directory / file).generic_string() + '\t' + destination_file.lexically_normal().generic_string() + '\n';
    mcpp::plugins::fs::write_if_changed(list_file, list);
    mcpp::action copy_action;
    copy_action.id = action_id.c_str();
    copy_action.role = mcpp::roles::artifact;
    copy_action.arg("${mcpp.self}").arg("stage").arg("--list").arg(list_file.c_str()).input(list_file.c_str());
    for (const auto& [file, destination_file] : placements)
        copy_action.input(("${mcpp.stage_dir}/" + file.generic_string()).c_str())
            .output(destination_file.lexically_normal().generic_string().c_str());
    copy_action.submit();
}

// 由 GPPCLI 与 GPPGUI 调用。格式总是声明；只有 `mcpp pack --format release` 在暂存完成后才提交复制动作，
// 因此 `mcpp build` 与 `mcpp run` 不写 Release。
void provide_release_pack(std::string_view member, const std::string& target_name) {
    mcpp::provides_pack_format(release_pack_format);
    if (std::string_view(mcpp::pack_format()) != release_pack_format) return;
    const fs::path stage_directory = mcpp::pack_stage_dir();
    if (stage_directory.empty()) return;

    // 暂存清单的第一行说明 DLL 闭包是否完整；不完整的目录不写入 Release。
    std::ifstream stage_manifest(fs::path(stage_directory.generic_string() + ".stage-manifest"));
    std::string closure;
    std::getline(stage_manifest, closure);
    if (!closure.starts_with("closure = walked"))
        throw std::runtime_error("mcpp pack 暂存的 DLL 闭包不完整（" + closure + "），见 " +
                                 stage_directory.generic_string() + ".stage-manifest");

    std::vector<fs::path> staged_files;
    for (const auto& entry : fs::recursive_directory_iterator(stage_directory))
        if (entry.is_regular_file()) staged_files.push_back(entry.path().lexically_relative(stage_directory));
    std::ranges::sort(staged_files);

    const auto release_directory = workspace_directory() / "Release";
    // Windows 的暂存目录是平铺的；其它平台的程序与其旁的文件位于 bin/ 下。以程序所在目录为基准判断。
    const fs::path program_directory = fs::is_directory(stage_directory / "bin") ? fs::path("bin") : fs::path();
    auto beside_program = [program_directory](const fs::path& file) {
        return (program_directory.empty() ? file : file.lexically_relative(program_directory)).generic_string();
    };
    const std::string python_directory = "BaseConfig/" + std::string(python_environment) + "/";
    auto whole = [](const fs::path& file) { return file; };
    auto updater_renamed = [](const fs::path& file) {
        return file.filename() == "Updater.exe" ? file.parent_path() / "Updater_new.exe" : file;
    };
    auto without_data = [=](const fs::path& file) {
        const auto name = beside_program(file);
        return name.starts_with("BaseConfig/") || name.starts_with("SampleProject/") ? fs::path() : updater_renamed(file);
    };
    auto gui_core = [=](const fs::path& file) {
        const auto name = beside_program(file);
        if (name == "BaseConfig/GlobalConfig.toml" || name.starts_with("BaseConfig/mecab/") ||
            name.starts_with(python_directory))
            return fs::path();
        return updater_renamed(file);
    };

    std::vector<std::pair<fs::path, release_layout>> layouts{{release_directory / member, whole}};
    if (member == "GPPGUI") layouts.emplace_back(release_directory / "GUICORE", gui_core);
    if (const auto private_directory = private_release_directory(release_directory, member); !private_directory.empty())
        layouts.emplace_back(private_directory, without_data);

    // 每个发布目录一个复制动作。
    unsigned next_action_number = 0;
    for (const auto& [directory, layout] : layouts) {
        std::vector<std::pair<fs::path, fs::path>> placements;
        for (const auto& file : staged_files)
            if (const auto destination = layout(file); !destination.empty())
                placements.emplace_back(file, directory / destination);
        stage_files("release-" + std::to_string(next_action_number++), stage_directory, placements);
    }
    // PDB 是链接副产物，不在暂存目录中；以 EXE 为依赖，避免把未声明的 PDB 当成 Ninja 输入。
    if (is_windows_target())
        stage_file("release-pdb", "${mcpp.bin_dir}/" + target_name + ".pdb",
                   release_directory / ".pdb" / (target_name + ".pdb"), "${mcpp.target_file:" + target_name + "}");
}

template<class Function>
int run_build_script(Function configure) {
    try { return configure(); }
    catch (const std::exception& error) {
        std::cerr << "GPP build: " << error.what() << '\n';
        return 1;
    }
}

} // namespace gpp
