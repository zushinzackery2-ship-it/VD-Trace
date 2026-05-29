#ifndef VDTRACE_EXTENDER_H
#define VDTRACE_EXTENDER_H

#include "core/heap_peek/VDTraceHeapPeek.h"
#include "core/runtime/VDTraceInternalTypes.h"

#include <memory>
#include <mutex>

namespace vdtrace
{
    class TextFileRecorderExtender
    {
      public:
        explicit TextFileRecorderExtender(const TextFileRecorderHeapPeek &heap_peek);
        ~TextFileRecorderExtender();

        TextFileRecorderExtender(const TextFileRecorderExtender &) = delete;
        TextFileRecorderExtender &operator=(const TextFileRecorderExtender &) = delete;

        std::string ProcessEvent(const RecorderQueuedEvent &event);
        void Stop();

      private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}

#endif
