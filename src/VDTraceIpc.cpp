#include "pch.h"
#include "VDTrace/VDTraceIpc.h"

namespace vdtrace
{
    std::wstring BuildPipeName(DWORD pid)
    {
        return L"\\\\.\\pipe\\VDTrace-" + std::to_wstring(pid);
    }

    void SetIpcMessage(IpcResponse &response, const std::string &message)
    {
        std::memset(response.message, 0, sizeof(response.message));
        const size_t copy_size = std::min(message.size(), sizeof(response.message) - 1);
        if (copy_size > 0)
        {
            std::memcpy(response.message, message.data(), copy_size);
        }
    }
}
