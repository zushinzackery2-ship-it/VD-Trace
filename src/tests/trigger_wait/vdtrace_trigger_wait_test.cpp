#include "VDTrace/VDTrace.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    using GetTriggerWaitHelperTriggerAddressFn = uintptr_t(__stdcall *)(void);
    using TriggerWaitHelperWorkFn = uint32_t(__stdcall *)(uint32_t rounds);

    struct WorkerContext
    {
        TriggerWaitHelperWorkFn helper = nullptr;
        uint32_t helper_rounds = 0;
    };

    struct HotLoopRecord
    {
        uint32_t pad0;
        uint32_t value;
        uint32_t pad1;
    };

    volatile LONG g_ready_flag = 0;
    volatile LONG g_start_flag = 0;
    volatile LONG64 g_test_sink = 0;
    HotLoopRecord g_hot_loop_records[50000] = {};

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
        return (GetExecutableDirectory() / L"VDTraceTriggerWaitTest.log").wstring();
    }

    std::wstring BuildHelperPath()
    {
        return (GetExecutableDirectory() / L"VDTraceTriggerWaitHelper.dll").wstring();
    }

    std::wstring GetFilenameOnly(const std::wstring &path)
    {
        return std::filesystem::path(path).filename().wstring();
    }

    __declspec(noinline) void TargetLeafA(uint32_t value)
    {
        if (value == 0xFFFFFFFFu)
        {
            SwitchToThread();
        }

        InterlockedExchangeAdd64(&g_test_sink, static_cast<LONG64>(value));
    }

    __declspec(noinline) void TargetLeafB(uint32_t value)
    {
        InterlockedXor64(&g_test_sink, static_cast<LONG64>(value * 0x10001u));
    }

    __declspec(noinline) void TargetModuleWorkload()
    {
        for (uint32_t index = 0; index < 8; index++)
        {
            TargetLeafA(index);
            TargetLeafB(index);
        }
    }

    __declspec(noinline) void TargetHotLoop()
    {
        uint64_t sum = 0;
        size_t count = _countof(g_hot_loop_records);
        if (count != 0)
        {
            const uint32_t *cursor = &g_hot_loop_records[0].value;
            do
            {
                sum += *cursor;
                cursor += 3;
                --count;
            } while (count != 0);
        }

        InterlockedExchangeAdd64(&g_test_sink, static_cast<LONG64>(sum));
    }

    DWORD WINAPI WorkerThreadMain(LPVOID parameter)
    {
        const auto *context = static_cast<const WorkerContext *>(parameter);
        InterlockedExchange(&g_ready_flag, 1);
        while (InterlockedCompareExchange(&g_start_flag, 0, 0) == 0)
        {
            YieldProcessor();
        }

        const uint32_t helper_value = context->helper(context->helper_rounds);
        InterlockedExchangeAdd64(&g_test_sink, static_cast<LONG64>(helper_value));
        TargetModuleWorkload();
        TargetHotLoop();
        return 0;
    }

    std::wstring ReadFirstMatchingEventLine(const std::wstring &path, const std::wstring &pattern)
    {
        std::ifstream input(std::filesystem::path(path), std::ios::binary);
        if (!input)
        {
            return {};
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.rfind("[tid=", 0) == 0)
            {
                const int count = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
                if (count <= 1)
                {
                    return {};
                }

                std::wstring result(static_cast<size_t>(count), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, result.data(), count);
                if (!result.empty() && result.back() == L'\0')
                {
                    result.pop_back();
                }
                if (pattern.empty() || result.find(pattern) != std::wstring::npos)
                {
                    return result;
                }
            }
        }

        return {};
    }

    uint64_t ParseStateCounter(const std::wstring &state_text, const std::wstring &key)
    {
        const size_t begin = state_text.find(key);
        if (begin == std::wstring::npos)
        {
            return 0;
        }

        size_t cursor = begin + key.size();
        uint64_t value = 0;
        while (cursor < state_text.size() && state_text[cursor] >= L'0' && state_text[cursor] <= L'9')
        {
            value = value * 10u + static_cast<uint64_t>(state_text[cursor] - L'0');
            ++cursor;
        }
        return value;
    }
}

