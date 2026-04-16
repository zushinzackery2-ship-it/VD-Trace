#include "pch.h"
#include "agent/VDTraceAgentIpc.h"

extern "C"
{
    __declspec(dllexport) BOOL vdtrace_agent_bootstrap_ipc(void);

    __declspec(dllexport) BOOL vdtrace_loader_bootstrap(void)
    {
        return vdtrace_agent_bootstrap_ipc();
    }

    __declspec(dllexport) BOOL vdtrace_agent_bootstrap_ipc(void)
    {
        return vdtrace::agent::IpcServer::Instance().Start() ? TRUE : FALSE;
    }

    __declspec(dllexport) void vdtrace_agent_request_stop(void)
    {
        vdtrace::agent::IpcServer::Instance().RequestStop();
    }
}
