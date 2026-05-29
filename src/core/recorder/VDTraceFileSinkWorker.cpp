#include "pch.h"
#include "core/recorder/VDTraceFileSinkInternal.h"

namespace vdtrace
{
    TextFileRecorder::Impl::Impl(const std::wstring &path, const Options &options)
        : static_reference_json_path(BuildStaticReferenceJsonPath(path))
    {
        for (const auto &probe : KnownAsyncProbes())
        {
            async_probe_map.emplace(probe.address, probe);
        }
    }

    TextFileRecorder::Impl::~Impl()
    {
        StopWorker();
        FlushStaticReferenceJson();
        if (file != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(file);
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
    }

    bool TextFileRecorder::Impl::IsOpen() const
    {
        return file != INVALID_HANDLE_VALUE;
    }

    bool TextFileRecorder::Impl::IsWriting() const
    {
        return worker_active.load(std::memory_order_acquire)
            || writer_active.load(std::memory_order_acquire)
            || PendingEventCount() != 0
            || PendingWriteBytes() != 0;
    }

    size_t TextFileRecorder::Impl::PendingEventCount() const
    {
        const size_t read = read_index.load(std::memory_order_acquire);
        const size_t write = write_index.load(std::memory_order_acquire);
        if (write >= read)
        {
            return write - read;
        }

        return (kRecorderRingCapacity - read) + write;
    }

    size_t TextFileRecorder::Impl::PendingWriteBytes() const
    {
        return pending_write_bytes.load(std::memory_order_acquire);
    }

    uint64_t TextFileRecorder::Impl::PendingWriteEventCount() const
    {
        return pending_write_event_count.load(std::memory_order_acquire);
    }

    uint64_t TextFileRecorder::Impl::WrittenEventCount() const
    {
        return written_event_count.load(std::memory_order_acquire);
    }

    uint64_t TextFileRecorder::Impl::DroppedEventCount() const
    {
        return total_dropped_events.load(std::memory_order_acquire);
    }

    uint64_t TextFileRecorder::Impl::DroppedWriteEventCount() const
    {
        return total_dropped_write_event_count.load(std::memory_order_acquire);
    }

    void TextFileRecorder::Impl::StartWorker()
    {
        if (!IsOpen())
        {
            return;
        }

        heap_peek = std::make_unique<TextFileRecorderHeapPeek>();
        extender = std::make_unique<TextFileRecorderExtender>(*heap_peek);
        worker = std::thread(&Impl::WorkerLoop, this);
    }

    void TextFileRecorder::Impl::StopWorker()
    {
        stop_requested.store(true, std::memory_order_release);
        worker_cv.notify_all();
        producer_cv.notify_all();
        if (worker.joinable())
        {
            worker.join();
        }

        if (extender != nullptr)
        {
            extender->Stop();
            extender.reset();
        }

        if (heap_peek != nullptr)
        {
            heap_peek->Stop();
            heap_peek.reset();
        }

        StopWriter();
    }

    void TextFileRecorder::Impl::Enqueue(const StepEvent &event)
    {
        if (!IsOpen() || stop_requested.load(std::memory_order_acquire))
        {
            return;
        }

        RecorderQueuedEvent queued = MakeRecorderQueuedEvent(event);
        std::unique_lock<std::mutex> lock(wake_lock);
        for (;;)
        {
            const size_t write = write_index.load(std::memory_order_relaxed);
            const size_t read = read_index.load(std::memory_order_acquire);
            const size_t next = (write + 1) % kRecorderRingCapacity;
            if (next != read)
            {
                ring[write] = queued;
                write_index.store(next, std::memory_order_release);
                if (write == read)
                {
                    worker_cv.notify_one();
                }
                return;
            }

            producer_cv.wait(
                lock,
                [this]()
                {
                    return stop_requested.load(std::memory_order_acquire)
                        || PendingEventCount() < (kRecorderRingCapacity - 1);
                });
            if (stop_requested.load(std::memory_order_acquire))
            {
                return;
            }
        }
    }
}
