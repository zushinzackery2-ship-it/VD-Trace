#include "session_smoke_trace_internal.h"

namespace session_smoke
{
    TraceCaseResult RunUnityWorkerTraceCase(const std::wstring &log_name, const TraceRunOptions &options, size_t worker_index)
    {
        const std::filesystem::path log_path = detail::GetExecutableDirectory() / log_name;
        std::filesystem::remove(log_path);
        TraceCaseResult result = {};
        std::wstring error;
        {
            vdtrace::TextFileRecorder recorder(log_path.wstring());
            Require(recorder.IsOpen(), "failed to open unity worker capture log");
            vdtrace::Session session;
            detail::ConfigureSession(options.routine, options, 0, recorder, session);
            Require(session.Start(error), "unity worker capture start failed");
            DispatchUnityWorkerTask(worker_index, options.routine);
            WaitForUnityWorkerTask(worker_index);

            result.worker_thread_id = UnityWorkerThreadId(worker_index);
            detail::WaitForSessionStop(session, result.auto_stopped);
            Require(session.Stop(error), "unity worker capture stop failed");
            result.state_text = session.DescribeState();
        }

        detail::LoadTraceResultFiles(log_path, result);
        return result;
    }

    TraceCaseResult RunUnityWorkerCaptureCase(const std::wstring &log_name)
    {
        TraceRunOptions options = {};
        options.routine = &UnityWorkerAssetEntry;
        options.max_call_depth = 4;
        options.hit_policy = vdtrace::FlowHitPolicy::FirstSeen;
        options.auto_select_thread = true;
        options.stop_on_root_return = true;
        return RunUnityWorkerTraceCase(log_name, options);
    }

    TraceCaseResult RunUnityWorkerQueueRotationCase(const std::wstring &log_name, const TraceRunOptions &options)
    {
        const std::filesystem::path log_path = detail::GetExecutableDirectory() / log_name;
        std::filesystem::remove(log_path);
        TraceCaseResult result = {};
        std::wstring error;
        {
            vdtrace::TextFileRecorder recorder(log_path.wstring());
            Require(recorder.IsOpen(), "failed to open unity worker queue log");
            vdtrace::Session session;
            detail::ConfigureSession(options.routine, options, 0, recorder, session);
            Require(session.Start(error), "unity worker queue start failed");

            DispatchUnityWorkerTask(0, options.routine);
            WaitForUnityWorkerTask(0);
            Sleep(40);
            DispatchUnityWorkerTask(1, options.routine);
            WaitForUnityWorkerTask(1);

            result.worker_thread_id = UnityWorkerThreadId(0);
            result.second_worker_thread_id = UnityWorkerThreadId(1);
            result.auto_stopped = !session.IsRunning();
            Require(session.Stop(error), "unity worker queue stop failed");
            result.state_text = session.DescribeState();
        }

        detail::LoadTraceResultFiles(log_path, result);
        return result;
    }

    TraceCaseResult RunMainThreadCompetitionCase(const std::wstring &log_name, const TraceRunOptions &options)
    {
        const std::filesystem::path log_path = detail::GetExecutableDirectory() / log_name;
        std::filesystem::remove(log_path);
        TraceCaseResult result = {};
        result.main_thread_id = GetCurrentThreadId();
        std::wstring error;
        {
            vdtrace::TextFileRecorder recorder(log_path.wstring());
            Require(recorder.IsOpen(), "failed to open main-thread competition log");
            vdtrace::Session session;
            detail::ConfigureSession(options.routine, options, 0, recorder, session);

            detail::StartSessionContext start_context = {};
            start_context.session = &session;
            HANDLE starter = CreateThread(nullptr, 0, detail::StartSessionMain, &start_context, 0, nullptr);
            Require(starter != nullptr, "failed to create smoke starter thread");
            Require(WaitForSingleObject(starter, 10000) == WAIT_OBJECT_0, "starter thread did not finish");
            Require(start_context.done != 0 && start_context.success, "competition session start failed");
            CloseHandle(starter);

            const uint64_t baseline_capture_waiting = options.auto_select_thread
                ? ParseStateCounter(session.DescribeState(), L"capture_waiting=")
                : 0;
            options.routine();
            Sleep(40);

            detail::WorkerContext context = {};
            context.routine = options.routine;
            HANDLE worker = CreateThread(nullptr, 0, detail::WorkerMain, &context, 0, nullptr);
            Require(worker != nullptr, "failed to create competition worker");
            while (InterlockedCompareExchange(&context.ready, 0, 0) == 0)
            {
                Sleep(0);
            }

            if (options.auto_select_thread && options.block_main_thread)
            {
                detail::WaitForCaptureWaitingGrowth(session, baseline_capture_waiting);
            }

            InterlockedExchange(&context.start, 1);
            Require(WaitForSingleObject(worker, 10000) == WAIT_OBJECT_0, "competition worker did not finish");
            result.worker_thread_id = GetThreadId(worker);
            CloseHandle(worker);

            detail::WaitForSessionStop(session, result.auto_stopped);
            detail::StopSessionContext stop_context = {};
            stop_context.session = &session;
            HANDLE stopper = CreateThread(nullptr, 0, detail::StopSessionMain, &stop_context, 0, nullptr);
            Require(stopper != nullptr, "failed to create competition stopper");
            Require(WaitForSingleObject(stopper, 10000) == WAIT_OBJECT_0, "competition stopper did not finish");
            Require(stop_context.done != 0 && stop_context.success, "competition session stop failed");
            CloseHandle(stopper);
            result.state_text = session.DescribeState();
        }

        detail::LoadTraceResultFiles(log_path, result);
        return result;
    }
}
