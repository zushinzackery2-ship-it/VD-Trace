#include "pch.h"
#include "core/sim_fastforward/VDTraceSimFastForwardInternal.h"

namespace vdtrace::sim_fastforward
{
    namespace
    {
        bool ReachedEventBudget(const Session::Impl &impl, size_t planned_count)
        {
            if (impl.options.max_events == 0)
            {
                return false;
            }

            return impl.event_count.load() + planned_count >= impl.options.max_events;
        }

        void SeedRegisterTracking(Session::Impl &impl, CONTEXT *context, detail::WalkState &state)
        {
            state.track_registers = impl.options.sim_fast_forward_indirect && context != nullptr;
            if (!state.track_registers)
            {
                return;
            }

            ThreadContextSnapshot snapshot = {};
            CaptureThreadContext(*context, snapshot);
            extender::detail::InitializeContext(snapshot, state.sim);
        }
    }

    uintptr_t FastForwardDeterministicFlow(Session::Impl &impl, uintptr_t entry, CONTEXT *context)
    {
        if (!impl.options.sim_fast_forward
            || impl.backend_mode != BackendMode::HardwareFlow
            || impl.active_probe.active
            || entry == 0)
        {
            return entry;
        }

        detail::WalkState state;
        SeedRegisterTracking(impl, context, state);

        const uint32_t call_depth = impl.current_call_depth.load();
        std::vector<detail::PredictedJump> planned;
        std::unordered_set<uintptr_t> visited;
        uintptr_t cursor = entry;
        bool cycle = false;

        while (planned.size() < detail::kMaxFastForwardBlocks)
        {
            if (ReachedEventBudget(impl, planned.size()))
            {
                break;
            }

            if (!visited.insert(cursor).second)
            {
                // A back-edge inside the deterministic window: abandon the pass so the
                // baseline single-edge arming handles the loop (and hot_bypass can act).
                cycle = true;
                break;
            }

            detail::PredictedJump plan = {};
            if (!detail::TryPlanSkippableJump(impl, cursor, state, plan))
            {
                break;
            }

            planned.push_back(plan);
            cursor = plan.target;
        }

        if (cycle || planned.empty())
        {
            return entry;
        }

        for (const detail::PredictedJump &plan : planned)
        {
            detail::EmitPredictedJump(impl, plan, call_depth);
        }

        return cursor;
    }
}
