#ifndef VDTRACE_FUNCTION_PREVIEW_INTERNAL_H
#define VDTRACE_FUNCTION_PREVIEW_INTERNAL_H

#include "VDTraceExtenderSupport.h"
#include "VDTraceInternal.h"
#include "VDTraceHeapPeek.h"
#include "third_party/zydis/Zydis.h"

namespace vdtrace::function_preview_detail
{
    constexpr size_t kMaxPreviewInstructions = 128;
    constexpr size_t kMaxPreviewDumpBytes = 0x400;
    constexpr uintptr_t kMaxPreviewAddressDistance = 0x10000;

    struct PreviewInstruction
    {
        uintptr_t address = 0;
        uint8_t size = 0;
        uint8_t bytes[16] = {};
        std::string text;
    };

    using PreviewInlineSuffixMap = std::unordered_map<uintptr_t, std::vector<std::string>>;

    struct PreviewRange
    {
        bool valid = false;
        uintptr_t base = 0;
        size_t size = 0;
    };

    ZydisDecoder &GetPreviewDecoder();
    ZydisFormatter &GetPreviewFormatter();
    std::string Narrow(const std::wstring &text);
    bool IsExecutableProtection(DWORD protection);
    bool TryFormatAnonymousExecLabel(uintptr_t address, std::string &text);
    std::string ResolveAddressLabelText(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map);
    bool IsPreviewTerminator(const ZydisDecodedInstruction &instruction);
    std::string FormatInstructionBytes(const uint8_t *bytes, uint8_t size);
    bool TryResolveDirectTarget(
        uintptr_t runtime_address,
        const ZydisDecodedInstruction &instruction,
        const ZydisDecodedOperand *operands,
        uintptr_t &target);
    bool IsAddressInsidePreviewRange(const PreviewRange &range, uintptr_t address);
    bool IsInsidePreviewWindow(uintptr_t entry, uintptr_t address);
    PreviewRange ResolvePreviewRange(uintptr_t entry);
    bool TryBuildPreviewEntryContext(const RecorderQueuedEvent &event, ThreadContextSnapshot &entry_context);
    uintptr_t FindPreviewEntryBlockEnd(uintptr_t entry, const std::vector<PreviewInstruction> &instructions);
    void BuildPreviewInlineSuffixes(
        const RecorderQueuedEvent &event,
        const std::vector<PreviewInstruction> &instructions,
        const TextFileRecorderHeapPeek *heap_peek,
        PreviewInlineSuffixMap &suffixes);
}

#endif
