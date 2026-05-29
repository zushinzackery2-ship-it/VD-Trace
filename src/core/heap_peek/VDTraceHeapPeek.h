#ifndef VDTRACE_HEAP_PEEK_H
#define VDTRACE_HEAP_PEEK_H

#include "core/runtime/VDTraceInternalTypes.h"

#include <string>

namespace vdtrace
{
    namespace heap_peek
    {
        struct HeapPeekRequest;
    }

    class TextFileRecorderHeapPeek
    {
      public:
        TextFileRecorderHeapPeek();
        ~TextFileRecorderHeapPeek();

        TextFileRecorderHeapPeek(const TextFileRecorderHeapPeek &) = delete;
        TextFileRecorderHeapPeek &operator=(const TextFileRecorderHeapPeek &) = delete;

        std::string BuildDirectEventInlineSuffix(const RecorderQueuedEvent &event) const;
        std::string BuildInlineSuffix(const heap_peek::HeapPeekRequest &request, const std::string &operand_text) const;
        void Stop();
    };
}

#endif
