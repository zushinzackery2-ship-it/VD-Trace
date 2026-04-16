#include "VDTrace/VDTrace.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    volatile LONG g_ready_flag = 0;
    volatile LONG g_start_flag = 0;
    volatile LONG g_keep_running = 1;
    volatile LONG64 g_sink = 0;

    std::wstring GetExecutablePath()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        buffer.resize(length);
        return buffer;
    }

    std::filesystem::path GetExecutableDirectory()
    {
        return std::filesystem::path(GetExecutablePath()).parent_path();
    }

    std::wstring BuildLogPath()
    {
        return (GetExecutableDirectory() / L"VDTraceStopRecoveryTest.log").wstring();
    }

    std::wstring GetFilenameOnly(const std::wstring &path)
    {
        return std::filesystem::path(path).filename().wstring();
    }

    __declspec(noinline) void StopLeafA(uint32_t value)
    {
        InterlockedExchangeAdd64(&g_sink, static_cast<LONG64>(value + 1));
    }

    __declspec(noinline) void StopLeafB(uint32_t value)
    {
        InterlockedXor64(&g_sink, static_cast<LONG64>(value * 0x10001u));
    }

    __declspec(noinline) void StopHotLoop()
    {
        uint32_t counter = 0;
        while (InterlockedCompareExchange(&g_keep_running, 0, 0) != 0)
        {
            StopLeafA(counter);
            StopLeafB(counter);
            counter++;
            if ((counter & 0x3FFu) == 0)
            {
                YieldProcessor();
            }
        }
    }

    DWORD WINAPI WorkerThreadMain(LPVOID)
    {
        InterlockedExchange(&g_ready_flag, 1);
        while (InterlockedCompareExchange(&g_start_flag, 0, 0) == 0)
        {
            YieldProcessor();
        }

        StopHotLoop();
        return 0;
    }

    bool LogHasEvents(const std::wstring &path)
    {
        std::ifstream input(std::filesystem::path(path), std::ios::binary);
        if (!input)
        {
            return false;
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.rfind("[tid=", 0) == 0)
            {
                return true;
            }
        }

        return false;
    }

    bool WaitForLoggedEvents(const std::wstring &path, DWORD timeout_ms)
    {
        const ULONGLONG deadline = GetTickCount64() + timeout_ms;
        while (GetTickCount64() <= deadline)
        {
            if (LogHasEvents(path))
            {
                return true;
            }

            Sleep(10);
        }

        return LogHasEvents(path);
    }
}

int wmain()
{
    const std::wstring exe_name = GetFilenameOnly(GetExecutablePath());
    std::filesystem::remove(BuildLogPath());
    vdtrace::TextFileRecorder recorder(BuildLogPath());
    if (!recorder.IsOpen())
    {
        std::wcerr << L"打开 stop recovery 测试日志失败。\n";
        return 1;
    }

    HANDLE worker_thread = CreateThread(nullptr, 0, WorkerThreadMain, nullptr, 0, nullptr);
    if (worker_thread == nullptr)
    {
        std::wcerr << L"创建 stop recovery 测试线程失败。\n";
        return 1;
    }

    while (InterlockedCompareExchange(&g_ready_flag, 0, 0) == 0)
    {
        Sleep(0);
    }

    vdtrace::Session session;
    vdtrace::Options options = {};
    options.thread_id = GetThreadId(worker_thread);
    options.module_names = {exe_name};
    options.max_events = 0;
    options.trace_outside_modules = false;
    options.control_flow_only = true;
    options.max_call_depth = 2;
    options.hit_policy = vdtrace::FlowHitPolicy::EveryHit;
    options.callback = vdtrace::TextFileRecorder::Callback;
    options.callback_context = &recorder;

    std::wstring error;
    if (!session.Configure(options, error))
    {
        std::wcerr << L"Configure 失败: " << error << L"\n";
        CloseHandle(worker_thread);
        return 1;
    }

    if (!session.Start(error))
    {
        std::wcerr << L"Start 失败: " << error << L"\n";
        CloseHandle(worker_thread);
        return 1;
    }

    InterlockedExchange(&g_start_flag, 1);
    Sleep(50);

    if (!session.Stop(error))
    {
        std::wcerr << L"Stop 失败: " << error << L"\n";
        InterlockedExchange(&g_keep_running, 0);
        CloseHandle(worker_thread);
        return 1;
    }

    InterlockedExchange(&g_keep_running, 0);
    const DWORD wait_result = WaitForSingleObject(worker_thread, 3000);
    CloseHandle(worker_thread);

    if (wait_result != WAIT_OBJECT_0)
    {
        std::wcerr << L"Stop 之后线程没有恢复并退出。\n";
        return 1;
    }

    if (!WaitForLoggedEvents(BuildLogPath(), 2000))
    {
        std::wcerr << L"Stop recovery 测试没有生成任何事件。\n";
        return 1;
    }

    std::wcout << L"Stop-recovery test passed, events=" << session.EventCount() << L"\n";
    return 0;
}
