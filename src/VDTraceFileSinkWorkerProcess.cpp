#include "pch.h"
#include "VDTraceFileSinkInternal.h"

#include <filesystem>
#include <fstream>

namespace vdtrace
{
    void TextFileRecorder::Impl::WorkerLoop()
    {
        try
        {
            for (;;)
            {
                {
                    std::unique_lock<std::mutex> lock(wake_lock);
                    worker_cv.wait(
                        lock,
                        [this]()
                        {
                            return stop_requested.load(std::memory_order_acquire)
                                || read_index.load(std::memory_order_acquire) != write_index.load(std::memory_order_acquire);
                        });
                }

                std::string write_batch;
                write_batch.reserve(64 * 1024);
                uint64_t write_batch_event_count = 0;

                auto flush_write_batch = [&]()
                {
                    if (!write_batch.empty())
                    {
                        EnqueueWrite(std::move(write_batch), write_batch_event_count);
                        write_batch.clear();
                        write_batch.reserve(64 * 1024);
                        write_batch_event_count = 0;
                    }
                };

                size_t local_read = read_index.load(std::memory_order_relaxed);
                const size_t local_write = write_index.load(std::memory_order_acquire);
                worker_active.store(local_read != local_write, std::memory_order_release);
                while (local_read != local_write)
                {
                    const RecorderQueuedEvent &event = ring[local_read];
                    if (CanSummarizeOutsideOtherEvent(event))
                    {
                        RecorderQueuedEvent first_event = {};
                        RecorderQueuedEvent last_event = {};
                        uint64_t first_block_visit_count = 0;
                        uint64_t last_block_visit_count = 0;
                        size_t run_read = local_read;
                        size_t run_count = 0;
                        std::string deferred_blocks;

                        while (run_read != local_write)
                        {
                            const RecorderQueuedEvent &candidate = ring[run_read];
                            if (!CanSummarizeOutsideOtherEvent(candidate)
                                || candidate.thread_id != event.thread_id
                                || candidate.call_depth != event.call_depth)
                            {
                                break;
                            }

                            const uint64_t block_visit_count = CountBlockVisit(candidate);
                            if (run_count == 0)
                            {
                                first_event = candidate;
                                first_block_visit_count = block_visit_count;
                            }

                            last_event = candidate;
                            last_block_visit_count = block_visit_count;
                            ++run_count;

                            const std::string static_reference_block = FormatStaticReferenceBlock(
                                candidate,
                                address_label_cache,
                                analyzed_static_blocks,
                                &static_reference_exports);
                            if (!static_reference_block.empty())
                            {
                                static_reference_json_dirty = true;
                                deferred_blocks += static_reference_block;
                            }

                            deferred_blocks += FormatFridaStyleEventBlock(
                                candidate,
                                address_label_cache,
                                async_probe_map,
                                active_calls,
                                previewed_functions,
                                heap_peek.get());
                            if (extender != nullptr)
                            {
                                deferred_blocks += extender->ProcessEvent(candidate);
                            }

                            run_read = (run_read + 1) % kRecorderRingCapacity;
                        }

                        if (run_count >= 2)
                        {
                            write_batch += FormatOutsideOtherRunBlock(
                                first_event,
                                last_event,
                                run_count,
                                first_block_visit_count,
                                last_block_visit_count,
                                address_label_cache,
                                async_probe_map);
                        }
                        else
                        {
                            write_batch += FormatRecorderEventLine(
                                first_event,
                                first_block_visit_count,
                                module_name_cache,
                                address_label_cache,
                                async_probe_map,
                                dynamic_range_ids,
                                heap_peek != nullptr ? heap_peek->BuildDirectEventInlineSuffix(first_event) : std::string {});
                        }
                        write_batch_event_count += static_cast<uint64_t>(run_count);
                        write_batch += deferred_blocks;
                        if (write_batch.size() >= 64 * 1024)
                        {
                            flush_write_batch();
                        }

                        local_read = run_read;
                        continue;
                    }

                    const uint64_t block_visit_count = CountBlockVisit(event);
                    write_batch += FormatRecorderEventLine(
                        event,
                        block_visit_count,
                        module_name_cache,
                        address_label_cache,
                        async_probe_map,
                        dynamic_range_ids,
                        heap_peek != nullptr ? heap_peek->BuildDirectEventInlineSuffix(event) : std::string {});
                    ++write_batch_event_count;

                    if (!event.minimal_record)
                    {
                        const std::string static_reference_block = FormatStaticReferenceBlock(
                            event,
                            address_label_cache,
                            analyzed_static_blocks,
                            &static_reference_exports);
                        if (!static_reference_block.empty())
                        {
                            static_reference_json_dirty = true;
                            write_batch += static_reference_block;
                        }

                        write_batch += FormatFridaStyleEventBlock(
                            event,
                            address_label_cache,
                            async_probe_map,
                            active_calls,
                            previewed_functions,
                            heap_peek.get());
                        if (extender != nullptr)
                        {
                            write_batch += extender->ProcessEvent(event);
                        }
                    }

                    if (write_batch.size() >= 64 * 1024)
                    {
                        flush_write_batch();
                    }

                    local_read = (local_read + 1) % kRecorderRingCapacity;
                }

                flush_write_batch();
                worker_active.store(false, std::memory_order_release);

                if (static_reference_json_dirty)
                {
                    const ULONGLONG now = GetTickCount64();
                    if (stop_requested.load(std::memory_order_acquire)
                        || last_static_reference_json_flush_tick == 0
                        || now - last_static_reference_json_flush_tick >= 1000)
                    {
                        FlushStaticReferenceJson();
                        last_static_reference_json_flush_tick = now;
                        static_reference_json_dirty = false;
                    }
                }

                const size_t drained_read = read_index.load(std::memory_order_relaxed);
                read_index.store(local_read, std::memory_order_release);
                if (local_read != drained_read)
                {
                    producer_cv.notify_all();
                }
                if (stop_requested.load(std::memory_order_acquire)
                    && local_read == write_index.load(std::memory_order_acquire))
                {
                    break;
                }
            }
        }
        catch (const std::exception &error)
        {
            EnqueueWrite(std::string("[vdtrace] recorder_worker_error=") + error.what() + "\n");
        }
        catch (...)
        {
            EnqueueWrite("[vdtrace] recorder_worker_error=unknown\n");
        }
    }

