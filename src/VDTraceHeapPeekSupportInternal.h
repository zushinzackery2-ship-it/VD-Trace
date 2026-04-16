#ifndef VDTRACE_HEAP_PEEK_SUPPORT_INTERNAL_H
#define VDTRACE_HEAP_PEEK_SUPPORT_INTERNAL_H

#include "VDTraceHeapPeekInternal.h"
#include "VDTraceIntegerHex.h"
#include "third_party/zydis/Zydis.h"

namespace vdtrace::heap_peek::detail
{
    ZydisDecoder &GetHeapPeekDecoder();
    ZydisFormatter &GetHeapPeekFormatter();
    std::string Narrow(const std::wstring &text);
    std::string FormatBytes(const uint8_t *bytes, uint8_t size);
    std::string FormatAscii(const uint8_t *bytes, uint8_t size);
    uint8_t ClampPeekSizeBits(uint32_t bits);
    uint8_t RegisterPeekSizeBytes(ZydisRegister reg);
    bool TryReadPeekBytes(const HeapPeekRequest &request, uint8_t *bytes, uint8_t &size);
    void FillRegisterContext(const ThreadContextSnapshot &snapshot, ZydisRegisterContext &context);
    const char *OriginText(HeapPeekOrigin origin);
    bool TryBuildHeapPeekRequestCore(
        const RecorderQueuedEvent &event,
        HeapPeekRequest &request,
        std::string *operand_text);
}

#endif
