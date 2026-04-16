#include "tools/VDTraceControlSupport.h"

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using BootstrapFn = BOOL(WINAPI *)();
    alignas(16) volatile unsigned char g_memory_probe[16] = {
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    };

    [[noreturn]] void Fail(const char *message)
    {
        std::printf("[fail] %s\n", message);
        std::fflush(stdout);
        std::exit(1);
    }

    void Require(bool condition, const char *message)
    {
        if (!condition)
        {
            Fail(message);
        }
    }

    std::string NarrowAscii(const std::wstring &text)
    {
        std::string result;
        result.reserve(text.size());
        for (wchar_t value : text)
        {
            result.push_back(value >= 0 && value <= 0x7f ? static_cast<char>(value) : '?');
        }
        return result;
    }

    std::vector<std::wstring> ParseModuleList(const std::wstring &text)
    {
        std::vector<std::wstring> modules;
        std::wstring current;
        std::set<std::wstring> seen;
        for (wchar_t value : text)
        {
            if (value == L'\r')
            {
                continue;
            }

            if (value == L'\n')
            {
                if (!current.empty())
                {
                    std::wstring lowered = current;
                    for (wchar_t &ch : lowered)
                    {
                        ch = static_cast<wchar_t>(towlower(ch));
                    }
                    if (seen.insert(lowered).second)
                    {
                        modules.push_back(current);
                    }
                    current.clear();
                }
                continue;
            }

            current.push_back(value);
        }

        if (!current.empty())
        {
            std::wstring lowered = current;
            for (wchar_t &ch : lowered)
            {
                ch = static_cast<wchar_t>(towlower(ch));
            }
            if (seen.insert(lowered).second)
            {
                modules.push_back(current);
            }
        }

        return modules;
    }

    std::filesystem::path BuildDumpPath(const std::filesystem::path &output_directory, const std::wstring &module_name, const std::wstring &suffix)
    {
        const std::filesystem::path file_name = std::filesystem::path(module_name).filename();
        const std::wstring stem = file_name.stem().wstring();
        const std::wstring extension = file_name.extension().wstring();
        if (!stem.empty())
        {
            return output_directory / (stem + suffix + extension);
        }

        return output_directory / (file_name.wstring() + suffix);
    }

    void RequireSuccess(const vdtrace::tools::CommandResult &result, const char *context)
    {
        if (result.success)
        {
            return;
        }

        const std::string detail = NarrowAscii(result.message);
        if (detail.empty())
        {
            Fail(context);
        }

        std::string text = context;
        text += ": ";
        text += detail;
        Fail(text.c_str());
    }

    std::filesystem::path CurrentExePath()
    {
        wchar_t buffer[MAX_PATH] = {};
        Require(GetModuleFileNameW(nullptr, buffer, MAX_PATH) != 0, "GetModuleFileNameW failed");
        return std::filesystem::path(buffer);
    }

    int HostMain()
    {
        const std::filesystem::path agent_path = CurrentExePath().parent_path() / "VDTraceAgent.dll";
        HMODULE agent = LoadLibraryW(agent_path.c_str());
        Require(agent != nullptr, "failed to load VDTraceAgent.dll");
        const auto bootstrap = reinterpret_cast<BootstrapFn>(GetProcAddress(agent, "vdtrace_agent_bootstrap_ipc"));
        Require(bootstrap != nullptr, "missing vdtrace_agent_bootstrap_ipc export");
        Require(bootstrap() != FALSE, "agent bootstrap failed");
        g_memory_probe[0] = g_memory_probe[0];
        Sleep(INFINITE);
        return 0;
    }

    PROCESS_INFORMATION SpawnHost()
    {
        const std::filesystem::path exe_path = CurrentExePath();
        std::wstring command_line = L"\"" + exe_path.wstring() + L"\" --host";
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        Require(
            CreateProcessW(
                exe_path.c_str(),
                command_line.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                exe_path.parent_path().c_str(),
                &startup,
                &process) != FALSE,
            "failed to spawn host process");
        return process;
    }
}

int wmain(int argc, wchar_t **argv)
{
    if (argc >= 2 && _wcsicmp(argv[1], L"--host") == 0)
    {
        return HostMain();
    }

    const std::filesystem::path exe_directory = CurrentExePath().parent_path();
    const std::filesystem::path dump_directory = exe_directory / "dump_smoke_auto";
    std::error_code ignore_error;
    std::filesystem::remove_all(dump_directory, ignore_error);

    PROCESS_INFORMATION host = SpawnHost();
    const DWORD pid = host.dwProcessId;
    CloseHandle(host.hThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const auto modules = vdtrace::tools::ListModules(pid, true, 3000);
    RequireSuccess(modules, "external module enumeration failed");
    const std::wstring module_text = modules.message;
    const std::string module_text_ascii = NarrowAscii(module_text);
    const std::vector<std::wstring> module_names = ParseModuleList(module_text);
    Require(!module_names.empty(), "module enumeration returned no modules");
    Require(module_text_ascii.find("vdtrace_agent_smoke_test.exe") != std::string::npos, "self module missing from module list");

    const uintptr_t module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const uintptr_t probe_rva = reinterpret_cast<uintptr_t>(g_memory_probe) - module_base;
    wchar_t memory_address_text[128] = {};
    swprintf_s(
        memory_address_text,
        L"%s+0x%Ix",
        std::filesystem::path(CurrentExePath()).filename().c_str(),
        probe_rva);

    const auto memory_read_before = vdtrace::tools::ReadMemory(pid, memory_address_text, 16, 3000);
    RequireSuccess(memory_read_before, "memory read before write failed");
    Require(memory_read_before.message.find(L"10 32 54 76 98 ba dc fe") != std::wstring::npos, "memory read before write missed expected bytes");

    const uint8_t new_memory_bytes[8] = {0x90, 0x91, 0x92, 0x93, 0xA0, 0xA1, 0xA2, 0xA3};
    const auto memory_write = vdtrace::tools::WriteMemory(pid, memory_address_text, new_memory_bytes, static_cast<uint32_t>(sizeof(new_memory_bytes)), 3000);
    RequireSuccess(memory_write, "memory write failed");
    Require(memory_write.message.find(L"90 91 92 93 a0 a1 a2 a3") != std::wstring::npos, "memory write result missed updated bytes");

    const auto memory_read_after = vdtrace::tools::ReadMemory(pid, memory_address_text, 16, 3000);
    RequireSuccess(memory_read_after, "memory read after write failed");
    Require(memory_read_after.message.find(L"90 91 92 93 a0 a1 a2 a3") != std::wstring::npos, "memory read after write missed updated bytes");

    for (const std::wstring &module_name : module_names)
    {
        vdtrace::tools::CommandResult dump = {};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        do
        {
            dump = vdtrace::tools::DumpModule(pid, module_name, dump_directory.wstring(), 10000);
            if (dump.success)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } while (std::chrono::steady_clock::now() < deadline);

        const std::string context = "dump+fix command failed: " + NarrowAscii(module_name);
        RequireSuccess(dump, context.c_str());
        Require(
            std::filesystem::exists(BuildDumpPath(dump_directory, module_name, L"_dump_fix")),
            "fixed dump file missing");
        Require(
            std::filesystem::exists(BuildDumpPath(dump_directory, module_name, L"_dump_raw")),
            "raw dump file missing");
    }

    TerminateProcess(host.hProcess, 0);
    CloseHandle(host.hProcess);
    std::printf("[ok] agent smoke passed modules=%zu\n", module_names.size());
    return 0;
}
