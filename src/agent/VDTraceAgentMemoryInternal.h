#ifndef VDTRACE_AGENT_MEMORY_INTERNAL_H
#define VDTRACE_AGENT_MEMORY_INTERNAL_H

#include "agent/VDTraceAgentMemory.h"
#include "VDTrace/VDTraceIpc.h"
#include "VDTraceInternal.h"

namespace vdtrace::agent::memory_detail
{
    constexpr uint32_t kMaxMemoryTransferSize = kIpcMemoryWriteCapacity;

    std::wstring TrimText(const std::wstring &text);
    std::wstring WidenUtf8(const char *text);
    std::string NarrowUtf8(const std::wstring &text);
    bool ParseAddressNumber(const std::wstring &text, uintptr_t &value);
    bool ResolveAddressText(const std::wstring &text, uintptr_t &address, std::string &message);
    std::wstring ModuleNameFromHandle(HMODULE module);
    std::wstring DescribeResolvedAddress(uintptr_t address);
    const wchar_t *ProtectText(DWORD protect);
    const wchar_t *StateText(DWORD state);
    const wchar_t *TypeText(DWORD type);
    bool IsWritableProtection(DWORD protect);
    bool WriteBytes(uintptr_t address, const uint8_t *bytes, uint32_t size);
    std::wstring FormatAscii(const uint8_t *bytes, uint32_t size);
    std::wstring FormatUtf16Preview(const uint8_t *bytes, uint32_t size);
    std::wstring FormatHexLine(uintptr_t line_address, const uint8_t *bytes, uint32_t size);
    std::string BuildReadMessage(uintptr_t address, const uint8_t *bytes, uint32_t size, const MEMORY_BASIC_INFORMATION &mbi);
    std::string BuildWriteMessage(
        uintptr_t address,
        const uint8_t *before,
        const uint8_t *after,
        uint32_t size,
        const MEMORY_BASIC_INFORMATION &mbi);
}

#endif
