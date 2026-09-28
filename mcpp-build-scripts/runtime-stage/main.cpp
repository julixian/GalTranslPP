import std;

namespace fs = std::filesystem;

namespace {

using byte_buffer = std::vector<unsigned char>;

std::uint16_t read_uint16(const byte_buffer& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2)
        throw std::runtime_error("truncated PE file");
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t read_uint32(const byte_buffer& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("truncated PE file");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::uint64_t read_uint64(const byte_buffer& data, std::size_t offset) {
    return static_cast<std::uint64_t>(read_uint32(data, offset)) |
           (static_cast<std::uint64_t>(read_uint32(data, offset + 4)) << 32);
}

struct section {
    std::uint32_t virtual_address;
    std::uint32_t raw_size;
    std::uint32_t raw_offset;
};

std::size_t resolve_file_offset(const byte_buffer& data, const std::vector<section>& sections,
                                std::uint32_t relative_virtual_address) {
    for (const auto& current_section : sections) {
        if (relative_virtual_address >= current_section.virtual_address &&
            relative_virtual_address - current_section.virtual_address < current_section.raw_size) {
            const auto offset = static_cast<std::size_t>(current_section.raw_offset) +
                                (relative_virtual_address - current_section.virtual_address);
            if (offset < data.size()) return offset;
        }
    }
    // 导入表通常位于节内，也允许 RVA 指向文件头。
    if (relative_virtual_address < data.size() &&
        (sections.empty() || relative_virtual_address < sections.front().raw_offset))
        return relative_virtual_address;
    throw std::runtime_error("PE import RVA is outside the file");
}

std::wstring read_import_library_name(const byte_buffer& data, const std::vector<section>& sections,
                                      std::uint32_t relative_virtual_address) {
    auto offset = resolve_file_offset(data, sections, relative_virtual_address);
    std::wstring result;
    while (offset < data.size() && result.size() < 512) {
        const auto character = data[offset++];
        if (character == 0) return result;
        result.push_back(static_cast<wchar_t>(character));
    }
    throw std::runtime_error("unterminated PE import name");
}

std::vector<std::wstring> read_imported_libraries(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open PE file: " + file.string());
    const byte_buffer data(std::istreambuf_iterator<char>{input}, {});
    if (read_uint16(data, 0) != 0x5a4d)
        throw std::runtime_error("not a PE file: " + file.string());
    const auto pe_header_offset = static_cast<std::size_t>(read_uint32(data, 0x3c));
    if (read_uint32(data, pe_header_offset) != 0x00004550)
        throw std::runtime_error("invalid PE signature: " + file.string());
    const auto section_count = read_uint16(data, pe_header_offset + 6);
    const auto optional_header_size = read_uint16(data, pe_header_offset + 20);
    const auto optional_header_offset = pe_header_offset + 24;
    const auto magic = read_uint16(data, optional_header_offset);
    if (magic != 0x10b && magic != 0x20b)
        throw std::runtime_error("unsupported PE optional header");
    const bool is_pe64 = magic == 0x20b;
    const auto image_base = is_pe64
        ? read_uint64(data, optional_header_offset + 24)
        : static_cast<std::uint64_t>(read_uint32(data, optional_header_offset + 28));
    const auto directory_count = read_uint32(data, optional_header_offset + (is_pe64 ? 108 : 92));
    const auto directories = optional_header_offset + (is_pe64 ? 112 : 96);
    const auto section_table = optional_header_offset + optional_header_size;
    std::vector<section> sections;
    sections.reserve(section_count);
    for (std::size_t i = 0; i < section_count; ++i) {
        const auto entry = section_table + i * 40;
        sections.push_back({read_uint32(data, entry + 12), read_uint32(data, entry + 16),
                            read_uint32(data, entry + 20)});
    }
    std::ranges::sort(sections, {}, &section::raw_offset);

    std::vector<std::wstring> imports;
    auto collect = [&](std::uint32_t directory_index, std::size_t descriptor_size,
                       std::size_t name_field, bool delayed) {
        if (directory_count <= directory_index ||
            directories + (directory_index + 1) * 8 > optional_header_offset + optional_header_size) return;
        const auto directory_rva = read_uint32(data, directories + directory_index * 8);
        if (directory_rva == 0) return;
        for (std::uint32_t i = 0; i < 4096; ++i) {
            const auto descriptor = resolve_file_offset(
                data, sections, directory_rva + i * descriptor_size);
            auto name_rva = read_uint32(data, descriptor + name_field);
            if (name_rva == 0) return;
            if (delayed && (read_uint32(data, descriptor) & 1) == 0) {
                if (name_rva < image_base)
                    throw std::runtime_error("invalid delay import address");
                name_rva = static_cast<std::uint32_t>(name_rva - image_base);
            }
            imports.push_back(read_import_library_name(data, sections, name_rva));
        }
        throw std::runtime_error("too many PE import descriptors");
    };
    collect(1, 20, 12, false);  // IMAGE_IMPORT_DESCRIPTOR
    collect(13, 32, 4, true);  // IMAGE_DELAYLOAD_DESCRIPTOR
    return imports;
}

std::wstring normalize_library_name(std::wstring name) {
    for (auto& character : name)
        if (character >= L'A' && character <= L'Z') character += L'a' - L'A';
    return name;
}

std::string narrow_library_name(const std::wstring& name) {
    return {name.begin(), name.end()};  // PE 导入表中的 DLL 文件名为 ASCII。
}

struct runtime_stage_options {
    fs::path exe;
    fs::path manifest;
    fs::path depfile;
    fs::path destination;
    std::vector<fs::path> search_directories;
    std::vector<std::wstring> explicit_libraries;
};

runtime_stage_options parse_arguments(int argc, char** argv) {
    runtime_stage_options result;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 == argc) throw std::runtime_error("missing option value");
        const fs::path flag(argv[i]);
        const fs::path value(argv[++i]);
        if (flag == "--exe") result.exe = value;
        else if (flag == "--manifest") result.manifest = value;
        else if (flag == "--depfile") result.depfile = value;
        else if (flag == "--dest") result.destination = value;
        else if (flag == "--search") result.search_directories.push_back(value);
        else if (flag == "--include-dll") result.explicit_libraries.push_back(value.wstring());
        else throw std::runtime_error("unknown runtime-stage option");
    }
    if (result.exe.empty() || result.manifest.empty() || result.destination.empty())
        throw std::runtime_error("--exe, --manifest and --dest are required");
    return result;
}

