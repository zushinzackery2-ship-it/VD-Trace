#ifndef VDTRACE_AGENT_DUMP_H
#define VDTRACE_AGENT_DUMP_H

#include <string>

namespace vdtrace::agent
{
    bool BuildLoadedModuleList(bool include_system_modules, std::string &message);
    bool DumpModuleToDirectory(const std::wstring &module_name, const std::wstring &output_directory, std::string &message);
}

#endif
