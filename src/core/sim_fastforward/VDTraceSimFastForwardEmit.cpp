#include "pch.h"
#include "core/sim_fastforward/VDTraceSimFastForwardInternal.h"
#include "core/hardware/VDTraceHardwareTransitionInternal.h"

#include <cstring>

namespace vdtrace::sim_fastforward::detail
{
    void EmitPredictedJump(Session::Impl &impl, const PredictedJump &plan, uint32_t call_depth)
    {
        // Route through the shared hit-policy / outside-suppression gate so predicted
        // edges dedup identically to real DR transitions.
        if (!hardware_transition_detail::ShouldEmitTransition(impl, plan.tail, plan.target, EventKind::Jump))
        {
            return;
        }

        StepEvent event = {};
        event.sequence = impl.event_count.fetch_add(1) + 1;
        event.thread_id = GetCurrentThreadId();
        event.instruction = plan.tail;
        event.stack_pointer = 0;
        event.block_begin = plan.block_entry;
        event.block_end = plan.fallthrough;
        event.target = plan.target;
        event.call_depth = call_depth;
        event.kind = EventKind::Jump;
        event.minimal_record = false;
        event.instruction_size = plan.tail_decode.size;
        std::memcpy(event.instruction_bytes, plan.tail_decode.bytes, sizeof(event.instruction_bytes));
        event.has_target = true;

        // The predicted edge is resolved before the CPU reaches it, so no live register
        // snapshot is captured; thread_context stays invalid and the recorder simply
        // skips extended per-block memory analysis for this event.
        if (const auto *range = FindModuleRange(impl.module_ranges, plan.tail); range != nullptr)
        {
            event.inside_module = true;
            event.module_base = range->base;
            event.module_size = range->size;
            event.relative_instruction = plan.tail - range->base;
            if (!impl.lightweight_module_capture)
            {
                event.module_name = range->name;
            }
        }

        if (impl.options.callback != nullptr)
        {
            impl.options.callback(event, impl.options.callback_context);
        }

        hardware_transition_detail::ResetSuppressedTransitionState(impl);
    }
}
