#include "pch.h"
#include "core/recorder/VDTraceFileSinkInternal.h"

namespace vdtrace
{
    TextFileRecorder::TextFileRecorder(const std::wstring &path, const Options &options)
        : impl_(std::make_unique<Impl>(path, options))
    {
        impl_->file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (impl_->IsOpen())
        {
            impl_->StartWriter();
            impl_->StartWorker();
        }
    }

    TextFileRecorder::~TextFileRecorder() = default;

    bool TextFileRecorder::IsOpen() const
    {
        return impl_ != nullptr && impl_->IsOpen();
    }

    bool TextFileRecorder::IsWriting() const
    {
        return impl_ != nullptr && impl_->IsWriting();
    }

    size_t TextFileRecorder::PendingEventCount() const
    {
        return impl_ != nullptr ? impl_->PendingEventCount() : 0;
    }

    size_t TextFileRecorder::PendingWriteBytes() const
    {
        return impl_ != nullptr ? impl_->PendingWriteBytes() : 0;
    }

    uint64_t TextFileRecorder::PendingWriteEventCount() const
    {
        return impl_ != nullptr ? impl_->PendingWriteEventCount() : 0;
    }

    uint64_t TextFileRecorder::WrittenEventCount() const
    {
        return impl_ != nullptr ? impl_->WrittenEventCount() : 0;
    }

    uint64_t TextFileRecorder::DroppedEventCount() const
    {
        return impl_ != nullptr ? impl_->DroppedEventCount() : 0;
    }

    uint64_t TextFileRecorder::DroppedWriteEventCount() const
    {
        return impl_ != nullptr ? impl_->DroppedWriteEventCount() : 0;
    }

    void TextFileRecorder::OnStep(const StepEvent &event)
    {
        if (impl_ != nullptr)
        {
            impl_->Enqueue(event);
        }
    }

    void TextFileRecorder::Callback(const StepEvent &event, void *context)
    {
        auto *self = static_cast<TextFileRecorder *>(context);
        if (self != nullptr)
        {
            self->OnStep(event);
        }
    }
}
