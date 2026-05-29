#include "pch.h"
#include "core/heap_peek/VDTraceHeapPeekInternal.h"

namespace vdtrace
{
    TextFileRecorderHeapPeek::TextFileRecorderHeapPeek() = default;

    TextFileRecorderHeapPeek::~TextFileRecorderHeapPeek() = default;

    std::string TextFileRecorderHeapPeek::BuildDirectEventInlineSuffix(const RecorderQueuedEvent &event) const
    {
        return heap_peek::BuildDirectEventHeapPeekInlineSuffix(event);
    }

    std::string TextFileRecorderHeapPeek::BuildInlineSuffix(
        const heap_peek::HeapPeekRequest &request,
        const std::string &operand_text) const
    {
        return heap_peek::BuildHeapPeekInlineSuffix(request, operand_text);
    }

    void TextFileRecorderHeapPeek::Stop()
    {
    }
}
