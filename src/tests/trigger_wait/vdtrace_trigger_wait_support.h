#ifndef VDTRACE_TRIGGER_WAIT_SUPPORT_H
#define VDTRACE_TRIGGER_WAIT_SUPPORT_H

#include <cstdint>
#include <filesystem>
#include <string>

namespace trigger_wait_test
{
    std::wstring GetExecutablePath();
    std::filesystem::path GetExecutableDirectory();
    std::wstring BuildLogPath();
    std::wstring BuildHelperPath();
    std::wstring GetFilenameOnly(const std::wstring &path);
    std::wstring ReadFirstMatchingEventLine(const std::wstring &path, const std::wstring &pattern);
    uint64_t ParseStateCounter(const std::wstring &state_text, const std::wstring &key);
}

#endif