std::string escape_dependency_path(const fs::path& file) {
    std::string escaped;
    for (char character : fs::absolute(file).lexically_normal().generic_string()) {
        if (character == ' ' || character == '#' || character == ':') escaped += '\\';
        if (character == '$') escaped += '$';
        escaped += character;
    }
    return escaped;
}

void write_depfile(const fs::path& depfile_path, const fs::path& output,
                   const std::vector<fs::path>& inputs) {
    if (depfile_path.empty()) return;
    if (!depfile_path.parent_path().empty()) fs::create_directories(depfile_path.parent_path());
    std::ofstream depfile(depfile_path, std::ios::binary | std::ios::trunc);
    depfile << escape_dependency_path(output) << ':';
    for (const auto& input : inputs) depfile << ' ' << escape_dependency_path(input);
    depfile << '\n';
    if (!depfile) throw std::runtime_error("cannot write depfile: " + depfile_path.string());
}

void stage(const runtime_stage_options& command_options) {
    std::map<std::wstring, fs::path> available_libraries;
    for (const auto& search_directory : command_options.search_directories) {
        if (!fs::is_directory(search_directory)) continue;
        for (const auto& entry : fs::directory_iterator(search_directory)) {
            if (!entry.is_regular_file() || normalize_library_name(entry.path().extension().wstring()) != L".dll")
                continue;
            const auto name = normalize_library_name(entry.path().filename().wstring());
            const auto [library_entry, inserted] = available_libraries.emplace(name, entry.path());
            if (!inserted && library_entry->second != entry.path())
                throw std::runtime_error("ambiguous DLL source: " + narrow_library_name(name));
        }
    }

    std::map<std::wstring, fs::path> selected_libraries;
    std::deque<fs::path> pending_binaries{command_options.exe};
    auto select_library = [&](const std::wstring& raw_name, bool required) {
        const auto name = normalize_library_name(raw_name);
        const auto found = available_libraries.find(name);
        if (found == available_libraries.end()) {
            if (required) throw std::runtime_error("missing runtime DLL: " + narrow_library_name(name));
            return;  // 系统 DLL 由 Windows 提供，Qt DLL 由用户另外部署。
        }
        if (selected_libraries.emplace(name, found->second).second) pending_binaries.push_back(found->second);
    };
    for (const auto& library_name : command_options.explicit_libraries) select_library(library_name, true);
    while (!pending_binaries.empty()) {
        const auto binary = pending_binaries.front();
        pending_binaries.pop_front();
        for (const auto& name : read_imported_libraries(binary)) select_library(name, false);
    }

    fs::create_directories(command_options.destination);
    for (const auto& [name, source] : selected_libraries)
        fs::copy_file(source, command_options.destination / source.filename(),
                      fs::copy_options::overwrite_existing);
    std::string manifest_text;
    for (const auto& [name, source] : selected_libraries)
        std::format_to(std::back_inserter(manifest_text), "{}\n", narrow_library_name(name));
    if (!command_options.manifest.parent_path().empty())
        fs::create_directories(command_options.manifest.parent_path());
    std::ofstream manifest(command_options.manifest, std::ios::binary | std::ios::trunc);
    if (!manifest) throw std::runtime_error("cannot write runtime manifest");
    manifest.write(manifest_text.data(), static_cast<std::streamsize>(manifest_text.size()));
    if (!manifest) throw std::runtime_error("cannot finish runtime manifest");

    // 首次构建时 vcpkg/Ela 的 DLL 可能尚不存在，build.mcpp 无法提前枚举。
    // 执行时记录实际读取的文件，让后续仅 DLL 更新时也能触发发布。
    if (!command_options.depfile.empty()) {
        std::vector<fs::path> inputs{command_options.exe};
        for (const auto& [name, source] : selected_libraries)
            inputs.push_back(source);
        write_depfile(command_options.depfile, command_options.manifest, inputs);
    }
}

