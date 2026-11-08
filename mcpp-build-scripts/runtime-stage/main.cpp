import std;

namespace fs = std::filesystem;

namespace {

using ByteBuffer = std::vector<unsigned char>;

std::uint16_t readUint16(const ByteBuffer& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2)
        throw std::runtime_error("truncated PE file");
    return (std::uint16_t)(data[offset]) |
           ((std::uint16_t)(data[offset + 1]) << 8);
}

std::uint32_t readUint32(const ByteBuffer& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("truncated PE file");
    return (std::uint32_t)(data[offset]) |
           ((std::uint32_t)(data[offset + 1]) << 8) |
           ((std::uint32_t)(data[offset + 2]) << 16) |
           ((std::uint32_t)(data[offset + 3]) << 24);
}

std::uint64_t readUint64(const ByteBuffer& data, std::size_t offset) {
    return (std::uint64_t)(readUint32(data, offset)) |
           ((std::uint64_t)(readUint32(data, offset + 4)) << 32);
}

struct Section {
    std::uint32_t virtualAddress;
    std::uint32_t rawSize;
    std::uint32_t rawOffset;
};

std::size_t resolveFileOffset(const ByteBuffer& data, const std::vector<Section>& sections,
                                std::uint32_t relativeVirtualAddress) {
    for (const auto& currentSection : sections) {
        if (relativeVirtualAddress >= currentSection.virtualAddress &&
            relativeVirtualAddress - currentSection.virtualAddress < currentSection.rawSize) {
            const auto offset = (std::size_t)(currentSection.rawOffset) +
                                (relativeVirtualAddress - currentSection.virtualAddress);
            if (offset < data.size()) return offset;
        }
    }
    // 导入表通常位于节内，也允许 RVA 指向文件头。
    if (relativeVirtualAddress < data.size() &&
        (sections.empty() || relativeVirtualAddress < sections.front().rawOffset))
        return relativeVirtualAddress;
    throw std::runtime_error("PE import RVA is outside the file");
}

std::wstring readImportLibraryName(const ByteBuffer& data, const std::vector<Section>& sections,
                                      std::uint32_t relativeVirtualAddress) {
    auto offset = resolveFileOffset(data, sections, relativeVirtualAddress);
    std::wstring result;
    while (offset < data.size() && result.size() < 512) {
        const auto character = data[offset++];
        if (character == 0) return result;
        result.push_back((wchar_t)(character));
    }
    throw std::runtime_error("unterminated PE import name");
}

std::vector<std::wstring> readImportedLibraries(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open PE file: " + file.string());
    const ByteBuffer data(std::istreambuf_iterator<char>{input}, {});
    if (readUint16(data, 0) != 0x5a4d)
        throw std::runtime_error("not a PE file: " + file.string());
    const auto peHeaderOffset = (std::size_t)(readUint32(data, 0x3c));
    if (readUint32(data, peHeaderOffset) != 0x00004550)
        throw std::runtime_error("invalid PE signature: " + file.string());
    const auto sectionCount = readUint16(data, peHeaderOffset + 6);
    const auto optionalHeaderSize = readUint16(data, peHeaderOffset + 20);
    const auto optionalHeaderOffset = peHeaderOffset + 24;
    const auto magic = readUint16(data, optionalHeaderOffset);
    if (magic != 0x10b && magic != 0x20b)
        throw std::runtime_error("unsupported PE optional header");
    const bool isPe64 = magic == 0x20b;
    const auto imageBase = isPe64
        ? readUint64(data, optionalHeaderOffset + 24)
        : (std::uint64_t)(readUint32(data, optionalHeaderOffset + 28));
    const auto directoryCount = readUint32(data, optionalHeaderOffset + (isPe64 ? 108 : 92));
    const auto directories = optionalHeaderOffset + (isPe64 ? 112 : 96);
    const auto sectionTable = optionalHeaderOffset + optionalHeaderSize;
    std::vector<Section> sections;
    sections.reserve(sectionCount);
    for (std::size_t i = 0; i < sectionCount; ++i) {
        const auto entry = sectionTable + i * 40;
        sections.push_back({readUint32(data, entry + 12), readUint32(data, entry + 16),
                            readUint32(data, entry + 20)});
    }
    std::ranges::sort(sections, {}, &Section::rawOffset);

    std::vector<std::wstring> imports;
    auto collectFunc = [&](std::uint32_t directoryIndex, std::size_t descriptorSize,
                       std::size_t nameField, bool delayed)
        {
            if (directoryCount <= directoryIndex ||
                directories + (directoryIndex + 1) * 8 > optionalHeaderOffset + optionalHeaderSize) return;
            const auto directoryRva = readUint32(data, directories + directoryIndex * 8);
            if (directoryRva == 0) return;
            for (std::uint32_t i = 0; i < 4096; ++i) {
                const auto descriptor = resolveFileOffset(
                    data, sections, directoryRva + i * descriptorSize);
                auto nameRva = readUint32(data, descriptor + nameField);
                if (nameRva == 0) return;
                if (delayed && (readUint32(data, descriptor) & 1) == 0) {
                    if (nameRva < imageBase)
                        throw std::runtime_error("invalid delay import address");
                    nameRva = (std::uint32_t)(nameRva - imageBase);
                }
                imports.push_back(readImportLibraryName(data, sections, nameRva));
            }
            throw std::runtime_error("too many PE import descriptors");
        };
    collectFunc(1, 20, 12, false);  // IMAGE_IMPORT_DESCRIPTOR
    collectFunc(13, 32, 4, true);  // IMAGE_DELAYLOAD_DESCRIPTOR
    return imports;
}

