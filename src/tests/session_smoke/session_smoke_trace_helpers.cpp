#include "session_smoke_trace_internal.h"

#include <fstream>

namespace session_smoke
{
    namespace detail
    {
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

        std::wstring GetFilenameOnly(const std::wstring &path)
        {
            return std::filesystem::path(path).filename().wstring();
        }

        std::filesystem::path GetStaticRefsJsonPath(const std::filesystem::path &log_path)
        {
            return log_path.parent_path() / (log_path.stem().wstring() + L".static_refs.json");
        }

        std::string ReadTextFile(const std::filesystem::path &path)
        {
            std::ifstream input(path, std::ios::binary);
            Require(input.is_open(), "failed to open smoke log");
            return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        }

        void WaitForFile(const std::filesystem::path &path)
        {
            const ULONGLONG deadline = GetTickCount64() + 30000;
            while (GetTickCount64() < deadline)
            {
                if (std::filesystem::exists(path))
                {
                    const uintmax_t current_size = std::filesystem::file_size(path);
                    if (current_size != 0)
                    {
                        return;
                    }
                }
                Sleep(50);
            }

            Fail("smoke log was not flushed in time");
        }

        void WaitForCaptureWaitingGrowth(vdtrace::Session &session, uint64_t baseline_count)
        {
            const ULONGLONG deadline = GetTickCount64() + 3000;
            while (GetTickCount64() < deadline)
            {
                if (ParseStateCounter(session.DescribeState(), L"capture_waiting=") > baseline_count)
                {
                    return;
                }
                Sleep(5);
            }

            Fail("delayed auto-capture thread was not armed in time");
        }

        DWORD WINAPI WorkerMain(LPVOID parameter)
        {
            auto *context = static_cast<WorkerContext *>(parameter);
            InterlockedExchange(&context->ready, 1);
            while (InterlockedCompareExchange(&context->start, 0, 0) == 0)
            {
                YieldProcessor();
            }

            context->routine();
            return 0;
        }

        DWORD WINAPI StartSessionMain(LPVOID parameter)
        {
            auto *context = static_cast<StartSessionContext *>(parameter);
            std::wstring error;
            context->success = context->session != nullptr && context->session->Start(error);
            context->error = error;
            InterlockedExchange(&context->done, 1);
            return 0;
        }

        DWORD WINAPI StopSessionMain(LPVOID parameter)
        {
            auto *context = static_cast<StopSessionContext *>(parameter);
            std::wstring error;
            context->success = context->session != nullptr && context->session->Stop(error);
            context->error = error;
            InterlockedExchange(&context->done, 1);
            return 0;
        }

        void ConfigureSession(
            void (*routine)(),
            const TraceRunOptions &options,
            DWORD thread_id,
            vdtrace::TextFileRecorder &recorder,
            vdtrace::Session &session)
        {
            const std::wstring exe_name = GetFilenameOnly(GetExecutablePath());
            const uintptr_t module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            const uintptr_t trigger_rva = reinterpret_cast<uintptr_t>(routine) - module_base;

            vdtrace::Options trace_options = {};
            trace_options.thread_id = thread_id;
            trace_options.auto_select_thread = options.auto_select_thread;
            trace_options.block_main_thread = options.block_main_thread;
            trace_options.queue_trigger_threads = options.queue_trigger_threads;
            trace_options.module_names = {exe_name};
            trace_options.max_events = options.max_events;
            trace_options.trace_outside_modules = options.trace_outside_modules;
            trace_options.control_flow_only = options.control_flow_only;
            trace_options.max_call_depth = options.max_call_depth;
            trace_options.hit_policy = options.hit_policy;
            trace_options.hot_bypass_threshold = options.hot_bypass_threshold;
            if (options.use_trigger)
            {
                trace_options.trigger_module_name = exe_name;
                trace_options.trigger_address = trigger_rva;
            }
            trace_options.probe_spec = options.probe_spec;
            trace_options.depth_filter_spec = options.depth_filter_spec;
            trace_options.stop_on_root_return = options.stop_on_root_return;
            trace_options.async_thread_handoff = options.async_thread_handoff;
            trace_options.callback = vdtrace::TextFileRecorder::Callback;
            trace_options.callback_context = &recorder;

            std::wstring error;
            Require(session.Configure(trace_options, error), "session configure failed");
        }

        void WaitForSessionStop(vdtrace::Session &session, bool &auto_stopped)
        {
            const ULONGLONG deadline = GetTickCount64() + 30000;
            while (session.IsRunning() && GetTickCount64() < deadline)
            {
                Sleep(1);
            }
            auto_stopped = !session.IsRunning();
        }

        void LoadTraceResultFiles(const std::filesystem::path &log_path, TraceCaseResult &result)
        {
            WaitForFile(log_path);
            result.log_text = ReadTextFile(log_path);
            const std::filesystem::path static_refs_json_path = GetStaticRefsJsonPath(log_path);
            if (std::filesystem::exists(static_refs_json_path))
            {
                result.static_refs_json_text = ReadTextFile(static_refs_json_path);
            }
        }
    }

    std::string Lowercase(std::string text)
    {
        for (char &value : text)
        {
            if (value >= 'A' && value <= 'Z')
            {
                value = static_cast<char>(value - 'A' + 'a');
            }
        }
        return text;
    }

    size_t CountLines(const std::string &text)
    {
        size_t count = 0;
        for (char value : text)
        {
            if (value == '\n')
            {
                ++count;
            }
        }
        return count;
    }

    uint64_t ParseStateCounter(const std::wstring &state_text, const std::wstring &key)
    {
        const size_t begin = state_text.find(key);
        if (begin == std::wstring::npos)
        {
            Fail("missing state counter");
        }

        size_t cursor = begin + key.size();
        uint64_t value = 0;
        bool seen_digit = false;
        while (cursor < state_text.size() && state_text[cursor] >= L'0' && state_text[cursor] <= L'9')
        {
            seen_digit = true;
            value = value * 10u + static_cast<uint64_t>(state_text[cursor] - L'0');
            ++cursor;
        }
        Require(seen_digit, "invalid state counter");
        return value;
    }
}
