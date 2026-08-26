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
}
