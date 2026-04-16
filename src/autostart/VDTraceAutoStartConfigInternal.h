#ifndef VDTRACE_AUTOSTART_CONFIG_INTERNAL_H
#define VDTRACE_AUTOSTART_CONFIG_INTERNAL_H

#include "autostart/VDTraceAutoStartConfig.h"

#include <map>

namespace vdtrace::autostart::detail
{
    using SectionMap = std::map<std::wstring, std::map<std::wstring, std::wstring>>;

    std::filesystem::path GetAutoStartModuleDirectory();
    std::wstring TrimAutoStartText(const std::wstring &text);
    bool ReadAutoStartUtf8TextFile(const std::filesystem::path &path, std::wstring &text);
    bool WriteAutoStartUtf8TextFile(const std::filesystem::path &path, const std::string &text);
    std::string BuildDefaultAutoStartConfigText();
    std::string NarrowAutoStartUtf8(const std::wstring &text);
    bool ParseAutoStartBool(const std::wstring &text, bool fallback);
    bool ParseAutoStartUint64(const std::wstring &text, uint64_t &value);
    bool ParseAutoStartCallDepth(const std::wstring &text, uint32_t &value);
    bool ParseAutoStartIniText(const std::wstring &text, SectionMap &sections);
    std::wstring GetAutoStartValue(const SectionMap &sections, const wchar_t *section_name, const wchar_t *key_name, const std::wstring &fallback);
    std::filesystem::path ResolveConfigRelativePath(const std::filesystem::path &base_directory, const std::wstring &text);
}

#endif
