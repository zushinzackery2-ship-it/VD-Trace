#ifndef VDTRACE_HEAP_PEEK_INTERNAL_H
#define VDTRACE_HEAP_PEEK_INTERNAL_H

#include "core/heap_peek/VDTraceHeapPeek.h"
#include "core/runtime/VDTraceInternal.h"
#include "third_party/zydis/Zydis.h"

namespace vdtrace
{
    namespace heap_peek
    {
        enum class HeapPeekOrigin : uint8_t
        {
            DirectEvent = 0,
            BlockExtendRead,
            BlockExtendWrite,
            BlockExtendReadWrite,
        };

        struct HeapPeekRequest
        {
            uint64_t sequence = 0;
            DWORD thread_id = 0;
            EventKind kind = EventKind::Unknown;
            uintptr_t instruction = 0;
            uintptr_t memory_address = 0;
            uintptr_t stack_pointer = 0;
            uint8_t peek_size = 0;
            HeapPeekOrigin origin = HeapPeekOrigin::DirectEvent;
        };

        uint8_t ResolveHeapPeekSize(
            const ZydisDecodedInstruction &instruction,
            const ZydisDecodedOperand *operands,
            uint8_t operand_index,
            bool address_only);
        bool DecodeHeapPeekInstruction(
            const uint8_t *bytes,
            size_t size,
            ZydisDecodedInstruction &instruction,
            ZydisDecodedOperand *operands);
        std::string BuildDirectEventHeapPeekInlineSuffix(const RecorderQueuedEvent &event);
        bool IsHeapLikeAddress(uintptr_t address, uintptr_t stack_pointer);
        std::string BuildHeapPeekInlineSuffix(const HeapPeekRequest &request, const std::string &operand_text);
    }
}

#endif
