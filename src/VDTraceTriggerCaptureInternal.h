#ifndef VDTRACE_TRIGGER_CAPTURE_INTERNAL_H
#define VDTRACE_TRIGGER_CAPTURE_INTERNAL_H

#include "VDTraceInternal.h"

namespace vdtrace::trigger_capture_detail
{
    bool GetThreadCreationTime(HANDLE thread_handle, FILETIME &creation_time);
    HANDLE OpenTracingThreadHandle(DWORD thread_id);
    TriggerCaptureThread *FindTriggerCaptureThreadLocked(Session::Impl &impl, DWORD thread_id);
    bool HasTriggerCaptureThreadLocked(Session::Impl &impl, DWORD thread_id);
    bool QueueTriggerCaptureThreadLocked(Session::Impl &impl, DWORD thread_id, CONTEXT &context);
    bool ShouldKeepRefreshWorkerAlive(const Session::Impl &impl);
    void ReleaseTriggerCaptureThreadsLocked(Session::Impl &impl, bool clear_state, HANDLE preserved_handle);
    bool ArmTriggerCaptureThreads(
        Session::Impl &impl,
        bool only_new_threads,
        uint32_t &candidate_count,
        uint32_t &armed_count,
        std::wstring &error);
    DWORD GetNativeThreadId(std::thread &worker);
}

#endif
