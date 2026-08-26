#ifndef VDTRACE_AGENT_DUMP_INTERNAL_H
#define VDTRACE_AGENT_DUMP_INTERNAL_H

#include "agent/VDTraceAgentDump.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vdtrace::agent::dump_detail
{
    struct ModuleInfo
    {
        std::wstring name;
        std::wstring path;
        uintptr_t base = 0;
        size_t size = 0;
    };

    std::wstring NarrowPathToFilename(const std::wstring &path);
    std::string NarrowUtf8(const std::wstring &text);
    std::filesystem::path GetModulePathFromAddress(const void *address);
    std::filesystem::path GetAgentModuleDirectory();
    bool IsSystemPath(const std::wstring &path);
    bool IsReadableProtection(DWORD protection);
    bool EnumerateModules(bool include_system_modules, std::vector<ModuleInfo> &modules, std::wstring &error);
    bool CopyModuleImage(const ModuleInfo &module, std::vector<std::uint8_t> &bytes, std::wstring &error);
    bool WriteBinaryFile(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes);
    std::filesystem::path BuildDumpPath(
        const std::filesystem::path &output_directory,
        const std::wstring &module_name,
        const std::wstring &suffix);
}

#endif
