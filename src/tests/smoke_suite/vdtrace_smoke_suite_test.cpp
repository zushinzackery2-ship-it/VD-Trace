#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    struct SmokeCommand
    {
        std::wstring label;
        std::wstring command_line;
    };

    [[noreturn]] void Fail(const std::wstring &message)
    {
        std::wcerr << L"[fail] " << message << L"\n";
        std::exit(1);
    }

    void Require(bool condition, const std::wstring &message)
    {
        if (!condition)
        {
            Fail(message);
        }
    }

    std::filesystem::path GetExecutableDirectory()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
    }

    void RunSmoke(const std::filesystem::path &working_directory, const SmokeCommand &command)
    {
        std::wcout << L"[run] " << command.label << L"\n";
        std::wstring command_line = command.command_line;
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        const BOOL created = CreateProcessW(
            nullptr,
            command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            working_directory.wstring().c_str(),
            &startup,
            &process);
        Require(created != FALSE, L"启动 smoke 失败: " + command.label);

        const DWORD wait_result = WaitForSingleObject(process.hProcess, 300000);
        if (wait_result != WAIT_OBJECT_0)
        {
            TerminateProcess(process.hProcess, 0xDEADu);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            Fail(L"smoke 超时: " + command.label);
        }

        DWORD exit_code = 0;
        Require(GetExitCodeProcess(process.hProcess, &exit_code) != FALSE, L"读取退出码失败: " + command.label);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::wcout << L"[done] " << command.label << L" exit=" << exit_code << L"\n";
        Require(exit_code == 0, L"smoke 失败: " + command.label + L" exit=" + std::to_wstring(exit_code));
    }
}

int wmain()
{
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    const std::filesystem::path bin_dir = GetExecutableDirectory();
    const std::vector<SmokeCommand> commands = {
        {L"vdtrace_async_handoff_smoke_test", L"\"vdtrace_async_handoff_smoke_test.exe\""},
        {L"vdtrace_agent_smoke_test", L"\"vdtrace_agent_smoke_test.exe\""},
        {L"vdtrace_trigger_wait_test", L"\"vdtrace_trigger_wait_test.exe\""},
        {L"vdtrace_rootstop_test", L"\"vdtrace_rootstop_test.exe\""},
        {L"vdtrace_stop_recovery_test", L"\"vdtrace_stop_recovery_test.exe\""},
        {L"vdtrace_decrypt_smoke_test", L"\"vdtrace_decrypt_smoke_test.exe\""},
    };

    for (const auto &command : commands)
    {
        RunSmoke(bin_dir, command);
    }

    std::wcout << L"[ok] smoke suite passed\n";
    return 0;
}
