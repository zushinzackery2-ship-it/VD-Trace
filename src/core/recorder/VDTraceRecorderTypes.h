#ifndef VDTRACE_RECORDER_TYPES_H
#define VDTRACE_RECORDER_TYPES_H

#include "VDTrace/VDTrace.h"
#include "VDTrace/VDTraceC.h"

namespace vdtrace
{
    constexpr size_t kRecordedModuleNameCapacity = 96;

    struct ActiveCallFrame
    {
        uint32_t depth = 0;
        uintptr_t target = 0;
        std::string display_name;
        uint8_t memory_sample_count = 0;
        MemorySample memory_samples[kEnhancedSampleSlotCount] = {};
    };

    struct RecorderQueuedEvent
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
        wchar_t module_name[kRecordedModuleNameCapacity] = {};
    };

    struct StaticReferenceCodeLocation
    {
        uint64_t sequence = 0;
        DWORD thread_id = 0;
        uint32_t call_depth = 0;
        uintptr_t instruction = 0;
        uintptr_t block_begin = 0;
        uintptr_t block_end = 0;
        std::string instruction_label;
    };

    struct StaticReferenceExportEntry
    {
        std::string key;
        std::string section_name;
        uintptr_t slot_address = 0;
        std::string slot_label;
        uint8_t slot_size = 0;
        uint8_t slot_bytes[kEnhancedSampleMaxBytes] = {};
        bool has_dereference = false;
        uintptr_t dereference_address = 0;
        std::string dereference_label;
        uint8_t dereference_size = 0;
        uint8_t dereference_bytes[kEnhancedSampleMaxBytes] = {};
        std::string dereference_guess;
        std::string dereference_detail;
        std::vector<StaticReferenceCodeLocation> references;
    };
}

#endif
