#include "pch.h"
#include "VDTraceRuntimeInternal.h"

namespace vdtrace::runtime_detail
{
    namespace
    {
        std::atomic<Session::Impl *> g_active_impl = nullptr;
        std::atomic<DWORD> g_recent_stop_thread_id = 0;
        std::atomic<ULONGLONG> g_recent_stop_deadline = 0;
        std::mutex g_recent_capture_lock;
        std::unordered_set<DWORD> g_recent_capture_thread_ids;

        bool TryConsumeRecentStoppedSingleStep(EXCEPTION_POINTERS *info)
        {
            if (info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr)
            {
                return false;
            }

            if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
            {
                return false;
            }

            const DWORD thread_id = GetCurrentThreadId();
            const ULONGLONG deadline = g_recent_stop_deadline.load();
            if (thread_id == 0
                || thread_id != g_recent_stop_thread_id.load()
                || deadline == 0
                || GetTickCount64() > deadline)
            {
                return false;
            }

            CONTEXT *context = info->ContextRecord;
            context->ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
            context->Dr0 = 0;
            context->Dr1 = 0;
            context->Dr2 = 0;
            context->Dr3 = 0;
            context->Dr6 = 0;
            context->Dr7 = 0;
            context->EFlags &= ~0x100u;
            return true;
        }

        bool TryConsumeRecentStoppedCaptureSingleStep(EXCEPTION_POINTERS *info)
        {
            if (info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr)
            {
                return false;
            }

            if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
            {
                return false;
            }

            const DWORD thread_id = GetCurrentThreadId();
            if (thread_id == 0)
            {
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(g_recent_capture_lock);
                const auto it = g_recent_capture_thread_ids.find(thread_id);
                if (it == g_recent_capture_thread_ids.end())
                {
                    return false;
                }
                g_recent_capture_thread_ids.erase(it);
            }

            CONTEXT *context = info->ContextRecord;
            context->ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
            context->Dr0 = 0;
            context->Dr1 = 0;
            context->Dr2 = 0;
            context->Dr3 = 0;
            context->Dr6 = 0;
            context->Dr7 = 0;
            context->EFlags &= ~0x100u;
            return true;
        }

        void RememberRecentCaptureThread(DWORD thread_id)
        {
            if (thread_id == 0)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(g_recent_capture_lock);
            g_recent_capture_thread_ids.insert(thread_id);
        }
    }

    void ActivateRuntimeSession(Session::Impl *impl)
    {
        g_active_impl.store(impl, std::memory_order_release);
    }

    void DeactivateRuntimeSession(Session::Impl *impl)
    {
        if (g_active_impl.load(std::memory_order_acquire) == impl)
        {
            g_active_impl.store(nullptr, std::memory_order_release);
        }
    }

    void ResetRecentStopState()
    {
        g_recent_stop_thread_id.store(0);
        g_recent_stop_deadline.store(0);
    }

    void MarkRecentStopThread(DWORD thread_id, ULONGLONG deadline)
    {
        g_recent_stop_thread_id.store(thread_id);
        g_recent_stop_deadline.store(deadline);
    }

    void ResetExecutionState(Session::Impl &impl)
    {
        impl.step_count.store(0);
        impl.event_count.store(0);
        impl.duplicate_edge_suppressed_count.store(0);
        impl.outside_suppressed_count.store(0);
        impl.hot_bypass_entry_count.store(0);
        impl.trigger_capture_hit_count.store(0);
        impl.trigger_capture_last_thread_id.store(0);
        impl.seen_edges.clear();
        impl.call_depth_offset = 0;
        ResetThreadObservationState(impl);
    }

    void ArmRootStop(Session::Impl &impl)
    {
        impl.root_stop_armed = impl.options.stop_on_root_return;
        impl.root_call_depth_base = impl.current_call_depth.load();
    }

    LONG CALLBACK GlobalVectoredHandler(EXCEPTION_POINTERS *info)
    {
        if (info == nullptr || info->ExceptionRecord == nullptr)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        Session::Impl *impl = g_active_impl.load(std::memory_order_acquire);
        if (impl == nullptr)
        {
            if (TryConsumeRecentStoppedCaptureSingleStep(info))
            {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (TryConsumeRecentStoppedSingleStep(info))
            {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }

        impl->active_handler_count.fetch_add(1, std::memory_order_acq_rel);
        const LONG result = impl->HandleException(info);
        impl->active_handler_count.fetch_sub(1, std::memory_order_acq_rel);
        if (result == EXCEPTION_CONTINUE_SEARCH && TryConsumeRecentStoppedSingleStep(info))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return result;
    }
}

namespace vdtrace
{
    void RememberRecentCaptureThreadForStop(DWORD thread_id)
    {
        runtime_detail::RememberRecentCaptureThread(thread_id);
    }
}
