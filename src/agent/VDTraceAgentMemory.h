#ifndef VDTRACE_AGENT_MEMORY_H
#define VDTRACE_AGENT_MEMORY_H

#include <cstdint>
#include <string>

namespace vdtrace::agent
{
    bool ReadMemoryText(const char *address_text, uint32_t size, std::string &message);
    bool WriteMemoryText(const char *address_text, const uint8_t *bytes, uint32_t size, std::string &message);
}

#endif