std::wstring normalizeLibraryName(std::wstring name) {
    for (auto& character : name)
        if (character >= L'A' && character <= L'Z') character += L'a' - L'A';
    return name;
}

std::string narrowLibraryName(const std::wstring& name) {
    return {name.begin(), name.end()};  // PE 导入表中的 DLL 文件名为 ASCII。
}

struct RuntimeStageOptions {
    fs::path exe;
    fs::path manifest;
    fs::path depfile;
    fs::path destination;
    std::vector<fs::path> searchDirectories;
    std::vector<std::wstring> explicitLibraries;
};

RuntimeStageOptions parseArguments(int argc, char** argv) {
    RuntimeStageOptions result;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 == argc) throw std::runtime_error("missing option value");
        const fs::path flag(argv[i]);
        const fs::path value(argv[++i]);
        if (flag == "--exe") result.exe = value;
        else if (flag == "--manifest") result.manifest = value;
        else if (flag == "--depfile") result.depfile = value;
        else if (flag == "--dest") result.destination = value;
        else if (flag == "--search") result.searchDirectories.push_back(value);
        else if (flag == "--include-dll") result.explicitLibraries.push_back(value.wstring());
        else throw std::runtime_error("unknown runtime-stage option");
    }
    if (result.exe.empty() || result.manifest.empty() || result.destination.empty())
        throw std::runtime_error("--exe, --manifest and --dest are required");
    return result;
}

std::string escapeDependencyPath(const fs::path& file) {
    std::string escaped;
    for (char character : fs::absolute(file).lexically_normal().generic_string()) {
        if (character == ' ' || character == '#' || character == ':') escaped += '\\';
        if (character == '$') escaped += '$';
        escaped += character;
    }
    return escaped;
}

void writeDepfile(const fs::path& depfilePath, const fs::path& output,
                   const std::vector<fs::path>& inputs) {
    if (depfilePath.empty()) return;
    if (!depfilePath.parent_path().empty()) fs::create_directories(depfilePath.parent_path());
    std::ofstream depfile(depfilePath, std::ios::binary | std::ios::trunc);
    depfile << escapeDependencyPath(output) << ':';
    for (const auto& input : inputs) depfile << ' ' << escapeDependencyPath(input);
    depfile << '\n';
    if (!depfile) throw std::runtime_error("cannot write depfile: " + depfilePath.string());
}

