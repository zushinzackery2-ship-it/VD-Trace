#ifndef VDTRACE_SESSION_SMOKE_CLI_H
#define VDTRACE_SESSION_SMOKE_CLI_H

#include "session_smoke_support.h"

#include <set>
#include <string>

namespace session_smoke
{
    struct SessionSmokeSelection
    {
        bool list_cases = false;
        bool full_core = false;
        uint32_t rounds = 1;
        std::set<std::string> cases;
        std::set<std::string> stability_cases;
    };

    struct SessionSmokeConfig
    {
        TraceRunOptions outside_depth_options;
        TraceRunOptions outside_tf_options;
        TraceRunOptions module_depth_options;
        TraceRunOptions module_tf_options;
        TraceRunOptions anonymous_depth_options;
        TraceRunOptions anonymous_tf_options;
        TraceRunOptions probe_options;
        TraceRunOptions delayed_all_events_options;
        TraceRunOptions unity_follow_options;
        TraceRunOptions probe_queue_options;
    };

    SessionSmokeSelection ParseSessionSmokeSelection(int argc, wchar_t **argv);
    void PrintSessionSmokeCases();
    bool ShouldRunCase(const SessionSmokeSelection &selection, const char *name);
    bool ShouldRunStabilityCase(const SessionSmokeSelection &selection, const char *name);
    const char *StabilityGroupCaseName();
    void AnnounceSessionSmokeCase(const char *name);
    std::wstring RoundLog(const wchar_t *prefix, int round);
    TraceRunOptions MakeSessionSmokeOptions(
        void (*routine)(),
        bool trace_outside_modules,
        bool control_flow_only,
        uint32_t max_call_depth,
        vdtrace::FlowHitPolicy hit_policy,
        uint64_t max_events,
        bool stop_on_root_return = true,
        bool auto_select_thread = false,
        bool queue_trigger_threads = false,
        bool async_thread_handoff = false,
        bool block_main_thread = false);
    SessionSmokeConfig BuildSessionSmokeConfig();
    void RunSessionSmokeTraceBasicCases(const SessionSmokeSelection &selection, const SessionSmokeConfig &config);
    void RunSessionSmokeTraceFeatureCases(const SessionSmokeSelection &selection, const SessionSmokeConfig &config);
    void RunSessionSmokeRecorderCases(const SessionSmokeSelection &selection);
    void RunSessionSmokeThreadCases(const SessionSmokeSelection &selection, const SessionSmokeConfig &config);
}

#endif
