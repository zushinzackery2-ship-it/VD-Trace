#ifndef VDTRACE_FILE_SINK_INTERNAL_H
#define VDTRACE_FILE_SINK_INTERNAL_H

#include "core/extender/VDTraceExtender.h"
#include "core/heap_peek/VDTraceHeapPeek.h"
#include "core/runtime/VDTraceInternal.h"

#include <deque>

namespace vdtrace
{
    constexpr size_t kRecorderRingCapacity = 655360;

    struct PendingWriteBatch
    {
        std::string text;
        uint64_t event_count = 0;
    };

    struct BlockVisitKey
    {
        DWORD thread_id = 0;
        uintptr_t block_begin = 0;
        uintptr_t block_end = 0;

        bool operator==(const BlockVisitKey &other) const
        {
            return thread_id == other.thread_id
                && block_begin == other.block_begin
                && block_end == other.block_end;
        }
    };

    struct BlockVisitKeyHash
    {
        size_t operator()(const BlockVisitKey &key) const
        {
            size_t value = std::hash<DWORD> {}(key.thread_id);
            value ^= std::hash<uintptr_t> {}(key.block_begin) + 0x9e3779b9 + (value << 6) + (value >> 2);
            value ^= std::hash<uintptr_t> {}(key.block_end) + 0x9e3779b9 + (value << 6) + (value >> 2);
            return value;
        }
    };

    struct TextFileRecorder::Impl
    {
        std::wstring static_reference_json_path;
        HANDLE file = INVALID_HANDLE_VALUE;
        std::mutex file_write_lock;
        std::mutex wake_lock;
        std::condition_variable worker_cv;
        std::condition_variable producer_cv;
        std::mutex write_queue_lock;
        std::condition_variable write_queue_cv;
        std::deque<PendingWriteBatch> pending_write_batches;
        std::atomic<size_t> pending_write_bytes = 0;
        std::atomic<uint64_t> pending_write_event_count = 0;
        std::atomic<uint64_t> written_event_count = 0;
        std::unique_ptr<RecorderQueuedEvent[]> ring = std::make_unique<RecorderQueuedEvent[]>(kRecorderRingCapacity);
        std::unordered_map<uintptr_t, std::string> module_name_cache;
        std::unordered_map<uintptr_t, std::string> address_label_cache;
        std::unordered_map<uintptr_t, ResolvedAsyncProbe> async_probe_map;
        std::unordered_map<uintptr_t, uint32_t> dynamic_range_ids;
        std::unordered_map<DWORD, std::vector<ActiveCallFrame>> active_calls;
        std::unordered_set<uintptr_t> previewed_functions;
        std::unordered_set<uintptr_t> analyzed_static_blocks;
        std::unordered_map<std::string, StaticReferenceExportEntry> static_reference_exports;
        std::unordered_map<BlockVisitKey, uint64_t, BlockVisitKeyHash> block_visit_counts;
        std::thread worker;
        std::thread file_writer;
        std::unique_ptr<TextFileRecorderHeapPeek> heap_peek;
        std::unique_ptr<TextFileRecorderExtender> extender;
        std::atomic<bool> worker_active = false;
        std::atomic<bool> writer_active = false;
        std::atomic<size_t> read_index = 0;
        std::atomic<size_t> write_index = 0;
        std::atomic<uint64_t> total_dropped_events = 0;
        std::atomic<uint64_t> total_dropped_write_event_count = 0;
        std::atomic<bool> stop_requested = false;
        std::atomic<bool> writer_stop_requested = false;
        ULONGLONG last_static_reference_json_flush_tick = 0;
        bool static_reference_json_dirty = false;

        Impl(const std::wstring &path, const Options &options);
        ~Impl();

        bool IsOpen() const;
        bool IsWriting() const;
        size_t PendingEventCount() const;
        size_t PendingWriteBytes() const;
        uint64_t PendingWriteEventCount() const;
        uint64_t WrittenEventCount() const;
        uint64_t DroppedEventCount() const;
        uint64_t DroppedWriteEventCount() const;
        void StartWriter();
        void StartWorker();
        void StopWriter();
        void StopWorker();
        void Enqueue(const StepEvent &event);
        void WorkerLoop();
        void WriterLoop();
        void EnqueueWrite(std::string text, uint64_t event_count = 0);
        bool WriteText(const std::string &text);
        void FlushStaticReferenceJson();
        uint64_t CountBlockVisit(const RecorderQueuedEvent &event);
    };
}

#endif
