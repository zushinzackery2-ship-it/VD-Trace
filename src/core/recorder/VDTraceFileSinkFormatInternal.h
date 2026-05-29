#ifndef VDTRACE_FILE_SINK_FORMAT_INTERNAL_H
#define VDTRACE_FILE_SINK_FORMAT_INTERNAL_H

#include "core/runtime/VDTraceInternal.h"
#include "third_party/zydis/Zydis.h"

namespace vdtrace::file_sink_format_detail
{
    enum class ExecutionRangeKind : uint8_t
    {
        Unknown = 0,
        Module,
        AnonymousExecutable,
        Other,
    };

    struct ExecutionRangeInfo
    {
        ExecutionRangeKind kind = ExecutionRangeKind::Unknown;
        uintptr_t identity = 0;
        std::string text;
    };

    uint32_t ResolveDynamicRangeId(std::unordered_map<uintptr_t, uint32_t> &dynamic_range_ids, uintptr_t identity);
    std::string Narrow(const std::wstring &text);
    std::string NarrowKind(EventKind kind);
    ZydisDecoder &GetProbeDecoder();
    ZydisFormatter &GetProbeFormatter();
    std::string FormatProbeBytes(const uint8_t *bytes, uint8_t size);
    std::string FormatProbeAscii(const uint8_t *bytes, uint8_t size);
    std::string FormatSingleProbeDisasm(const RecorderQueuedEvent &event);
    std::string HexText(uintptr_t value);
    bool TryFormatAnonymousExecLabel(uintptr_t address, std::string &text);
    ExecutionRangeInfo DescribeExecutionRange(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map);
    bool ShouldMarkTraceRangeJump(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        ExecutionRangeInfo &source_info,
        ExecutionRangeInfo &target_info,
        bool &outside_jump_in);
    std::wstring ResolveModuleFilename(uintptr_t module_base);
    std::string ResolveModuleName(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &module_name_cache);
    std::string ResolveAddressLabelText(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map);
    uintptr_t ReadCallArgument(const RecorderQueuedEvent &event, uint8_t index);
    std::string CallIndent(uint32_t call_depth);
    void AppendNamedArgument(std::ostringstream &out, bool &need_separator, const std::string &name, const std::string &value);
    std::string PrefixMultiline(const std::string &text, const std::string &prefix);
    std::string BuildCallSummaryText(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map);
    std::string FormatProbeBlock(const RecorderQueuedEvent &event, const std::string &indent);
    std::string FormatThreadContextBlock(const RecorderQueuedEvent &event, const char *label);
}

#endif
