#include <algorithm>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

using bytes = std::vector<unsigned char>;

std::uint16_t read16(const bytes& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2)
        throw std::runtime_error("truncated PE file");
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t read32(const bytes& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("truncated PE file");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::uint64_t read64(const bytes& data, std::size_t offset) {
    return static_cast<std::uint64_t>(read32(data, offset)) |
           (static_cast<std::uint64_t>(read32(data, offset + 4)) << 32);
}

struct section {
    std::uint32_t virtual_address;
    std::uint32_t raw_size;
    std::uint32_t raw_offset;
};

std::size_t file_offset(const bytes& data, const std::vector<section>& sections,
                        std::uint32_t rva) {
    for (const auto& s : sections) {
        if (rva >= s.virtual_address && rva - s.virtual_address < s.raw_size) {
            const auto offset = static_cast<std::size_t>(s.raw_offset) +
                                (rva - s.virtual_address);
            if (offset < data.size()) return offset;
        }
    }
    // Import tables normally live in a section, but a header RVA is legal.
    if (rva < data.size() &&
        (sections.empty() || rva < sections.front().raw_offset)) return rva;
    throw std::runtime_error("PE import RVA is outside the file");
}

std::wstring import_name(const bytes& data, const std::vector<section>& sections,
                         std::uint32_t rva) {
    auto offset = file_offset(data, sections, rva);
    std::wstring result;
    while (offset < data.size() && result.size() < 512) {
        const auto ch = data[offset++];
        if (ch == 0) return result;
        result.push_back(static_cast<wchar_t>(ch));
    }
    throw std::runtime_error("unterminated PE import name");
}

std::vector<std::wstring> pe_imports(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open PE file: " + file.string());
    const bytes data(std::istreambuf_iterator<char>{input}, {});
    if (read16(data, 0) != 0x5a4d)
        throw std::runtime_error("not a PE file: " + file.string());
    const auto pe = static_cast<std::size_t>(read32(data, 0x3c));
    if (read32(data, pe) != 0x00004550)
        throw std::runtime_error("invalid PE signature: " + file.string());
    const auto section_count = read16(data, pe + 6);
    const auto optional_size = read16(data, pe + 20);
    const auto optional = pe + 24;
    const auto magic = read16(data, optional);
    if (magic != 0x10b && magic != 0x20b)
        throw std::runtime_error("unsupported PE optional header");
    const bool pe64 = magic == 0x20b;
    const auto image_base = pe64 ? read64(data, optional + 24)
                                 : static_cast<std::uint64_t>(read32(data, optional + 28));
    const auto directory_count = read32(data, optional + (pe64 ? 108 : 92));
    const auto directories = optional + (pe64 ? 112 : 96);
    const auto section_table = optional + optional_size;
    std::vector<section> sections;
    sections.reserve(section_count);
    for (std::size_t i = 0; i < section_count; ++i) {
        const auto entry = section_table + i * 40;
        sections.push_back({read32(data, entry + 12), read32(data, entry + 16),
                            read32(data, entry + 20)});
    }
    std::ranges::sort(sections, {}, &section::raw_offset);

    std::vector<std::wstring> imports;
    auto collect = [&](std::uint32_t directory_index, std::size_t descriptor_size,
                       std::size_t name_field, bool delayed) {
        if (directory_count <= directory_index ||
            directories + (directory_index + 1) * 8 > optional + optional_size) return;
        const auto directory_rva = read32(data, directories + directory_index * 8);
        if (directory_rva == 0) return;
        for (std::uint32_t i = 0; i < 4096; ++i) {
            const auto descriptor = file_offset(data, sections,
                                                directory_rva + i * descriptor_size);
            auto name_rva = read32(data, descriptor + name_field);
            if (name_rva == 0) return;
            if (delayed && (read32(data, descriptor) & 1) == 0) {
                if (name_rva < image_base)
                    throw std::runtime_error("invalid delay import address");
                name_rva = static_cast<std::uint32_t>(name_rva - image_base);
            }
            imports.push_back(import_name(data, sections, name_rva));
        }
        throw std::runtime_error("too many PE import descriptors");
    };
    collect(1, 20, 12, false);  // IMAGE_IMPORT_DESCRIPTOR
    collect(13, 32, 4, true);  // IMAGE_DELAYLOAD_DESCRIPTOR
    return imports;
}

std::wstring key(std::wstring name) {
    for (auto& ch : name)
        if (ch >= L'A' && ch <= L'Z') ch += L'a' - L'A';
    return name;
}

std::string ascii(const std::wstring& name) {
    return {name.begin(), name.end()};  // PE DLL basenames are ASCII.
}

struct options {
    fs::path exe;
    fs::path manifest;
    fs::path destination;
    std::vector<fs::path> search_dirs;
    std::vector<std::wstring> seeds;
};

options parse_options(int argc, wchar_t** argv) {
    options result;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 == argc) throw std::runtime_error("missing option value");
        const std::wstring_view flag(argv[i]);
        const auto value = argv[++i];
        if (flag == L"--exe") result.exe = value;
        else if (flag == L"--manifest") result.manifest = value;
        else if (flag == L"--dest") result.destination = value;
        else if (flag == L"--search") result.search_dirs.emplace_back(value);
        else if (flag == L"--seed") result.seeds.emplace_back(value);
        else throw std::runtime_error("unknown runtime-stage option");
    }
    if (result.exe.empty() || result.manifest.empty() || result.destination.empty())
        throw std::runtime_error("--exe, --manifest and --dest are required");
    return result;
}

void stage(const options& opts) {
    std::map<std::wstring, fs::path> available;
    for (const auto& dir : opts.search_dirs) {
        if (!fs::is_directory(dir)) continue;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file() || key(entry.path().extension().wstring()) != L".dll")
                continue;
            const auto name = key(entry.path().filename().wstring());
            const auto [it, inserted] = available.emplace(name, entry.path());
            if (!inserted && it->second != entry.path())
                throw std::runtime_error("ambiguous DLL source: " + ascii(name));
        }
    }

    std::map<std::wstring, fs::path> selected;
    std::deque<fs::path> pending{opts.exe};
    auto select = [&](const std::wstring& raw_name, bool required) {
        const auto name = key(raw_name);
        const auto found = available.find(name);
        if (found == available.end()) {
            if (required) throw std::runtime_error("missing runtime DLL: " + ascii(name));
            return;  // Windows and Qt DLLs are deployed by their own providers.
        }
        if (selected.emplace(name, found->second).second) pending.push_back(found->second);
    };
    for (const auto& seed : opts.seeds) select(seed, true);
    while (!pending.empty()) {
        const auto binary = pending.front();
        pending.pop_front();
        for (const auto& name : pe_imports(binary)) select(name, false);
    }

    fs::create_directories(opts.destination);
    for (const auto& [name, source] : selected)
        fs::copy_file(source, opts.destination / source.filename(),
                      fs::copy_options::overwrite_existing);
    std::ofstream manifest(opts.manifest, std::ios::binary | std::ios::trunc);
    if (!manifest) throw std::runtime_error("cannot write runtime manifest");
    for (const auto& [name, source] : selected) manifest << ascii(name) << '\n';
    if (!manifest) throw std::runtime_error("cannot finish runtime manifest");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        stage(parse_options(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "runtime-stage: " << error.what() << '\n';
        return 1;
    }
}
