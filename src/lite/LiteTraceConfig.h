#ifndef VDTRACE_LITE_CONFIG_H
#define VDTRACE_LITE_CONFIG_H

#include "VDTrace/VDTrace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace vdtrace::lite
{
    enum class TraceExecutionMode : uint32_t
    {
        Edge = 0,
        TrapFlag = 1,
    };

    struct LiteTraceConfig
    {
        std::filesystem::path config_path;
        std::filesystem::path config_directory;

        // [trace] - ported from the VDTraceAutoStart [trace] section / vdtrace::Options.
        std::wstring modules;
        std::wstring output_path = L".\\traces\\LiteTrace.log";
        std::wstring trigger_point;
        std::wstring probe_spec;
        DWORD thread_id = 0;
        bool auto_select_thread = true;
        bool block_main_thread = false;
        uint64_t max_events = 0;
        bool trace_outside_modules = false;
        TraceBackend backend = TraceBackend::DrControlFlow;
        bool control_flow_only = true;
        uint32_t max_call_depth = kUnlimitedCallDepth;
        bool has_outside_module_depth = false;
        uint32_t outside_module_depth = kUnlimitedCallDepth;
        TraceExecutionMode outside_module_execution_mode = TraceExecutionMode::Edge;
        bool has_anonymous_exec_depth = false;
        uint32_t anonymous_exec_depth = kUnlimitedCallDepth;
        TraceExecutionMode anonymous_exec_execution_mode = TraceExecutionMode::Edge;
        std::wstring module_call_depths;
        FlowHitPolicy hit_policy = FlowHitPolicy::FirstSeen;
        uint32_t hot_bypass_threshold = 32;
        bool sim_fast_forward = false;
        bool sim_fast_forward_indirect = false;
        bool enhanced_sampling = false;
        bool trigger_enabled = true;
        bool stop_on_root_return = false;
        bool async_thread_handoff = true;

        // [lite] - LiteTrace specific runtime behaviour.
        bool exit_process_on_finish = false;
        DWORD finish_timeout_ms = 0; // 0 waits indefinitely for the trace to finish.
        DWORD poll_interval_ms = 50;
    };

    std::filesystem::path LiteTraceModuleDirectory();
    std::filesystem::path DefaultLiteTraceConfigPath();
    std::string BuildDefaultLiteTraceConfigText();
    bool LoadLiteTraceConfig(const std::filesystem::path &path, LiteTraceConfig &config, std::wstring &error);

    std::vector<std::wstring> SplitModuleNames(const std::wstring &text);
    bool ParseLiteTriggerPoint(const std::wstring &text, std::wstring &module_name, uintptr_t &relative_address, std::wstring &error);
    std::wstring BuildLiteDepthFilterSpec(const LiteTraceConfig &config);
    std::wstring ResolveLiteOutputPath(const LiteTraceConfig &config);
}

#endif
