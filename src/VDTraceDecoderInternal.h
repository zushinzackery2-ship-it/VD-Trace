#ifndef VDTRACE_DECODER_INTERNAL_H
#define VDTRACE_DECODER_INTERNAL_H

#include "VDTraceInternal.h"

namespace vdtrace::decoder_detail
{
    bool TryConsumeDetachedThreadSingleStep(Session::Impl &impl, EXCEPTION_POINTERS *info);
    bool ShouldAutoStopOnRootReturn(const Session::Impl &impl, EventKind kind, uint32_t call_depth);
    void PushTrapFlagCallFrame(Session::Impl &impl, uintptr_t return_address, bool suppress_callee, const EnhancedSamplingFrame &sample_frame);
    void TryPopTrapFlagCallFrame(Session::Impl &impl, uintptr_t resume_address);
    bool TrapFlagFrameSuppressed(const Session::Impl &impl);
    bool ShouldSuppressTrapFlagCall(const Session::Impl &impl, uint32_t call_depth, uintptr_t target);
    bool ShouldEmitTrapFlagEvent(const Session::Impl &impl, const InstructionDecodeResult &decode_result);
    void UpdateTrapFlagCallStack(
        Session::Impl &impl,
        const InstructionDecodeResult &decode_result,
        uintptr_t executed_rip,
        uintptr_t current_rip,
        bool suppress_callee,
        const EnhancedSamplingFrame &sample_frame);
}

#endif
