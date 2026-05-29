#ifndef VDTRACE_EXTENDER_SUPPORT_H
#define VDTRACE_EXTENDER_SUPPORT_H

#include "core/heap_peek/VDTraceHeapPeekInternal.h"
#include "core/runtime/VDTraceInternal.h"

#include <string>
#include <vector>

namespace vdtrace
{
    namespace extender
    {
        enum class ExtendedAccessKind : uint8_t
        {
            Memory = 0,
            Address,
        };

        struct ExtendedMemoryAccess
        {
            uint64_t sequence = 0;
            DWORD thread_id = 0;
            EventKind kind = EventKind::Unknown;
            uintptr_t block_begin = 0;
            uintptr_t block_end = 0;
            uintptr_t instruction = 0;
            uintptr_t memory_address = 0;
            uintptr_t stack_pointer = 0;
            uint8_t peek_size = 0;
            uint8_t known_value_size = 0;
            uint8_t instruction_size = 0;
            uint8_t instruction_bytes[16] = {};
            uint8_t known_value_bytes[kEnhancedSampleMaxBytes] = {};
            ExtendedAccessKind access_kind = ExtendedAccessKind::Memory;
            heap_peek::HeapPeekOrigin origin = heap_peek::HeapPeekOrigin::DirectEvent;
            bool has_known_value = false;
            std::string disasm;
            std::string operand_text;
        };

        bool AnalyzeBlockMemoryAccesses(
            const RecorderQueuedEvent &event,
            const ThreadContextSnapshot &entry_context,
            std::vector<ExtendedMemoryAccess> &accesses);

        std::string FormatExtendedMemoryAccessLine(const ExtendedMemoryAccess &access);
    }
}

#endif
