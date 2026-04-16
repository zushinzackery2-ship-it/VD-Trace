#ifndef VDTRACE_ASYNC_SUPPORT_INTERNAL_H
#define VDTRACE_ASYNC_SUPPORT_INTERNAL_H

#include "VDTraceInternal.h"

namespace vdtrace::async_support_detail
{
    struct ExportSymbol
    {
        uintptr_t address = 0;
        std::wstring name;
    };

    struct ExportCache
    {
        bool initialized = false;
        std::vector<ExportSymbol> symbols;
    };

    struct KnownAsyncProbeSpec
    {
        const wchar_t *module_name;
        const char *symbol_name;
        AsyncDispatchKind kind;
        uint8_t argument_count;
        uint8_t argument_indices[4];
        bool argument_is_pointer[4];
        const char *argument_names[4];
        ResolvedAsyncProbe::ThreadHandleSource thread_handle_source;
        uint8_t thread_handle_argument_index;
        uint8_t thread_id_argument_index;
        uint8_t handoff_entry_argument_index;
    };

    constexpr size_t kKnownAsyncProbeSpecCount = 19;
    extern const KnownAsyncProbeSpec kKnownAsyncProbeSpecs[];

    bool TryReadPointerValue(uintptr_t address, uintptr_t &value);
    std::wstring WideText(const char *text);
    std::wstring ResolveModuleFilename(HMODULE module);
    std::wstring WideUtf8(const char *text);
    bool BuildExportCache(HMODULE module, ExportCache &cache);
    bool TryResolveExportSymbol(HMODULE module, uintptr_t address, std::wstring &symbol_name, uintptr_t &symbol_offset);
}

#endif