    bool TextFileRecorder::Impl::WriteText(const std::string &text)
    {
        if (file == INVALID_HANDLE_VALUE || text.empty())
        {
            return file != INVALID_HANDLE_VALUE;
        }

        std::lock_guard<std::mutex> lock(file_write_lock);
        const char *cursor = text.data();
        size_t remaining = text.size();
        while (remaining != 0)
        {
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, static_cast<size_t>(std::numeric_limits<DWORD>::max())));
            DWORD written = 0;
            if (!WriteFile(file, cursor, chunk, &written, nullptr))
            {
                return false;
            }
            if (written == 0)
            {
                SetLastError(ERROR_WRITE_FAULT);
                return false;
            }

            cursor += written;
            remaining -= written;
        }
        return true;
    }

    void TextFileRecorder::Impl::FlushStaticReferenceJson()
    {
        if (static_reference_json_path.empty())
        {
            return;
        }

        const std::filesystem::path path(static_reference_json_path);
        if (static_reference_exports.empty())
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            return;
        }

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            return;
        }

        const std::string json = RenderStaticReferenceJson(static_reference_exports);
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
    }

    uint64_t TextFileRecorder::Impl::CountBlockVisit(const RecorderQueuedEvent &event)
    {
        BlockVisitKey key = {};
        key.thread_id = event.thread_id;
        key.block_begin = event.block_begin != 0 ? event.block_begin : event.instruction;
        key.block_end = event.block_end != 0 ? event.block_end : (event.instruction + event.instruction_size);
        return ++block_visit_counts[key];
    }
}
