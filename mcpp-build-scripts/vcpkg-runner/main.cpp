#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

import std;

namespace fs = std::filesystem;

namespace {

struct UniqueHandle {
    HANDLE handle = nullptr;

    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE value) : handle(value) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    ~UniqueHandle() { reset(); }

    void reset(HANDLE value = nullptr) {
        if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        handle = value;
    }

    HANDLE* out() {
        reset();
        return &handle;
    }

    HANDLE get() const { return handle; }
    bool valid() const { return handle && handle != INVALID_HANDLE_VALUE; }
};

std::string utf8Path(const fs::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::wstring quoteWindowsArg(std::wstring_view arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
        return std::wstring(arg);
    std::wstring out = L"\"";
    for (std::size_t i = 0; i < arg.size(); ++i) {
        std::size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++backslashes;
            ++i;
        }
        if (i == arg.size()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
        }
        else {
            out.append(backslashes, L'\\');
            out.push_back(arg[i]);
        }
    }
    out.push_back(L'"');
    return out;
}

void writeAll(HANDLE handle, const char* data, DWORD size) {
    if (!handle || handle == INVALID_HANDLE_VALUE) return;
    DWORD offset = 0;
    while (offset < size) {
        DWORD written = 0;
        if (!WriteFile(handle, data + offset, size - offset, &written, nullptr) || written == 0)
            break;
        offset += written;
    }
}

void forwardStream(HANDLE pipeRead, HANDLE mirrorHandle, HANDLE logHandle, std::mutex& logMutex) {
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD bytesRead = 0;
        if (!ReadFile(pipeRead, buffer.data(), (DWORD)(buffer.size()), &bytesRead, nullptr) || bytesRead == 0)
            break;
        {
            std::scoped_lock lock(logMutex);
            writeAll(logHandle, buffer.data(), bytesRead);
        }
        writeAll(mirrorHandle, buffer.data(), bytesRead);
    }
}

struct RunnerOptions {
    fs::path logFile;
    std::vector<std::wstring> command;
};

RunnerOptions parseCommandLine() {
    int argc = 0;
    LPWSTR* rawArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!rawArgv) throw std::runtime_error(std::format("解析命令行失败：{}", GetLastError()));
    struct Guard {
        LPWSTR* ptr;
        ~Guard() { LocalFree(ptr); }
    } guard{rawArgv};

    RunnerOptions options;
    int commandStart = argc;
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view arg = rawArgv[i];
        if (arg == L"--") {
            commandStart = i + 1;
            break;
        }
        if (arg == L"--log") {
            if (++i == argc) throw std::runtime_error("缺少 --log 参数值");
            options.logFile = rawArgv[i];
        }
        else {
            throw std::runtime_error("未知参数；请使用 vcpkg-runner --log <日志文件> -- <命令> [参数...]");
        }
    }
    if (options.logFile.empty() || commandStart >= argc)
        throw std::runtime_error("需要 --log <日志文件> 和 `-- <命令> [参数...]`");
    for (int i = commandStart; i < argc; ++i) options.command.emplace_back(rawArgv[i]);
    return options;
}

} // namespace

int main() {
    try {
        const auto options = parseCommandLine();
        if (options.logFile.has_parent_path()) fs::create_directories(options.logFile.parent_path());

        UniqueHandle logHandle(CreateFileW(
            options.logFile.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (!logHandle.valid())
            throw std::runtime_error(std::format("无法创建日志文件 {}：{}",
                utf8Path(options.logFile), GetLastError()));

        std::wstring commandLine;
        for (std::size_t i = 0; i < options.command.size(); ++i) {
            if (i != 0) commandLine.push_back(L' ');
            commandLine += quoteWindowsArg(options.command[i]);
        }

        SECURITY_ATTRIBUTES inheritSa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        UniqueHandle stdoutRead, stdoutWrite, stderrRead, stderrWrite, nullInput;
        if (!CreatePipe(stdoutRead.out(), stdoutWrite.out(), &inheritSa, 0) ||
            !CreatePipe(stderrRead.out(), stderrWrite.out(), &inheritSa, 0))
            throw std::runtime_error(std::format("创建管道失败：{}", GetLastError()));
        if (!SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(stderrRead.get(), HANDLE_FLAG_INHERIT, 0))
            throw std::runtime_error(std::format("设置管道属性失败：{}", GetLastError()));

        nullInput.reset(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &inheritSa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

        STARTUPINFOW startup{sizeof(STARTUPINFOW)};
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = nullInput.valid() ? nullInput.get() : GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = stdoutWrite.get();
        startup.hStdError = stderrWrite.get();

        PROCESS_INFORMATION process{};
        fs::path executable(options.command.front());
        executable.make_preferred();
        const LPCWSTR appName = executable.has_parent_path() ? executable.c_str() : nullptr;
        if (!CreateProcessW(appName, commandLine.data(), nullptr, nullptr, TRUE,
                            0, nullptr, nullptr, &startup, &process))
            throw std::runtime_error(std::format("启动进程 {} 失败：{}",
                utf8Path(executable), GetLastError()));

        UniqueHandle childProcess(process.hProcess);
        UniqueHandle childThread(process.hThread);
        nullInput.reset();
        stdoutWrite.reset();
        stderrWrite.reset();

        std::mutex logMutex;
        const HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        const HANDLE stdErr = GetStdHandle(STD_ERROR_HANDLE);
        std::jthread outThread(forwardStream, stdoutRead.get(), stdOut, logHandle.get(), std::ref(logMutex));
        std::jthread errThread(forwardStream, stderrRead.get(), stdErr, logHandle.get(), std::ref(logMutex));
        outThread.join();
        errThread.join();

        WaitForSingleObject(childProcess.get(), INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(childProcess.get(), &exitCode);
        FlushFileBuffers(logHandle.get());
        return (int)(exitCode);
    }
    catch (const std::exception& error) {
        std::cerr << "vcpkg-runner: " << error.what() << '\n';
        return 1;
    }
}
