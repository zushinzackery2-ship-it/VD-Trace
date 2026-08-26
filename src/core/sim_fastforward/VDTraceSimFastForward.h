#ifndef VDTRACE_SIM_FASTFORWARD_H
#define VDTRACE_SIM_FASTFORWARD_H

#include "core/runtime/VDTraceInternal.h"

namespace vdtrace::sim_fastforward
{
    // Simulated fast-forward for the DR hardware-flow backend.
    //
    // Given the entry the hardware backend is about to arm, this walks forward across
    // deterministic unconditional-jump edges (targets that are fully known from the
    // instruction encoding, or resolvable from a pure register computation), emits the
    // predicted jump events, and returns the first "frontier" entry whose successor is
    // not statically certain. The caller then arms real hardware breakpoints on that
    // frontier as usual, so the CPU still executes every skipped instruction for real -
    // only the redundant per-jump single-step exceptions are avoided.
    //
    // When the mode is disabled, nothing is skippable, or a cycle/budget limit is hit,
    // the original entry is returned unchanged and no events are emitted.
    uintptr_t FastForwardDeterministicFlow(Session::Impl &impl, uintptr_t entry, CONTEXT *context);
}

#endif
