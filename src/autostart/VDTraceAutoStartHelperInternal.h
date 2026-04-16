#ifndef VDTRACE_AUTOSTART_HELPER_INTERNAL_H
#define VDTRACE_AUTOSTART_HELPER_INTERNAL_H

#include "autostart/VDTraceAutoStartRuntime.h"

namespace vdtrace::autostart::helper_detail
{
    std::wstring FormatDepthText(uint32_t value);
    std::wstring FormatExecutionModeText(TraceExecutionMode mode);
    std::wstring BuildDepthFilterSpec(const TraceConfig &config);
    std::filesystem::path GetModuleDirectoryFromAddress(const void *address);
    std::wstring ReadEnvironmentText(const wchar_t *name);
    std::wstring BuildTimestampSuffix();
    std::wstring CurrentProcessPath();
    std::string NarrowUtf8(const std::wstring &text);
    std::filesystem::path BuildDefaultLogPath();
}

#endif
