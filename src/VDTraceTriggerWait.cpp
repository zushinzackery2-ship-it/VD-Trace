#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        void ClearDebugControls(CONTEXT &context)
        {
            context.ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
            context.Dr0 = 0;
            context.Dr1 = 0;
            context.Dr2 = 0;
            context.Dr3 = 0;
            context.Dr6 = 0;
            context.Dr7 = 0;
        }

        void MarkDetachedThread(Session::Impl &impl, DWORD thread_id)
        {
            if (thread_id == 0)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(impl.detached_thread_lock);
            impl.detached_thread_ids.insert(thread_id);
        }
    }

    bool TryConsumeStaleSingleStep(Session::Impl &impl, EXCEPTION_POINTERS *info)
    {
        if (impl.running.load() || info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr)
        {
            return false;
        }

        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        {
            return false;
        }

        CONTEXT *context = info->ContextRecord;
        ClearDebugControls(*context);
        impl.observation_state = ObservationState::Idle;
        impl.waiting_for_trigger = false;
        impl.last_rip_valid = false;
        return true;
    }

    LONG HandleTriggerWaitException(Session::Impl &impl, EXCEPTION_POINTERS *info)
    {
        if (info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        CONTEXT *context = info->ContextRecord;
        const uintptr_t current_rip = static_cast<uintptr_t>(context->Rip);
        if (current_rip != impl.resolved_trigger_address)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (!impl.options.trace_outside_modules
            && FindModuleRange(impl.module_ranges, current_rip) == nullptr)
        {
            uintptr_t inside_resume = 0;
            if (TryFindInsideReturnAddress(impl, static_cast<uintptr_t>(context->Rsp), inside_resume)
                && inside_resume != 0
                && inside_resume != current_rip)
            {
                impl.waiting_for_trigger = true;
                impl.resolved_trigger_address = inside_resume;
                impl.observed_addresses[0] = inside_resume;
                impl.observed_addresses[1] = 0;
                impl.observed_addresses[2] = 0;
                impl.observed_addresses[3] = 0;
                impl.observed_count = 1;
                ApplyHardwareContextObservations(*context, impl.observed_addresses, impl.observed_count, false);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }

        impl.step_count.fetch_add(1);
        impl.waiting_for_trigger = false;
        impl.root_stop_armed = impl.options.stop_on_root_return || impl.call_depth_offset != 0;
        impl.root_call_depth_base = impl.current_call_depth.load();
        TryEmitValueProbeEvent(impl, current_rip, context);

        if (impl.backend_mode == BackendMode::HardwareFlow)
        {
            std::wstring error;
            if (!StartRegionAwareHardwareFlow(impl, current_rip, context, error))
            {
                SnapshotExecutionSummary(impl);
                impl.running.store(false);
                impl.observation_state = ObservationState::Idle;
                ClearDebugControls(*context);
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        impl.last_rip = current_rip;
        impl.last_rip_valid = true;
        ClearDebugControls(*context);
        context->EFlags |= 0x100u;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    LONG HandleTriggerThreadCaptureException(Session::Impl &impl, EXCEPTION_POINTERS *info)
    {
        if (info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
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
        DWORD expected_thread_id = 0;
        if (!impl.active_thread_id.compare_exchange_strong(expected_thread_id, current_thread_id))
        {
            if (SupportsQueuedTriggerTracing(impl))
            {
                return HandleQueuedTriggerHitException(impl, info);
            }
            impl.trigger_capture_hit_count.fetch_add(1);
            impl.trigger_capture_last_thread_id.store(current_thread_id);
            MarkDetachedThread(impl, current_thread_id);
            ClearDebugControls(*context);
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        std::wstring promote_error;
        if (!PromoteCapturedTriggerThread(impl, current_thread_id, promote_error))
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            ClearDebugControls(*context);
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        return HandleTriggerWaitException(impl, info);
    }
}
