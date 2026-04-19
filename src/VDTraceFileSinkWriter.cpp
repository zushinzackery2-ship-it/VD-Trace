#include "pch.h"
#include "VDTraceFileSinkInternal.h"

namespace vdtrace
{
    namespace
    {
        constexpr size_t kMaxPendingWriteBytes = 16 * 1024 * 1024;

        void SignalWriterStop(TextFileRecorder::Impl &impl, uint64_t dropped_events)
        {
            if (dropped_events != 0)
            {
                impl.total_dropped_write_event_count.fetch_add(dropped_events, std::memory_order_relaxed);
            }

            // Wake any producer blocked on queue pressure when the writer aborts.
            impl.writer_active.store(false, std::memory_order_release);
            impl.stop_requested.store(true, std::memory_order_release);
            impl.writer_stop_requested.store(true, std::memory_order_release);
            impl.worker_cv.notify_all();
            impl.producer_cv.notify_all();
            impl.write_queue_cv.notify_all();
        }

        void EmitWriterDebugError(const char *prefix, DWORD error)
        {
            std::ostringstream out;
            out << "[vdtrace] " << prefix << "=" << error << "\n";
            OutputDebugStringA(out.str().c_str());
        }
    }

    void TextFileRecorder::Impl::StartWriter()
    {
        if (!IsOpen() || file_writer.joinable())
        {
            return;
        }

        writer_stop_requested.store(false, std::memory_order_release);
        file_writer = std::thread(&Impl::WriterLoop, this);
    }

    void TextFileRecorder::Impl::StopWriter()
    {
        writer_stop_requested.store(true, std::memory_order_release);
        write_queue_cv.notify_all();
        if (file_writer.joinable())
        {
            file_writer.join();
        }
    }

    void TextFileRecorder::Impl::EnqueueWrite(std::string text, uint64_t event_count)
    {
        if (file == INVALID_HANDLE_VALUE
            || text.empty()
            || writer_stop_requested.load(std::memory_order_acquire))
        {
            if (event_count != 0)
            {
                total_dropped_write_event_count.fetch_add(event_count, std::memory_order_relaxed);
            }
            return;
        }

        const size_t text_size = text.size();
        std::unique_lock<std::mutex> lock(write_queue_lock);
        // Keep the queue bounded without silently dropping batches.
        write_queue_cv.wait(
            lock,
            [this, text_size]()
            {
                const size_t pending_bytes = pending_write_bytes.load(std::memory_order_relaxed);
                return writer_stop_requested.load(std::memory_order_acquire)
                    || pending_bytes == 0
                    || (text_size <= kMaxPendingWriteBytes && pending_bytes <= kMaxPendingWriteBytes - text_size);
            });

        if (writer_stop_requested.load(std::memory_order_acquire))
        {
            if (event_count != 0)
            {
                total_dropped_write_event_count.fetch_add(event_count, std::memory_order_relaxed);
            }
            return;
        }

        const size_t pending_bytes = pending_write_bytes.load(std::memory_order_relaxed);
        pending_write_bytes.store(pending_bytes + text_size, std::memory_order_relaxed);
        pending_write_event_count.fetch_add(event_count, std::memory_order_relaxed);
        PendingWriteBatch batch = {};
        batch.text = std::move(text);
        batch.event_count = event_count;
        pending_write_batches.emplace_back(std::move(batch));
        lock.unlock();
        write_queue_cv.notify_one();
    }

    void TextFileRecorder::Impl::WriterLoop()
    {
        try
        {
            for (;;)
            {
                std::string batch;
                uint64_t batch_event_count = 0;
                {
                    std::unique_lock<std::mutex> lock(write_queue_lock);
                    write_queue_cv.wait(
                        lock,
                        [this]()
                        {
                            return writer_stop_requested.load(std::memory_order_acquire)
                                || !pending_write_batches.empty();
                        });

                    if (!pending_write_batches.empty())
                    {
                        batch = std::move(pending_write_batches.front().text);
                        batch_event_count = pending_write_batches.front().event_count;
                        pending_write_bytes.fetch_sub(batch.size(), std::memory_order_relaxed);
                        pending_write_event_count.fetch_sub(batch_event_count, std::memory_order_relaxed);
                        pending_write_batches.pop_front();
                        lock.unlock();
                        write_queue_cv.notify_one();
                    }
                    else
                    {
                        lock.unlock();
                    }
                }

                if (!batch.empty())
                {
                    writer_active.store(true, std::memory_order_release);
                    if (!WriteText(batch))
                    {
                        writer_active.store(false, std::memory_order_release);
                        EmitWriterDebugError("file_writer_error", GetLastError());
                        SignalWriterStop(*this, batch_event_count);
                        break;
                    }
                    writer_active.store(false, std::memory_order_release);
                    written_event_count.fetch_add(batch_event_count, std::memory_order_release);
                    continue;
                }

                if (writer_stop_requested.load(std::memory_order_acquire))
                {
                    break;
                }
            }
        }
        catch (const std::exception &error)
        {
            OutputDebugStringA((std::string("[vdtrace] file_writer_error=") + error.what() + "\n").c_str());
            SignalWriterStop(*this, 0);
        }
        catch (...)
        {
            OutputDebugStringA("[vdtrace] file_writer_error=unknown\n");
            SignalWriterStop(*this, 0);
        }
    }
}
