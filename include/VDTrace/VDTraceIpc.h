#ifndef VDTRACE_VDTRACE_IPC_H
#define VDTRACE_VDTRACE_IPC_H

#include "VDTrace.h"

#include <Windows.h>

#include <cstdint>
#include <string>

namespace vdtrace
{
    constexpr uint32_t kIpcVersion = 17;
    constexpr size_t kIpcModuleListCapacity = 512;
    constexpr size_t kIpcOutputPathCapacity = 512;
    constexpr size_t kIpcTriggerPointCapacity = 256;
    constexpr size_t kIpcProbeSpecCapacity = 1024;
    constexpr size_t kIpcDepthFilterSpecCapacity = 2048;
    constexpr size_t kIpcDumpModuleNameCapacity = 260;
    constexpr size_t kIpcDumpOutputDirectoryCapacity = 512;
    constexpr size_t kIpcMemoryAddressCapacity = 256;
    constexpr size_t kIpcMemoryWriteCapacity = 512;
    constexpr size_t kIpcMessageCapacity = 16384;

    enum class IpcCommandType : uint32_t
    {
        Invalid = 0,
        Ping = 1,
        Configure = 2,
        Start = 3,
        Stop = 4,
        Status = 5,
        Shutdown = 6,
        ListModules = 7,
        DumpModule = 8,
        ReadMemory = 9,
        WriteMemory = 10,
    };

    enum IpcStatus : int32_t
    {
        IPC_STATUS_OK = 0,
        IPC_STATUS_INVALID_VERSION = 1,
        IPC_STATUS_INVALID_COMMAND = 2,
        IPC_STATUS_INVALID_ARGUMENT = 3,
        IPC_STATUS_INVALID_STATE = 4,
        IPC_STATUS_INTERNAL_ERROR = 5,
        IPC_STATUS_PIPE_ERROR = 6,
    };

    struct IpcConfigurePayload
    {
        char module_names[kIpcModuleListCapacity] = {};
        char output_path[kIpcOutputPathCapacity] = {};
        char trigger_point[kIpcTriggerPointCapacity] = {};
        char probe_spec[kIpcProbeSpecCapacity] = {};
        char depth_filter_spec[kIpcDepthFilterSpecCapacity] = {};
        int32_t thread_id = 0;
        uint64_t max_events = 0;
        uint32_t trace_outside_modules = 0;
        uint32_t backend = static_cast<uint32_t>(TraceBackend::DrControlFlow);
        uint32_t control_flow_only = 1;
        uint32_t max_call_depth = kUnlimitedCallDepth;
        uint32_t hit_policy = static_cast<uint32_t>(FlowHitPolicy::FirstSeen);
        uint32_t hot_bypass_threshold = 32;
        uint32_t enhanced_sampling = 0;
        uint32_t stop_on_root_return = 0;
        uint32_t async_thread_handoff = 0;
        uint32_t auto_select_thread = 0;
        uint32_t block_main_thread = 0;
        uint32_t queue_trigger_threads = 0;
    };

    struct IpcListModulesPayload
    {
        uint32_t include_system_modules = 1;
    };

    struct IpcDumpModulePayload
    {
        char module_name[kIpcDumpModuleNameCapacity] = {};
        char output_directory[kIpcDumpOutputDirectoryCapacity] = {};
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;
    };

    struct IpcReadMemoryPayload
    {
        char address_text[kIpcMemoryAddressCapacity] = {};
        uint32_t size = 0;
        uint32_t reserved = 0;
    };

    struct IpcWriteMemoryPayload
    {
        char address_text[kIpcMemoryAddressCapacity] = {};
        uint32_t size = 0;
        uint8_t bytes[kIpcMemoryWriteCapacity] = {};
    };

    struct IpcCommand
    {
        uint32_t version = kIpcVersion;
        IpcCommandType type = IpcCommandType::Invalid;
        IpcConfigurePayload configure = {};
        IpcListModulesPayload list_modules = {};
        IpcDumpModulePayload dump_module = {};
        IpcReadMemoryPayload read_memory = {};
        IpcWriteMemoryPayload write_memory = {};
    };

    struct IpcResponse
    {
        uint32_t version = kIpcVersion;
        int32_t status = IPC_STATUS_OK;
        uint32_t reserved = 0;
        char message[kIpcMessageCapacity] = {};
    };

    std::wstring BuildPipeName(DWORD pid);
    void SetIpcMessage(IpcResponse &response, const std::string &message);
}

#endif
