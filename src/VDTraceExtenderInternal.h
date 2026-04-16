#ifndef VDTRACE_EXTENDER_INTERNAL_H
#define VDTRACE_EXTENDER_INTERNAL_H

#include "VDTraceExtender.h"
#include "VDTraceExtenderSupport.h"

namespace vdtrace::extender_detail
{
    constexpr uintptr_t kStaticWindowSize = 1024;
    constexpr uintptr_t kStaticWindowMask = ~(kStaticWindowSize - 1);
    constexpr uint32_t kMaxStaticWindowsPerInstruction = 4;

    struct PendingEntrySnapshot
    {
        bool valid = false;
        uintptr_t rip = 0;
        ThreadContextSnapshot context = {};
    };

    struct StaticWindowKey
    {
        uintptr_t instruction = 0;
        uintptr_t window_base = 0;

        bool operator==(const StaticWindowKey &other) const
        {
            return instruction == other.instruction
                && window_base == other.window_base;
        }
    };

    struct StaticWindowKeyHash
    {
        size_t operator()(const StaticWindowKey &key) const
        {
            size_t value = std::hash<uintptr_t> {}(key.instruction);
            value ^= std::hash<uintptr_t> {}(key.window_base) + 0x9e3779b9 + (value << 6) + (value >> 2);
            return value;
        }
    };

    struct StaticWindowCandidate
    {
        uintptr_t instruction = 0;
        uintptr_t address = 0;
        uintptr_t window_base = 0;
        uintptr_t capture_begin = 0;
        size_t capture_size = 0;
        uint8_t width = 0;
        bool from_address_only = false;
        wchar_t section_name[16] = {};
        std::string operand_text;
    };

    std::string FormatBytes(const uint8_t *bytes, uint8_t size);
    std::string FormatAscii(const uint8_t *bytes, uint8_t size);
    std::string BuildExtendWriteSuffix(const extender::ExtendedMemoryAccess &access);
    bool ResolveSectionRange(
        uintptr_t module_base,
        uintptr_t address,
        wchar_t *name,
        size_t name_capacity,
        uintptr_t &section_begin,
        uintptr_t &section_end);
    std::string Narrow(const std::wstring &text);
    std::string BuildStaticWindowBlock(const StaticWindowCandidate &candidate);
}

namespace vdtrace
{
    struct TextFileRecorderExtender::Impl
    {
        const TextFileRecorderHeapPeek &heap_peek;
        std::unordered_map<DWORD, extender_detail::PendingEntrySnapshot> entry_slots;
        std::unordered_set<extender_detail::StaticWindowKey, extender_detail::StaticWindowKeyHash> emitted_static_windows;
        std::unordered_map<uintptr_t, uint32_t> per_instruction_static_counts;

        explicit Impl(const TextFileRecorderHeapPeek &peek);
        bool TryBuildStaticWindowCandidate(
            const extender::ExtendedMemoryAccess &access,
            extender_detail::StaticWindowCandidate &candidate) const;
        std::string ProcessEvent(const RecorderQueuedEvent &event);
    };
}

#endif
