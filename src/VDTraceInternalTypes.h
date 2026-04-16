#ifndef VDTRACE_INTERNAL_TYPES_H
#define VDTRACE_INTERNAL_TYPES_H

#include "VDTraceRecorderTypes.h"
#include "VDTrace/VDTrace.h"
#include "VDTrace/VDTraceC.h"

namespace vdtrace
{
    enum class BackendMode : uint32_t
    {
        HardwareFlow = 0,
        TrapFlagContext = 1,
    };

    enum class ObservationState : uint32_t
    {
        Idle = 0,
        WaitingForDestination = 1,
        WaitingForTail = 2,
        WaitingForSingleStep = 3,
        LinearScan = 4,
        WaitingForHotReturn = 5,
    };

    struct FlowEdgeKey
    {
        uintptr_t instruction = 0;
        uintptr_t target = 0;
        EventKind kind = EventKind::Unknown;

        bool operator==(const FlowEdgeKey &other) const
        {
            return instruction == other.instruction
                && target == other.target
                && kind == other.kind;
        }
    };

    struct FlowEdgeKeyHash
    {
        size_t operator()(const FlowEdgeKey &key) const
        {
            size_t value = std::hash<uintptr_t> {}(key.instruction);
            value ^= std::hash<uintptr_t> {}(key.target) + 0x9e3779b9 + (value << 6) + (value >> 2);
            value ^= std::hash<uint32_t> {}(static_cast<uint32_t>(key.kind)) + 0x9e3779b9 + (value << 6) + (value >> 2);
            return value;
        }
    };

    struct InstructionDecodeResult
    {
        EventKind kind = EventKind::Unknown;
        uintptr_t target = 0;
        bool has_target = false;
        uint8_t size = 0;
        uint8_t bytes[16] = {};
    };

    struct BasicBlockInfo
    {
        uintptr_t entry = 0;
        uintptr_t tail = 0;
        uintptr_t fallthrough = 0;
        InstructionDecodeResult tail_decode = {};
        uint32_t instruction_count = 0;
        bool valid = false;
        bool truncated = false;
        bool emits_edge = false;
    };

    enum class AsyncDispatchKind : uint32_t
    {
        None = 0,
        ThreadStart,
        WorkItem,
        ThreadPool,
        Apc,
    };

    struct ResolvedAsyncProbe
    {
        enum class ThreadHandleSource : uint32_t
        {
            None = 0,
            ReturnValue,
            OutputPointerArgument,
        };

        uintptr_t address = 0;
        AsyncDispatchKind kind = AsyncDispatchKind::None;
        uint8_t argument_count = 0;
        uint8_t argument_indices[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        bool argument_is_pointer[4] = {false, false, false, false};
        std::wstring argument_names[4];
        ThreadHandleSource thread_handle_source = ThreadHandleSource::None;
        uint8_t thread_handle_argument_index = 0xFF;
        uint8_t thread_id_argument_index = 0xFF;
        uint8_t handoff_entry_argument_index = 0xFF;
        std::wstring module_name;
        std::wstring symbol_name;
    };

    struct AddressLabel
    {
        bool valid = false;
        uintptr_t module_base = 0;
        size_t module_size = 0;
        uintptr_t relative = 0;
        std::wstring module_name;
        std::wstring symbol_name;
        uintptr_t symbol_offset = 0;
    };

    struct EnhancedSamplingTrackedArgument
    {
        bool valid = false;
        uint8_t source_index = kEnhancedSampleSourceReturnValue;
        uint8_t size = 0;
        uintptr_t address = 0;
    };

    struct EnhancedSamplingFrame
    {
        bool active = false;
        uint8_t argument_count = 0;
        EnhancedSamplingTrackedArgument arguments[kEnhancedSampleSlotCount] = {};
    };

    enum class ProbeSourceKind : uint8_t
    {
        RegisterValue = 0,
        AbsoluteMemory = 1,
        RegisterMemory = 2,
    };

