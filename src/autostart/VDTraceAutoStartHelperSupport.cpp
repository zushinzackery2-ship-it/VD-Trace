#include "pch.h"
#include "autostart/VDTraceAutoStartHelperInternal.h"

namespace vdtrace::autostart::helper_detail
{
    std::wstring FormatDepthText(uint32_t value)
    {
        if (value == kUnlimitedCallDepth)
        {
            return L"all";
        }
        if (value == 0)
        {
            return L"single";
        }
        return std::to_wstring(value);
    }

    std::wstring FormatExecutionModeText(TraceExecutionMode mode)
    {
        return mode == TraceExecutionMode::TrapFlag ? L"tf" : L"edge";
    }

    std::wstring BuildDepthFilterSpec(const TraceConfig &config)
    {
        std::wstring spec;
        const auto append_token = [&spec](const std::wstring &token)
        {
            if (token.empty())
            {
                return;
            }
            if (!spec.empty())
            {
                spec += L",";
            }
            spec += token;
        };

        if (config.has_outside_module_depth)
        {
            append_token(L"outside=" + FormatDepthText(config.outside_module_depth) + L":" + FormatExecutionModeText(config.outside_module_execution_mode));
        }

        if (config.has_anonymous_exec_depth)
        {
            append_token(L"anon=" + FormatDepthText(config.anonymous_exec_depth) + L":" + FormatExecutionModeText(config.anonymous_exec_execution_mode));
        }

        const std::wstring module_rules = config.module_call_depths;
        if (!module_rules.empty())
        {
            size_t begin = 0;
            while (begin <= module_rules.size())
            {
                size_t end = module_rules.find_first_of(L",;", begin);
                if (end == std::wstring::npos)
                {
                    end = module_rules.size();
                }

                std::wstring token = module_rules.substr(begin, end - begin);
                const size_t token_begin = token.find_first_not_of(L" \t\r\n");
                if (token_begin != std::wstring::npos)
                {
                    const size_t token_end = token.find_last_not_of(L" \t\r\n");
                    token = token.substr(token_begin, token_end - token_begin + 1);
                    if (!token.empty())
                    {
                        append_token(L"module=" + token);
                    }
                }

                if (end == module_rules.size())
                {
                    break;
                }
                begin = end + 1;
            }
        }

        return spec;
    }

    std::filesystem::path GetModuleDirectoryFromAddress(const void *address)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &module))
        {
            return std::filesystem::current_path();
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
            return std::filesystem::current_path();
        }

        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
    }

    std::wstring ReadEnvironmentText(const wchar_t *name)
    {
        wchar_t buffer[2048] = {};
        const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
        if (length == 0 || length >= std::size(buffer))
        {
            return {};
        }
        return std::wstring(buffer, buffer + length);
    }

    std::wstring BuildTimestampSuffix()
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        wchar_t buffer[64] = {};
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

    std::wstring CurrentProcessPath()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return buffer;
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

    std::filesystem::path BuildDefaultLogPath()
    {
        const auto base = GetModuleDirectoryFromAddress(reinterpret_cast<const void *>(&BuildDefaultLogPath));
        std::error_code ec;
        std::filesystem::create_directories(base / L"traces", ec);
        return base / L"traces" / (L"VDTraceAutoStart-" + BuildTimestampSuffix() + L".log");
    }
}
