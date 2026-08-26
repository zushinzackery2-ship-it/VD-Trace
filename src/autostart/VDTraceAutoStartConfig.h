#ifndef VDTRACE_AUTOSTART_CONFIG_H
#define VDTRACE_AUTOSTART_CONFIG_H

#include "VDTrace/VDTrace.h"

#include <filesystem>
#include <string>

namespace vdtrace::autostart
{
    enum class WaitMode : uint32_t
    {
        Disabled = 0,
        BepInExIl2CppSceneChange = 1,
    };

    enum class TraceExecutionMode : uint32_t
    {
        Edge = 0,
        TrapFlag = 1,
    };

    struct TraceConfig
    {
        std::wstring agent_path;
        std::wstring modules;
        std::wstring output_path;
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
        uint32_t hot_bypass_threshold = 0;
        bool enhanced_sampling = false;
        bool trigger_enabled = true;
        bool stop_on_root_return = false;
        bool async_thread_handoff = true;
    };

    struct LaunchConfig
    {
        std::wstring game_path;
        std::wstring working_directory;
        std::wstring arguments;
        std::wstring helper_path;
        DWORD loader_timeout_ms = 60000;
        DWORD agent_timeout_ms = 30000;
        DWORD trace_start_timeout_ms = 120000;
        DWORD trace_finish_timeout_ms = 1800000;
        bool wait_for_trace_end = true;
    };

    struct WaitConfig
    {
        WaitMode mode = WaitMode::BepInExIl2CppSceneChange;
        std::wstring module_name = L"GameAssembly.dll";
        std::string invoke_export = "il2cpp_runtime_invoke";
        std::string method_name_export = "il2cpp_method_get_name";
        std::string target_method_name = "Internal_ActiveSceneChanged";
        DWORD module_poll_interval_ms = 50;
        DWORD wait_timeout_ms = 600000;
    };

    struct AutoStartConfig
    {
        std::filesystem::path config_path;
        std::filesystem::path config_directory;
        LaunchConfig launch = {};
        WaitConfig wait = {};
        TraceConfig trace = {};
    };

    std::filesystem::path DefaultConfigPath();
    bool LoadConfig(const std::filesystem::path &path, AutoStartConfig &config, std::wstring &error);
}

#endif
