export module gpp.deps.vcpkg;

import std;
import mcpp.core;
import mcpp.plugins;
import mcpp.deps;
import mcpp.plugins.fs;
import mcpp.plugins.toolset;
import gpp.deps.tools;

export namespace gpp::deps::vcpkg {
namespace fs = std::filesystem;

struct Options {
    fs::path manifestRoot;
    std::string triplet;
    fs::path vcpkgRoot; // 留空时从 PATH 查找。
    bool lto = false;   // 只对 clang-cl 端口生效。
};

struct Prefix {
    std::string root;
    std::string include;
    std::string lib;
    std::string bin;
    std::string librarian;
    std::string installStamp; // 库汇集和发布动作依赖此安装凭据。
    explicit operator bool() const { return !root.empty(); }
};

namespace win {
namespace ts = mcpp::plugins::toolset;

std::string readText(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("无法读取 " + file.generic_string());
    return {std::istreambuf_iterator<char>(input), {}};
}

struct Overlays {
    std::vector<fs::path> triplets;
    std::vector<fs::path> ports;
};

// overlay 只有 JSON 这一个配置入口，路径相对于清单目录。
Overlays readOverlays(const fs::path& manifest) {
    const auto file = manifest / "vcpkg-configuration.json";
    if (!fs::exists(file)) return {};
    mcpp::plugins::json::value document;
    if (!mcpp::plugins::json::parse_json(readText(file), document))
        throw std::runtime_error("无法解析 " + file.generic_string());
    const auto pathsFunc = [&](std::string_view key)
        {
            std::vector<fs::path> result;
            if (const auto* list = document.get(key))
                for (const auto& item : list->items)
                    result.push_back((manifest / item.text).lexically_normal());
            return result;
        };
    return {pathsFunc("overlay-triplets"), pathsFunc("overlay-ports")};
}

fs::path findTriplet(std::string_view name, const Overlays& overlay, const fs::path& vcpkg) {
    auto directories = overlay.triplets;
    directories.push_back(vcpkg / "triplets");
    directories.push_back(vcpkg / "triplets/community");
    for (const auto& directory : directories) {
        const auto file = directory / (std::string(name) + ".cmake");
        if (fs::is_regular_file(file)) return file;
    }
    throw std::runtime_error("找不到 vcpkg triplet：" + std::string(name));
}

struct Toolchain {
    ts::resolved_tools tools;
    fs::path assembler;
    bool clangCl = false;
};

// 只解析一次原生工具和 INCLUDE/LIB 环境；使用 LLVM 时替换可执行文件，保留 STL/SDK。
Toolchain selectToolchain() {
    auto resolved = ts::resolve_named();
    if (!resolved) throw std::runtime_error(resolved.error());
    Toolchain result{std::move(*resolved), mcpp::abi_tool("as")};

    if (std::string_view(mcpp::compiler()) == "clang") {
        const auto llvm = fs::path(mcpp::tool("cxx")).parent_path();
        const auto compiler = llvm / "clang-cl.exe";
        mcpp::rerun_if_changed(compiler.generic_string().c_str());
        if (fs::is_regular_file(compiler)) {
            auto& tools = result.tools;
            tools.cc = tools.cxx = compiler.generic_string();
            tools.ld = (llvm / "lld-link.exe").generic_string();
            tools.ar = (llvm / "llvm-lib.exe").generic_string();
            tools.rc = (llvm / "llvm-rc.exe").generic_string();
            tools.path_dirs.insert(tools.path_dirs.begin(), llvm.generic_string());
            tools.identity += "; clang-cl " + compiler.generic_string();
            result.clangCl = true;
        }
    }
    return result;
}

// 工具路径直接写入 chainload 文件；版本和路径变化自然进入 vcpkg 的 ABI 计算。
std::string clangToolchain(const Toolchain& selected, const fs::path& vcpkg) {
    const auto& tools = selected.tools;
    std::string text = "# 由本地 vcpkg 插件生成：clang-cl 编译，沿用 MSVC STL 和 Windows SDK。\n";
    const auto setFunc = [&](std::string_view name, const fs::path& value)
        {
            text += std::format("set({} [[{}]])\n", name, value.generic_string());
        };
    setFunc("CMAKE_C_COMPILER", tools.cc);
    setFunc("CMAKE_CXX_COMPILER", tools.cxx);
    setFunc("CMAKE_LINKER", tools.ld);
    setFunc("CMAKE_AR", tools.ar);
    setFunc("CMAKE_RC_COMPILER", tools.rc);
    setFunc("CMAKE_MT", tools.mt);
    setFunc("CMAKE_ASM_MASM_COMPILER", selected.assembler);
    text += R"(if(VCPKG_TARGET_ARCHITECTURE STREQUAL "x86")
  set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
else()
  set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
endif()
set(CMAKE_CXX_COMPILER_TARGET "${CMAKE_C_COMPILER_TARGET}")
)";
    text += std::format("include([[{}]])\n", (vcpkg / "scripts/toolchains/windows.cmake").generic_string());
    // vcpkg 的资源编译参数按 rc.exe 生成，llvm-rc 要求大写 /C。
    text += "string(REPLACE \"/c65001\" \"/C65001\" CMAKE_RC_FLAGS \"${CMAKE_RC_FLAGS}\")\n";
    return text;
}

std::string derivedTriplet(const Options& opt, const fs::path& base, const Toolchain& selected) {
    const auto& tools = selected.tools;
    std::string text = readText(base);
    text += "\n# 本地依赖工具链：" + tools.identity + "\n";
    // CRT 和静态/动态库策略完全由原 triplet 负责。
    if (selected.clangCl) {
        text += "set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE \"${CMAKE_CURRENT_LIST_DIR}/mcpp-chain-windows.cmake\")\n";
        text += "set(VCPKG_ENV_PASSTHROUGH_UNTRACKED ${VCPKG_ENV_PASSTHROUGH_UNTRACKED}";
        for (const auto& [name, value] : tools.env) text += " " + name;
        text += ")\n";
        if (opt.lto)
            text += "string(APPEND VCPKG_C_FLAGS_RELEASE \" -flto=full -fuse-ld=lld\")\n"
                    "string(APPEND VCPKG_CXX_FLAGS_RELEASE \" -flto=full -fuse-ld=lld\")\n";
    }
    else {
        // 默认工具集交给 vcpkg；显式选择了不同版本时才锁定版本。
        const auto defaultVersion = ts::instance_default_version(mcpp::msvc_instance_dir());
        if (tools.toolset_version != defaultVersion)
            text += std::format("set(VCPKG_PLATFORM_TOOLSET_VERSION {})\n", tools.toolset_version);
    }
    return text;
}

// 只向端口传递编译工具目录和 Windows 系统目录，避免 Git/MSYS 工具混入。
void configureEnvironment(mcpp::action& action, const Toolchain& selected) {
    action.env("VCPKG_VISUAL_STUDIO_PATH", mcpp::msvc_instance_dir());
    if (!selected.clangCl) return;
    for (const auto& [name, value] : selected.tools.env) action.env(name.c_str(), value.c_str());
    std::string path;
    for (const auto& directory : selected.tools.path_dirs) path += fs::path(directory).string() + ";";
    const char* systemRoot = std::getenv("SystemRoot");
    const fs::path windows = systemRoot ? systemRoot : R"(C:\Windows)";
    for (const auto& directory : {windows / "System32", windows, windows / "System32/Wbem",
                                  windows / "System32/WindowsPowerShell/v1.0"})
        path += directory.string() + ";";
    action.env("PATH", path.c_str());
    const char* keep = std::getenv("VCPKG_KEEP_ENV_VARS");
    action.env("VCPKG_KEEP_ENV_VARS", (keep ? std::string(keep) + ";PATH" : "PATH").c_str());
}

// 文件内容和文件集合都要监听，新增、删除 port 或 patch 才会触发重新配置和安装。
void watchOverlay(mcpp::action& action, const fs::path& directory) {
    mcpp::rerun_if_changed_glob((directory / "**/*").generic_string().c_str());
    for (const auto& file : mcpp::deps::files_under(directory)) {
        mcpp::rerun_if_changed(file.c_str());
        action.input(file.c_str());
    }
}

Prefix use(const Options& opt) {
    const auto manifest = fs::absolute(opt.manifestRoot).lexically_normal();
    const auto executable = opt.vcpkgRoot.empty()
        ? gpp::deps::tools::win::findExecutable("vcpkg")
        : fs::absolute(opt.vcpkgRoot) / "vcpkg.exe";
    const auto vcpkg = executable.parent_path();
    const auto overlay = readOverlays(manifest);
    const auto base = findTriplet(opt.triplet, overlay, vcpkg);
    const auto selected = selectToolchain();
    const auto& tools = selected.tools;
    const auto triplet = opt.triplet + "-mcpp";
    const auto generated = fs::path(mcpp::out_dir()) / "deps-vcpkg";

    // 安装位置不含编译器/profile 哈希；工具变化交给 vcpkg 自身判断是否需要重建。
    const auto installation = manifest / "vcpkg_installed" / triplet;
    const auto root = installation / triplet;
    Prefix result{
        root.generic_string(), (root / "include").generic_string(),
        (root / "lib").generic_string(), (root / "bin").generic_string(),
        tools.ar, (generated / "install.stamp").generic_string()
    };
    const auto tripletFile = generated / (triplet + ".cmake");
    const auto chainFile = generated / "mcpp-chain-windows.cmake";
    mcpp::plugins::fs::write_if_changed(tripletFile, derivedTriplet(opt, base, selected));
    if (selected.clangCl)
        mcpp::plugins::fs::write_if_changed(chainFile, clangToolchain(selected, vcpkg));

    const auto description = "vcpkg install " + triplet;
    mcpp::action action;
    action.id = "vcpkg-install";
    action.role = mcpp::roles::prepare;
    action.description = description.c_str();
    action.arg(executable.generic_string().c_str()).arg("install")
        .arg(("--vcpkg-root=" + vcpkg.generic_string()).c_str())
        .arg("--x-wait-for-lock") // 多个成员共用 vcpkg，等待其他安装进程释放锁。
        .arg("--triplet").arg(triplet.c_str())
        .arg(("--x-manifest-root=" + manifest.generic_string()).c_str())
        .arg(("--x-install-root=" + installation.generic_string()).c_str())
        .arg(("--overlay-triplets=" + generated.generic_string()).c_str());
    mcpp::rerun_if_env_changed("VCPKG_KEEP_ENV_VARS");
    configureEnvironment(action, selected);

    for (const auto& file : {manifest / "vcpkg.json", manifest / "vcpkg-configuration.json", base}) {
        mcpp::rerun_if_changed(file.generic_string().c_str());
        if (fs::exists(file)) action.input(file.generic_string().c_str());
    }
    action.input(executable.generic_string().c_str()).input(tripletFile.generic_string().c_str());
    if (selected.clangCl) action.input(chainFile.generic_string().c_str());
    for (const auto& tool : {tools.cxx, tools.ld, tools.ar, tools.rc, tools.mt, selected.assembler.generic_string()}) {
        mcpp::rerun_if_changed(tool.c_str());
        action.input(tool.c_str());
    }
    for (const auto& directory : overlay.triplets) watchOverlay(action, directory);
    for (const auto& directory : overlay.ports) watchOverlay(action, directory);
    action.output(result.installStamp.c_str()).output_dir(result.root.c_str()).submit();
    return result;
}
} // namespace win

// 平台分派只在入口进行。以后其他平台或跨系统构建在这里接入独立实现。
Prefix use(const Options& opt) {
    if (std::string_view(mcpp::host()).contains("windows") &&
        std::string_view(mcpp::target_os()) == "windows" &&
        std::string_view(mcpp::target_env()) == "msvc" &&
        (std::string_view(mcpp::target_arch()) == "x86_64" || std::string_view(mcpp::target_arch()) == "i686"))
        return win::use(opt);
    throw std::runtime_error("vcpkg 当前只支持 Windows 本机的 x86/x64 MSVC ABI 构建");
}
} // namespace gpp::deps::vcpkg
