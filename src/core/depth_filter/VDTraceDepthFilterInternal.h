#ifndef VDTRACE_DEPTH_FILTER_INTERNAL_H
#define VDTRACE_DEPTH_FILTER_INTERNAL_H

#include "core/runtime/VDTraceInternal.h"

#include <string>
#include <vector>

namespace vdtrace::depth_filter_detail
{
    struct TextDepthModuleRule
    {
        std::wstring module_name;
        uint32_t max_call_depth = kUnlimitedCallDepth;
        DepthFilterExecutionMode execution_mode = DepthFilterExecutionMode::Edge;
    };

    std::wstring TrimDepthText(const std::wstring &text);
    bool ParseDepthValue(const std::wstring &text, uint32_t &value, std::wstring &error);
    bool ParseExecutionModeValue(const std::wstring &text, DepthFilterExecutionMode &mode);
    bool ParseDepthModeValue(
        const std::wstring &text,
        uint32_t &value,
        DepthFilterExecutionMode &mode,
        std::wstring &error);
    bool StartsWithInsensitive(const std::wstring &text, const wchar_t *prefix);
    void SplitDepthFilterTokens(const std::wstring &text, std::vector<std::wstring> &tokens);
    bool IsSystemModuleRange(const std::vector<ModuleRange> &system_modules, const ModuleRange &range);
    void AddOrReplaceModuleRule(
        std::vector<ResolvedDepthFilterModuleRule> &rules,
        const ModuleRange &range,
        uint32_t max_call_depth,
        DepthFilterExecutionMode execution_mode);
}

#endif
