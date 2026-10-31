module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <crt_externs.h>
#include <stdio.h>
#endif

module AgentPythonExecutor;

import PythonManager;
import Tool;

NAMESPACE_BEGIN(gpp)

#ifdef _WIN32
NAMESPACE_BEGIN(agent_python)

struct Handle {
    HANDLE value = nullptr;

    Handle() = default;

    explicit Handle(HANDLE handle) : value(handle)
    {
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : value(std::exchange(other.value, nullptr))
    {
    }

    ~Handle()
    {
        reset();
    }

    void reset()
    {
        if (value) {
            CloseHandle(std::exchange(value, nullptr));
        }
    }
};

void check(BOOL success, const char* operation)
{
    if (!success) {
        throw std::system_error(GetLastError(), std::system_category(), operation);
    }
}

struct AttributeList {
    std::vector<std::byte> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST value;

    explicit AttributeList(const std::array<HANDLE, 3>& handles)
    {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        storage.resize(bytes);
        value = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        check(InitializeProcThreadAttributeList(value, 1, 0, &bytes), "InitializeProcThreadAttributeList");
        if (!UpdateProcThreadAttribute(
            value, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            const_cast<HANDLE*>(handles.data()), sizeof(handles), nullptr, nullptr))
        {
            const auto error = GetLastError();
            DeleteProcThreadAttributeList(value);
            throw std::system_error(error, std::system_category(), "UpdateProcThreadAttribute");
        }
    }

    ~AttributeList()
    {
        DeleteProcThreadAttributeList(value);
    }
};

struct Process {
    Handle handle;

    // 异常离开执行器时也先结束子进程，解除 stdin 写线程的阻塞。
    ~Process()
    {
        if (handle.value && WaitForSingleObject(handle.value, 0) == WAIT_TIMEOUT) {
            TerminateProcess(handle.value, 1);
            WaitForSingleObject(handle.value, INFINITE);
        }
    }
};

struct Output {
    std::string text;
    bool truncated = false;

    void read(HANDLE pipe)
    {
        constexpr size_t limit = 64 * 1024;
        // 每轮最多排空 64 KiB，持续大量输出时仍能检查停止和超时。
        char buffer[16 * 1024];
        for (int i = 0; i < 4; ++i) {
            DWORD available = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    return;
                }
                check(FALSE, "PeekNamedPipe");
            }
            if (!available) {
                return;
            }
            DWORD bytes = 0;
            check(ReadFile(pipe, buffer, std::min<DWORD>(available, sizeof(buffer)), &bytes, nullptr), "ReadFile");
            const size_t keep = std::min<size_t>(bytes, limit - text.size());
            text.append(buffer, keep);
            truncated |= keep < bytes;
        }
    }
};

NAMESPACE_END(agent_python)
#endif

