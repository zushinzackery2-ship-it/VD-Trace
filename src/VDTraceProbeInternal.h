#ifndef VDTRACE_PROBE_INTERNAL_H
#define VDTRACE_PROBE_INTERNAL_H

#include "VDTraceInternal.h"

namespace vdtrace::probe_detail
{
    std::wstring TrimText(const std::wstring &text);
    std::wstring ToLowerCopy(const std::wstring &text);
    std::vector<std::wstring> SplitText(const std::wstring &text, wchar_t separator);
    bool ParseUnsignedText(const std::wstring &text, uintptr_t &value);
    bool ParseSignedText(const std::wstring &text, intptr_t &value);
    bool ResolveConfiguredAddressText(const std::wstring &text, uintptr_t &address, std::wstring &error);
    bool ParseProbeRegister(const std::wstring &text, ProbeRegister &reg);
    bool ParseProbeRegisterExpression(const std::wstring &text, ProbeRegister &reg, intptr_t &offset);
    bool CopyProbeLabel(const std::wstring &text, wchar_t *buffer, size_t capacity);
    bool ParseProbeSize(const std::wstring &text, uint8_t &size);
    bool ParseProbeCaptureToken(const std::wstring &text, ResolvedProbeCapture &capture, std::wstring &error);
    uintptr_t ReadProbeRegisterValue(const CONTEXT &context, ProbeRegister reg);
    const ResolvedValueProbe *FindValueProbeByAddress(const std::vector<ResolvedValueProbe> &probes, uintptr_t address);
    void FillProbeCapture(const ResolvedProbeCapture &resolved, const CONTEXT &context, ProbeCapture &capture);
    bool ParseProbeExitKind(const std::wstring &text, ProbeExitKind &exit_kind);
    bool ParseProbeWatchToken(const std::wstring &text, ResolvedProbeWatch &watch, std::wstring &error);
    bool ParseObserverRule(const std::wstring &text, ResolvedValueProbe &probe, std::wstring &error);
    bool ParseLegacyCaptureRule(const std::wstring &rule, std::vector<ResolvedValueProbe> &probes, std::wstring &error);
}

#endif
