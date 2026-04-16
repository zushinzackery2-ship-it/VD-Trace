#include "VDTrace/VDTrace.h"

#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
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

    volatile LONG g_start_flag = 0;
    volatile LONG g_ready_flag = 0;
    HANDLE g_async_done_event = nullptr;
    volatile ULONGLONG g_sink = 0;
    DWORD g_spawned_thread_id = 0;

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
        if (value == 0x55AA)
        {
            SwitchToThread();
        }
        g_sink ^= static_cast<ULONGLONG>(value);
    }

    DWORD WINAPI AsyncWorker(LPVOID)
    {
        g_spawned_thread_id = GetCurrentThreadId();
        Sleep(50);
        ExampleLeaf(1);
        ExampleLeaf(2);
        SetEvent(g_async_done_event);
        return 0;
    }

    __declspec(noinline) void AsyncDispatch()
    {
        HANDLE thread = CreateThread(nullptr, 0, AsyncWorker, nullptr, 0, nullptr);
        if (thread != nullptr)
        {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }

    __declspec(noinline) void TraceEntry()
    {
        InterlockedExchange(&g_ready_flag, 1);
        while (InterlockedCompareExchange(&g_start_flag, 0, 0) == 0)
        {
            YieldProcessor();
        }

        AsyncDispatch();
        ExampleLeaf(3);
    }

    DWORD WINAPI WorkerThreadMain(LPVOID)
    {
        TraceEntry();
        return 0;
    }
}

int wmain()
{
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    g_async_done_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Require(g_async_done_event != nullptr, "failed to create async event");

    const std::filesystem::path log_path = GetExecutableDirectory() / L"VDTraceAsyncHandoffSmoke.log";
    std::filesystem::remove(log_path);
    vdtrace::TextFileRecorder recorder(log_path.wstring());
    Require(recorder.IsOpen(), "failed to open async smoke recorder");

    HANDLE worker_thread = CreateThread(nullptr, 0, WorkerThreadMain, nullptr, 0, nullptr);
    Require(worker_thread != nullptr, "failed to create worker thread");
    const DWORD worker_thread_id = GetThreadId(worker_thread);
    while (InterlockedCompareExchange(&g_ready_flag, 0, 0) == 0)
    {
        Sleep(0);
    }

    vdtrace::Session session;
    vdtrace::Options options = {};
    options.thread_id = GetThreadId(worker_thread);
    options.module_names = {L"vdtrace_async_handoff_smoke_test.exe"};
    options.max_events = 0;
    options.trace_outside_modules = false;
    options.control_flow_only = true;
    options.max_call_depth = 2;
    options.hit_policy = vdtrace::FlowHitPolicy::EveryHit;
    options.async_thread_handoff = true;
    options.callback = vdtrace::TextFileRecorder::Callback;
    options.callback_context = &recorder;

    std::wstring error;
    Require(session.Configure(options, error), "async smoke configure failed");
    Require(session.Start(error), "async smoke start failed");

    InterlockedExchange(&g_start_flag, 1);
    Require(WaitForSingleObject(worker_thread, 10000) == WAIT_OBJECT_0, "async smoke worker did not finish");
    Require(WaitForSingleObject(g_async_done_event, 10000) == WAIT_OBJECT_0, "async smoke spawned worker did not finish");

    Require(session.Stop(error), "async smoke stop failed");
    CloseHandle(worker_thread);
    CloseHandle(g_async_done_event);

    const std::wstring state_text = session.DescribeState();
    const DWORD active_thread = static_cast<DWORD>(state_text.find(L"active_thread=") != std::wstring::npos
        ? std::stoul(state_text.substr(state_text.find(L"active_thread=") + 14))
        : 0);
    Require(g_spawned_thread_id != 0, "async smoke spawned thread id was zero");
    Require(active_thread == g_spawned_thread_id, "async smoke did not switch to spawned thread");
    Require(active_thread != worker_thread_id, "async smoke stayed on original worker thread");

    std::printf("[ok] async handoff smoke passed\n");
    std::fflush(stdout);
    return 0;
}
