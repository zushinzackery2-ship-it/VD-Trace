#ifndef VDTRACE_AGENT_IPC_H
#define VDTRACE_AGENT_IPC_H

#include "VDTrace/VDTraceIpc.h"

namespace vdtrace::agent
{
    class IpcServer
    {
      public:
        static IpcServer &Instance();

        bool Start();
        void RequestStop();

      private:
        IpcServer() = default;
        ~IpcServer() = default;

        IpcServer(const IpcServer &) = delete;
        IpcServer &operator=(const IpcServer &) = delete;

        static DWORD WINAPI WorkerEntry(LPVOID param);
        void Run();
        void HandleCommand(const IpcCommand &command, IpcResponse &response);

        std::atomic<bool> running_ = false;
        std::atomic<bool> stop_requested_ = false;
        HANDLE thread_ = nullptr;
        std::wstring pipe_name_;
        std::mutex lock_;
    };
}

#endif
