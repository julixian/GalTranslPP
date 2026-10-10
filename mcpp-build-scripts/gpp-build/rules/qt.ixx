module;
#include <cctype>

export module gpp.rules.qt;

import std;
import mcpp;
import mcpp.plugins;

export namespace gpp::rules::qt {
namespace fs = std::filesystem;

struct translations {
    std::vector<std::string> ts;
    bool update_sources = false;
    std::vector<std::string> tr_function_alias;
    std::vector<std::string> sources;
    fs::path out_dir;
};

struct options {
    fs::path root;
    std::vector<std::string> modules = {"Core"};
    std::vector<std::string> forms;
    std::vector<std::string> resources;
    translations i18n;
};

namespace win {
std::string generic(const fs::path& path) {
    return path.lexically_normal().generic_string();
}

fs::path absolute_from_root(const fs::path& path) {
    return (fs::path(mcpp::manifest_dir()) / path).lexically_normal();
}

std::string upper(std::string text) {
    for (char& character : text)
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    return text;
}

// 只扫描手写文件；VS 和 mcpp 的生成目录都不参与 moc/lupdate。
std::vector<fs::path> project_files(std::initializer_list<std::string_view> extensions) {
    std::vector<fs::path> files;
    for (auto entry = fs::recursive_directory_iterator(mcpp::manifest_dir());
         entry != fs::recursive_directory_iterator(); ++entry) {
        if (entry->is_directory()) {
            const auto name = entry->path().filename().string();
            if (name == "x86" || name == "x64" || name == "target" || name == ".git" ||
                name == "mcpp-generated" || name == "vcpkg_installed" ||
                mcpp::plugins::tree::build_output(entry->path()))
                entry.disable_recursion_pending();
        } else if (entry->is_regular_file() &&
                   std::ranges::find(extensions, entry->path().extension().string()) != extensions.end()) {
            files.push_back(entry->path());
        }
    }
    std::ranges::sort(files);
    return files;
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("无法读取 " + path.generic_string());
    return {std::istreambuf_iterator<char>(input), {}};
}

bool declares_meta_object(std::string_view text) {
    const auto identifier = [](char character) {
        return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
    };
    for (const std::string_view macro : {"Q_OBJECT", "Q_GADGET", "Q_NAMESPACE", "Q_GADGET_EXPORT", "Q_NAMESPACE_EXPORT"}) {
        for (auto pos = text.find(macro); pos != std::string_view::npos; pos = text.find(macro, pos + 1)) {
            const auto end = pos + macro.size();
            if ((pos == 0 || !identifier(text[pos - 1])) &&
                (end == text.size() || !identifier(text[end])))
                return true;
        }
    }
    return false;
}

std::vector<std::string> qrc_files(const fs::path& file) {
    mcpp::plugins::xml::node document;
    std::string error;
    if (!mcpp::plugins::xml::parse(read_file(file), document, error))
        throw std::runtime_error("无法解析 " + file.generic_string() + "：" + error);
    std::vector<std::string> files;
    const auto visit = [&](this auto&& self, const mcpp::plugins::xml::node& node) -> void {
        if (node.name == "file") {
            // XML 解析器把 <file> 内的路径放在匿名文本子节点中。
            std::string text;
            for (const auto& child : node.children)
                if (child.name.empty()) text += child.text;
            text = mcpp::plugins::xml::trim_copy(text);
            if (!text.empty()) files.push_back(generic(file.parent_path() / text));
        }
        for (const auto& child : node.children) self(child);
    };
    visit(document);
    return files;
}

// 所有 Qt 工具都取自同一个 SDK。
std::string tool(const fs::path& sdk, std::string_view name) {
    const auto executable = sdk / "bin" / (std::string(name) + ".exe");
    if (!fs::is_regular_file(executable))
        throw std::runtime_error("Qt 工具不存在：" + executable.generic_string());
    return executable.generic_string();
}


// 一个实例负责一个成员的生成文件；各阶段共用相同的宏、include 和输出目录。
struct generator {
    const options& opt;
    fs::path sdk;
    fs::path output = fs::path(mcpp::out_dir()) / "qt";
    std::vector<std::string> definitions;
    std::vector<std::string> includes;
    std::map<std::string, fs::path> outputs;

