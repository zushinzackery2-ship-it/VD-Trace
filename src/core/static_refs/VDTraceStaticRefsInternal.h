#ifndef VDTRACE_STATIC_REFS_INTERNAL_H
#define VDTRACE_STATIC_REFS_INTERNAL_H

#include "core/runtime/VDTraceInternal.h"
#include "third_party/zydis/Zydis.h"

namespace vdtrace::static_refs_detail
{
    struct StaticReferenceHit
    {
        uintptr_t address = 0;
        uint8_t size = 0;
        wchar_t section_name[16] = {};
        uint8_t bytes[kEnhancedSampleMaxBytes] = {};
        bool has_dereference = false;
        uintptr_t dereference_address = 0;
        uint8_t dereference_size = 0;
        uint8_t dereference_bytes[kEnhancedSampleMaxBytes] = {};
        std::string dereference_guess;
        std::string dereference_detail;
    };

    ZydisDecoder &GetStaticRefDecoder();
    bool IsInterestingSectionName(const wchar_t *name);
    bool ResolveSectionName(uintptr_t module_base, uintptr_t address, wchar_t *name, size_t capacity);
    bool IsPrintableAscii(uint8_t value);
    bool LooksLikeString(const uint8_t *bytes, uint8_t size);
    bool IsExecutableAddress(uintptr_t address);
    bool IsReadableAddress(uintptr_t address, MEMORY_BASIC_INFORMATION &information);
    bool LooksLikePointerValue(uintptr_t value);
    bool LooksLikeCodePointerTable(const uint8_t *bytes, uint8_t size);
    std::string EscapePreviewText(const std::string &text);
    bool TryDecodeAsciiString(const uint8_t *bytes, uint8_t size, std::string &text);
    bool TryDecodeUtf16String(const uint8_t *bytes, uint8_t size, std::string &text);
    std::string Narrow(const std::wstring &text);
    std::string FormatResolvedAddressLabel(uintptr_t address);
    bool TryGuessObjectWithVftable(const uint8_t *bytes, uint8_t size, std::string &detail);
    bool TryGuessPointerArray(const uint8_t *bytes, uint8_t size, std::string &detail);
    void ExpandStaticPointerHit(StaticReferenceHit &hit);
    bool CaptureStaticReference(uintptr_t address, uint8_t operand_size, StaticReferenceHit &hit);
    std::string FormatBytes(const uint8_t *bytes, uint8_t size);
    std::string FormatAscii(const uint8_t *bytes, uint8_t size);
    std::string ResolveCachedAddressLabel(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache);
    std::string BuildStaticReferenceEntryKey(
        const StaticReferenceHit &hit,
        const std::string &slot_label,
        const std::string &dereference_label);
    void AppendStaticReferenceExport(
        const RecorderQueuedEvent &event,
        const StaticReferenceHit &hit,
        const std::string &slot_label,
        const std::string &dereference_label,
        std::unordered_map<std::string, StaticReferenceExportEntry> &export_entries,
        std::unordered_map<uintptr_t, std::string> &address_label_cache);
}

#endif
