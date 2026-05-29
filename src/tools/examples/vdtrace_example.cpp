#include "VDTrace/VDTrace.h"

#include <Windows.h>

#include <fstream>
#include <iostream>
#include <string>

namespace
{
    std::wstring ReadEnvironmentPath(const wchar_t *name)
    {
        if (name == nullptr || name[0] == L'\0')
        {
            return {};
        }

        wchar_t buffer[1024] = {};
        const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length == 0 || length >= std::size(buffer))
        {
            return {};
        }

        return std::wstring(buffer, buffer + length);
    }

    volatile LONG g_start_flag = 0;
    volatile LONG g_ready_flag = 0;
    volatile ULONGLONG g_external_sink = 0;

    std::wstring BuildLogPath()
    {
        const std::wstring override_path = ReadEnvironmentPath(L"VDTRACE_EXAMPLE_LOG");
        if (!override_path.empty())
        {
            return override_path;
        }

        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return L".\\VDTraceExample.log";
        }

        buffer.resize(length);
        const size_t slash = buffer.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L".\\VDTraceExample.log";
        }

        return buffer.substr(0, slash) + L"\\VDTraceExample.log";
    }

    std::wstring BuildStatePath()
    {
        const std::wstring override_path = ReadEnvironmentPath(L"VDTRACE_EXAMPLE_STATE");
        if (!override_path.empty())
        {
            return override_path;
        }

        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return L".\\VDTraceExample.state";
        }

        buffer.resize(length);
        const size_t slash = buffer.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L".\\VDTraceExample.state";
        }

        return buffer.substr(0, slash) + L"\\VDTraceExample.state";
    }

    std::string Narrow(const std::wstring &text)
    {
        if (text.empty())
        {
            return {};
        }

        const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
        {
            return {};
        }

        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }
        return result;
    }

    __declspec(noinline) void ExampleLeaf(int value)
    {
        if (value == 123456)
        {
            ::Sleep(1);
        }
    }

    __declspec(noinline) void ExampleExternalLeaf()
    {
        g_external_sink ^= ::GetTickCount64();
    }

    DWORD WINAPI ExampleAsyncWorker(LPVOID parameter)
    {
        const auto value = static_cast<ULONG_PTR>(reinterpret_cast<uintptr_t>(parameter));
        ::Sleep(50);
        ExampleLeaf(static_cast<int>(value & 0x3u));
        ExampleExternalLeaf();
        g_external_sink ^= value;
        return static_cast<DWORD>(value & 0xFFFFFFFFu);
    }

    __declspec(noinline) void ExampleAsyncDispatch()
    {
        HANDLE thread = CreateThread(nullptr, 0, ExampleAsyncWorker, reinterpret_cast<LPVOID>(0x1234u), 0, nullptr);
        if (thread != nullptr)
        {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }

    __declspec(noinline) void ExampleWorkload()
    {
        for (int i = 0; i < 4; i++)
        {
            ExampleLeaf(i);
            ExampleExternalLeaf();
        }
        ExampleAsyncDispatch();
    }

    __declspec(noinline) void ExampleTraceEntry()
    {
        InterlockedExchange(&g_ready_flag, 1);
        while (InterlockedCompareExchange(&g_start_flag, 0, 0) == 0)
        {
            YieldProcessor();
        }
        ExampleWorkload();
    }

    DWORD WINAPI WorkerThreadMain(LPVOID)
    {
        ExampleTraceEntry();
        return 0;
    }
}

int wmain(int argc, wchar_t **argv)
{
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    const bool enable_async_handoff = argc > 1 && argv != nullptr && std::wstring(argv[1]) == L"--async-handoff";

    vdtrace::TextFileRecorder recorder(BuildLogPath());
    if (!recorder.IsOpen())
    {
        std::wcerr << L"打开示例日志失败。\n";
        return 1;
    }

    HANDLE worker_thread = CreateThread(nullptr, 0, WorkerThreadMain, nullptr, 0, nullptr);
    if (worker_thread == nullptr)
    {
        std::wcerr << L"创建工作线程失败。\n";
        return 1;
    }

    while (InterlockedCompareExchange(&g_ready_flag, 0, 0) == 0)
    {
        Sleep(0);
    }

    vdtrace::Session session;
    vdtrace::Options options = {};
    options.thread_id = GetThreadId(worker_thread);
    options.module_names = {L"vdtrace_example.exe"};
    options.max_events = 0;
    options.trace_outside_modules = false;
    options.control_flow_only = true;
    options.max_call_depth = 2;
    options.hit_policy = vdtrace::FlowHitPolicy::EveryHit;
    options.async_thread_handoff = enable_async_handoff;
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
    WaitForSingleObject(worker_thread, INFINITE);

    session.Stop(error);
    CloseHandle(worker_thread);
    const std::wstring state_text = session.DescribeState();
    {
        std::ofstream state_file(BuildStatePath(), std::ios::binary | std::ios::trunc);
        if (state_file.is_open())
        {
            state_file << "done-ascii events=" << session.EventCount() << " state=" << Narrow(state_text) << "\n";
        }
    }
    std::wcout << L"Done, events=" << session.EventCount() << L" state=" << state_text << L"\n";
    std::cout << "done-ascii events=" << session.EventCount() << " state=" << Narrow(state_text) << "\n";
    return 0;
}