    explicit generator(const options& options)
        : opt(options), sdk(fs::absolute(options.root).lexically_normal()) {
        fs::create_directories(output);
    }

    void define(std::string value) {
        mcpp::define(value.c_str());
        definitions.push_back(std::move(value));
    }

    void claim(const std::string& filename, const fs::path& source) {
        const auto [previous, inserted] = outputs.try_emplace(filename, source);
        if (!inserted)
            throw std::runtime_error(std::format("Qt 生成文件重名 {}：{} 与 {}", filename,
                generic(previous->second), generic(source)));
    }

    void configure_modules() {
        includes = {generic(sdk / "include"), std::string(mcpp::manifest_dir())};
        const std::string_view profile = mcpp::profile();
        if (profile != "dev" && profile != "debug") define("QT_NO_DEBUG");
        for (const char* value : {"UNICODE", "_UNICODE", "WIN32", "_ENABLE_EXTENDED_ALIGNED_STORAGE"})
            define(value);
        if (std::string_view(mcpp::target_arch()) == "x86_64") define("WIN64");
        for (const auto& module : opt.modules) {
            includes.push_back(generic(sdk / "include" / ("Qt" + module)));
            mcpp::link_flag(generic(sdk / "lib" / ("Qt6" + module + ".lib")).c_str());
            define("QT_" + upper(module) + "_LIB");
        }
        for (const auto& directory : includes) mcpp::include_dir(directory.c_str());
        mcpp::include_dir(generic(output).c_str());
    }

    void moc_source(const fs::path& source, const std::string& filename) {
        claim(filename, source);
        const auto executable = tool(sdk, "moc");
        const auto generated = generic(output / filename);
        const auto depfile = generated + ".d";
        const auto id = "qt:moc:" + filename;
        const auto description = "moc " + source.filename().string();
        mcpp::action action;
        action.id = id.c_str();
        action.role = mcpp::roles::source;
        action.description = description.c_str();
        action.depfile = depfile.c_str();
        action.arg(executable.c_str()).arg(generic(source).c_str())
            .arg("-o").arg(generated.c_str())
            .arg("--output-dep-file").arg("--dep-file-path").arg(depfile.c_str());
        for (const auto& directory : includes) action.arg(("-I" + directory).c_str());
        for (const auto& definition : definitions) action.arg(("-D" + definition).c_str());
        action.input(executable.c_str()).input(generic(source).c_str()).output(generated.c_str()).submit();
    }

    // 头文件生成独立 moc cpp；源码中的元对象生成供该源码 include 的 .moc。
    void configure_moc() {
        for (const char* pattern : {"**/*.h", "**/*.hpp", "**/*.hxx", "**/*.cpp", "**/*.cc", "**/*.cxx"})
            mcpp::rerun_if_changed_glob(pattern);
        for (const auto& source : project_files({".h", ".hpp", ".hxx", ".cpp", ".cc", ".cxx"})) {
            mcpp::rerun_if_changed(generic(source).c_str());
            const auto text = read_file(source);
            if (!declares_meta_object(text)) continue;
            const auto extension = source.extension();
            const auto stem = source.stem().string();
            if (extension == ".h" || extension == ".hpp" || extension == ".hxx")
                moc_source(source, "moc_" + stem + ".cpp");
            else if (text.contains("\"" + stem + ".moc\""))
                moc_source(source, stem + ".moc");
        }
    }

    void configure_forms() {
        if (opt.forms.empty()) return;
        const auto executable = tool(sdk, "uic");
        for (const auto& form : opt.forms) {
            const auto source = absolute_from_root(form);
            const auto stem = source.stem().string();
            const auto filename = "ui_" + stem + ".h";
            claim(filename, source);
            const auto id = "qt:uic:" + stem;
            const auto description = "uic " + source.filename().string();
            mcpp::action action;
            action.id = id.c_str();
            action.role = mcpp::roles::source;
            action.description = description.c_str();
            action.arg(executable.c_str()).arg(generic(source).c_str())
                .arg("-o").arg(generic(output / filename).c_str())
                .input(executable.c_str()).input(generic(source).c_str())
                .output(generic(output / filename).c_str()).submit();
        }
    }

