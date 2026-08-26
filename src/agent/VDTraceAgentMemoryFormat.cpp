#include "pch.h"
#include "agent/VDTraceAgentMemoryInternal.h"

namespace vdtrace::agent::memory_detail
{
    const wchar_t *ProtectText(DWORD protect)
    {
        switch (protect & 0xff)
        {
        case PAGE_NOACCESS: return L"NOACCESS";
        case PAGE_READONLY: return L"READONLY";
        case PAGE_READWRITE: return L"READWRITE";
        case PAGE_WRITECOPY: return L"WRITECOPY";
        case PAGE_EXECUTE: return L"EXECUTE";
        case PAGE_EXECUTE_READ: return L"EXECUTE_READ";
        case PAGE_EXECUTE_READWRITE: return L"EXECUTE_READWRITE";
        case PAGE_EXECUTE_WRITECOPY: return L"EXECUTE_WRITECOPY";
        default: return L"UNKNOWN";
        }
    }

    const wchar_t *StateText(DWORD state)
    {
        switch (state)
        {
        case MEM_COMMIT: return L"COMMIT";
        case MEM_RESERVE: return L"RESERVE";
        case MEM_FREE: return L"FREE";
        default: return L"UNKNOWN";
        }
    }

    const wchar_t *TypeText(DWORD type)
    {
        switch (type)
        {
        case MEM_IMAGE: return L"IMAGE";
        case MEM_MAPPED: return L"MAPPED";
        case MEM_PRIVATE: return L"PRIVATE";
        default: return L"UNKNOWN";
        }
    }

    std::wstring FormatAscii(const uint8_t *bytes, uint32_t size)
    {
        std::wstring text;
        text.reserve(size);
        for (uint32_t index = 0; index < size; ++index)
        {
            const uint8_t value = bytes[index];
            text.push_back(value >= 0x20 && value <= 0x7e ? static_cast<wchar_t>(value) : L'.');
        }
        return text;
    }

    std::wstring FormatUtf16Preview(const uint8_t *bytes, uint32_t size)
    {
        std::wstring text;
        for (uint32_t index = 0; index + 1 < size; index += 2)
        {
            const wchar_t value = static_cast<wchar_t>(bytes[index] | (static_cast<uint16_t>(bytes[index + 1]) << 8));
            if (value == 0)
            {
                break;
            }
            text.push_back((value >= 0x20 && value <= 0x7e) ? value : L'.');
        }
        return text;
    }

    std::wstring FormatHexLine(uintptr_t line_address, const uint8_t *bytes, uint32_t size)
    {
        std::wostringstream out;
        out << L"0x" << std::hex << line_address << L"  ";
        for (uint32_t index = 0; index < size; ++index)
        {
            if (index != 0)
            {
                out << L' ';
            }
            out << std::setw(2) << std::setfill(L'0') << std::hex << static_cast<unsigned>(bytes[index]);
        }
        out << L"  " << FormatAscii(bytes, size);
        return out.str();
    }

    std::string BuildReadMessage(uintptr_t address, const uint8_t *bytes, uint32_t size, const MEMORY_BASIC_INFORMATION &mbi)
    {
        std::wostringstream out;
        out << L"resolved=" << DescribeResolvedAddress(address) << L"\n";
        out << L"address=0x" << std::hex << address << L"\n";
        out << L"size=" << std::dec << size << L"\n";
        out << L"region_base=0x" << std::hex << reinterpret_cast<uintptr_t>(mbi.BaseAddress) << L"\n";
        out << L"region_size=0x" << std::hex << static_cast<uintptr_t>(mbi.RegionSize) << L"\n";
        out << L"state=" << StateText(mbi.State) << L"\n";
        out << L"type=" << TypeText(mbi.Type) << L"\n";
        out << L"protect=" << ProtectText(mbi.Protect) << L"\n";
        out << L"ascii=\"" << FormatAscii(bytes, size) << L"\"\n";
        out << L"utf16=\"" << FormatUtf16Preview(bytes, size) << L"\"\n";
        if (size >= sizeof(uint32_t))
        {
            uint32_t value32 = 0;
            std::memcpy(&value32, bytes, sizeof(value32));
            out << L"u32=0x" << std::hex << value32 << L"\n";
        }
        if (size >= sizeof(uint64_t))
        {
            uint64_t value64 = 0;
            std::memcpy(&value64, bytes, sizeof(value64));
            out << L"u64=0x" << std::hex << value64 << L"\n";
        }
        if (size >= sizeof(float))
        {
            float valuef = 0.0f;
            std::memcpy(&valuef, bytes, sizeof(valuef));
            out << std::dec << L"float=" << valuef << L"\n";
        }
        if (size >= sizeof(double))
        {
            double valued = 0.0;
            std::memcpy(&valued, bytes, sizeof(valued));
            out << std::dec << L"double=" << valued << L"\n";
        }
        out << L"dump:\n";
        for (uint32_t offset = 0; offset < size; offset += 16)
        {
            const uint32_t line_size = std::min<uint32_t>(16, size - offset);
            out << FormatHexLine(address + offset, bytes + offset, line_size) << L"\n";
        }
        return NarrowUtf8(out.str());
    }

    std::string BuildWriteMessage(
        uintptr_t address,
        const uint8_t *before,
        const uint8_t *after,
        uint32_t size,
        const MEMORY_BASIC_INFORMATION &mbi)
    {
        std::wostringstream out;
        out << L"resolved=" << DescribeResolvedAddress(address) << L"\n";
        out << L"address=0x" << std::hex << address << L"\n";
        out << L"size=" << std::dec << size << L"\n";
        out << L"protect=" << ProtectText(mbi.Protect) << L"\n";
        out << L"before=" << FormatHexLine(address, before, size) << L"\n";
        out << L"after=" << FormatHexLine(address, after, size) << L"\n";
        return NarrowUtf8(out.str());
    }
}
