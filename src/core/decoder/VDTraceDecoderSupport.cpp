#include "pch.h"
#include "core/decoder/VDTraceDecoderInternal.h"

namespace vdtrace::decoder_detail
{
    bool TryConsumeDetachedThreadSingleStep(Session::Impl &impl, EXCEPTION_POINTERS *info)
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
        {
            std::lock_guard<std::mutex> lock(impl.detached_thread_lock);
            if (impl.detached_thread_ids.find(thread_id) == impl.detached_thread_ids.end())
            {
                return false;
            }
            impl.detached_thread_ids.erase(thread_id);
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

    bool ShouldAutoStopOnRootReturn(const Session::Impl &impl, EventKind kind, uint32_t call_depth)
    {
        return impl.root_stop_armed
            && kind == EventKind::Return
            && call_depth == impl.root_call_depth_base;
    }

    void PushTrapFlagCallFrame(Session::Impl &impl, uintptr_t return_address, bool suppress_callee, const EnhancedSamplingFrame &sample_frame)
    {
        impl.call_return_stack.push_back(return_address);
        PushEnhancedSamplingFrame(impl, sample_frame);
        impl.trap_suppression_stack.push_back(suppress_callee ? 1u : 0u);
        RefreshCurrentCallDepth(impl);
    }

    void TryPopTrapFlagCallFrame(Session::Impl &impl, uintptr_t resume_address)
    {
        if (impl.call_return_stack.empty() || impl.call_return_stack.back() != resume_address)
        {
            return;
        }

        impl.call_return_stack.pop_back();
        PopEnhancedSamplingFrame(impl);
        if (!impl.trap_suppression_stack.empty())
        {
            impl.trap_suppression_stack.pop_back();
        }
        RefreshCurrentCallDepth(impl);
    }

    bool TrapFlagFrameSuppressed(const Session::Impl &impl)
    {
        return !impl.trap_suppression_stack.empty() && impl.trap_suppression_stack.back() != 0;
    }

    bool ShouldSuppressTrapFlagCall(const Session::Impl &impl, uint32_t call_depth, uintptr_t target)
    {
        if (TrapFlagFrameSuppressed(impl))
        {
            return true;
        }

        const ResolvedExecutionAddress resolved = ResolveExecutionAddress(impl, target);
        if (resolved.kind == ExecutionAddressKind::Unknown || resolved.kind == ExecutionAddressKind::SystemModule)
        {
            return true;
        }

        uint32_t depth_limit = 0;
        bool has_limit = false;
        if (resolved.depth_rule != nullptr)
        {
            depth_limit = resolved.depth_rule->max_call_depth;
            has_limit = true;
        }
        else if (resolved.kind == ExecutionAddressKind::TrackedModule)
        {
            depth_limit = impl.options.max_call_depth;
            has_limit = true;
        }
        else if (resolved.kind == ExecutionAddressKind::OutsideImage)
        {
            depth_limit = impl.depth_filters.has_outside_module_depth
                ? impl.depth_filters.outside_module_depth
                : impl.options.max_call_depth;
            has_limit = true;
        }
        else if (resolved.kind == ExecutionAddressKind::AnonymousExecutable)
        {
            depth_limit = impl.depth_filters.has_anonymous_exec_depth
                ? impl.depth_filters.anonymous_exec_depth
                : impl.options.max_call_depth;
            has_limit = true;
        }
        if (!has_limit)
        {
            return true;
        }
        if (depth_limit != kUnlimitedCallDepth && call_depth >= depth_limit)
        {
            return true;
        }
        if (resolved.kind == ExecutionAddressKind::OutsideImage && !impl.options.trace_outside_modules)
        {
            return true;
        }
        return false;
    }

    bool ShouldEmitTrapFlagEvent(const Session::Impl &impl, const InstructionDecodeResult &decode_result)
    {
        if (impl.active_probe.active && impl.active_probe.mode == ProbeMode::FilterTrace)
        {
            return true;
        }
        return ShouldEmitEvent(impl.options, decode_result);
    }

    void UpdateTrapFlagCallStack(
        Session::Impl &impl,
        const InstructionDecodeResult &decode_result,
        uintptr_t executed_rip,
        uintptr_t current_rip,
        bool suppress_callee,
        const EnhancedSamplingFrame &sample_frame)
    {
        if (decode_result.kind == EventKind::Call && decode_result.size != 0)
        {
            PushTrapFlagCallFrame(impl, executed_rip + decode_result.size, suppress_callee, sample_frame);
            return;
        }

        if (decode_result.kind == EventKind::Return)
        {
            TryPopTrapFlagCallFrame(impl, current_rip);
        }
    }
}
