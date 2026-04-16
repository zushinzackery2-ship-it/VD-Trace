#include "pch.h"
#include "VDTraceTriggerCaptureInternal.h"

namespace vdtrace
{
    bool SupportsQueuedTriggerTracing(const Session::Impl &impl)
    {
        return impl.options.queue_trigger_threads
            && impl.options.auto_select_thread
            && impl.configured_trigger_address != 0;
    }

    void ReleaseTriggerCaptureThreads(Session::Impl &impl, bool clear_state, HANDLE preserved_handle)
    {
        std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
        trigger_capture_detail::ReleaseTriggerCaptureThreadsLocked(impl, clear_state, preserved_handle);
    }

    void StartTriggerCaptureRefreshWorker(Session::Impl &impl)
    {
        StopTriggerCaptureRefreshWorker(impl);

        {
            std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
            impl.trigger_capture_worker_stop = false;
        }

        impl.trigger_capture_worker = std::thread(
            [&impl]()
            {
                for (;;)
                {
                    {
                        std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
                        if (impl.trigger_capture_worker_stop)
                        {
                            break;
                        }
                    }

                    if (!trigger_capture_detail::ShouldKeepRefreshWorkerAlive(impl))
                    {
                        break;
                    }

                    uint32_t ignored_candidates = 0;
                    uint32_t ignored_armed = 0;
                    std::wstring ignored_error;
                    trigger_capture_detail::ArmTriggerCaptureThreads(impl, true, ignored_candidates, ignored_armed, ignored_error);
                    Sleep(10);
                }
            });

        const DWORD worker_thread_id = trigger_capture_detail::GetNativeThreadId(impl.trigger_capture_worker);
        if (worker_thread_id != 0)
        {
            std::lock_guard<std::mutex> lock(impl.known_thread_lock);
            impl.known_thread_ids.insert(worker_thread_id);
        }
    }

    void StopTriggerCaptureRefreshWorker(Session::Impl &impl)
    {
        {
            std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
            impl.trigger_capture_worker_stop = true;
        }

        if (impl.trigger_capture_worker.joinable())
        {
            impl.trigger_capture_worker.join();
        }

        {
            std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
            impl.trigger_capture_worker_stop = false;
        }
    }

    bool BeginTriggerThreadCapture(Session::Impl &impl, std::wstring &error)
    {
        error.clear();
        ReleaseTriggerCaptureThreads(impl, true);

        if (impl.configured_trigger_address == 0)
        {
            error = L"线程自动捕获需要定点触发。";
            return false;
        }

        uint32_t candidate_count = 0;
        uint32_t armed_count = 0;
        if (!trigger_capture_detail::ArmTriggerCaptureThreads(impl, false, candidate_count, armed_count, error))
        {
            return false;
        }

        if (candidate_count != 0 && armed_count == 0)
        {
            error = L"现有线程都无法挂上自动捕获断点。";
            ReleaseTriggerCaptureThreads(impl, true);
            return false;
        }

        impl.resolved_trigger_address = impl.configured_trigger_address;
        impl.observed_addresses[0] = impl.configured_trigger_address;
        impl.observed_addresses[1] = 0;
        impl.observed_addresses[2] = 0;
        impl.observed_addresses[3] = 0;
        impl.observed_count = 1;
        impl.waiting_for_trigger = true;
        StartTriggerCaptureRefreshWorker(impl);
        return true;
    }

    bool PromoteCapturedTriggerThread(Session::Impl &impl, DWORD thread_id, std::wstring &error)
    {
        error.clear();
        impl.trigger_capture_hit_count.fetch_add(1);
        impl.trigger_capture_last_thread_id.store(thread_id);

        HANDLE selected_handle = nullptr;
        {
            std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
            for (auto it = impl.trigger_capture_threads.begin(); it != impl.trigger_capture_threads.end(); ++it)
            {
                if (it->thread_id == thread_id)
                {
                    selected_handle = it->handle;
                    impl.trigger_capture_threads.erase(it);
                    break;
                }
            }

            trigger_capture_detail::ReleaseTriggerCaptureThreadsLocked(impl, true, selected_handle);
        }

        if (selected_handle == nullptr)
        {
            selected_handle = trigger_capture_detail::OpenTracingThreadHandle(thread_id);
            if (selected_handle == nullptr)
            {
                error = FormatWin32Error(L"打开捕获到的目标线程失败。", GetLastError());
                return false;
            }
        }

        if (impl.thread_handle != nullptr && impl.thread_handle != selected_handle)
        {
            std::wstring ignored_error;
            ClearThreadTraceState(impl.thread_handle, ignored_error);
            CloseHandle(impl.thread_handle);
        }

        impl.thread_handle = selected_handle;
        impl.active_thread_id.store(thread_id);
        {
            std::lock_guard<std::mutex> lock(impl.known_thread_lock);
            impl.known_thread_ids.insert(thread_id);
        }
        return true;
    }

    LONG HandleQueuedTriggerHitException(Session::Impl &impl, EXCEPTION_POINTERS *info)
    {
        if (!SupportsQueuedTriggerTracing(impl)
            || info == nullptr
            || info->ExceptionRecord == nullptr
            || info->ContextRecord == nullptr
            || info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        CONTEXT *context = info->ContextRecord;
        const uintptr_t current_rip = static_cast<uintptr_t>(context->Rip);
        if (current_rip != impl.configured_trigger_address)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const DWORD current_thread_id = GetCurrentThreadId();
        impl.trigger_capture_hit_count.fetch_add(1);
        impl.trigger_capture_last_thread_id.store(current_thread_id);

        std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
        if (!trigger_capture_detail::QueueTriggerCaptureThreadLocked(impl, current_thread_id, *context))
        {
            context->ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
            context->Dr0 = 0;
            context->Dr1 = 0;
            context->Dr2 = 0;
            context->Dr3 = 0;
            context->Dr6 = 0;
            context->Dr7 = 0;
            context->EFlags &= ~0x100u;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    bool RotateQueuedTriggerTrace(Session::Impl &impl, CONTEXT *current_context, std::wstring &error)
    {
        error.clear();
        if (!SupportsQueuedTriggerTracing(impl) || current_context == nullptr)
        {
            return false;
        }

        if (impl.thread_handle != nullptr)
        {
            CloseHandle(impl.thread_handle);
            impl.thread_handle = nullptr;
        }

        ResetThreadObservationState(impl);
        impl.active_thread_id.store(0);
        if (!BeginTriggerThreadCapture(impl, error))
        {
            return false;
        }

        current_context->ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
        current_context->Dr0 = impl.configured_trigger_address;
        current_context->Dr1 = 0;
        current_context->Dr2 = 0;
        current_context->Dr3 = 0;
        current_context->Dr6 = 0;
        current_context->Dr7 = 1;
        current_context->EFlags &= ~0x100u;
        return true;
    }
}
