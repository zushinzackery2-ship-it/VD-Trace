#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        struct TextDepthModuleRule
        {
            std::wstring module_name;
            uint32_t max_call_depth = kUnlimitedCallDepth;
            DepthFilterExecutionMode execution_mode = DepthFilterExecutionMode::Edge;
        };

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

    bool ParseDepthFilterSpec(
        const std::wstring &text,
        const std::vector<ModuleRange> &tracked_modules,
        const std::vector<ModuleRange> &system_modules,
        ResolvedDepthFilterSet &filters,
        std::wstring &error)
    {
        error.clear();
        filters = {};

        const std::wstring trimmed = TrimDepthText(text);
        if (trimmed.empty())
        {
            return true;
        }

        std::vector<std::wstring> tokens;
        SplitDepthFilterTokens(trimmed, tokens);

        std::vector<TextDepthModuleRule> module_rules;
        for (const auto &token : tokens)
        {
            if (StartsWithInsensitive(token, L"outside="))
            {
                if (!ParseDepthModeValue(
                        token.substr(8),
                        filters.outside_module_depth,
                        filters.outside_module_execution_mode,
                        error))
                {
                    error = L"模块外层级规则无效: " + token + L"。 " + error;
                    return false;
                }

                filters.has_outside_module_depth = true;
                continue;
            }

            if (StartsWithInsensitive(token, L"anon="))
            {
                if (!ParseDepthModeValue(
                        token.substr(5),
                        filters.anonymous_exec_depth,
                        filters.anonymous_exec_execution_mode,
                        error))
                {
                    error = L"匿名执行页层级规则无效: " + token + L"。 " + error;
                    return false;
                }

                filters.has_anonymous_exec_depth = true;
                continue;
            }

            if (StartsWithInsensitive(token, L"anonymous="))
            {
                if (!ParseDepthModeValue(
                        token.substr(10),
                        filters.anonymous_exec_depth,
                        filters.anonymous_exec_execution_mode,
                        error))
                {
                    error = L"匿名执行页层级规则无效: " + token + L"。 " + error;
                    return false;
                }

                filters.has_anonymous_exec_depth = true;
                continue;
            }

            if (StartsWithInsensitive(token, L"module="))
            {
                const std::wstring payload = TrimDepthText(token.substr(7));
                DepthFilterExecutionMode execution_mode = DepthFilterExecutionMode::Edge;
                std::wstring body = payload;
                const size_t mode_split = payload.find_last_of(L':');
                if (mode_split != std::wstring::npos)
                {
                    DepthFilterExecutionMode parsed_mode = DepthFilterExecutionMode::Edge;
                    if (ParseExecutionModeValue(payload.substr(mode_split + 1), parsed_mode))
                    {
                        body = TrimDepthText(payload.substr(0, mode_split));
                        execution_mode = parsed_mode;
                    }
                }

                const size_t split = body.find_last_of(L':');
                if (split == std::wstring::npos)
                {
                    error = L"模块层级规则无效，格式必须是 module=<模块名>:<层级>[:edge|tf]。";
                    return false;
                }

                TextDepthModuleRule rule = {};
                rule.module_name = TrimDepthText(body.substr(0, split));
                if (rule.module_name.empty())
                {
                    error = L"模块层级规则缺少模块名。";
                    return false;
                }

                if (!ParseDepthValue(body.substr(split + 1), rule.max_call_depth, error))
                {
                    error = L"模块层级规则无效: " + token + L"。 " + error;
                    return false;
                }
                rule.execution_mode = execution_mode;

                module_rules.push_back(std::move(rule));
                continue;
            }

            error = L"未知的层级过滤规则: " + token
                + L"。支持 outside=<层级>[:edge|tf] / anon=<层级>[:edge|tf] / module=<模块名>:<层级>[:edge|tf]。";
            return false;
        }

        for (const auto &rule : module_rules)
        {
            ModuleRange range = {};
            if (!ResolveModuleRange(rule.module_name, range, error))
            {
                error = L"模块层级规则解析失败: " + rule.module_name + L"。 " + error;
                return false;
            }

            if (IsSystemModuleRange(system_modules, range))
            {
                error = L"系统模块固定只记边，不支持单独配置层级: " + rule.module_name;
                return false;
            }

            AddOrReplaceModuleRule(filters.module_rules, range, rule.max_call_depth, rule.execution_mode);
        }

        std::sort(
            filters.module_rules.begin(),
            filters.module_rules.end(),
            [](const ResolvedDepthFilterModuleRule &left, const ResolvedDepthFilterModuleRule &right)
            {
                return left.range.base < right.range.base;
            });

        for (const auto &rule : filters.module_rules)
        {
            const ModuleRange *tracked = FindModuleRange(tracked_modules, rule.range.base);
            if (tracked != nullptr && tracked->base != rule.range.base)
            {
                error = L"模块层级规则与主记录模块范围发生了异常重叠。";
                return false;
            }
        }

        return true;
    }
}