    enum class ProbeMode : uint8_t
    {
        Capture = 0,
        SingleStep = 1,
        WriteTrace = 2,
        FilterTrace = 3,
    };

    enum class DepthFilterExecutionMode : uint8_t
    {
        Edge = 0,
        TrapFlag = 1,
    };

    enum class ExecutionAddressKind : uint8_t
    {
        Unknown = 0,
        TrackedModule,
        SystemModule,
        OutsideImage,
        AnonymousExecutable,
    };

    enum class ProbeExitKind : uint8_t
    {
        Return = 0,
        LeaveRegion = 1,
        ReturnOrLeaveRegion = 2,
    };

    enum class ProbeRegister : uint8_t
    {
        None = 0,
        Rax,
        Rbx,
        Rcx,
        Rdx,
        Rsi,
        Rdi,
        Rbp,
        Rsp,
        Rip,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15,
    };

    struct ResolvedProbeCapture
    {
        ProbeSourceKind kind = ProbeSourceKind::RegisterValue;
        ProbeRegister reg = ProbeRegister::None;
        uintptr_t absolute_address = 0;
        intptr_t offset = 0;
        uint8_t size = 0;
        wchar_t label[kProbeLabelMaxChars] = {};
    };

    struct ResolvedProbeWatch
    {
        uintptr_t address = 0;
        uint8_t size = 0;
        wchar_t label[kProbeLabelMaxChars] = {};
    };

    struct ResolvedValueProbe
    {
        uintptr_t address = 0;
        ProbeMode mode = ProbeMode::Capture;
        ProbeExitKind exit_kind = ProbeExitKind::ReturnOrLeaveRegion;
        uint32_t step_limit = 0;
        uint8_t capture_count = 0;
        ResolvedProbeCapture captures[kProbeCaptureMaxCount] = {};
        uint8_t watch_count = 0;
        ResolvedProbeWatch watches[kProbeCaptureMaxCount] = {};
    };

    struct ActiveProbeWatchState
    {
        bool valid = false;
        uintptr_t address = 0;
        uint8_t size = 0;
        wchar_t label[kProbeLabelMaxChars] = {};
        uint8_t bytes[kEnhancedSampleMaxBytes] = {};
    };

    struct ActiveProbeSession
    {
        bool active = false;
        bool restore_hardware_backend = false;
        ProbeMode mode = ProbeMode::Capture;
        ProbeExitKind exit_kind = ProbeExitKind::ReturnOrLeaveRegion;
        uint32_t remaining_steps = 0;
        uint32_t start_call_depth = 0;
        uintptr_t pending_entry = 0;
        uintptr_t region_base = 0;
        size_t region_size = 0;
        uint8_t watch_count = 0;
        ActiveProbeWatchState watches[kProbeCaptureMaxCount] = {};
    };

    struct TriggerCaptureThread
    {
        DWORD thread_id = 0;
        HANDLE handle = nullptr;
        bool parked = false;
    };

    struct ResolvedDepthFilterModuleRule
    {
        ModuleRange range = {};
        uint32_t max_call_depth = kUnlimitedCallDepth;
        DepthFilterExecutionMode execution_mode = DepthFilterExecutionMode::Edge;
    };

    struct ResolvedDepthFilterSet
    {
        std::vector<ResolvedDepthFilterModuleRule> module_rules;
        bool has_outside_module_depth = false;
        uint32_t outside_module_depth = kUnlimitedCallDepth;
        DepthFilterExecutionMode outside_module_execution_mode = DepthFilterExecutionMode::Edge;
        bool has_anonymous_exec_depth = false;
        uint32_t anonymous_exec_depth = kUnlimitedCallDepth;
        DepthFilterExecutionMode anonymous_exec_execution_mode = DepthFilterExecutionMode::Edge;
    };

    struct ResolvedExecutionAddress
    {
        ExecutionAddressKind kind = ExecutionAddressKind::Unknown;
        const ModuleRange *module_range = nullptr;
        const ResolvedDepthFilterModuleRule *depth_rule = nullptr;
        size_t region_size = 0;
    };

}

#endif
