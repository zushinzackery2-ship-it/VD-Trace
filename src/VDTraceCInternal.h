#ifndef VDTRACE_C_INTERNAL_H
#define VDTRACE_C_INTERNAL_H

#include "VDTraceInternal.h"

namespace vdtrace::c_detail
{
    struct CSessionHandle
    {
        vdtrace::Session session;
        VDTRACE_STEP_CALLBACK callback = nullptr;
        void *callback_context = nullptr;
    };

    void CopyMemorySampleToC(const vdtrace::MemorySample &source, VDTRACE_MEMORY_SAMPLE &destination);
    void CopyMemorySampleFromC(const VDTRACE_MEMORY_SAMPLE &source, vdtrace::MemorySample &destination);
    void CopyProbeCaptureToC(const vdtrace::ProbeCapture &source, VDTRACE_PROBE_CAPTURE &destination);
    void CopyProbeCaptureFromC(const VDTRACE_PROBE_CAPTURE &source, vdtrace::ProbeCapture &destination);
    void CCallbackBridge(const vdtrace::StepEvent &event, void *context);
}

#endif
