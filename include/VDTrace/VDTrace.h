#ifndef VDTRACE_VDTRACE_H
#define VDTRACE_VDTRACE_H

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vdtrace
{
    constexpr uint32_t kUnlimitedCallDepth = 0xFFFFFFFFu;
    constexpr uint8_t kEnhancedSampleSlotCount = 2;
    constexpr uint8_t kEnhancedSampleMaxBytes = 32;
    constexpr uint8_t kEnhancedSampleSourceReturnValue = 0xFFu;
    constexpr uint8_t kProbeCaptureMaxCount = 4;
    constexpr uint8_t kProbeLabelMaxChars = 32;

    enum class EventKind : uint32_t
    {
        Unknown = 0,
        Other,
        Probe,
        Call,
        Jump,
        ConditionalJump,
        Return,
        Syscall,
        Interrupt,
    };

    enum class FlowHitPolicy : uint32_t
    {
        FirstSeen = 0,
        EveryHit = 1,
    };

    enum class TraceBackend : uint32_t
    {
        DrControlFlow = 0,
        TfFullTrace = 1,
        PtControlFlow = 2,
    };

    struct ModuleRange
    {
        std::wstring name;
        uintptr_t base = 0;
        size_t size = 0;
    };

    struct MemorySample
    {
        bool valid = false;
        uint8_t source_index = kEnhancedSampleSourceReturnValue;
        uint8_t size = 0;
        uintptr_t address = 0;
        uint8_t bytes[kEnhancedSampleMaxBytes] = {};
    };

    struct ProbeCapture
    {
        bool valid = false;
        bool has_bytes = false;
        uint8_t size = 0;
        uintptr_t value = 0;
        wchar_t label[kProbeLabelMaxChars] = {};
        uint8_t bytes[kEnhancedSampleMaxBytes] = {};
    };

    struct ThreadContextSnapshot
    {
        bool valid = false;
        uintptr_t rip = 0;
        uintptr_t rsp = 0;
        uintptr_t rbp = 0;
        uintptr_t rax = 0;
        uintptr_t rbx = 0;
        uintptr_t rcx = 0;
        uintptr_t rdx = 0;
        uintptr_t rsi = 0;
        uintptr_t rdi = 0;
        uintptr_t r8 = 0;
        uintptr_t r9 = 0;
        uintptr_t r10 = 0;
        uintptr_t r11 = 0;
        uintptr_t r12 = 0;
        uintptr_t r13 = 0;
        uintptr_t r14 = 0;
        uintptr_t r15 = 0;
        uint64_t rflags = 0;
    };

    struct StepEvent
    {
        uint64_t sequence = 0;
        DWORD thread_id = 0;
        uintptr_t instruction = 0;
        uintptr_t relative_instruction = 0;
        uintptr_t module_base = 0;
        uintptr_t stack_pointer = 0;
        uintptr_t block_begin = 0;
        uintptr_t block_end = 0;
        uintptr_t target = 0;
        uint32_t call_depth = 0;
        size_t module_size = 0;
        bool has_target = false;
        bool inside_module = false;
        bool minimal_record = false;
        EventKind kind = EventKind::Unknown;
        uint8_t instruction_size = 0;
        uint8_t instruction_bytes[16] = {};
        uint8_t call_argument_count = 0;
        uintptr_t call_arguments[8] = {};
        bool has_return_value = false;
        uintptr_t return_value = 0;
        uint8_t memory_sample_count = 0;
        MemorySample memory_samples[kEnhancedSampleSlotCount] = {};
        bool has_return_memory_sample = false;
        MemorySample return_memory_sample = {};
        uint8_t probe_capture_count = 0;
        ProbeCapture probe_captures[kProbeCaptureMaxCount] = {};
        ThreadContextSnapshot thread_context = {};
        std::wstring module_name;
    };

    using StepCallback = void (*)(const StepEvent &event, void *context);

    const wchar_t *EventKindName(EventKind kind);
    const wchar_t *FlowHitPolicyName(FlowHitPolicy policy);
    const wchar_t *TraceBackendName(TraceBackend backend);

    struct Options
    {
        DWORD thread_id = 0;
        bool auto_select_thread = false;
        bool block_main_thread = false;
        bool queue_trigger_threads = false;
        std::vector<std::wstring> module_names;
        std::wstring output_path;
        uint64_t max_events = 0;
        bool trace_outside_modules = false;
        TraceBackend backend = TraceBackend::DrControlFlow;
        bool control_flow_only = true;
        uint32_t max_call_depth = kUnlimitedCallDepth;
        std::wstring depth_filter_spec;
        FlowHitPolicy hit_policy = FlowHitPolicy::FirstSeen;
        // Loop hot-bypass (FirstSeen only): after a loop back-edge repeats this many
        // times, stop faulting per iteration and let the loop run free to its exit.
        // Kept low so the per-loop-entry warm-up tax stays small; 0 disables it.
        uint32_t hot_bypass_threshold = 8;
        bool sim_fast_forward = false;
        bool sim_fast_forward_indirect = false;
        bool enhanced_sampling = false;
        std::wstring trigger_module_name;
        uintptr_t trigger_address = 0;
        // Optional end point: when execution reaches this address the session stops.
        // stop_address is an RVA when stop_module_name is set, otherwise an absolute
        // address. Left at 0 with an empty module name it is disabled (the default).
        std::wstring stop_module_name;
        uintptr_t stop_address = 0;
        std::wstring probe_spec;
        bool stop_on_root_return = false;
        bool async_thread_handoff = false;
        StepCallback callback = nullptr;
        void *callback_context = nullptr;
    };

    class Session
    {
      public:
        struct Impl;

        Session();
        ~Session();

        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;

        bool Configure(const Options &options, std::wstring &error);
        bool Start(std::wstring &error);
        bool Stop(std::wstring &error);

        bool IsConfigured() const;
        bool IsRunning() const;
        uint64_t EventCount() const;
        std::wstring DescribeState() const;
        std::vector<ModuleRange> ModuleRanges() const;

      private:
        std::unique_ptr<Impl> impl_;
    };

    class TextFileRecorder
    {
      public:
        struct Impl;

        explicit TextFileRecorder(const std::wstring &path, const Options &options = Options());
        ~TextFileRecorder();

        TextFileRecorder(const TextFileRecorder &) = delete;
        TextFileRecorder &operator=(const TextFileRecorder &) = delete;

        bool IsOpen() const;
        bool IsWriting() const;
        size_t PendingEventCount() const;
        size_t PendingWriteBytes() const;
        uint64_t PendingWriteEventCount() const;
        uint64_t WrittenEventCount() const;
        uint64_t DroppedEventCount() const;
        uint64_t DroppedWriteEventCount() const;
        void OnStep(const StepEvent &event);

        static void Callback(const StepEvent &event, void *context);

      private:
        std::unique_ptr<Impl> impl_;
    };
}

#endif
