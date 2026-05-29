#include "pch.h"
#include "core/runtime/VDTraceInternal.h"
#include "core/hardware/VDTraceHardwareTransitionInternal.h"

namespace vdtrace
{
    bool FinalizeHardwareTransition(
        Session::Impl &impl,
        uintptr_t observed_rip,
        const CONTEXT *context,
        bool from_single_step,
        bool &waiting_for_resume,
        uintptr_t &next_entry,
        bool &context_already_programmed)
    {
        waiting_for_resume = false;
        context_already_programmed = false;
        const uintptr_t stack_pointer = context != nullptr ? static_cast<uintptr_t>(context->Rsp) : 0;
        next_entry = impl.pending_resume_entry_valid ? impl.pending_resume_entry : observed_rip;
        if (!impl.pending_block.valid || !impl.pending_block.emits_edge)
        {
            ClearPendingResolution(impl);
            return true;
        }

        const uintptr_t event_target = impl.pending_target_override_valid ? impl.pending_target_override : observed_rip;
        if (!from_single_step && impl.pending_async_handoff_valid && context != nullptr)
        {
            std::wstring handoff_error;
            const bool switched = TryActivateAsyncThreadHandoff(
                impl,
                impl.pending_async_handoff_target,
                impl.pending_async_handoff_event,
                const_cast<CONTEXT *>(context),
                handoff_error);
            impl.pending_async_handoff_target = 0;
            impl.pending_async_handoff_event = {};
            impl.pending_async_handoff_valid = false;
            if (switched)
            {
                ClearPendingResolution(impl);
                return true;
            }
        }

        if (impl.pending_block.tail_decode.kind == EventKind::Call)
        {
            const bool should_follow_target = ShouldFollowKnownCallTarget(impl, event_target)
                || ShouldFollowMinimalOutsideTarget(impl, event_target);
            next_entry = should_follow_target
                ? observed_rip
                : impl.pending_block.fallthrough;
        }

        const uint32_t call_depth = impl.current_call_depth.load();
        const bool should_emit = hardware_transition_detail::ShouldEmitTransition(
            impl,
            impl.pending_block.tail,
            event_target,
            impl.pending_block.tail_decode.kind);
        StepEvent emitted_event = {};
        EnhancedSamplingFrame sampled_frame = {};
        bool emitted = false;
        if (should_emit)
        {
            emitted_event.sequence = impl.event_count.fetch_add(1) + 1;
            emitted_event.thread_id = GetCurrentThreadId();
            emitted_event.instruction = impl.pending_block.tail;
            emitted_event.stack_pointer = stack_pointer;
            emitted_event.block_begin = impl.pending_block.entry;
            emitted_event.block_end = impl.pending_block.fallthrough;
            emitted_event.target = event_target;
            emitted_event.call_depth = call_depth;
            emitted_event.kind = impl.pending_block.tail_decode.kind;
            emitted_event.minimal_record = false;
            emitted_event.instruction_size = impl.pending_block.tail_decode.size;
            std::memcpy(emitted_event.instruction_bytes, impl.pending_block.tail_decode.bytes, sizeof(emitted_event.instruction_bytes));
            if (context != nullptr)
            {
                CaptureThreadContext(*context, emitted_event.thread_context);
            }
            emitted_event.has_target = true;
            if (!emitted_event.minimal_record && emitted_event.kind == EventKind::Call && context != nullptr)
            {
                CaptureCallArguments(*context, emitted_event.call_arguments, emitted_event.call_argument_count);
                PrepareEnhancedSamplingCallEvent(
                    impl,
                    impl.pending_block.tail,
                    event_target,
                    context,
                    emitted_event,
                    sampled_frame);
            }
            else if (!emitted_event.minimal_record && emitted_event.kind == EventKind::Return && context != nullptr)
            {
                emitted_event.has_return_value = true;
                emitted_event.return_value = static_cast<uintptr_t>(context->Rax);
                PrepareEnhancedSamplingReturnEvent(impl, context, emitted_event);
            }

            if (const auto *range = FindModuleRange(impl.module_ranges, impl.pending_block.tail); range != nullptr)
            {
                emitted_event.inside_module = true;
                emitted_event.module_base = range->base;
                emitted_event.module_size = range->size;
                emitted_event.relative_instruction = impl.pending_block.tail - range->base;
                if (!impl.lightweight_module_capture)
                {
                    emitted_event.module_name = range->name;
                }
            }
            if (impl.options.callback != nullptr)
            {
                impl.options.callback(emitted_event, impl.options.callback_context);
            }
            emitted = true;
            hardware_transition_detail::ResetSuppressedTransitionState(impl);
        }

        hardware_transition_detail::UpdateCallStackForTransition(impl, next_entry, sampled_frame);
        if (emitted && emitted_event.kind == EventKind::Call && context != nullptr)
        {
            if (!from_single_step)
            {
                std::wstring handoff_error;
                if (TryActivateAsyncThreadHandoff(impl, event_target, emitted_event, const_cast<CONTEXT *>(context), handoff_error))
                {
                    ClearPendingResolution(impl);
                    return true;
                }
            }
            if (from_single_step)
            {
                impl.pending_async_handoff_target = event_target;
                impl.pending_async_handoff_event = emitted_event;
                impl.pending_async_handoff_valid = true;
            }
        }

        if (from_single_step
            && impl.pending_block.tail_decode.kind == EventKind::Call
            && impl.pending_resume_entry_valid
            && impl.pending_target_override_valid)
        {
            impl.observed_addresses[0] = impl.pending_resume_entry;
            impl.observed_addresses[1] = 0;
            impl.observed_addresses[2] = 0;
            impl.observed_addresses[3] = 0;
            impl.observed_count = 1;
            impl.observation_state = ObservationState::WaitingForDestination;
            waiting_for_resume = true;
        }

        if (!should_emit && hardware_transition_detail::TryArmHotReturnBypass(impl, event_target, call_depth))
        {
            return true;
        }

        ClearPendingResolution(impl);
        if (should_emit
            && hardware_transition_detail::ShouldAutoStopOnRootReturn(
                impl,
                impl.pending_block.tail_decode.kind,
                call_depth))
        {
            std::wstring rotate_error;
            if (RotateQueuedTriggerTrace(impl, const_cast<CONTEXT *>(context), rotate_error))
            {
                context_already_programmed = true;
                return true;
            }

            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            return false;
        }

        return true;
    }
}
