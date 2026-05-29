#ifndef VDTRACE_HARDWARE_TRANSITION_INTERNAL_H
#define VDTRACE_HARDWARE_TRANSITION_INTERNAL_H

#include "core/runtime/VDTraceInternal.h"

namespace vdtrace::hardware_transition_detail
{
    bool ShouldAutoStopOnRootReturn(const Session::Impl &impl, EventKind kind, uint32_t call_depth);
    void ResetSuppressedTransitionState(Session::Impl &impl);
    void ResetHotBypassState(Session::Impl &impl);
    uintptr_t ResolveHotBypassResumeAddress(const Session::Impl &impl, uintptr_t repeated_target);
    bool ShouldEmitTransition(Session::Impl &impl, uintptr_t source, uintptr_t target, EventKind kind);
    void PushCallFrame(Session::Impl &impl, uintptr_t return_address, const EnhancedSamplingFrame &sample_frame);
    void TryPopCallFrame(Session::Impl &impl, uintptr_t resume_address);
    void UpdateCallStackForTransition(Session::Impl &impl, uintptr_t next_entry, const EnhancedSamplingFrame &sample_frame);
    bool TryArmHotReturnBypass(Session::Impl &impl, uintptr_t repeated_target, uint32_t call_depth);
}

#endif
