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
        return (GetExecutableDirectory() / L"VDTraceRootStopTest.log").wstring();
    }

    std::wstring GetFilenameOnly(const std::wstring &path)
    {
        return std::filesystem::path(path).filename().wstring();
    }

    __declspec(noinline) void RootLeafA(uint32_t value)
    {
        InterlockedExchangeAdd64(&g_sink, static_cast<LONG64>(value));
    }

    __declspec(noinline) void RootLeafB(uint32_t value)
    {
        InterlockedXor64(&g_sink, static_cast<LONG64>(value * 0x10001u));
    }

    __declspec(noinline) void RootTriggeredFunction()
    {
        for (uint32_t i = 0; i < 4; i++)
        {
            RootLeafA(i);
            RootLeafB(i);
        }
    }

    __declspec(noinline) void AfterRootShouldNotBeTraced()
    {
        for (uint32_t i = 0; i < 32; i++)
        {
            InterlockedExchangeAdd64(&g_sink, static_cast<LONG64>(i + 0x1000u));
        }
    }

    DWORD WINAPI WorkerThreadMain(LPVOID)
    {
        InterlockedExchange(&g_ready_flag, 1);
        while (InterlockedCompareExchange(&g_start_flag, 0, 0) == 0)
        {
            YieldProcessor();
        }

        RootTriggeredFunction();
        AfterRootShouldNotBeTraced();
        return 0;
    }

    std::wstring ReadAllText(const std::wstring &path)
    {
        std::ifstream input(std::filesystem::path(path), std::ios::binary);
        if (!input)
        {
            return {};
        }

        std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (data.empty())
        {
            return {};
        }

        const int count = MultiByteToWideChar(CP_UTF8, 0, data.c_str(), static_cast<int>(data.size()), nullptr, 0);
        if (count <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, data.c_str(), static_cast<int>(data.size()), result.data(), count);
        return result;
    }
}

int wmain()
{
    const std::wstring exe_path = GetExecutablePath();
    const std::wstring exe_name = GetFilenameOnly(exe_path);
    const uintptr_t module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const uintptr_t trigger_address = reinterpret_cast<uintptr_t>(&RootTriggeredFunction) - module_base;
    const uintptr_t forbidden_rel = reinterpret_cast<uintptr_t>(&AfterRootShouldNotBeTraced) - module_base;

    std::filesystem::remove(BuildLogPath());
    vdtrace::TextFileRecorder recorder(BuildLogPath());
    if (!recorder.IsOpen())
    {
        std::wcerr << L"打开 rootstop 测试日志失败。\n";
        return 1;
    }

    HANDLE worker_thread = CreateThread(nullptr, 0, WorkerThreadMain, nullptr, 0, nullptr);
    if (worker_thread == nullptr)
    {
        std::wcerr << L"创建 rootstop 测试线程失败。\n";
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
    options.max_events = 256;
    options.trace_outside_modules = false;
    options.control_flow_only = true;
    options.max_call_depth = 2;
    options.hit_policy = vdtrace::FlowHitPolicy::FirstSeen;
    options.trigger_module_name = exe_name;
    options.trigger_address = trigger_address;
    options.stop_on_root_return = true;
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

    const ULONGLONG deadline = GetTickCount64() + 3000;
    while (session.IsRunning() && GetTickCount64() < deadline)
    {
        Sleep(1);
    }

    WaitForSingleObject(worker_thread, 3000);
    session.Stop(error);
    CloseHandle(worker_thread);

    if (session.IsRunning())
    {
        std::wcerr << L"rootstop 没有自动停。\n";
        return 1;
    }

    const std::wstring text = ReadAllText(BuildLogPath());
    if (text.empty())
    {
        std::wcerr << L"rootstop 产生了空日志。\n";
        return 1;
    }

    wchar_t forbidden_pattern[64] = {};
    swprintf_s(forbidden_pattern, L"rel=0x%llx", static_cast<unsigned long long>(forbidden_rel));
    if (text.find(forbidden_pattern) != std::wstring::npos)
    {
        std::wcerr << L"rootstop 之后仍然 trace 到了根函数返回后的代码。\n";
        return 1;
    }

    std::wcout
        << L"Root-stop test passed, events=" << session.EventCount()
        << L" trigger=0x" << std::hex << trigger_address
        << L" forbidden=0x" << std::hex << forbidden_rel << L"\n";
    return 0;
}
