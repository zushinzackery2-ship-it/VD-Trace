#ifndef VDTRACE_SESSION_SMOKE_SUPPORT_H
#define VDTRACE_SESSION_SMOKE_SUPPORT_H

#include "VDTrace/VDTrace.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace session_smoke
{
    struct TraceCaseResult
    {
        std::string log_text;
        std::string static_refs_json_text;
        std::wstring state_text;
        size_t pending_write_bytes = 0;
        uint64_t pending_write_events = 0;
        uint64_t written_events = 0;
        uint64_t dropped_events_total = 0;
        uint64_t dropped_write_events = 0;
        bool auto_stopped = false;
        DWORD main_thread_id = 0;
        DWORD worker_thread_id = 0;
        DWORD second_worker_thread_id = 0;
    };

    struct TraceRunOptions
    {
        void (*routine)() = nullptr;
        bool trace_outside_modules = false;
        bool control_flow_only = true;
        uint32_t max_call_depth = 0;
        vdtrace::FlowHitPolicy hit_policy = vdtrace::FlowHitPolicy::EveryHit;
        uint64_t max_events = 0;
        bool stop_on_root_return = true;
        bool auto_select_thread = false;
        bool block_main_thread = false;
        bool queue_trigger_threads = false;
        bool async_thread_handoff = false;
        uint32_t hot_bypass_threshold = 32;
        bool use_trigger = true;
        std::wstring probe_spec;
        std::wstring depth_filter_spec;
    };

    [[noreturn]] inline void Fail(const char *message)
    {
        std::printf("[fail] %s\n", message);
        std::fflush(stdout);
        std::exit(1);
    }

    inline void Require(bool condition, const char *message)
    {
        if (!condition)
        {
            Fail(message);
        }
    }

    void InitializeEnvironment();
    void ShutdownEnvironment();

    void SameLevelEntry();
    void RepeatEntry();
    void CrossModuleHelperEntry();
    void CrossModuleSystemEntry();
    void AllEventsEntry();
    void HeapExtendEntry();
    void StaticWindowSampleEntry();
    void StaticRefEntry();
    void ProbeObservedEntry();
    void AnonymousExecEntry();
    void HotLoopEntry();
    void SceneHotLoopEntry();
    void UnityWorkerAssetEntry();
    void AsyncSpawnEntry();
    void DispatchUnityWorkerTask(size_t index, void (*routine)());
    void WaitForUnityWorkerTask(size_t index);
    DWORD UnityWorkerThreadId(size_t index);
    DWORD AsyncSpawnedThreadId();
    uintptr_t ProbeObservedMidpoint();
    uintptr_t ProbeBytesAddress();
    uintptr_t AnonymousHeapBytesAddress();
    uintptr_t AnonymousExecCodeAddress();
    void WaitForAsyncWorkerDone();

    TraceCaseResult RunTraceCase(const std::wstring &log_name, const TraceRunOptions &options);
    TraceCaseResult RunDelayedTraceCase(const std::wstring &log_name, const TraceRunOptions &options, DWORD start_delay_ms = 80);
    TraceCaseResult RunDelayedAutoThreadCaptureCase(const std::wstring &log_name, void (*routine)());
    TraceCaseResult RunUnityWorkerTraceCase(const std::wstring &log_name, const TraceRunOptions &options, size_t worker_index = 1);
    TraceCaseResult RunUnityWorkerCaptureCase(const std::wstring &log_name);
    TraceCaseResult RunUnityWorkerQueueRotationCase(const std::wstring &log_name, const TraceRunOptions &options);
    TraceCaseResult RunMainThreadCompetitionCase(const std::wstring &log_name, const TraceRunOptions &options);

    uint64_t ParseStateCounter(const std::wstring &state_text, const std::wstring &key);
    std::string Lowercase(std::string text);
    size_t CountLines(const std::string &text);
}

#endif
