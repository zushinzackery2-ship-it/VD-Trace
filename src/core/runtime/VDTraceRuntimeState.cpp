#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    void SnapshotExecutionSummary(Session::Impl &impl)
    {
        impl.retained_step_count.store(impl.step_count.load());
        impl.retained_event_count.store(impl.event_count.load());
        impl.retained_duplicate_edge_suppressed_count.store(impl.duplicate_edge_suppressed_count.load());
        impl.retained_outside_suppressed_count.store(impl.outside_suppressed_count.load());
        impl.retained_hot_bypass_entry_count.store(impl.hot_bypass_entry_count.load());
        impl.retained_call_depth.store(impl.current_call_depth.load());
        impl.retained_trigger_capture_hit_count.store(impl.trigger_capture_hit_count.load());
        impl.retained_trigger_capture_last_thread_id.store(impl.trigger_capture_last_thread_id.load());
    }

    void ClearExecutionSummary(Session::Impl &impl)
    {
        impl.retained_step_count.store(0);
        impl.retained_event_count.store(0);
        impl.retained_duplicate_edge_suppressed_count.store(0);
        impl.retained_outside_suppressed_count.store(0);
        impl.retained_hot_bypass_entry_count.store(0);
        impl.retained_call_depth.store(0);
        impl.retained_trigger_capture_hit_count.store(0);
        impl.retained_trigger_capture_last_thread_id.store(0);
    }

    void RefreshCurrentCallDepth(Session::Impl &impl)
    {
        impl.current_call_depth.store(impl.call_depth_offset + static_cast<uint32_t>(impl.call_return_stack.size()));
    }

    void ResetThreadObservationState(Session::Impl &impl)
    {
        impl.observation_state = ObservationState::Idle;
        impl.stop_requested.store(false);
        impl.last_rip = 0;
        impl.last_rip_valid = false;
        impl.waiting_for_trigger = false;
        impl.root_stop_armed = false;
        impl.root_call_depth_base = impl.call_depth_offset;
        impl.pending_block = {};
        impl.tf_probe_source = 0;
        impl.tf_probe_decode = {};
        impl.pending_resume_entry = 0;
        impl.pending_target_override = 0;
        impl.pending_resume_entry_valid = false;
        impl.pending_target_override_valid = false;
        impl.pending_async_handoff_target = 0;
        impl.pending_async_handoff_event = {};
        impl.pending_async_handoff_valid = false;
        ResetActiveProbeSession(impl);
        impl.async_handoff_entry = 0;
        impl.async_handoff_depth = 0;
        impl.async_handoff_handle_value = 0;
        impl.async_handoff_thread_id = 0;
        impl.suppressed_transition_streak = 0;
        impl.suppressed_hot_anchor = 0;
        impl.suppressed_frame_depth = 0;
        impl.hot_bypass_resume = 0;
        impl.hot_bypass_return = 0;
        impl.hot_bypass_call_depth = 0;
        impl.hot_bypass_root_stop = false;
        std::memset(impl.observed_addresses, 0, sizeof(impl.observed_addresses));
        impl.observed_count = 0;
        impl.call_return_stack.clear();
        impl.enhanced_sampling_stack.clear();
        impl.trap_suppression_stack.clear();
        {
            std::lock_guard<std::mutex> lock(impl.detached_thread_lock);
            impl.detached_thread_ids.clear();
        }
        RefreshCurrentCallDepth(impl);
    }
}
