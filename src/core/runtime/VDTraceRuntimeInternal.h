#ifndef VDTRACE_RUNTIME_INTERNAL_H
#define VDTRACE_RUNTIME_INTERNAL_H

#include "core/runtime/VDTraceInternal.h"

namespace vdtrace::runtime_detail
{
    void ActivateRuntimeSession(Session::Impl *impl);
    void DeactivateRuntimeSession(Session::Impl *impl);
    void ResetRecentStopState();
    void MarkRecentStopThread(DWORD thread_id, ULONGLONG deadline);
    void ResetExecutionState(Session::Impl &impl);
    void ArmRootStop(Session::Impl &impl);
    LONG CALLBACK GlobalVectoredHandler(EXCEPTION_POINTERS *info);
}

#endif
