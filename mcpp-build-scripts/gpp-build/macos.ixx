export module gpp.build.macos;

import std;
import mcpp.core;
import tomlplusplus;
import gpp.rules.qt;

export namespace gpp::macos {
namespace fs = std::filesystem;

// 依赖安装及便携包组装由 build-macos.py 负责；这里只描述本次编译需要的输入。
int configure(std::string_view member) {
    if (!std::string_view(mcpp::host()).contains("macos"))
        throw std::runtime_error("macOS 构建需要 macOS 宿主");
    const auto workspace = fs::path(__FILE__).parent_path().parent_path().parent_path();
    const auto settings = workspace / "build-local.toml";
    mcpp::rerun_if_changed(settings.generic_string().c_str());
    const auto config = toml::parse_file(settings.string());
    const auto path = [&](const char* key) {
        const fs::path value = config["macos"][key].value_or(std::string{});
        if (value.empty()) throw std::runtime_error(std::string("build-local.toml 缺少 macos.") + key);
        return fs::absolute(workspace / value);
    };
    const auto dependencies = path("dependencies_root");
    const auto python = path("python_root");
    const auto ela = path("ela_root");
    mcpp::include_dir((dependencies / "include").generic_string().c_str());
    mcpp::include_dir((python / "include/python3.12").generic_string().c_str());
    if (std::string_view(mcpp::profile()) == "release" ||
        std::string_view(mcpp::profile()) == "fast-release") mcpp::define("NDEBUG");
    const bool core = member == "GalTranslPP";
    const bool gui = member == "GPPGUI";
    gpp::rules::qt::options qt;
    qt.root = path("qt_root");
    qt.modules = gui ? std::vector<std::string>{"Core", "Gui", "Network", "Widgets"}
                    : std::vector<std::string>{"Core"};
    if (gui) qt.resources = {"GPPGUI.qrc"};
    const auto translation = core ? "qt_gpp_en.ts" : gui ? "qt_gppgui_en.ts"
        : member == "GPPCLI" ? "qt_gppcli_en.ts" : "qt_gppupdater_en.ts";
    qt.i18n.ts = {translation};
    qt.i18n.out_dir = mcpp::manifest_dir();
    gpp::rules::qt::use(qt);
    if (gui) {
        mcpp::include_dir((workspace / "3rdParty/ElaWidgetTools/ElaWidgetTools").generic_string().c_str());
        mcpp::link_flag((ela / "libElaWidgetTools.dylib").generic_string().c_str());
    }
    if (!core) {
        // Darwin 链接器会重新搜索归档以满足新出现的引用，无需把所有库强制装入。
        mcpp::rerun_if_changed_glob((dependencies / "lib/*.a").generic_string().c_str());
        std::vector<fs::path> libraries;
        for (const auto& file : fs::directory_iterator(dependencies / "lib"))
            if (file.path().extension() == ".a") libraries.push_back(file.path());
        std::ranges::sort(libraries);
        for (const auto& library : libraries) mcpp::link_flag(library.generic_string().c_str());
        mcpp::link_flag((python / "lib/libpython3.12.dylib").generic_string().c_str());
        for (const char* framework : {"CoreFoundation", "SystemConfiguration", "Security"}) {
            mcpp::link_flag((std::string("-Wl,-framework,") + framework).c_str());
        }
        mcpp::link_flag("-liconv");
        mcpp::link_flag("-lresolv");
        mcpp::link_flag("-Wl,-rpath,@executable_path/lib");
        mcpp::link_flag("-Wl,-rpath,@executable_path/BaseConfig/Python/lib");
        mcpp::link_flag(("-Wl,-rpath," + (python / "lib").string()).c_str());
    }
    return 0;
}
}
