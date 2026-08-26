#ifndef VDTRACE_SIM_FASTFORWARD_INTERNAL_H
#define VDTRACE_SIM_FASTFORWARD_INTERNAL_H

#include "core/sim_fastforward/VDTraceSimFastForward.h"
#include "core/extender/VDTraceExtenderAnalyzeInternal.h"

namespace vdtrace::sim_fastforward::detail
{
    // Upper bound on how many jump edges a single fast-forward pass may collapse.
    // Keeps the synchronous walk inside the exception handler bounded.
    constexpr uint32_t kMaxFastForwardBlocks = 64;

    struct PredictedJump
    {
        uintptr_t block_entry = 0;
        uintptr_t tail = 0;
        uintptr_t fallthrough = 0;
        uintptr_t target = 0;
        InstructionDecodeResult tail_decode = {};
    };

    struct WalkState
    {
        // Register tracking is only enabled when indirect resolution is requested and a
        // live CONTEXT is available to seed the simulated registers.
        bool track_registers = false;

        // Cleared permanently once the scanned window consumes a real memory read or an
        // instruction the effect model cannot follow, after which a register-indirect
        // target can no longer be proven equal to what the CPU will compute.
        bool registers_pure = true;

        extender::detail::SimContext sim = {};
        std::vector<extender::detail::SimMemoryCell> overlay;
    };

    bool TryPlanSkippableJump(Session::Impl &impl, uintptr_t entry, WalkState &state, PredictedJump &plan);
    void EmitPredictedJump(Session::Impl &impl, const PredictedJump &plan, uint32_t call_depth);
}

#endif
