#ifndef VDTRACE_INTERNAL_H
#define VDTRACE_INTERNAL_H

#include "VDTraceInternalTypes.h"
#include "VDTraceRecorderInternal.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace vdtrace
{
    class TextFileRecorderHeapPeek;

    struct Session::Impl
    {
        bool Configure(const Options &options, std::wstring &error);
        bool Start(std::wstring &error);
        bool Stop(std::wstring &error);

        bool IsConfigured() const;
        bool IsRunning() const;
        uint64_t EventCount() const;
        std::wstring DescribeState() const;
        std::vector<ModuleRange> ModuleRanges() const;
        LONG HandleException(EXCEPTION_POINTERS *info);

        Options options = {};
        std::vector<ModuleRange> module_ranges;
        std::vector<ModuleRange> system_module_ranges;
        ResolvedDepthFilterSet depth_filters = {};
        std::atomic<bool> configured = false;
        std::atomic<bool> running = false;
        std::atomic<bool> stop_requested = false;
        BackendMode backend_mode = BackendMode::HardwareFlow;
        ObservationState observation_state = ObservationState::Idle;
        std::atomic<uint64_t> step_count = 0;
        std::atomic<uint64_t> event_count = 0;
        std::atomic<uint64_t> retained_step_count = 0;
        std::atomic<uint64_t> retained_event_count = 0;
        std::atomic<uint64_t> retained_duplicate_edge_suppressed_count = 0;
        std::atomic<uint64_t> retained_outside_suppressed_count = 0;
        std::atomic<uint64_t> retained_hot_bypass_entry_count = 0;
        std::atomic<uint32_t> retained_call_depth = 0;
        std::atomic<uint64_t> retained_trigger_capture_hit_count = 0;
        std::atomic<DWORD> retained_trigger_capture_last_thread_id = 0;
        std::atomic<DWORD> active_thread_id = 0;
        std::atomic<DWORD> main_thread_id = 0;
        std::atomic<uint32_t> current_call_depth = 0;
        std::atomic<uint64_t> duplicate_edge_suppressed_count = 0;
        std::atomic<uint64_t> outside_suppressed_count = 0;
        std::atomic<uint64_t> hot_bypass_entry_count = 0;
        std::atomic<uint64_t> trigger_capture_hit_count = 0;
        std::atomic<DWORD> trigger_capture_last_thread_id = 0;
        std::atomic<uint32_t> trigger_capture_waiting_count = 0;
        std::unordered_set<FlowEdgeKey, FlowEdgeKeyHash> seen_edges;
        uintptr_t configured_trigger_address = 0;
        uintptr_t resolved_trigger_address = 0;
        uint32_t call_depth_offset = 0;
        uint32_t root_call_depth_base = 0;
        uintptr_t last_rip = 0;
        bool last_rip_valid = false;
        bool waiting_for_trigger = false;
        bool root_stop_armed = false;
        bool lightweight_module_capture = false;
        std::vector<ResolvedAsyncProbe> async_probes;
        std::vector<ResolvedValueProbe> value_probes;
        ActiveProbeSession active_probe = {};
        BasicBlockInfo pending_block = {};
        uintptr_t tf_probe_source = 0;
        InstructionDecodeResult tf_probe_decode = {};
        uintptr_t pending_resume_entry = 0;
        uintptr_t pending_target_override = 0;
        bool pending_resume_entry_valid = false;
        bool pending_target_override_valid = false;
        uintptr_t pending_async_handoff_target = 0;
        StepEvent pending_async_handoff_event = {};
        bool pending_async_handoff_valid = false;
        uintptr_t observed_addresses[4] = {};
        uint32_t observed_count = 0;
        uint32_t suppressed_transition_streak = 0;
        uintptr_t suppressed_hot_anchor = 0;
        uint32_t suppressed_frame_depth = 0;
        uintptr_t hot_bypass_resume = 0;
        uintptr_t hot_bypass_return = 0;
        uint32_t hot_bypass_call_depth = 0;
        bool hot_bypass_root_stop = false;
        std::vector<uintptr_t> call_return_stack;
        std::vector<EnhancedSamplingFrame> enhanced_sampling_stack;
        std::vector<uint8_t> trap_suppression_stack;
        std::mutex known_thread_lock;
        std::unordered_set<DWORD> known_thread_ids;
        std::mutex detached_thread_lock;
        std::unordered_set<DWORD> detached_thread_ids;
        std::vector<TriggerCaptureThread> trigger_capture_threads;
        std::mutex trigger_capture_lock;
        std::thread trigger_capture_worker;
        bool trigger_capture_worker_stop = false;
        std::mutex async_handoff_lock;
        std::condition_variable async_handoff_cv;
        std::thread async_handoff_worker;
        bool async_handoff_worker_stop = false;
        bool async_handoff_request_pending = false;
        uintptr_t async_handoff_entry = 0;
        uint32_t async_handoff_depth = 0;
        uintptr_t async_handoff_handle_value = 0;
        DWORD async_handoff_thread_id = 0;
        HANDLE thread_handle = nullptr;
        PVOID veh_handle = nullptr;
        mutable std::mutex state_lock;
    };

    std::wstring FormatWin32Error(const wchar_t *prefix, DWORD error);
    bool ResolveTriggerAddress(const Options &options, uintptr_t &resolved_address, std::wstring &error);
    bool ParseProbeSpec(const std::wstring &text, std::vector<ResolvedValueProbe> &probes, std::wstring &error);
    bool ParseDepthFilterSpec(
        const std::wstring &text,
        const std::vector<ModuleRange> &tracked_modules,
        const std::vector<ModuleRange> &system_modules,
        ResolvedDepthFilterSet &filters,
        std::wstring &error);
    size_t DetermineSeenEdgeReserve(const Options &options);
    bool ResolveModuleRange(const std::wstring &module_name, ModuleRange &range, std::wstring &error);
    void EnumerateSystemModuleRanges(std::vector<ModuleRange> &ranges);
    bool SetSingleStepFlag(HANDLE thread_handle, bool enabled, std::wstring &error);
    bool ClearThreadTraceState(HANDLE thread_handle, std::wstring &error);
    bool ReadThreadInstructionPointer(HANDLE thread_handle, uintptr_t &rip, std::wstring &error);
    bool ReadThreadControlState(HANDLE thread_handle, uintptr_t &rip, uintptr_t &rsp, std::wstring &error);
    bool ArmHardwareExecution(HANDLE thread_handle, const uintptr_t *addresses, uint32_t count, std::wstring &error);
    bool ArmSingleStep(HANDLE thread_handle, std::wstring &error);
    bool GuessCurrentProcessMainThread(DWORD &thread_id, std::wstring &error);
    bool SupportsQueuedTriggerTracing(const Session::Impl &impl);
    void RememberRecentCaptureThreadForStop(DWORD thread_id);
    bool BeginTriggerThreadCapture(Session::Impl &impl, std::wstring &error);
    bool PromoteCapturedTriggerThread(Session::Impl &impl, DWORD thread_id, std::wstring &error);
    LONG HandleQueuedTriggerHitException(Session::Impl &impl, EXCEPTION_POINTERS *info);
    bool RotateQueuedTriggerTrace(Session::Impl &impl, CONTEXT *current_context, std::wstring &error);
    void ReleaseTriggerCaptureThreads(Session::Impl &impl, bool clear_state, HANDLE preserved_handle = nullptr);
    void StartTriggerCaptureRefreshWorker(Session::Impl &impl);
    void StopTriggerCaptureRefreshWorker(Session::Impl &impl);
    bool TryFindNewProcessThread(Session::Impl &impl, DWORD &thread_id);
    bool ActivateAsyncThreadById(Session::Impl &impl, DWORD thread_id, uintptr_t entry, uint32_t depth, std::wstring &error);
    bool ActivateAsyncThreadByHandle(Session::Impl &impl, HANDLE trace_handle, uintptr_t entry, uint32_t depth, std::wstring &error);
    bool TryFindInsideReturnAddress(const Session::Impl &impl, uintptr_t stack_pointer, uintptr_t &address);
    bool AnalyzeBasicBlock(uintptr_t entry, BasicBlockInfo &block, std::wstring &error);
    bool PrepareHardwareFlowStart(Session::Impl &impl, uintptr_t entry, std::wstring &error);
    bool ProgramHardwareObservation(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error);
    bool ProgramHardwareObservationImpl(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error);
    const ModuleRange *FindModuleRange(const std::vector<ModuleRange> &ranges, uintptr_t instruction);
    InstructionDecodeResult DecodeInstruction(uintptr_t instruction);
    bool SafeReadMemoryBytes(uintptr_t address, void *buffer, size_t size);
    bool ShouldEmitEvent(const Options &options, const InstructionDecodeResult &decode_result);
    const std::vector<ResolvedAsyncProbe> &KnownAsyncProbes();
    const ResolvedAsyncProbe *FindAsyncProbe(const std::vector<ResolvedAsyncProbe> &probes, uintptr_t address);
    void CaptureCallArguments(const CONTEXT &context, uintptr_t *arguments, uint8_t &count);
    void CaptureThreadContext(const CONTEXT &context, ThreadContextSnapshot &snapshot);
    bool HasValueProbeInRange(const Session::Impl &impl, uintptr_t begin, uintptr_t end);
    bool TryEmitValueProbeEvent(Session::Impl &impl, uintptr_t instruction, const CONTEXT *context);
    bool ActivateTrapWindow(
        Session::Impl &impl,
        const ActiveProbeSession &session,
        uintptr_t entry,
        CONTEXT *context,
        std::wstring &error);
    bool TryActivateFilterTrapSession(
        Session::Impl &impl,
        uintptr_t instruction,
        CONTEXT *context,
        bool &activated,
        std::wstring &error);
    bool TryActivateLocalProbeSession(
        Session::Impl &impl,
        uintptr_t instruction,
        CONTEXT *context,
        bool switch_from_hardware_flow,
        bool &activated,
        std::wstring &error);
    bool HandleActiveProbeSingleStep(
        Session::Impl &impl,
        uintptr_t executed_rip,
        uintptr_t current_rip,
        const CONTEXT *context,
        const InstructionDecodeResult &decode_result,
        uint32_t call_depth,
        uintptr_t actual_target,
        bool has_actual_target,
        bool &restored_hardware_flow,
        std::wstring &error);
    void ResetActiveProbeSession(Session::Impl &impl);
    bool StartRegionAwareHardwareFlow(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error);
    bool RestoreHardwareFlowAfterTrapWindow(Session::Impl &impl, uintptr_t current_rip, CONTEXT *context, std::wstring &error);
    bool ResolveAddressLabel(uintptr_t address, AddressLabel &label);
    const wchar_t *AsyncDispatchKindName(AsyncDispatchKind kind);
    void PrepareEnhancedSamplingCallEvent(
        const Session::Impl &impl,
        uintptr_t source,
        uintptr_t target,
        const CONTEXT *context,
        StepEvent &event,
        EnhancedSamplingFrame &frame);
    void PrepareEnhancedSamplingReturnEvent(
        const Session::Impl &impl,
        const CONTEXT *context,
        StepEvent &event);
    void PushEnhancedSamplingFrame(Session::Impl &impl, const EnhancedSamplingFrame &frame);
    void PopEnhancedSamplingFrame(Session::Impl &impl);
    RecorderQueuedEvent MakeRecorderQueuedEvent(const StepEvent &event);
    void SnapshotExecutionSummary(Session::Impl &impl);
    void ClearExecutionSummary(Session::Impl &impl);
    void RefreshCurrentCallDepth(Session::Impl &impl);
    void ResetThreadObservationState(Session::Impl &impl);
    void RememberCurrentProcessThreads(Session::Impl &impl);
    void StartAsyncHandoffWorker(Session::Impl &impl);
    void StopAsyncHandoffWorker(Session::Impl &impl);
    void QueueAsyncHandoffRequest(Session::Impl &impl, uintptr_t entry, uint32_t depth, uintptr_t handle_value, DWORD thread_id);
    bool BeginHardwareLinearScan(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error);
    bool TryActivateAsyncThreadHandoff(
        Session::Impl &impl,
        uintptr_t async_target,
        const StepEvent &event,
        CONTEXT *context,
        std::wstring &error);
    void ClearPendingResolution(Session::Impl &impl);
    const ResolvedDepthFilterModuleRule *FindDepthFilterModuleRule(const ResolvedDepthFilterSet &filters, uintptr_t address);
    ResolvedExecutionAddress ResolveExecutionAddress(const Session::Impl &impl, uintptr_t address, size_t *region_size = nullptr);
    bool QueryNonModuleExecutableRegion(const Session::Impl &impl, uintptr_t address, bool &is_image, size_t *region_size = nullptr);
    uint32_t ResolveCallDepthLimitForAddress(const Session::Impl &impl, uintptr_t address, bool &has_limit, bool &is_system_target);
    DepthFilterExecutionMode ResolveExecutionModeForAddress(const Session::Impl &impl, uintptr_t address, bool &has_mode, bool &is_system_target);
    bool IsAnonymousExecutableOutsideTrackedModules(const Session::Impl &impl, uintptr_t address);
    bool ShouldRecordOutsideAddress(const Session::Impl &impl, uintptr_t address);
    bool ShouldFollowKnownCallTarget(const Session::Impl &impl, uintptr_t target);
    bool ShouldFollowMinimalOutsideTarget(const Session::Impl &impl, uintptr_t target);
    bool IsMinimalOutsideAddress(const Session::Impl &impl, uintptr_t address);
    bool ShouldProbeAsyncCallTarget(const Session::Impl &impl, uintptr_t target);
    uint32_t StoreObservedAddressesUnique(Session::Impl &impl, const uintptr_t *addresses, uint32_t count);
    bool FinalizeHardwareTransition(
        Session::Impl &impl,
        uintptr_t observed_rip,
        const CONTEXT *context,
        bool from_single_step,
        bool &waiting_for_resume,
        uintptr_t &next_entry,
        bool &context_already_programmed);
    void ApplyHardwareContextObservations(CONTEXT &context, const uintptr_t *addresses, uint32_t count, bool single_step);
    LONG HandleHardwareFlowException(Session::Impl &impl, EXCEPTION_POINTERS *info);
    LONG HandleTrapFlagTraceException(Session::Impl &impl, EXCEPTION_POINTERS *info);
    void CopyEventToCStruct(const StepEvent &event, VDTRACE_STEP_EVENT &out);
    void StoreErrorString(const std::wstring &error, wchar_t *buffer, size_t capacity);
    bool TryConsumeStaleSingleStep(Session::Impl &impl, EXCEPTION_POINTERS *info);
    LONG HandleTriggerWaitException(Session::Impl &impl, EXCEPTION_POINTERS *info);
    LONG HandleTriggerThreadCaptureException(Session::Impl &impl, EXCEPTION_POINTERS *info);
}

#endif
