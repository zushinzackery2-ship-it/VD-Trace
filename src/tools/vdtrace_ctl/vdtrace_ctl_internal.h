#ifndef VDTRACE_CTL_INTERNAL_H
#define VDTRACE_CTL_INTERNAL_H

#include "VDTrace/VDTraceIpc.h"
#include "control/ipc_client/VDTraceControlSupport.h"

#include <Windows.h>

#include <string>
#include <vector>

namespace vdtrace::tools::cli
{
    void PrintUsage();
    bool ParseDword(const wchar_t *text, DWORD &value);
    bool ParseUint64(const wchar_t *text, uint64_t &value);
    bool ParseCallDepthText(const std::wstring &text, uint32_t &value);
    bool ParseHexBytes(const std::wstring &text, std::vector<uint8_t> &bytes);
    std::string NarrowUtf8(const std::wstring &text);
    bool ApplyConfigureOption(const std::wstring &text, vdtrace::IpcCommand &command);
    void PrintResult(const CommandResult &result);
    vdtrace::IpcCommand BuildSimpleCommand(vdtrace::IpcCommandType type);
    bool BuildConfigureCommand(int argc, wchar_t **argv, int start_index, vdtrace::IpcCommand &command);
}

#endif
