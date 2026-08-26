#include "pch.h"
#include "core/depth_filter/VDTraceDepthFilterInternal.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    using namespace depth_filter_detail;

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