void stage(const RuntimeStageOptions& commandOptions) {
    std::map<std::wstring, fs::path> availableLibraries;
    for (const auto& searchDirectory : commandOptions.searchDirectories) {
        if (!fs::is_directory(searchDirectory)) continue;
        for (const auto& entry : fs::directory_iterator(searchDirectory)) {
            if (!entry.is_regular_file() || normalizeLibraryName(entry.path().extension().wstring()) != L".dll")
                continue;
            const auto name = normalizeLibraryName(entry.path().filename().wstring());
            const auto [libraryEntry, inserted] = availableLibraries.emplace(name, entry.path());
            if (!inserted && libraryEntry->second != entry.path())
                throw std::runtime_error("ambiguous DLL source: " + narrowLibraryName(name));
        }
    }

    std::map<std::wstring, fs::path> selectedLibraries;
    std::deque<fs::path> pendingBinaries{commandOptions.exe};
    auto selectLibraryFunc = [&](const std::wstring& rawName, bool required)
        {
            const auto name = normalizeLibraryName(rawName);
            const auto found = availableLibraries.find(name);
            if (found == availableLibraries.end()) {
                if (required) throw std::runtime_error("missing runtime DLL: " + narrowLibraryName(name));
                return;  // 系统 DLL 由 Windows 提供，Qt DLL 由用户另外部署。
            }
            if (selectedLibraries.emplace(name, found->second).second) pendingBinaries.push_back(found->second);
        };
    for (const auto& libraryName : commandOptions.explicitLibraries) selectLibraryFunc(libraryName, true);
    while (!pendingBinaries.empty()) {
        const auto binary = pendingBinaries.front();
        pendingBinaries.pop_front();
        for (const auto& name : readImportedLibraries(binary)) selectLibraryFunc(name, false);
    }

    fs::create_directories(commandOptions.destination);
    for (const auto& [name, source] : selectedLibraries)
        fs::copy_file(source, commandOptions.destination / source.filename(),
                      fs::copy_options::overwrite_existing);
    std::string manifestText;
    for (const auto& [name, source] : selectedLibraries)
        std::format_to(std::back_inserter(manifestText), "{}\n", narrowLibraryName(name));
    if (!commandOptions.manifest.parent_path().empty())
        fs::create_directories(commandOptions.manifest.parent_path());
    std::ofstream manifest(commandOptions.manifest, std::ios::binary | std::ios::trunc);
    if (!manifest) throw std::runtime_error("cannot write runtime manifest");
    manifest.write(manifestText.data(), (std::streamsize)(manifestText.size()));
    if (!manifest) throw std::runtime_error("cannot finish runtime manifest");

    // 首次构建时 vcpkg/Ela 的 DLL 可能尚不存在，build.mcpp 无法提前枚举。
    // 执行时记录源文件和已部署文件，让源 DLL 更新或目标 DLL 缺失时重新发布。
    if (!commandOptions.depfile.empty()) {
        std::vector<fs::path> inputs{commandOptions.exe};
        for (const auto& [name, source] : selectedLibraries) {
            inputs.push_back(source);
            inputs.push_back(commandOptions.destination / source.filename());
        }
        writeDepfile(commandOptions.depfile, commandOptions.manifest, inputs);
    }
}

struct TreeStageOptions {
    fs::path source;
    fs::path destination;
    fs::path manifest;
    fs::path depfile;
};

TreeStageOptions parseTreeArguments(int argc, char** argv) {
    TreeStageOptions result;
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

void copyTree(const TreeStageOptions& options) {
    if (!fs::is_directory(options.source))
        throw std::runtime_error("source directory does not exist: " + options.source.string());

    fs::create_directories(options.destination);
    std::vector<fs::path> directories{options.source};
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(options.source)) {
        if (entry.is_directory()) directories.push_back(entry.path());
        else if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::ranges::sort(directories);
    std::ranges::sort(files);

    std::string manifestText;
    for (const auto& source : files) {
        const fs::path relative = source.lexically_relative(options.source);
        const fs::path destination = options.destination / relative;
        fs::create_directories(destination.parent_path());
        bool different = !fs::is_regular_file(destination) ||
                         fs::file_size(source) != fs::file_size(destination);
        if (!different) {
            std::ifstream sourceStream(source, std::ios::binary);
            std::ifstream destinationStream(destination, std::ios::binary);
            if (!sourceStream || !destinationStream)
                throw std::runtime_error("cannot compare staged file: " + source.string());
            different = !std::equal(std::istreambuf_iterator<char>(sourceStream),
                                    std::istreambuf_iterator<char>(),
                                    std::istreambuf_iterator<char>(destinationStream),
                                    std::istreambuf_iterator<char>());
        }
        if (different) fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        std::format_to(std::back_inserter(manifestText), "{}\n", relative.generic_string());
    }

    if (!options.manifest.parent_path().empty()) fs::create_directories(options.manifest.parent_path());
    std::ofstream manifest(options.manifest, std::ios::binary | std::ios::trunc);
    if (!manifest) throw std::runtime_error("cannot write tree manifest");
    manifest.write(manifestText.data(), (std::streamsize)(manifestText.size()));
    if (!manifest) throw std::runtime_error("cannot finish tree manifest");
    manifest.close();
    if (!manifest) throw std::runtime_error("cannot close tree manifest");
    // 目录也是声明的输出；更新其时间戳，避免未变化的文件使复制动作反复执行。
    fs::last_write_time(options.destination, fs::last_write_time(options.manifest));

    // 记录源目录和文件依赖；文件集合变化另由构建脚本的 glob 监视。
    directories.insert(directories.end(), files.begin(), files.end());
    // 目标文件作为存在性依赖，删除其中任何文件都重新复制，包括运行时才发现的新文件。
    for (const auto& file : files)
        directories.push_back(options.destination / file.lexically_relative(options.source));
    writeDepfile(options.depfile, options.manifest, directories);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--copy-tree")
            copyTree(parseTreeArguments(argc, argv));
        else
            stage(parseArguments(argc, argv));
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << std::format("runtime-stage: {}\n", error.what());
        return 1;
    }
}
