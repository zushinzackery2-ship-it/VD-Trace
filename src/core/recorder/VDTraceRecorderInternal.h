#ifndef VDTRACE_RECORDER_INTERNAL_H
#define VDTRACE_RECORDER_INTERNAL_H

#include "core/runtime/VDTraceInternalTypes.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace vdtrace
{
    class TextFileRecorderHeapPeek;

    std::string FormatRecorderEventLine(
        const RecorderQueuedEvent &event,
        uint64_t block_visit_count,
        std::unordered_map<uintptr_t, std::string> &module_name_cache,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        std::unordered_map<uintptr_t, uint32_t> &dynamic_range_ids,
        const std::string &inline_suffix);
    bool CanSummarizeOutsideOtherEvent(const RecorderQueuedEvent &event);
    std::string FormatOutsideOtherRunBlock(
        const RecorderQueuedEvent &first,
        const RecorderQueuedEvent &last,
        size_t count,
        uint64_t first_block_visit_count,
        uint64_t last_block_visit_count,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map);
    std::string FormatStaticReferenceBlock(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        std::unordered_set<uintptr_t> &analyzed_blocks,
        std::unordered_map<std::string, StaticReferenceExportEntry> *export_entries);
    std::wstring BuildStaticReferenceJsonPath(const std::wstring &trace_path);
    std::string RenderStaticReferenceJson(const std::unordered_map<std::string, StaticReferenceExportEntry> &entries);
    std::string BuildFunctionPreviewText(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        const TextFileRecorderHeapPeek *heap_peek);
    std::string FormatValueWithPreview(
        uintptr_t value,
        bool prefer_symbol,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map);
    std::string FormatFridaStyleEventBlock(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        std::unordered_map<DWORD, std::vector<ActiveCallFrame>> &active_calls,
        std::unordered_set<uintptr_t> &previewed_functions,
        const TextFileRecorderHeapPeek *heap_peek);
    void RememberEnhancedSamplingFrame(ActiveCallFrame &frame, const RecorderQueuedEvent &event);
    std::string FormatEnhancedSamplingEntryBlock(const RecorderQueuedEvent &event, const std::string &indent);
    std::string FormatEnhancedSamplingReturnBlock(
        const RecorderQueuedEvent &event,
        const std::string &indent,
        const ActiveCallFrame *frame);
}

#endif