    void configure_resources() {
        if (opt.resources.empty()) return;
        const auto executable = tool(sdk, "rcc");
        for (const auto& resource : opt.resources) {
            const auto source = absolute_from_root(resource);
            const auto stem = source.stem().string();
            const auto filename = "qrc_" + stem + ".cpp";
            claim(filename, source);
            const auto id = "qt:rcc:" + stem;
            const auto description = "rcc " + source.filename().string();
            mcpp::action action;
            action.id = id.c_str();
            action.role = mcpp::roles::source;
            action.description = description.c_str();
            action.arg(executable.c_str()).arg("--name").arg(stem.c_str()).arg(generic(source).c_str())
                .arg("-o").arg(generic(output / filename).c_str())
                .input(executable.c_str()).input(generic(source).c_str());
            // qrc 改变会重建依赖列表；资源内容改变则直接重跑 rcc，包括 SampleProject.zip。
            mcpp::rerun_if_changed(generic(source).c_str());
            for (const auto& file : qrc_files(source)) action.input(file.c_str());
            action.output(generic(output / filename).c_str()).submit();
        }
    }

    std::vector<std::string> translation_sources() {
        std::vector<std::string> sources;
        if (!opt.i18n.sources.empty()) {
            for (const auto& source : opt.i18n.sources)
                sources.push_back(generic(absolute_from_root(source)));
        } else {
            for (const char* pattern : {"**/*.cpp", "**/*.h", "**/*.hpp", "**/*.ixx", "**/*.cppm"})
                mcpp::rerun_if_changed_glob(pattern);
            for (const auto& source : project_files({".cpp", ".h", ".hpp", ".ixx", ".cppm"}))
                sources.push_back(generic(source));
        }
        return sources;
    }

    void update_translation(const fs::path& file, const std::vector<std::string>& sources) {
        const auto executable = tool(sdk, "lupdate");
        const auto id = "qt:lupdate:" + file.stem().string();
        const auto description = "lupdate " + file.filename().string();
        mcpp::action action;
        action.id = id.c_str();
        action.role = mcpp::roles::source;
        action.description = description.c_str();
        action.arg(executable.c_str()).arg("-silent").arg("-extensions").arg("cpp,h,hpp,ixx,cppm");
        for (const auto& alias : opt.i18n.tr_function_alias)
            action.arg("-tr-function-alias").arg(alias.c_str());
        for (const auto& source : sources) action.arg(source.c_str()).input(source.c_str());
        action.arg("-ts").arg(generic(file).c_str())
            .input(executable.c_str()).output(generic(file).c_str()).submit();
    }

    void configure_translations() {
        if (opt.i18n.ts.empty()) return;
        const auto executable = tool(sdk, "lrelease");
        const auto sources = opt.i18n.update_sources ? translation_sources() : std::vector<std::string>{};
        const auto directory = opt.i18n.out_dir.empty() ? output / "translations"
            : fs::path(mcpp::manifest_dir()) / opt.i18n.out_dir;
        fs::create_directories(directory);
        for (const auto& translation : opt.i18n.ts) {
            const auto source = absolute_from_root(translation);
            const auto stem = source.stem().string();
            claim(stem + ".qm", source);
            if (opt.i18n.update_sources) update_translation(source, sources);
            const auto id = "qt:lrelease:" + stem;
            const auto description = "lrelease " + source.filename().string();
            const auto qm = generic(directory / (stem + ".qm"));
            mcpp::action action;
            action.id = id.c_str();
            action.role = mcpp::roles::source;
            action.description = description.c_str();
            action.arg(executable.c_str()).arg("-silent").arg(generic(source).c_str())
                .arg("-qm").arg(qm.c_str()).input(executable.c_str())
                .input(generic(source).c_str()).output(qm.c_str()).submit();
        }
    }
};

void use(const options& opt) {
    generator build(opt);
    build.configure_modules();
    build.configure_moc();
    build.configure_forms();
    build.configure_resources();
    build.configure_translations();
}
} // namespace win

void use(const options& opt) {
    if (std::string_view(mcpp::host()).contains("windows") &&
        std::string_view(mcpp::target_os()) == "windows" &&
        std::string_view(mcpp::target_env()) == "msvc")
        return win::use(opt);
    throw std::runtime_error("Qt 当前只支持 Windows 本机的 MSVC ABI 构建");
}
} // namespace gpp::rules::qt
