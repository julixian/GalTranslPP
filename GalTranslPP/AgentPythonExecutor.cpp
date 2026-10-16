module;

#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
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
        throw std::runtime_error("Python environment is not initialized.");
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
#else
    throw std::runtime_error("Agent Python execution is currently implemented for Windows only.");
#endif
}

NAMESPACE_END(gpp)
