#include "pch.h"
#include "agent/VDTraceAgentStateInternal.h"

#include "agent/VDTraceAgentMemoryInternal.h"

#include <regex>

namespace vdtrace::agent::state_detail
{
    std::wstring TrimText(const std::wstring &text)
    {
        return memory_detail::TrimText(text);
    }

    bool ParseAddressText(const std::wstring &text, uintptr_t &value)
    {
        return memory_detail::ParseAddressNumber(text, value);
    }

    std::wstring BuildTimestampSuffix()
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);

        wchar_t buffer[32] = {};
        swprintf_s(
            buffer,
            L"%04u%02u%02u-%02u%02u%02u",
            static_cast<unsigned>(now.wYear),
            static_cast<unsigned>(now.wMonth),
            static_cast<unsigned>(now.wDay),
            static_cast<unsigned>(now.wHour),
            static_cast<unsigned>(now.wMinute),
            static_cast<unsigned>(now.wSecond));
        return buffer;
    }

    FlowHitPolicy NormalizeHitPolicy(uint32_t value)
    {
        return value == static_cast<uint32_t>(FlowHitPolicy::EveryHit)
            ? FlowHitPolicy::EveryHit
            : FlowHitPolicy::FirstSeen;
    }

    std::filesystem::path GetModulePathFromAddress(const void *address)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &module))
        {
            return {};
        }

        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return std::filesystem::path(buffer);
    }
}

namespace vdtrace::agent
{
    std::vector<std::wstring> State::ParseModuleNames(const char *text)
    {
        const std::wstring raw = WidenUtf8(text);
        const std::wstring trimmed = state_detail::TrimText(raw);
        if (trimmed.empty() || trimmed == L"-")
        {
            return {};
        }

        std::vector<std::wstring> modules;
        size_t begin = 0;
        while (begin < trimmed.size())
        {
            const size_t end = trimmed.find_first_of(L",;\r\n", begin);
            const std::wstring part = state_detail::TrimText(
                trimmed.substr(begin, end == std::wstring::npos ? std::wstring::npos : end - begin));
            if (!part.empty() && part != L"-")
            {
                const auto it = std::find_if(
                    modules.begin(),
                    modules.end(),
                    [&](const std::wstring &existing)
                    {
                        return _wcsicmp(existing.c_str(), part.c_str()) == 0;
                    });
                if (it == modules.end())
                {
                    modules.push_back(part);
                }
            }

            if (end == std::wstring::npos)
            {
                break;
            }
            begin = end + 1;
        }

        return modules;
    }

    bool State::ParseTriggerPoint(const char *text, std::wstring &module_name, uintptr_t &address, std::string &message)
    {
        module_name.clear();
        address = 0;
        message.clear();

        const std::wstring raw = state_detail::TrimText(WidenUtf8(text));
        if (raw.empty())
        {
            return true;
        }

        const size_t split = raw.find_first_of(L"!+");
        if (split == std::wstring::npos)
        {
            if (!state_detail::ParseAddressText(raw, address))
            {
                message = "invalid trigger point. use 0xADDRESS or module!0xRVA or module+0xRVA";
                return false;
            }
            return true;
        }

        module_name = state_detail::TrimText(raw.substr(0, split));
        uintptr_t relative = 0;
        if (module_name.empty() || !state_detail::ParseAddressText(raw.substr(split + 1), relative))
        {
            message = "invalid trigger point. use module!0xRVA or module+0xRVA";
            module_name.clear();
            return false;
        }

        ModuleRange range = {};
        std::wstring error;
        if (!ResolveModuleRange(module_name, range, error))
        {
            message = NarrowUtf8(error);
            module_name.clear();
            address = 0;
            return false;
        }

        address = range.base + relative;
        return true;
    }

    std::filesystem::path State::GetAgentModuleDirectory()
    {
        const auto module_path = state_detail::GetModulePathFromAddress(reinterpret_cast<const void *>(&State::Instance));
        if (module_path.empty())
        {
            return std::filesystem::current_path();
        }

        return module_path.parent_path();
    }

    std::wstring State::NormalizeOutputPath(const std::wstring &text)
    {
        const std::wstring trimmed = state_detail::TrimText(text);
        static const std::wregex auto_output_pattern(
            LR"(^\.\\traces\\VDTrace(?:-\d+(?:-\d{8}-\d{6})?)?\.log$)",
            std::regex_constants::icase);
        const bool use_auto_path = trimmed.empty() || std::regex_match(trimmed, auto_output_pattern);
        const std::filesystem::path base_directory = GetAgentModuleDirectory();
        const std::filesystem::path resolved = use_auto_path
            ? base_directory / L"traces" / (L"VDTrace-" + std::to_wstring(GetCurrentProcessId()) + L"-" + state_detail::BuildTimestampSuffix() + L".log")
            : (std::filesystem::path(trimmed).is_relative()
                ? base_directory / std::filesystem::path(trimmed)
                : std::filesystem::path(trimmed));
        return resolved.lexically_normal().wstring();
    }

    std::wstring State::WidenUtf8(const char *text)
    {
        return memory_detail::WidenUtf8(text);
    }

    std::string State::NarrowUtf8(const std::wstring &text)
    {
        return memory_detail::NarrowUtf8(text);
    }
}