int wmain()
{
    for (size_t index = 0; index < _countof(g_hot_loop_records); ++index)
    {
        g_hot_loop_records[index].pad0 = static_cast<uint32_t>(index ^ 0x1111u);
        g_hot_loop_records[index].value = static_cast<uint32_t>((index * 7u) ^ 0x55AA4321u);
        g_hot_loop_records[index].pad1 = static_cast<uint32_t>(index + 13u);
    }

    const std::wstring helper_path = BuildHelperPath();
    const HMODULE helper_module = LoadLibraryW(helper_path.c_str());
    if (helper_module == nullptr)
    {
        std::wcerr << L"加载测试 helper 失败: " << helper_path << L"\n";
        return 1;
    }

    const auto helper = reinterpret_cast<TriggerWaitHelperWorkFn>(GetProcAddress(helper_module, "TriggerWaitHelperWork"));
    const auto get_trigger_address = reinterpret_cast<GetTriggerWaitHelperTriggerAddressFn>(GetProcAddress(helper_module, "GetTriggerWaitHelperTriggerAddress"));
    if (helper == nullptr || get_trigger_address == nullptr)
    {
        std::wcerr << L"解析 helper 导出失败。\n";
        FreeLibrary(helper_module);
        return 1;
    }

    const uintptr_t trigger_address = get_trigger_address();
    if (trigger_address == 0)
    {
        std::wcerr << L"helper 没有返回有效触发点地址。\n";
        FreeLibrary(helper_module);
        return 1;
    }

    std::filesystem::remove(BuildLogPath());

    WorkerContext worker_context = {};
    worker_context.helper = helper;
    worker_context.helper_rounds = 0x400u;

    HANDLE worker_thread = CreateThread(nullptr, 0, WorkerThreadMain, &worker_context, 0, nullptr);
    if (worker_thread == nullptr)
    {
        std::wcerr << L"创建工作线程失败。\n";
        FreeLibrary(helper_module);
        return 1;
    }

    while (InterlockedCompareExchange(&g_ready_flag, 0, 0) == 0)
    {
        Sleep(0);
    }

    std::wstring error;
    std::wstring final_state;
    uint64_t event_count = 0;
    DWORD wait_result = WAIT_FAILED;
    ULONGLONG elapsed_ms = 0;
    {
        vdtrace::TextFileRecorder recorder(BuildLogPath());
        if (!recorder.IsOpen())
        {
            std::wcerr << L"打开测试日志失败。\n";
            CloseHandle(worker_thread);
            FreeLibrary(helper_module);
            return 1;
        }

        vdtrace::Session session;
        vdtrace::Options options = {};
        options.thread_id = GetThreadId(worker_thread);
        options.module_names = {GetFilenameOnly(GetExecutablePath())};
        options.max_events = 256;
        options.trace_outside_modules = false;
        options.control_flow_only = true;
        options.max_call_depth = 2;
        options.hit_policy = vdtrace::FlowHitPolicy::FirstSeen;
        options.trigger_module_name = GetFilenameOnly(helper_path);
        options.trigger_address = trigger_address - reinterpret_cast<uintptr_t>(helper_module);
        options.callback = vdtrace::TextFileRecorder::Callback;
        options.callback_context = &recorder;

        if (!session.Configure(options, error))
        {
            std::wcerr << L"Configure 失败: " << error << L"\n";
            CloseHandle(worker_thread);
            FreeLibrary(helper_module);
            return 1;
        }

        if (!session.Start(error))
        {
            std::wcerr << L"Start 失败: " << error << L"\n";
            CloseHandle(worker_thread);
            FreeLibrary(helper_module);
            return 1;
        }

        const ULONGLONG begin = GetTickCount64();
        InterlockedExchange(&g_start_flag, 1);
        wait_result = WaitForSingleObject(worker_thread, 3000);
        elapsed_ms = GetTickCount64() - begin;
        session.Stop(error);
        final_state = session.DescribeState();
        event_count = session.EventCount();
    }
    CloseHandle(worker_thread);
    FreeLibrary(helper_module);

    if (wait_result != WAIT_OBJECT_0)
    {
        std::wcerr << L"测试线程未在 3 秒内结束，疑似仍被模块外触发拖住。\n";
        return 1;
    }

    const std::wstring exe_name = GetFilenameOnly(GetExecutablePath());
    const std::wstring first_inside_event = ReadFirstMatchingEventLine(BuildLogPath(), exe_name);
    if (first_inside_event.empty())
    {
        std::wcerr << L"测试日志里没有拿到目标模块内的正式 Trace 事件。\n";
        return 1;
    }

    const std::wstring helper_name = GetFilenameOnly(helper_path);
    const std::wstring first_outside_event = ReadFirstMatchingEventLine(BuildLogPath(), helper_name);

    const uint64_t step_count = ParseStateCounter(final_state, L"steps=");
    if (step_count >= 25000)
    {
        std::wcerr << L"trigger-wait 热循环旁路没有压住，步数过高: " << step_count << L"\n";
        std::wcerr << final_state << L"\n";
        return 1;
    }

    std::wcout
        << L"Trigger-wait test passed, elapsed_ms=" << elapsed_ms
        << L" events=" << event_count
        << L" first_inside_event=" << first_inside_event;
    if (!first_outside_event.empty())
    {
        std::wcout << L" first_outside_event=" << first_outside_event;
    }
    std::wcout
        << L" state=" << final_state << L"\n";
    return 0;
}
