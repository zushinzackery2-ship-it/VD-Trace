#include "pch.h"
#include "agent/VDTraceAgentIpcInternal.h"

namespace vdtrace::agent::ipc_detail
{
    namespace
    {
        constexpr DWORD kIpcServerWakeRetryDelayMs = 10;
        constexpr int kIpcServerWakeAttempts = 100;

        bool TryWakePipeServer(const std::wstring &pipe_name)
        {
            HANDLE pipe = CreateFileW(
                pipe_name.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);
            if (pipe == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            CloseHandle(pipe);
            return true;
        }
    }

    void PrepareResponse(IpcResponse &response)
    {
        response = {};
        response.version = kIpcVersion;
        response.status = IPC_STATUS_OK;
    }

    bool BuildPipeSecurityAttributes(SECURITY_ATTRIBUTES &attributes, SECURITY_DESCRIPTOR &descriptor)
    {
        std::memset(&attributes, 0, sizeof(attributes));
        std::memset(&descriptor, 0, sizeof(descriptor));
        if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION))
        {
            return false;
        }

        if (!SetSecurityDescriptorDacl(&descriptor, TRUE, nullptr, FALSE))
        {
            return false;
        }

        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = &descriptor;
        attributes.bInheritHandle = FALSE;
        return true;
    }

    void CancelWorkerIo(HANDLE thread)
    {
        if (thread != nullptr)
        {
            CancelSynchronousIo(thread);
        }
    }

    void WakePipeServer(const std::wstring &pipe_name, HANDLE thread)
    {
        if (pipe_name.empty())
        {
            return;
        }

        for (int attempt = 0; attempt < kIpcServerWakeAttempts; attempt++)
        {
            if (thread != nullptr && WaitForSingleObject(thread, 0) == WAIT_OBJECT_0)
            {
                return;
            }

            if (TryWakePipeServer(pipe_name))
            {
                return;
            }

            Sleep(kIpcServerWakeRetryDelayMs);
        }
    }
}