#ifndef VDTRACE_AGENT_IPC_INTERNAL_H
#define VDTRACE_AGENT_IPC_INTERNAL_H

#include "VDTrace/VDTraceIpc.h"

#include <string>

namespace vdtrace::agent::ipc_detail
{
    constexpr DWORD kIpcServerPipeTimeoutMs = 1000;
    constexpr DWORD kIpcServerStopWaitMs = 5000;

    void PrepareResponse(IpcResponse &response);
    bool BuildPipeSecurityAttributes(SECURITY_ATTRIBUTES &attributes, SECURITY_DESCRIPTOR &descriptor);
    void CancelWorkerIo(HANDLE thread);
    void WakePipeServer(const std::wstring &pipe_name, HANDLE thread);
}

#endif