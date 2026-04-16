#ifndef VDTRACE_AGENT_PE_LAYOUT_H
#define VDTRACE_AGENT_PE_LAYOUT_H

#include <cstdint>
#include <vector>

namespace vdtrace::agent
{
    bool NormalizeMemoryDumpPeLayout(std::vector<std::uint8_t> &image_bytes);
}

#endif
