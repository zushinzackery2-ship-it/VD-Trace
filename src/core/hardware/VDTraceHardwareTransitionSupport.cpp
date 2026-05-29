#include "pch.h"
#include "core/hardware/VDTraceHardwareTransitionInternal.h"

namespace vdtrace::hardware_transition_detail
{
    bool ShouldAutoStopOnRootReturn(const Session::Impl &impl, EventKind kind, uint32_t call_depth)
    {
        return impl.root_stop_armed
            && kind == EventKind::Return
            && call_depth == impl.root_call_depth_base;
    }

    void ResetSuppressedTransitionState(Session::Impl &impl)
    {
        impl.suppressed_transition_streak = 0;
        impl.suppressed_hot_anchor = 0;
        impl.suppressed_frame_depth = 0;
    }

    void ResetHotBypassState(Session::Impl &impl)
    {
        impl.hot_bypass_resume = 0;
        impl.hot_bypass_return = 0;
        impl.hot_bypass_call_depth = 0;
        impl.hot_bypass_root_stop = false;
    }

    uintptr_t ResolveHotBypassResumeAddress(const Session::Impl &impl, uintptr_t repeated_target)
    {
        if (!impl.pending_block.valid || !impl.pending_block.emits_edge)
        {
            return 0;
        }

        if (impl.pending_block.tail_decode.kind != EventKind::ConditionalJump)
        {
            return 0;
        }

        const uintptr_t branch_target = impl.pending_block.tail_decode.target;
        const uintptr_t fallthrough = impl.pending_block.fallthrough;
        if (branch_target != 0 && repeated_target == branch_target && fallthrough != 0)
        {
            return fallthrough;
        }

        if (fallthrough != 0 && repeated_target == fallthrough && branch_target != 0)
        {
            return branch_target;
        }

        if (branch_target == 0)
        {
            return fallthrough;
        }

        if (fallthrough == 0)
        {
            return branch_target;
        }

        return 0;
    }

    bool ShouldEmitTransition(Session::Impl &impl, uintptr_t source, uintptr_t target, EventKind kind)
    {
        const ExecutionAddressKind source_kind = ResolveExecutionAddress(impl, source).kind;
        const bool source_inside = source_kind == ExecutionAddressKind::TrackedModule;
        const bool outside_recordable = source_kind == ExecutionAddressKind::AnonymousExecutable
            || (source_kind == ExecutionAddressKind::OutsideImage && impl.options.trace_outside_modules);
        if (!source_inside && !outside_recordable)
        {
            impl.outside_suppressed_count.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        FlowEdgeKey key = {};
        key.instruction = source;
        key.target = target;
        key.kind = kind;
        if (impl.options.hit_policy == FlowHitPolicy::EveryHit)
        {
            return true;
        }

        if (impl.seen_edges.insert(key).second)
        {
            return true;
        }

        impl.duplicate_edge_suppressed_count.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    void PushCallFrame(Session::Impl &impl, uintptr_t return_address, const EnhancedSamplingFrame &sample_frame)
    {
        impl.call_return_stack.push_back(return_address);
        PushEnhancedSamplingFrame(impl, sample_frame);
        RefreshCurrentCallDepth(impl);
    }

    void TryPopCallFrame(Session::Impl &impl, uintptr_t resume_address)
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

    void UpdateCallStackForTransition(Session::Impl &impl, uintptr_t next_entry, const EnhancedSamplingFrame &sample_frame)
    {
        if (!impl.pending_block.valid || !impl.pending_block.emits_edge)
        {
            return;
        }

        if (impl.pending_block.tail_decode.kind == EventKind::Call)
        {
            if (next_entry != impl.pending_block.fallthrough)
            {
                PushCallFrame(impl, impl.pending_block.fallthrough, sample_frame);
            }
            return;
        }

        if (impl.pending_block.tail_decode.kind == EventKind::Return)
        {
            TryPopCallFrame(impl, next_entry);
        }
    }

    bool TryArmHotReturnBypass(Session::Impl &impl, uintptr_t repeated_target, uint32_t call_depth)
    {
        if (impl.options.hit_policy != FlowHitPolicy::FirstSeen
            || repeated_target == 0
            || impl.options.hot_bypass_threshold == 0)
        {
            ResetSuppressedTransitionState(impl);
            ResetHotBypassState(impl);
            return false;
        }

        const EventKind kind = impl.pending_block.tail_decode.kind;
        if (kind != EventKind::ConditionalJump && kind != EventKind::Jump)
        {
            ResetSuppressedTransitionState(impl);
            ResetHotBypassState(impl);
            return false;
        }

        const uintptr_t frame_return = impl.call_return_stack.empty() ? 0 : impl.call_return_stack.back();
        const uintptr_t resume_address = ResolveHotBypassResumeAddress(impl, repeated_target);
        const uintptr_t hot_anchor = frame_return != 0 ? frame_return : resume_address;
        if (hot_anchor == 0)
        {
            ResetSuppressedTransitionState(impl);
            ResetHotBypassState(impl);
            return false;
        }

        if (impl.suppressed_hot_anchor != hot_anchor || impl.suppressed_frame_depth != call_depth)
        {
            impl.suppressed_hot_anchor = hot_anchor;
            impl.suppressed_frame_depth = call_depth;
            impl.suppressed_transition_streak = 1;
            return false;
        }

        if (++impl.suppressed_transition_streak < impl.options.hot_bypass_threshold)
        {
            return false;
        }

        uintptr_t next_addresses[4] = {};
        uint32_t next_count = 0;
        if (resume_address != 0 && resume_address != repeated_target)
        {
            next_addresses[next_count++] = resume_address;
        }
        if (frame_return != 0)
        {
            next_addresses[next_count++] = frame_return;
        }

        if (StoreObservedAddressesUnique(impl, next_addresses, next_count) == 0)
        {
            ResetSuppressedTransitionState(impl);
            ResetHotBypassState(impl);
            return false;
        }

        impl.hot_bypass_resume = resume_address;
        impl.hot_bypass_return = frame_return;
        impl.hot_bypass_call_depth = call_depth;
        impl.hot_bypass_root_stop = frame_return != 0
            && ShouldAutoStopOnRootReturn(impl, EventKind::Return, call_depth);
        impl.hot_bypass_entry_count.fetch_add(1, std::memory_order_relaxed);
        impl.observation_state = ObservationState::WaitingForHotReturn;
        impl.pending_block = {};
        ResetSuppressedTransitionState(impl);
        ClearPendingResolution(impl);
        return true;
    }
}
