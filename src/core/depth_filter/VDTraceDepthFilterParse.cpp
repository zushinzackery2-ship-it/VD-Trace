#include "pch.h"
#include "core/depth_filter/VDTraceDepthFilterInternal.h"

namespace vdtrace::depth_filter_detail
{
    std::wstring TrimDepthText(const std::wstring &text)
    {
        const size_t begin = text.find_first_not_of(L" \t\r\n");
        if (begin == std::wstring::npos)
        {
            return {};
        }

        const size_t end = text.find_last_not_of(L" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }

    bool ParseDepthValue(const std::wstring &text, uint32_t &value, std::wstring &error)
    {
        const std::wstring trimmed = TrimDepthText(text);
        if (trimmed.empty())
        {
            error = L"层级值为空。";
            return false;
        }

        if (_wcsicmp(trimmed.c_str(), L"all") == 0)
        {
            value = kUnlimitedCallDepth;
            return true;
        }

        if (_wcsicmp(trimmed.c_str(), L"same") == 0 || _wcsicmp(trimmed.c_str(), L"single") == 0)
        {
            value = 0;
            return true;
        }

        wchar_t *end = nullptr;
        const unsigned long long parsed = wcstoull(trimmed.c_str(), &end, 10);
        if (end == trimmed.c_str() || end == nullptr || *end != L'\0' || parsed > kUnlimitedCallDepth)
        {
            error = L"层级值无效，只支持 all / single / 数字。";
            return false;
        }

        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool ParseExecutionModeValue(const std::wstring &text, DepthFilterExecutionMode &mode)
    {
        const std::wstring trimmed = TrimDepthText(text);
        if (trimmed.empty())
        {
            return false;
        }

        if (_wcsicmp(trimmed.c_str(), L"edge") == 0)
        {
            mode = DepthFilterExecutionMode::Edge;
            return true;
        }

        if (_wcsicmp(trimmed.c_str(), L"tf") == 0)
        {
            mode = DepthFilterExecutionMode::TrapFlag;
            return true;
        }

        return false;
    }

    bool ParseDepthModeValue(
        const std::wstring &text,
        uint32_t &value,
        DepthFilterExecutionMode &mode,
        std::wstring &error)
    {
        const std::wstring trimmed = TrimDepthText(text);
        const size_t split = trimmed.find_last_of(L':');
        if (split != std::wstring::npos)
        {
            DepthFilterExecutionMode parsed_mode = DepthFilterExecutionMode::Edge;
            if (ParseExecutionModeValue(trimmed.substr(split + 1), parsed_mode))
            {
                if (!ParseDepthValue(trimmed.substr(0, split), value, error))
                {
                    return false;
                }

                mode = parsed_mode;
                return true;
            }
        }

        mode = DepthFilterExecutionMode::Edge;
        return ParseDepthValue(trimmed, value, error);
    }

    bool StartsWithInsensitive(const std::wstring &text, const wchar_t *prefix)
    {
        if (prefix == nullptr)
        {
            return false;
        }

        const size_t prefix_length = wcslen(prefix);
        return text.size() >= prefix_length && _wcsnicmp(text.c_str(), prefix, prefix_length) == 0;
    }

    void SplitDepthFilterTokens(const std::wstring &text, std::vector<std::wstring> &tokens)
    {
        tokens.clear();

        size_t begin = 0;
        while (begin <= text.size())
        {
            size_t end = text.find_first_of(L",;", begin);
            if (end == std::wstring::npos)
            {
                end = text.size();
            }

            const std::wstring token = TrimDepthText(text.substr(begin, end - begin));
            if (!token.empty())
            {
                tokens.push_back(token);
            }

            if (end == text.size())
            {
                break;
            }

            begin = end + 1;
        }
    }

    bool IsSystemModuleRange(const std::vector<ModuleRange> &system_modules, const ModuleRange &range)
    {
        const ModuleRange *resolved = FindModuleRange(system_modules, range.base);
        return resolved != nullptr && resolved->base == range.base;
    }

    void AddOrReplaceModuleRule(
        std::vector<ResolvedDepthFilterModuleRule> &rules,
        const ModuleRange &range,
        uint32_t max_call_depth,
        DepthFilterExecutionMode execution_mode)
    {
        for (auto &existing : rules)
        {
            if (existing.range.base == range.base && existing.range.size == range.size)
            {
                existing.range = range;
                existing.max_call_depth = max_call_depth;
                existing.execution_mode = execution_mode;
                return;
            }
        }

        ResolvedDepthFilterModuleRule rule = {};
        rule.range = range;
        rule.max_call_depth = max_call_depth;
        rule.execution_mode = execution_mode;
        rules.push_back(std::move(rule));
    }
}
