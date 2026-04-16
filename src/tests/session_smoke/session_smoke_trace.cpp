#include "session_smoke_trace_internal.h"

namespace session_smoke
{
    TraceCaseResult RunTraceCase(const std::wstring &log_name, const TraceRunOptions &options)
    {
        detail::WorkerContext context = {};
        context.routine = options.routine;
        HANDLE worker = CreateThread(nullptr, 0, detail::WorkerMain, &context, 0, nullptr);
        Require(worker != nullptr, "failed to create smoke worker");
        while (InterlockedCompareExchange(&context.ready, 0, 0) == 0)
        {
            Sleep(0);
        }

        const std::filesystem::path log_path = detail::GetExecutableDirectory() / log_name;
        std::filesystem::remove(log_path);
        TraceCaseResult result = {};
        std::wstring error;
        {
            vdtrace::TextFileRecorder recorder(log_path.wstring());
            Require(recorder.IsOpen(), "failed to open smoke recorder");
            vdtrace::Session session;
            detail::ConfigureSession(options.routine, options, options.auto_select_thread ? 0u : GetThreadId(worker), recorder, session);
            Require(session.Start(error), "session start failed");
            InterlockedExchange(&context.start, 1);
            Require(WaitForSingleObject(worker, 10000) == WAIT_OBJECT_0, "worker did not finish");
            result.worker_thread_id = GetThreadId(worker);
            if (options.async_thread_handoff)
            {
                WaitForAsyncWorkerDone();
            }

            detail::WaitForSessionStop(session, result.auto_stopped);
            Require(session.Stop(error), "session stop failed");
            result.pending_write_bytes = recorder.PendingWriteBytes();
            result.pending_write_events = recorder.PendingWriteEventCount();
            result.written_events = recorder.WrittenEventCount();
            result.dropped_events_total = recorder.DroppedEventCount();
            result.dropped_write_events = recorder.DroppedWriteEventCount();
            result.state_text = session.DescribeState();
        }

        CloseHandle(worker);
        detail::LoadTraceResultFiles(log_path, result);
        return result;
    }

    TraceCaseResult RunDelayedTraceCase(const std::wstring &log_name, const TraceRunOptions &options, DWORD start_delay_ms)
    {
        const std::filesystem::path log_path = detail::GetExecutableDirectory() / log_name;
        std::filesystem::remove(log_path);
        TraceCaseResult result = {};
        std::wstring error;
        {
            vdtrace::TextFileRecorder recorder(log_path.wstring());
            Require(recorder.IsOpen(), "failed to open delayed auto-capture log");
            vdtrace::Session session;
            detail::ConfigureSession(options.routine, options, 0, recorder, session);
            Require(session.Start(error), "delayed auto-capture start failed");
            const uint64_t baseline_capture_waiting = ParseStateCounter(session.DescribeState(), L"capture_waiting=");

            detail::WorkerContext context = {};
            context.routine = options.routine;
            HANDLE worker = CreateThread(nullptr, 0, detail::WorkerMain, &context, 0, nullptr);
            Require(worker != nullptr, "failed to create delayed auto-capture worker");
            while (InterlockedCompareExchange(&context.ready, 0, 0) == 0)
            {
                Sleep(0);
            }

            detail::WaitForCaptureWaitingGrowth(session, baseline_capture_waiting);
            Sleep(start_delay_ms);
            InterlockedExchange(&context.start, 1);
            Require(WaitForSingleObject(worker, 10000) == WAIT_OBJECT_0, "delayed auto-capture worker did not finish");

            result.worker_thread_id = GetThreadId(worker);
            CloseHandle(worker);
            detail::WaitForSessionStop(session, result.auto_stopped);
            Require(session.Stop(error), "delayed auto-capture stop failed");
            result.pending_write_bytes = recorder.PendingWriteBytes();
            result.pending_write_events = recorder.PendingWriteEventCount();
            result.written_events = recorder.WrittenEventCount();
            result.dropped_events_total = recorder.DroppedEventCount();
            result.dropped_write_events = recorder.DroppedWriteEventCount();
            result.state_text = session.DescribeState();
        }

        detail::LoadTraceResultFiles(log_path, result);
        return result;
    }

    TraceCaseResult RunDelayedAutoThreadCaptureCase(const std::wstring &log_name, void (*routine)())
    {
        TraceRunOptions options = {};
        options.routine = routine;
        options.max_call_depth = vdtrace::kUnlimitedCallDepth;
        options.hit_policy = vdtrace::FlowHitPolicy::EveryHit;
        options.auto_select_thread = true;
        return RunDelayedTraceCase(log_name, options);
    }
}
