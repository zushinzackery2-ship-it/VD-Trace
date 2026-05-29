#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    bool IsAnonymousExecutableOutsideTrackedModules(const Session::Impl &impl, uintptr_t address)
    {
        return ResolveExecutionAddress(impl, address).kind == ExecutionAddressKind::AnonymousExecutable;
    }

    bool ShouldRecordOutsideAddress(const Session::Impl &impl, uintptr_t address)
    {
        if (impl.options.trace_outside_modules)
        {
            return true;
        }

        return ResolveExecutionAddress(impl, address).kind == ExecutionAddressKind::AnonymousExecutable;
    }

    void ClearPendingResolution(Session::Impl &impl)
    {
        impl.pending_resume_entry = 0;
        impl.pending_target_override = 0;
        impl.pending_resume_entry_valid = false;
        impl.pending_target_override_valid = false;
    }

    bool ShouldFollowKnownCallTarget(const Session::Impl &impl, uintptr_t target)
    {
        const ResolvedExecutionAddress resolved = ResolveExecutionAddress(impl, target);
        if (resolved.kind == ExecutionAddressKind::Unknown || resolved.kind == ExecutionAddressKind::SystemModule)
        {
            return false;
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
            return false;
        }

        if (depth_limit != kUnlimitedCallDepth && impl.current_call_depth.load() >= depth_limit)
        {
            return false;
        }

        return resolved.kind == ExecutionAddressKind::TrackedModule
            || resolved.kind == ExecutionAddressKind::AnonymousExecutable
            || (resolved.kind == ExecutionAddressKind::OutsideImage && impl.options.trace_outside_modules);
    }

    bool ShouldFollowMinimalOutsideTarget(const Session::Impl &impl, uintptr_t target)
    {
        (void)impl;
        (void)target;
        return false;
    }

    bool IsMinimalOutsideAddress(const Session::Impl &impl, uintptr_t address)
    {
        (void)impl;
        (void)address;
        return false;
    }

    bool ShouldProbeAsyncCallTarget(const Session::Impl &impl, uintptr_t target)
    {
        return FindAsyncProbe(impl.async_probes, target) != nullptr;
    }

    bool ProgramHardwareObservationImpl(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error)
    {
        BasicBlockInfo block = {};
        if (!AnalyzeBasicBlock(entry, block, error))
        {
            return false;
        }

        impl.pending_block = block;
        impl.observation_state = ObservationState::Idle;
        impl.tf_probe_source = 0;
        impl.tf_probe_decode = {};
        ClearPendingResolution(impl);

        if (HasValueProbeInRange(impl, entry + 1, block.fallthrough))
        {
            error = L"命中取值观测块，切换线性扫描。";
            return false;
        }

        if (!block.emits_edge || block.truncated)
        {
            error = L"基本块截断，切换线性扫描。";
            return false;
        }

        uintptr_t addresses[4] = {};
        uint32_t count = 0;
        switch (block.tail_decode.kind)
        {
        case EventKind::ConditionalJump:
            addresses[count++] = block.tail_decode.target;
            addresses[count++] = block.fallthrough;
            impl.observation_state = ObservationState::WaitingForDestination;
            break;

        case EventKind::Call:
        {
            if (block.tail_decode.has_target)
            {
                const bool should_follow = ShouldFollowKnownCallTarget(impl, block.tail_decode.target);
                const bool should_follow_minimal_outside = !should_follow && ShouldFollowMinimalOutsideTarget(impl, block.tail_decode.target);
                const bool should_probe_async = !should_follow && ShouldProbeAsyncCallTarget(impl, block.tail_decode.target);
                if (should_follow || should_follow_minimal_outside)
                {
                    addresses[count++] = block.tail_decode.target;
                    impl.observation_state = ObservationState::WaitingForDestination;
                }
                else if (should_probe_async)
                {
                    impl.pending_resume_entry = block.fallthrough;
                    impl.pending_target_override = block.tail_decode.target;
                    impl.pending_resume_entry_valid = true;
                    impl.pending_target_override_valid = true;
                    addresses[count++] = block.tail;
                    impl.observation_state = ObservationState::WaitingForTail;
                }
                else
                {
                    addresses[count++] = block.fallthrough;
                    impl.pending_resume_entry = block.fallthrough;
                    impl.pending_target_override = block.tail_decode.target;
                    impl.pending_resume_entry_valid = true;
                    impl.pending_target_override_valid = true;
                    impl.observation_state = ObservationState::WaitingForDestination;
                }
            }
            else
            {
                impl.pending_resume_entry = block.fallthrough;
                impl.pending_resume_entry_valid = true;
                addresses[count++] = block.tail;
                impl.observation_state = ObservationState::WaitingForTail;
            }
            break;
        }

        case EventKind::Jump:
            if (block.tail_decode.has_target)
            {
                addresses[count++] = block.tail_decode.target;
                impl.observation_state = ObservationState::WaitingForDestination;
            }
            else
            {
                addresses[count++] = block.tail;
                impl.observation_state = ObservationState::WaitingForTail;
            }
            break;

        case EventKind::Return:
        case EventKind::Syscall:
        case EventKind::Interrupt:
            addresses[count++] = block.tail;
            impl.observation_state = ObservationState::WaitingForTail;
            break;

        default:
            break;
        }

        if (StoreObservedAddressesUnique(impl, addresses, count) == 0)
        {
            error = L"没有可用的下一观察点。";
            return false;
        }

        if (context != nullptr)
        {
            ApplyHardwareContextObservations(*context, impl.observed_addresses, impl.observed_count, false);
        }

        return true;
    }
}
