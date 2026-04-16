#include "pch.h"
#include "agent/VDTraceAgentMemoryInternal.h"

#include <Psapi.h>

namespace vdtrace::agent::memory_detail
{
    std::wstring TrimText(const std::wstring &text)
    {
        const size_t begin = text.find_first_not_of(L" \t\r\n");
        if (begin == std::wstring::npos)
        {
            return {};
        }

        const size_t end = text.find_last_not_of(L" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }

    std::wstring WidenUtf8(const char *text)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return {};
        }

        const int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (count <= 1)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), count);
        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }
        return result;
    }

    std::string NarrowUtf8(const std::wstring &text)
    {
        if (text.empty())
        {
            return {};
        }

        const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
        {
            return {};
        }

        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }
        return result;
    }

    bool ParseAddressNumber(const std::wstring &text, uintptr_t &value)
    {
        const std::wstring trimmed = TrimText(text);
        if (trimmed.empty())
        {
            value = 0;
            return false;
        }

        wchar_t *end = nullptr;
        const unsigned long long parsed = wcstoull(trimmed.c_str(), &end, 0);
        if (end == trimmed.c_str() || (end != nullptr && *end != L'\0'))
        {
            value = 0;
            return false;
        }

        value = static_cast<uintptr_t>(parsed);
        return true;
    }

    bool ResolveAddressText(const std::wstring &text, uintptr_t &address, std::string &message)
    {
        message.clear();
        address = 0;

        const std::wstring raw = TrimText(text);
        if (raw.empty())
        {
            message = "address is empty";
            return false;
        }

        const size_t split = raw.find_first_of(L"!+");
        if (split == std::wstring::npos)
        {
            if (!ParseAddressNumber(raw, address))
            {
                message = "invalid address. use 0xADDRESS or module!0xRVA or module+0xRVA";
                return false;
            }
            return true;
        }

        const std::wstring module_name = TrimText(raw.substr(0, split));
        uintptr_t relative = 0;
        if (module_name.empty() || !ParseAddressNumber(raw.substr(split + 1), relative))
        {
            message = "invalid address. use module!0xRVA or module+0xRVA";
            return false;
        }

        ModuleRange range = {};
        std::wstring error;
        if (!ResolveModuleRange(module_name, range, error))
        {
            message = NarrowUtf8(error);
            return false;
        }

        address = range.base + relative;
        return true;
    }

    std::wstring ModuleNameFromHandle(HMODULE module)
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        if (length == 0)
        {
            return L"unknown";
        }

        return std::filesystem::path(std::wstring(path, length)).filename().wstring();
    }

    std::wstring DescribeResolvedAddress(uintptr_t address)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &module))
        {
            std::wostringstream out;
            out << L"0x" << std::hex << address;
            return out.str();
        }

        MODULEINFO info = {};
        if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        {
            std::wostringstream out;
            out << L"0x" << std::hex << address;
            return out.str();
        }

        std::wostringstream out;
        out << ModuleNameFromHandle(module) << L"+0x" << std::hex
            << (address - reinterpret_cast<uintptr_t>(module));
        return out.str();
    }

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

    bool IsWritableProtection(DWORD protect)
    {
        const DWORD basic = protect & 0xff;
        return basic == PAGE_READWRITE
            || basic == PAGE_WRITECOPY
            || basic == PAGE_EXECUTE_READWRITE
            || basic == PAGE_EXECUTE_WRITECOPY;
    }

    bool WriteBytes(uintptr_t address, const uint8_t *bytes, uint32_t size)
    {
        __try
        {
            std::memcpy(reinterpret_cast<void *>(address), bytes, size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
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
