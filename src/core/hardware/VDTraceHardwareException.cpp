#include "pch.h"
#include "core/hardware/VDTraceHardwareInternal.h"

namespace vdtrace
{
    namespace
    {
        void PopHotBypassFrame(Session::Impl &impl, uintptr_t resume_address)
        {
            if (impl.call_return_stack.empty())
            {
                return;
            }

            if (impl.call_return_stack.back() != resume_address)
            {
                return;
            }

            impl.call_return_stack.pop_back();
            PopEnhancedSamplingFrame(impl);
            RefreshCurrentCallDepth(impl);
        }

        bool TransitionToLinearScan(Session::Impl &impl, uintptr_t entry, CONTEXT *context)
        {
            std::wstring ignored_error;
            if (!BeginHardwareLinearScan(impl, entry, context, ignored_error))
            {
                SnapshotExecutionSummary(impl);
                impl.running.store(false);
                impl.observation_state = ObservationState::Idle;
                uintptr_t empty[4] = {};
                if (context != nullptr)
                {
                    ApplyHardwareContextObservations(*context, empty, 0, false);
                }
                return false;
            }

            return true;
        }
    }

    LONG HandleHardwareFlowException(Session::Impl &impl, EXCEPTION_POINTERS *info)
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
        bool activated_probe = false;
        std::wstring probe_error;
        if (!TryActivateLocalProbeSession(impl, current_rip, context, true, activated_probe, probe_error))
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            uintptr_t empty[4] = {};
            ApplyHardwareContextObservations(*context, empty, 0, false);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (activated_probe)
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        bool activated_filter_tf = false;
        std::wstring filter_error;
        if (!TryActivateFilterTrapSession(impl, current_rip, context, activated_filter_tf, filter_error))
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            uintptr_t empty[4] = {};
            ApplyHardwareContextObservations(*context, empty, 0, false);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (activated_filter_tf)
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        impl.step_count.fetch_add(1);
        TryEmitValueProbeEvent(impl, current_rip, context);

        if (impl.observation_state == ObservationState::LinearScan)
        {
            const InstructionDecodeResult decode = DecodeInstruction(current_rip);
            if (decode.size == 0)
            {
                SnapshotExecutionSummary(impl);
                impl.running.store(false);
                impl.observation_state = ObservationState::Idle;
                uintptr_t empty[4] = {};
                ApplyHardwareContextObservations(*context, empty, 0, false);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            if (IsHardwareTerminator(decode.kind))
            {
                std::wstring plan_error;
                if (!ProgramHardwareObservationImpl(impl, current_rip, context, plan_error))
                {
                    SnapshotExecutionSummary(impl);
                    impl.running.store(false);
                    impl.observation_state = ObservationState::Idle;
                    uintptr_t empty[4] = {};
                    ApplyHardwareContextObservations(*context, empty, 0, false);
                }
            }
            else
            {
                uintptr_t empty[4] = {};
                ApplyHardwareContextObservations(*context, empty, 0, true);
            }

            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (impl.observation_state == ObservationState::WaitingForSingleStep)
        {
            bool waiting_for_resume = false;
            uintptr_t next_entry = current_rip;
            bool context_already_programmed = false;
            if (!FinalizeHardwareTransition(impl, current_rip, context, true, waiting_for_resume, next_entry, context_already_programmed))
            {
                uintptr_t empty[4] = {};
                ApplyHardwareContextObservations(*context, empty, 0, false);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (context_already_programmed)
            {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (waiting_for_resume)
            {
                ApplyHardwareContextObservations(*context, impl.observed_addresses, impl.observed_count, false);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            std::wstring plan_error;
            if (!ProgramHardwareObservationImpl(impl, next_entry, context, plan_error))
            {
                if (!TransitionToLinearScan(impl, next_entry, context))
                {
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }
        else
        {
            bool matched = false;
            for (uint32_t index = 0; index < impl.observed_count; index++)
            {
                if (impl.observed_addresses[index] == current_rip)
                {
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (impl.observation_state == ObservationState::WaitingForHotReturn)
            {
                const uint32_t current_call_depth = impl.current_call_depth.load();
                const bool hit_hot_resume =
                    impl.hot_bypass_resume != 0
                    && impl.hot_bypass_resume == current_rip
                    && impl.hot_bypass_call_depth == current_call_depth;
                const bool hit_hot_return =
                    impl.hot_bypass_return != 0
                    && impl.hot_bypass_return == current_rip
                    && impl.hot_bypass_call_depth == current_call_depth;
                const bool stop_on_hot_bypass_return =
                    impl.hot_bypass_root_stop
                    && hit_hot_return;
                if (!hit_hot_resume && !hit_hot_return)
                {
                    ApplyHardwareContextObservations(*context, impl.observed_addresses, impl.observed_count, false);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }

                if (hit_hot_return)
                {
                    PopHotBypassFrame(impl, current_rip);
                }
                impl.observation_state = ObservationState::Idle;
                impl.hot_bypass_resume = 0;
                impl.hot_bypass_return = 0;
                impl.hot_bypass_call_depth = 0;
                impl.hot_bypass_root_stop = false;
                if (stop_on_hot_bypass_return)
                {
                    std::wstring rotate_error;
                    if (RotateQueuedTriggerTrace(impl, context, rotate_error))
                    {
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }

                    SnapshotExecutionSummary(impl);
                    impl.running.store(false);
                    uintptr_t empty[4] = {};
                    ApplyHardwareContextObservations(*context, empty, 0, false);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }

                std::wstring plan_error;
                if (!ProgramHardwareObservationImpl(impl, current_rip, context, plan_error))
                {
                    if (!TransitionToLinearScan(impl, current_rip, context))
                    {
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            if (impl.observation_state == ObservationState::WaitingForTail)
            {
                impl.tf_probe_source = impl.pending_block.tail;
                impl.tf_probe_decode = impl.pending_block.tail_decode;
                impl.observation_state = ObservationState::WaitingForSingleStep;
                if (impl.pending_block.tail_decode.kind != EventKind::Call)
                {
                    impl.pending_resume_entry_valid = false;
                }
                uintptr_t empty[4] = {};
                ApplyHardwareContextObservations(*context, empty, 0, true);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            uintptr_t next_entry = current_rip;
            bool waiting_for_resume = false;
            bool context_already_programmed = false;
            if (!FinalizeHardwareTransition(impl, current_rip, context, false, waiting_for_resume, next_entry, context_already_programmed))
            {
                uintptr_t empty[4] = {};
                ApplyHardwareContextObservations(*context, empty, 0, false);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (context_already_programmed)
            {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if (impl.observation_state == ObservationState::WaitingForHotReturn)
            {
                ApplyHardwareContextObservations(*context, impl.observed_addresses, impl.observed_count, false);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            std::wstring plan_error;
            if (!ProgramHardwareObservationImpl(impl, next_entry, context, plan_error))
            {
                if (!TransitionToLinearScan(impl, next_entry, context))
                {
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }

        // End point reached: the fault landed on the configured stop address (an edge
        // target in DR mode). The arriving edge has already been recorded above, so we
        // just disarm and end the session. Takes priority over max_events.
        if (impl.resolved_stop_address != 0 && current_rip == impl.resolved_stop_address)
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            uintptr_t empty[4] = {};
            ApplyHardwareContextObservations(*context, empty, 0, false);
        }
        else if (impl.options.max_events > 0 && impl.event_count.load() >= impl.options.max_events)
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            uintptr_t empty[4] = {};
            ApplyHardwareContextObservations(*context, empty, 0, false);
        }

        return EXCEPTION_CONTINUE_EXECUTION;
    }
}