struct tree_stage_options {
    fs::path source;
    fs::path destination;
    fs::path manifest;
    fs::path depfile;
};

tree_stage_options parse_tree_arguments(int argc, char** argv) {
    tree_stage_options result;
    for (int i = 2; i < argc; ++i) {
        if (i + 1 == argc) throw std::runtime_error("missing option value");
        const std::string_view flag(argv[i]);
        const fs::path value(argv[++i]);
        if (flag == "--source") result.source = value;
        else if (flag == "--dest") result.destination = value;
        else if (flag == "--manifest") result.manifest = value;
        else if (flag == "--depfile") result.depfile = value;
        else throw std::runtime_error("unknown copy-tree option");
    }
    if (result.source.empty() || result.destination.empty() || result.manifest.empty() || result.depfile.empty())
        throw std::runtime_error("--copy-tree requires --source, --dest, --manifest and --depfile");
    return result;
}

void copy_tree(const tree_stage_options& options) {
    if (!fs::is_directory(options.source))
        throw std::runtime_error("source directory does not exist: " + options.source.string());

    std::vector<fs::path> directories{options.source};
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(options.source)) {
        if (entry.is_directory()) directories.push_back(entry.path());
        else if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::ranges::sort(directories);
    std::ranges::sort(files);

    std::string manifest_text;
    for (const auto& source : files) {
        const fs::path relative = source.lexically_relative(options.source);
        const fs::path destination = options.destination / relative;
        fs::create_directories(destination.parent_path());
        bool different = !fs::is_regular_file(destination) ||
                         fs::file_size(source) != fs::file_size(destination);
        if (!different) {
            std::ifstream source_stream(source, std::ios::binary);
            std::ifstream destination_stream(destination, std::ios::binary);
            if (!source_stream || !destination_stream)
                throw std::runtime_error("cannot compare staged file: " + source.string());
            different = !std::equal(std::istreambuf_iterator<char>(source_stream),
                                    std::istreambuf_iterator<char>(),
                                    std::istreambuf_iterator<char>(destination_stream),
                                    std::istreambuf_iterator<char>());
        }
        if (different) fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        std::format_to(std::back_inserter(manifest_text), "{}\n", relative.generic_string());
    }

    if (!options.manifest.parent_path().empty()) fs::create_directories(options.manifest.parent_path());
    std::ofstream manifest(options.manifest, std::ios::binary | std::ios::trunc);
    if (!manifest) throw std::runtime_error("cannot write tree manifest");
    manifest.write(manifest_text.data(), static_cast<std::streamsize>(manifest_text.size()));
    if (!manifest) throw std::runtime_error("cannot finish tree manifest");

    // 目录追踪新增/删除的文件，文件追踪内容变化；安装目录首次由 prepare action 创建。
    directories.insert(directories.end(), files.begin(), files.end());
    write_depfile(options.depfile, options.manifest, directories);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--copy-tree")
            copy_tree(parse_tree_arguments(argc, argv));
        else
            stage(parse_arguments(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << std::format("runtime-stage: {}\n", error.what());
        return 1;
    }
}
