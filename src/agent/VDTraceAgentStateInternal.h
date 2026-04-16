#ifndef VDTRACE_AGENT_STATE_INTERNAL_H
#define VDTRACE_AGENT_STATE_INTERNAL_H

#include "agent/VDTraceAgentState.h"

namespace vdtrace::agent::state_detail
{
    std::wstring TrimText(const std::wstring &text);
    bool ParseAddressText(const std::wstring &text, uintptr_t &value);
    std::wstring BuildTimestampSuffix();
    FlowHitPolicy NormalizeHitPolicy(uint32_t value);
    std::filesystem::path GetModulePathFromAddress(const void *address);
}

#endif