json runAgentPython(
    const std::string& code, const json& paths,
    const std::shared_ptr<IController>& controller, bool fastPause)
{
#ifdef _WIN32
    using namespace agent_python;
    const auto& executable = pythonExecutablePath();
    if (executable.empty()) {
        throw std::runtime_error("Python environment is not initialized");
    }
    if (fastPause && controller->shouldStop()) {
        return {{"cancelled", true}};
    }

    // 用 JSON 字符串转义注入路径和代码，避免 Windows 路径、引号或换行变成启动代码。
    const std::string script = "import json, sys\n"
        "sys.stdout.reconfigure(encoding='utf-8', errors='backslashreplace')\n"
        "sys.stderr.reconfigure(encoding='utf-8', errors='backslashreplace')\n"
        "paths = json.loads(" + json(paths.dump()).dump() + ")\n"
        "exec(compile(" + json(code).dump() + ", '<agent-python>', 'exec'))\n";

    Handle inputRead, inputWrite, outputRead, outputWrite, errorRead, errorWrite;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    check(CreatePipe(&inputRead.value, &inputWrite.value, &security, 0), "CreatePipe stdin");
    check(CreatePipe(&outputRead.value, &outputWrite.value, &security, 0), "CreatePipe stdout");
    check(CreatePipe(&errorRead.value, &errorWrite.value, &security, 0), "CreatePipe stderr");
    check(SetHandleInformation(inputWrite.value, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stdin");
    check(SetHandleInformation(outputRead.value, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stdout");
    check(SetHandleInformation(errorRead.value, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stderr");

    // 多个翻译 worker 同时启动时，仅继承本次的三条标准流，避免互相持有管道使 EOF 无法到达。
    AttributeList attributes({inputRead.value, outputWrite.value, errorWrite.value});
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = inputRead.value;
    startup.StartupInfo.hStdOutput = outputWrite.value;
    startup.StartupInfo.hStdError = errorWrite.value;
    startup.lpAttributeList = attributes.value;
    const auto directory = ascii2Wide(paths.at("project_dir").get<std::string>());
    std::wstring command = L"\"" + executable.wstring() + L"\" -X utf8 -u -";
    PROCESS_INFORMATION info{};
    check(
        CreateProcessW(
            executable.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, directory.c_str(),
            &startup.StartupInfo, &info),
        "CreateProcess Python");
    Process process{Handle(info.hProcess)};
    Handle processThread(info.hThread);
    inputRead.reset();
    outputWrite.reset();
    errorWrite.reset();

    std::atomic<DWORD> writeError = ERROR_SUCCESS;
    // stdin 与输出同时处理，长代码或启动阶段的输出都不会撑满管道而互相等待。
    std::jthread writer([pipe = std::move(inputWrite), &script, &writeError]()
        {
            size_t offset = 0;
            while (offset < script.size()) {
                DWORD bytes = 0;
                if (!WriteFile(
                    pipe.value, script.data() + offset,
                    static_cast<DWORD>(std::min<size_t>(script.size() - offset, 64 * 1024)),
                    &bytes, nullptr))
                {
                    writeError = GetLastError();
                    break;
                }
                offset += bytes;
            }
        });
    Output output, error;
    bool timedOut = false, cancelled = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    try {
        for (;;) {
            output.read(outputRead.value);
            error.read(errorRead.value);
            const DWORD state = WaitForSingleObject(process.handle.value, 20);
            if (state == WAIT_OBJECT_0) {
                break;
            }
            if (state == WAIT_FAILED) {
                check(FALSE, "WaitForSingleObject Python");
            }
            cancelled = fastPause && controller->shouldStop();
            timedOut = std::chrono::steady_clock::now() >= deadline;
            if (cancelled || timedOut) {
                check(TerminateProcess(process.handle.value, 1), "TerminateProcess Python");
                WaitForSingleObject(process.handle.value, INFINITE);
                break;
            }
        }
        output.read(outputRead.value);
        error.read(errorRead.value);
    }
    catch (...) {
        TerminateProcess(process.handle.value, 1);
        WaitForSingleObject(process.handle.value, INFINITE);
        throw;
    }
    writer.join();
    DWORD exitCode = 0;
    check(GetExitCodeProcess(process.handle.value, &exitCode), "GetExitCodeProcess Python");
    json result = {
        {"stdout", output.text},
        {"stderr", error.text},
        {"exit_code", static_cast<int>(exitCode)},
        {"timed_out", timedOut},
        {"cancelled", cancelled},
        {"stdout_truncated", output.truncated},
        {"stderr_truncated", error.truncated}
    };
    if (timedOut) {
        result["error"] = "Python execution exceeded 120 seconds.";
    }
    else if (cancelled) {
        result["error"] = "Python execution was cancelled.";
    }
    else if (writeError != ERROR_SUCCESS) {
        result["error"] = std::system_category().message(writeError.load());
    }
    // 截断可能位于 UTF-8 字符中间；归一化后才能安全回填 API 的 JSON 消息。
    return json::parse(result.dump(-1, ' ', false, json::error_handler_t::replace));
#elif defined(__APPLE__)
    const auto& executable = pythonExecutablePath();
    if (executable.empty()) throw std::runtime_error("Python environment is not initialized.");
    if (fastPause && controller->shouldStop()) return {{"cancelled", true}};

    // stdin 使用匿名临时文件，避免写入长代码时与 stdout/stderr 互相等待。
    const std::string script = "import json, sys\n"
        "sys.stdout.reconfigure(encoding='utf-8', errors='backslashreplace')\n"
        "sys.stderr.reconfigure(encoding='utf-8', errors='backslashreplace')\n"
        "paths = json.loads(" + json(paths.dump()).dump() + ")\n"
        "exec(compile(" + json(code).dump() + ", '<agent-python>', 'exec'))\n";
    struct Process {
        FILE* input = nullptr;
        int output[2] = {-1, -1};
        int error[2] = {-1, -1};
        pid_t pid = -1;
        ~Process() {
            if (pid > 0) {
                kill(-pid, SIGKILL);
                while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
            }
            if (input) fclose(input);
            for (int fd : {output[0], output[1], error[0], error[1]})
                if (fd >= 0) close(fd);
        }
    } process;
    const auto check = [](int error, const char* operation) {
        if (error) throw std::system_error(error, std::generic_category(), operation);
    };
    process.input = tmpfile();
    if (!process.input) check(errno, "tmpfile Python stdin");
    if (fwrite(script.data(), 1, script.size(), process.input) != script.size())
        check(errno ? errno : EIO, "write Python stdin");
    if (fseek(process.input, 0, SEEK_SET)) check(errno, "rewind Python stdin");
    if (pipe(process.output) || pipe(process.error)) check(errno, "pipe Python output");

    struct SpawnOptions {
        posix_spawn_file_actions_t files;
        posix_spawnattr_t attributes;
        SpawnOptions() {
            const int fileError = posix_spawn_file_actions_init(&files);
            if (fileError) throw std::system_error(fileError, std::generic_category());
            const int attributeError = posix_spawnattr_init(&attributes);
            if (attributeError) {
                posix_spawn_file_actions_destroy(&files);
                throw std::system_error(attributeError, std::generic_category());
            }
        }
        ~SpawnOptions() {
            posix_spawn_file_actions_destroy(&files);
            posix_spawnattr_destroy(&attributes);
        }
    } spawn;
    check(posix_spawn_file_actions_adddup2(&spawn.files, fileno(process.input), STDIN_FILENO), "stdin");
    check(posix_spawn_file_actions_adddup2(&spawn.files, process.output[1], STDOUT_FILENO), "stdout");
    check(posix_spawn_file_actions_adddup2(&spawn.files, process.error[1], STDERR_FILENO), "stderr");
    const auto directory = paths.at("project_dir").get<std::string>();
    check(posix_spawn_file_actions_addchdir_np(&spawn.files, directory.c_str()), "Python cwd");
    // 独立进程组用于停止整棵子进程树；未显式映射的描述符不传给 Python。
    check(posix_spawnattr_setflags(&spawn.attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT), "spawn flags");
    check(posix_spawnattr_setpgroup(&spawn.attributes, 0), "spawn process group");
    char* arguments[] = {(char*)executable.c_str(), (char*)"-X", (char*)"utf8", (char*)"-u", (char*)"-", nullptr};
    check(posix_spawn(&process.pid, executable.c_str(), &spawn.files, &spawn.attributes,
        arguments, *_NSGetEnviron()), "posix_spawn Python");
    close(std::exchange(process.output[1], -1));
    close(std::exchange(process.error[1], -1));
    for (int fd : {process.output[0], process.error[0]}) {
        if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) check(errno, "fcntl Python output");
    }
    std::string output, error;
    bool outputTruncated = false, errorTruncated = false, timedOut = false, cancelled = false;
    const auto readOutput = [&](int fd, std::string& text, bool& truncated) {
        char buffer[16 * 1024];
        for (int i = 0; i < 4; ++i) {
            const auto bytes = read(fd, buffer, sizeof(buffer));
            if (bytes == 0 || (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) break;
            if (bytes < 0) {
                if (errno == EINTR) continue;
                check(errno, "read Python output");
            }
            const auto keep = std::min<size_t>((size_t)bytes, 64 * 1024 - text.size());
            text.append(buffer, keep);
            truncated |= keep < (size_t)bytes;
        }
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    int status = 0;
    for (;;) {
        readOutput(process.output[0], output, outputTruncated);
        readOutput(process.error[0], error, errorTruncated);
        const auto state = waitpid(process.pid, &status, WNOHANG);
        if (state == process.pid) break;
        if (state < 0 && errno != EINTR) check(errno, "waitpid Python");
        cancelled = fastPause && controller->shouldStop();
        timedOut = std::chrono::steady_clock::now() >= deadline;
        if (cancelled || timedOut) {
            kill(-process.pid, SIGKILL);
            while (waitpid(process.pid, &status, 0) < 0) {
                if (errno != EINTR) check(errno, "waitpid stopped Python");
            }
            break;
        }
        pollfd streams[] = {{process.output[0], POLLIN, 0}, {process.error[0], POLLIN, 0}};
        poll(streams, 2, 20);
    }
    // 主脚本退出后也结束它遗留的子进程，避免后台进程持有管道和项目文件。
    kill(-process.pid, SIGKILL);
    process.pid = -1;
    readOutput(process.output[0], output, outputTruncated);
    readOutput(process.error[0], error, errorTruncated);
    json result = {
        {"stdout", output}, {"stderr", error},
        {"exit_code", WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status)},
        {"timed_out", timedOut}, {"cancelled", cancelled},
        {"stdout_truncated", outputTruncated}, {"stderr_truncated", errorTruncated}
    };
    if (timedOut) result["error"] = "Python execution exceeded 120 seconds.";
    else if (cancelled) result["error"] = "Python execution was cancelled.";
    return json::parse(result.dump(-1, ' ', false, json::error_handler_t::replace));
#else
    throw std::runtime_error("Agent Python execution is currently implemented for Windows only");
#endif
}

NAMESPACE_END(gpp)
