#include "pch.h"
#include "core/decoder/VDTraceDecoderInternal.h"

namespace vdtrace
{
    bool ShouldEmitEvent(const Options &options, const InstructionDecodeResult &decode_result)
    {
        if (!options.control_flow_only)
        {
            return true;
        }

        return decode_result.kind == EventKind::Call
            || decode_result.kind == EventKind::Jump
            || decode_result.kind == EventKind::ConditionalJump
            || decode_result.kind == EventKind::Return
            || decode_result.kind == EventKind::Syscall
            || decode_result.kind == EventKind::Interrupt;
    }

    LONG HandleTrapFlagTraceException(Session::Impl &impl, EXCEPTION_POINTERS *info)
    {
        using namespace decoder_detail;

        if (info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        CONTEXT *context = info->ContextRecord;
        if (impl.stop_requested.load())
        {
            impl.stop_requested.store(false);
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.last_rip = 0;
            impl.last_rip_valid = false;
            impl.call_return_stack.clear();
            impl.trap_suppression_stack.clear();
            RefreshCurrentCallDepth(impl);
            context->EFlags &= ~0x100u;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (!impl.running.load())
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const uintptr_t current_rip = static_cast<uintptr_t>(context->Rip);
        const uintptr_t executed_rip = impl.last_rip_valid ? impl.last_rip : current_rip;
        impl.last_rip = current_rip;
        impl.last_rip_valid = true;
        TryEmitValueProbeEvent(impl, current_rip, context);
        if (!impl.active_probe.active)
        {
            bool activated_probe = false;
            std::wstring probe_error;
            if (!TryActivateLocalProbeSession(impl, current_rip, context, false, activated_probe, probe_error))
            {
                SnapshotExecutionSummary(impl);
                impl.running.store(false);
                impl.last_rip_valid = false;
                context->EFlags &= ~0x100u;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }

        impl.step_count.fetch_add(1);
        const InstructionDecodeResult decode_result = DecodeInstruction(executed_rip);
        const uint32_t call_depth = impl.current_call_depth.load();
        const ResolvedExecutionAddress execution_address = ResolveExecutionAddress(impl, executed_rip);
        const ModuleRange *range = execution_address.kind == ExecutionAddressKind::TrackedModule
            ? execution_address.module_range
            : nullptr;
        const bool outside_recordable = execution_address.kind == ExecutionAddressKind::AnonymousExecutable
            || (execution_address.kind == ExecutionAddressKind::OutsideImage && impl.options.trace_outside_modules);
        uintptr_t actual_target = 0;
        bool has_actual_target = false;
        if (decode_result.has_target)
        {
            actual_target = decode_result.target;
            has_actual_target = true;
        }
        else if (decode_result.kind == EventKind::Call && current_rip != executed_rip + decode_result.size)
        {
            actual_target = current_rip;
            has_actual_target = true;
        }
        else if (decode_result.kind == EventKind::Return && current_rip != 0)
        {
            actual_target = current_rip;
            has_actual_target = true;
        }

        const bool should_emit = (range != nullptr || outside_recordable)
            && ShouldEmitTrapFlagEvent(impl, decode_result)
            && !TrapFlagFrameSuppressed(impl);
        EnhancedSamplingFrame sampled_frame = {};

        if (should_emit)
        {
            StepEvent event = {};
            event.sequence = impl.event_count.fetch_add(1) + 1;
            event.thread_id = GetCurrentThreadId();
            event.instruction = executed_rip;
            event.stack_pointer = static_cast<uintptr_t>(context->Rsp);
            event.block_begin = executed_rip;
            event.block_end = executed_rip + decode_result.size;
            event.call_depth = call_depth;
            event.kind = decode_result.kind;
            event.minimal_record = false;
            event.instruction_size = decode_result.size;
            std::memcpy(event.instruction_bytes, decode_result.bytes, sizeof(event.instruction_bytes));
            CaptureThreadContext(*context, event.thread_context);
            if (has_actual_target)
            {
                event.has_target = true;
                event.target = actual_target;
            }
            if (!event.minimal_record && event.kind == EventKind::Call && event.has_target)
            {
                CaptureCallArguments(*context, event.call_arguments, event.call_argument_count);
                PrepareEnhancedSamplingCallEvent(impl, executed_rip, actual_target, context, event, sampled_frame);
            }
            else if (!event.minimal_record && event.kind == EventKind::Return)
            {
                event.has_return_value = true;
                event.return_value = static_cast<uintptr_t>(context->Rax);
                PrepareEnhancedSamplingReturnEvent(impl, context, event);
            }

            if (range != nullptr)
            {
                event.inside_module = true;
                event.module_base = range->base;
                event.module_size = range->size;
                event.relative_instruction = executed_rip - range->base;
                if (!impl.lightweight_module_capture)
                {
                    event.module_name = range->name;
                }
            }

            if (impl.options.callback != nullptr)
            {
                impl.options.callback(event, impl.options.callback_context);
            }

            if (event.kind == EventKind::Call)
            {
                std::wstring handoff_error;
                if (TryActivateAsyncThreadHandoff(impl, actual_target, event, context, handoff_error))
                {
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }

        const bool suppress_callee = decode_result.kind == EventKind::Call && has_actual_target
            ? ShouldSuppressTrapFlagCall(impl, call_depth, actual_target)
            : TrapFlagFrameSuppressed(impl);
        UpdateTrapFlagCallStack(impl, decode_result, executed_rip, current_rip, suppress_callee, sampled_frame);

        bool restored_hardware_flow = false;
        std::wstring active_probe_error;
        if (!HandleActiveProbeSingleStep(
                impl,
                executed_rip,
                current_rip,
                context,
                decode_result,
                call_depth,
                actual_target,
                has_actual_target,
                restored_hardware_flow,
                active_probe_error))
        {
            impl.last_rip_valid = false;
            context->EFlags &= ~0x100u;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (restored_hardware_flow)
        {
            impl.last_rip_valid = false;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (should_emit && ShouldAutoStopOnRootReturn(impl, decode_result.kind, call_depth))
        {
            std::wstring rotate_error;
            if (RotateQueuedTriggerTrace(impl, context, rotate_error))
            {
                impl.last_rip_valid = false;
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.last_rip_valid = false;
            context->EFlags &= ~0x100u;
        }
        else if (impl.options.max_events > 0 && impl.event_count.load() >= impl.options.max_events)
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.last_rip_valid = false;
            context->EFlags &= ~0x100u;
        }
        else
        {
            context->EFlags |= 0x100u;
        }

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    LONG Session::Impl::HandleException(EXCEPTION_POINTERS *info)
    {
        if (options.auto_select_thread && waiting_for_trigger && active_thread_id.load() == 0)
        {
            return HandleTriggerThreadCaptureException(*this, info);
        }

        if (GetCurrentThreadId() != active_thread_id.load())
        {
            const LONG queued_result = HandleQueuedTriggerHitException(*this, info);
            if (queued_result != EXCEPTION_CONTINUE_SEARCH)
            {
                return queued_result;
            }
            if (decoder_detail::TryConsumeDetachedThreadSingleStep(*this, info))
            {
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }

        if (TryConsumeStaleSingleStep(*this, info))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (waiting_for_trigger)
        {
            return HandleTriggerWaitException(*this, info);
        }

        if (backend_mode == BackendMode::HardwareFlow)
        {
            return HandleHardwareFlowException(*this, info);
        }

        return HandleTrapFlagTraceException(*this, info);
    }
}
