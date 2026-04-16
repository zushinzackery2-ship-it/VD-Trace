#ifndef VDTRACE_CONTROL_SUPPORT_H
#define VDTRACE_CONTROL_SUPPORT_H

#include "VDTrace/VDTraceIpc.h"

namespace vdtrace::tools
{
    struct CommandResult
    {
        bool success = false;
        int32_t status = IPC_STATUS_INTERNAL_ERROR;
        std::wstring message;
    };

    std::wstring GetExecutableDirectory();
    std::wstring BuildDefaultAgentPath();
    std::wstring BuildDefaultOutputPath(DWORD pid);
    std::wstring BuildDefaultDumpOutputDirectory();
    bool ResolveFullPath(const std::wstring &input, std::wstring &output);
    bool GuessMainThreadId(DWORD pid, DWORD &thread_id, std::wstring &error);
    bool WaitForPipeReady(DWORD pid, DWORD timeout_ms);
    CommandResult Configure(
        DWORD pid,
        DWORD thread_id,
        const std::wstring &modules,
        const std::wstring &output_path,
        uint64_t max_events,
        bool trace_outside_modules,
        TraceBackend backend,
        bool control_flow_only,
        uint32_t max_call_depth,
        const std::wstring &depth_filter_spec,
        FlowHitPolicy hit_policy,
        uint32_t hot_bypass_threshold,
        bool enhanced_sampling,
        bool auto_select_thread,
        bool block_main_thread,
        bool queue_trigger_threads,
        const std::wstring &trigger_point,
        const std::wstring &probe_spec,
        bool stop_on_root_return,
        bool async_thread_handoff,
        DWORD timeout_ms);
    CommandResult ListModules(DWORD pid, bool include_system_modules, DWORD timeout_ms);
    CommandResult DumpModule(DWORD pid, const std::wstring &module_name, const std::wstring &output_directory, DWORD timeout_ms);
    CommandResult ReadMemory(DWORD pid, const std::wstring &address_text, uint32_t size, DWORD timeout_ms);
    CommandResult WriteMemory(DWORD pid, const std::wstring &address_text, const uint8_t *bytes, uint32_t size, DWORD timeout_ms);
    CommandResult Start(DWORD pid, DWORD timeout_ms);
    CommandResult Stop(DWORD pid, DWORD timeout_ms);
    CommandResult Status(DWORD pid, DWORD timeout_ms);
    CommandResult SendCommand(DWORD pid, const IpcCommand &command, DWORD timeout_ms);
    CommandResult InjectAgent(DWORD pid, const std::wstring &dll_path, DWORD timeout_ms);
    CommandResult InjectBootstrapDll(DWORD pid, const std::wstring &dll_path);
}

#endif
