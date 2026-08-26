#include "pch.h"
#include "lite/LiteTraceConfig.h"

#include "autostart/VDTraceAutoStartConfigInternal.h"

namespace vdtrace::lite
{
    namespace as = vdtrace::autostart::detail;

    namespace
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
    }

    std::vector<std::wstring> SplitModuleNames(const std::wstring &text)
    {
        std::vector<std::wstring> modules;
        const std::wstring trimmed = as::TrimAutoStartText(text);
        if (trimmed.empty() || trimmed == L"-")
        {
            return modules;
        }

        size_t begin = 0;
        while (begin < trimmed.size())
        {
            const size_t end = trimmed.find_first_of(L",;\r\n", begin);
            const std::wstring part = as::TrimAutoStartText(
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

    bool ParseLiteTriggerPoint(const std::wstring &text, std::wstring &module_name, uintptr_t &relative_address, std::wstring &error)
    {
        module_name.clear();
        relative_address = 0;
        error.clear();

        const std::wstring raw = as::TrimAutoStartText(text);
        if (raw.empty())
        {
            return true;
        }

        const size_t split = raw.find_first_of(L"!+");
        if (split == std::wstring::npos)
        {
            uint64_t absolute = 0;
            if (!as::ParseAutoStartUint64(raw, absolute))
            {
                error = L"trigger_point 无效，应为 0x地址 或 模块+0x偏移 / 模块!0x偏移。";
                return false;
            }

            relative_address = static_cast<uintptr_t>(absolute);
            return true;
        }

        module_name = as::TrimAutoStartText(raw.substr(0, split));
        uint64_t offset = 0;
        if (module_name.empty() || !as::ParseAutoStartUint64(raw.substr(split + 1), offset))
        {
            module_name.clear();
            error = L"trigger_point 无效，应为 模块+0x偏移 / 模块!0x偏移。";
            return false;
        }

        relative_address = static_cast<uintptr_t>(offset);
        return true;
    }

    std::wstring BuildLiteDepthFilterSpec(const LiteTraceConfig &config)
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
        if (module_rules.empty())
        {
            return spec;
        }

        size_t begin = 0;
        while (begin <= module_rules.size())
        {
            size_t end = module_rules.find_first_of(L",;", begin);
            if (end == std::wstring::npos)
            {
                end = module_rules.size();
            }

            const std::wstring token = as::TrimAutoStartText(module_rules.substr(begin, end - begin));
            if (!token.empty())
            {
                append_token(L"module=" + token);
            }

            if (end == module_rules.size())
            {
                break;
            }
            begin = end + 1;
        }

        return spec;
    }

    std::wstring ResolveLiteOutputPath(const LiteTraceConfig &config)
    {
        std::filesystem::path path = as::TrimAutoStartText(config.output_path);
        if (path.empty())
        {
            path = L".\\traces\\LiteTrace.log";
        }

        if (path.is_relative())
        {
            const std::filesystem::path base = config.config_directory.empty()
                ? LiteTraceModuleDirectory()
                : config.config_directory;
            path = base / path;
        }

        path = path.lexically_normal();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        return path.wstring();
    }
}
