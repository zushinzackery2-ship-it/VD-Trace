#include "pch.h"
#include "core/extender/VDTraceExtender.h"
#include "core/extender/VDTraceExtenderInternal.h"

namespace vdtrace
{
    TextFileRecorderExtender::TextFileRecorderExtender(const TextFileRecorderHeapPeek &heap_peek)
        : impl_(std::make_unique<Impl>(heap_peek))
    {
    }

    TextFileRecorderExtender::~TextFileRecorderExtender() = default;

    std::string TextFileRecorderExtender::ProcessEvent(const RecorderQueuedEvent &event)
    {
        return impl_->ProcessEvent(event);
    }

    void TextFileRecorderExtender::Stop()
    {
    }
}
