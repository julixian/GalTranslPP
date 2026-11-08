export module gpp.build;

import std;
import mcpp.core;
import tomlplusplus;
import mcpp.deps;
import mcpp.plugins.fs;
import gpp.deps.vcpkg;
import gpp.rules.qt;
export import gpp.build.macos;

export namespace gpp {

namespace fs = std::filesystem;
namespace win {

fs::path workspaceDirectory() {
    // 通过宿主模块的位置定位工作区，不依赖调用它的项目目录。
    return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

// 本机工具配置按宿主系统分表，修改后重新执行构建脚本。
const toml::table& localTools() {
    const auto file = workspaceDirectory() / "build-local.toml";
    mcpp::rerun_if_changed(file.generic_string().c_str());
    static const auto config = toml::parse_file(file.string());
    return config;
}

fs::path localToolPath(std::string_view key) {
    // 当前为 Windows 宿主入口；mcpp::host() 返回完整 triple，不能直接作为系统表名。
    return localTools()["windows"][key].value_or(std::string{});
}

bool isReleaseProfile() {
    const std::string_view profile = mcpp::profile();
    return profile == "release" || profile == "fast-release";
}

// 配置文件第一行允许 UTF-8 BOM、CRLF 和首尾空白。
std::string readFirstLine(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    std::string text;
    if (!std::getline(input, text)) return {};
    if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// Windows 系统库在 mcpp.toml 中声明；这里只处理随 profile 变化的链接选项。
void configureExecutableLinkOptions() {
    const bool usesMSVCDriver = std::string_view(mcpp::compiler()) == "msvc";
    auto addLinkerOptionFunc = [usesMSVCDriver](const char* option)
        {
            mcpp::link_flag((usesMSVCDriver ? std::string(option) : "-Wl," + std::string(option)).c_str());
        };
    // 禁用增量模式，避免未变更的导入库保留旧时间戳，导致 Ninja 每次重新链接。
    addLinkerOptionFunc("/INCREMENTAL:NO");
    if (isReleaseProfile()) addLinkerOptionFunc("/DEBUG");
    if (std::string_view(mcpp::profile()) == "release") {
        addLinkerOptionFunc("/OPT:REF");
        addLinkerOptionFunc("/OPT:ICF");
    }
}

constexpr const char* kWindowsTriplet = "gpp-x64-windows-release";

gpp::deps::vcpkg::Prefix configureVcpkg(const char* linkTarget = nullptr) {
    gpp::deps::vcpkg::Options options;
    options.vcpkgRoot = localToolPath("vcpkg_root");
    options.triplet = kWindowsTriplet;
    // 空标记文件只控制 clang-cl 依赖的 LTO。
    const auto ltoMarker = workspaceDirectory() / "vcpkg-scripts" / ".vcpkg-use-lto";
    mcpp::rerun_if_changed(ltoMarker.generic_string().c_str());
    options.lto = fs::is_regular_file(ltoMarker);
    options.manifestRoot = workspaceDirectory();
    const auto dependencies = gpp::deps::vcpkg::use(options);
    mcpp::include_dir(dependencies.include.c_str());
    if (linkTarget) {
        // 安装完成后收集所有库，不再维护库名列表；只加入最终程序的链接输入。
        const fs::path collector = mcpp::dep_bin("gpp.vcpkg-link-libs", "vcpkg-link-libs");
        const fs::path librarian = dependencies.librarian;
        const auto output = fs::path(mcpp::out_dir()) / "vcpkg-libs.lib";
        mcpp::action action;
        action.id = "vcpkg-link-libs";
        action.role = mcpp::roles::object;
        action.target(linkTarget);
        action.arg(collector.generic_string().c_str())
            .arg("--librarian").arg(librarian.generic_string().c_str())
            .arg("--lib-dir").arg(dependencies.lib.c_str())
            .arg("--output").arg(output.generic_string().c_str())
            .input(collector.generic_string().c_str()).input(librarian.generic_string().c_str())
            .input(dependencies.installStamp.c_str())
            .output(output.generic_string().c_str()).submit();
    }
    return dependencies;
}

void linkPythonLibraries() {
    const auto pythonLibraryDirectory = workspaceDirectory() / "3rdParty" / "pybind11" / "bin";
    for (const char* libraryName : {"python3", "python312"})
        mcpp::link_flag((pythonLibraryDirectory / (std::string(libraryName) + ".lib")).generic_string().c_str());
}

gpp::rules::qt::Options makeQtOptions(std::vector<std::string> modules) {
    gpp::rules::qt::Options qtOptions;
    qtOptions.root = localToolPath("qt_root");
    qtOptions.modules = std::move(modules);
    return qtOptions;
}

// 在 qt::use(qt_options) 前配置；插件负责 lupdate/lrelease，返回供 Release 发布使用的 QM 路径。
fs::path configureTranslation(gpp::rules::qt::Options& qtOptions, const char* translationSourceFilename) {
    const fs::path projectDirectory = mcpp::manifest_dir();
    qtOptions.i18n.ts = {translationSourceFilename};
    qtOptions.i18n.updateSources = true;
    qtOptions.i18n.trFunctionAlias = {"translate+=gppTr"};
    qtOptions.i18n.outDir = projectDirectory;
    return projectDirectory / (fs::path(translationSourceFilename).stem().string() + ".qm");
}

fs::path useElaWidgetTools(const gpp::rules::qt::Options& qtOptions) {
    const auto source = workspaceDirectory() / "3rdParty" / "ElaWidgetTools";
    const auto install = source / "target" / "mcpp-install" / mcpp::target() / mcpp::profile();
    const auto sourceDirectory = source.generic_string();
    const auto installDirectory = install.generic_string();
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
        .cwd(sourceDirectory.c_str()).env("QT_ROOT_DIR", qtOptions.root.generic_string().c_str())
        .env("ELAWIDGETTOOLS_INSTALL_DIR", installDirectory.c_str())
        .output(stamp.c_str()).output_dir(installDirectory.c_str());
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
    std::ifstream lockInput(lock, std::ios::binary);
    const std::string lockContent{std::istreambuf_iterator<char>(lockInput), {}};
    const auto lockSnapshot = fs::path(mcpp::out_dir()) / "ela-dependencies.lock";
    fs::create_directories(lockSnapshot.parent_path());
    mcpp::plugins::fs::write_if_changed(lockSnapshot, lockContent);
    build.input(lockSnapshot.generic_string().c_str());
    build.submit();
    mcpp::include_dir((source / "ElaWidgetTools").generic_string().c_str());
    const std::vector<std::string> libraries = {"ElaWidgetTools"};
    mcpp::deps::link_libraries(install, libraries, true);
    return install;
}

// 自定义 Release 发布不读取可能尚未部署完成的 bin 目录。
// runtime-stage 从 vcpkg/Python/Ela 源目录收集非 Qt DLL；Qt 由用户另外部署。
struct ReleasePublisher {
    fs::path releaseDirectory = workspaceDirectory() / "Release";
    fs::path vcpkgInstallationDirectory;
    fs::path vcpkgInstallStamp;
    std::vector<fs::path> additionalRuntimeDirectories;
    std::string targetName;
    std::string executableFile;
    unsigned nextActionNumber = 0;

    ReleasePublisher(std::string executableTarget, const gpp::deps::vcpkg::Prefix& vcpkg)
        : vcpkgInstallationDirectory(vcpkg.root), vcpkgInstallStamp(vcpkg.installStamp),
          targetName(std::move(executableTarget)),
          executableFile("${mcpp.target_file:" + targetName + "}") {}

    fs::path privateReleaseDirectory(std::string_view member) const {
        const auto configurationFile = releaseDirectory /
            (member == "GPPCLI" ? "GPPCLI_PRIVATE.txt" : "GPPGUI_PRIVATE.txt");
        mcpp::rerun_if_changed(configurationFile.generic_string().c_str());
        if (!fs::is_regular_file(configurationFile)) return {};
        const auto configuredPath = readFirstLine(configurationFile);
        if (configuredPath.empty()) return {};
        fs::path destinationDirectory(configuredPath);
        if (destinationDirectory.is_relative()) destinationDirectory = releaseDirectory / destinationDirectory;
        destinationDirectory = destinationDirectory.lexically_normal();
        if (!fs::is_directory(destinationDirectory)) {
            mcpp::warning(("PRIVATE 发布路径不存在：" + destinationDirectory.generic_string()).c_str());
            return {};
        }
        return destinationDirectory;
    }

    void copyFile(const std::string& sourceFile, const fs::path& destinationFile,
                   bool trackSource = true) {
        const auto outputFile = destinationFile.lexically_normal().generic_string();
        const auto actionId = "release-stage-" + std::to_string(nextActionNumber++);
        mcpp::action copyAction;
        copyAction.id = actionId.c_str();
        copyAction.role = mcpp::roles::artifact;
        copyAction.arg("${mcpp.self}").arg("stage")
            .arg("--verify").arg("content")
            .arg("--output").arg(outputFile.c_str())
            .arg(sourceFile.c_str())
            .input(executableFile.c_str());
        if (trackSource) copyAction.input(sourceFile.c_str());
        // Release 由多个 profile 共用；每次检查内容，不能依赖目标文件的新旧判断当前配置。
        // 此输出有意不生成，让 Ninja 调度发布检查；stage 在内容相同时不重写实际产物。
        const auto check = (fs::path(mcpp::out_dir()) / (actionId + ".check")).generic_string();
        copyAction.output(outputFile.c_str()).output(check.c_str()).submit();
    }

    void copyRuntimeLibraries(std::string_view member, const fs::path& destinationDirectory,
                                std::string_view destinationName) {
        const std::string runtimeStageExecutable = mcpp::dep_bin("gpp.runtime-stage", "runtime-stage");
        if (runtimeStageExecutable.empty()) throw std::runtime_error("未声明 runtime-stage 宿主工具");
        const auto manifestFile = (releaseDirectory / ".mcpp-runtime" /
            (std::string(member) + "-" + std::string(destinationName) + ".txt")).generic_string();
        const auto dependencyFile = manifestFile + ".d";
        const auto actionId = "release-runtime-" + std::to_string(nextActionNumber++);
        mcpp::action copyAction;
        copyAction.id = actionId.c_str();
        copyAction.role = mcpp::roles::artifact;
        copyAction.depfile = dependencyFile.c_str();
        copyAction.arg(runtimeStageExecutable.c_str())
            .arg("--exe").arg(executableFile.c_str())
            .arg("--dest").arg(destinationDirectory.generic_string().c_str())
            .arg("--manifest").arg(manifestFile.c_str())
            .arg("--depfile").arg(dependencyFile.c_str())
            .input(executableFile.c_str())
            .input(runtimeStageExecutable.c_str())
            .output(manifestFile.c_str());
        std::vector<fs::path> runtimeSearchDirectories = {
            vcpkgInstallationDirectory / "bin",
            workspaceDirectory() / "3rdParty" / "pybind11" / "bin",
            workspaceDirectory() / "3rdParty"
        };
        runtimeSearchDirectories.insert(runtimeSearchDirectories.end(),
            additionalRuntimeDirectories.begin(), additionalRuntimeDirectories.end());
        for (const auto& searchDirectory : runtimeSearchDirectories) {
            // Ela 首次构建时尚未安装，搜索目录也必须传入；exe 的链接已依赖 prepare。
            copyAction.arg("--search").arg(searchDirectory.generic_string().c_str());
            if (!fs::is_directory(searchDirectory)) continue;
            for (const auto& entry : fs::directory_iterator(searchDirectory))
                if (entry.is_regular_file() && entry.path().extension() == ".dll")
                    copyAction.input(entry.path().generic_string().c_str());
        }
        // 这些 DLL 可能由程序动态加载，无法只靠 EXE 的导入表找到。
        copyAction.arg("--include-dll").arg("7z.dll");
        if (member != "Updater") {
            copyAction.arg("--include-dll").arg("python3.dll");
            copyAction.arg("--include-dll").arg("python312.dll");
        }
        // 运行库同样可能被其他 profile 覆盖，发布时重新核对，内容相同则不复制。
        const auto check = (fs::path(mcpp::out_dir()) / (actionId + ".check")).generic_string();
        copyAction.output(check.c_str()).submit();
    }

    void copyOpenccShare(std::string_view member, std::string_view destinationName,
                           const fs::path& releaseDestinationDirectory) {
        const std::string runtimeStageExecutable = mcpp::dep_bin("gpp.runtime-stage", "runtime-stage");
        if (runtimeStageExecutable.empty()) throw std::runtime_error("未声明 runtime-stage 宿主工具");
        const fs::path sourceDirectory = vcpkgInstallationDirectory / "share" / "opencc";
        const fs::path destinationDirectory = releaseDestinationDirectory / "BaseConfig" / "opencc";
        // Windows 下 Ninja 不根据目录时间戳检测新文件，由构建脚本监视文件集合。
        mcpp::rerun_if_changed_glob((sourceDirectory / "**/*").generic_string().c_str());
        std::string manifestName = "OpenCC-" + std::string(member);
        if (destinationName != member) manifestName += "-" + std::string(destinationName);
        const fs::path manifestFile = releaseDirectory / ".mcpp-runtime" / (manifestName + ".txt");
        const fs::path dependencyFile = manifestFile.generic_string() + ".d";
        const std::string manifestPath = manifestFile.generic_string();
        const std::string dependencyPath = dependencyFile.generic_string();
        const auto actionId = "release-opencc-" + std::to_string(nextActionNumber++);
        mcpp::action copyAction;
        copyAction.id = actionId.c_str();
        copyAction.role = mcpp::roles::artifact;
        copyAction.depfile = dependencyPath.c_str();
        copyAction.arg(runtimeStageExecutable.c_str()).arg("--copy-tree")
            .arg("--source").arg(sourceDirectory.generic_string().c_str())
            .arg("--dest").arg(destinationDirectory.generic_string().c_str())
            .arg("--manifest").arg(manifestPath.c_str())
            .arg("--depfile").arg(dependencyPath.c_str())
            .input(executableFile.c_str())
            .input(vcpkgInstallStamp.generic_string().c_str())
            .input(runtimeStageExecutable.c_str())
            .output(manifestPath.c_str())
            .output(destinationDirectory.generic_string().c_str());
        // 新增文件在重新配置后直接成为输入，内容更新和目标文件缺失另由 depfile 追踪。
        if (fs::is_directory(sourceDirectory)) {
            std::vector<fs::path> files;
            for (const auto& entry : fs::recursive_directory_iterator(sourceDirectory))
                if (entry.is_regular_file()) files.push_back(entry.path());
            std::ranges::sort(files);
            for (const auto& file : files) copyAction.input(file.generic_string().c_str());
        }
        copyAction.submit();
    }

    void publishRelease(std::string_view member, const fs::path& translationFile) {
        if (!isReleaseProfile()) return;
        const bool isCLI = member == "GPPCLI";
        const bool isGUI = member == "GPPGUI";
        const auto packageReleaseDirectory = releaseDirectory / (isCLI ? "GPPCLI" : "GPPGUI");
        const auto privateDirectory = privateReleaseDirectory(member);
        std::vector<std::pair<fs::path, std::string>> destinations{
            {packageReleaseDirectory, isCLI ? "GPPCLI" : "GPPGUI"}
        };
        if (isGUI || member == "Updater")
            destinations.emplace_back(releaseDirectory / "GUICORE", "GUICORE");
        if (!privateDirectory.empty())
            destinations.emplace_back(privateDirectory, isCLI ? "GPPCLI_PRIVATE" : "GPPGUI_PRIVATE");
        if (isCLI || isGUI) {
            // PDB 是链接副产物；以 EXE 为依赖，避免把未声明的 PDB 当成 Ninja 输入。
            copyFile("${mcpp.bin_dir}/" + targetName + ".pdb",
                      releaseDirectory / ".pdb" / (targetName + ".pdb"), false);
            // 安装完成后递归复制 share/opencc；GUI 的 GUICORE 也需要完整配置。
            copyOpenccShare(member, member, packageReleaseDirectory);
            if (isGUI)
                copyOpenccShare(member, "GUICORE", releaseDirectory / "GUICORE");
        }
        for (const auto& [destinationDirectory, destinationName] : destinations) {
            const auto executableFilename = member == "Updater" && destinationName != "GPPGUI"
                ? "Updater_new.exe" : targetName + ".exe";
            copyFile(executableFile, destinationDirectory / executableFilename);
            copyRuntimeLibraries(member, destinationDirectory, destinationName);
            copyFile(translationFile.generic_string(),
                      destinationDirectory / "translations" / translationFile.filename());
            if (member != "Updater")
                copyFile((workspaceDirectory() / "GalTranslPP" / "qt_gpp_en.qm").generic_string(),
                          destinationDirectory / "translations" / "qt_gpp_en.qm");
        }
    }
};

template<class Function>
int runBuildScript(Function configure) {
    if (isReleaseProfile()) mcpp::define("NDEBUG");
    return configure();
}
} // namespace win

// 只在入口选择平台；win 内部可以直接使用 Windows 路径、库和发布规则。
template<class Function>
int runBuildScript(Function configure) {
    try {
        if (std::string_view(mcpp::host()).contains("windows") &&
            std::string_view(mcpp::target_os()) == "windows" &&
            std::string_view(mcpp::target_env()) == "msvc" &&
            std::string_view(mcpp::target_arch()) == "x86_64")
            return win::runBuildScript(std::move(configure));
        throw std::runtime_error("当前项目只配置了 Windows 本机构建");
    }
    catch (const std::exception& error) {
        std::cerr << "GalTranslPP build: " << error.what() << '\n';
        return 1;
    }
}
} // namespace gpp
