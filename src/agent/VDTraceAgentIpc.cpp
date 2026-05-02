#include "pch.h"
#include "agent/VDTraceAgentIpc.h"
#include "agent/VDTraceAgentState.h"

namespace vdtrace::agent
{
    namespace
    {
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
    }

    IpcServer &IpcServer::Instance()
    {
        static IpcServer instance;
        return instance;
    }

    bool IpcServer::Start()
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (running_.load() || thread_ != nullptr)
        {
            return true;
        }

        pipe_name_ = BuildPipeName(GetCurrentProcessId());
        stop_requested_.store(false);
        thread_ = CreateThread(nullptr, 0, WorkerEntry, this, 0, nullptr);
        if (thread_ == nullptr)
        {
            return false;
        }

        return true;
    }

    IpcServer::~IpcServer()
    {
        RequestStop();
    }

    void IpcServer::RequestStop()
    {
        stop_requested_.store(true);
        if (thread_ != nullptr)
        {
            WaitForSingleObject(thread_, 5000);
            CloseHandle(thread_);
            thread_ = nullptr;
        }
    }

    DWORD WINAPI IpcServer::WorkerEntry(LPVOID param)
    {
        auto *self = static_cast<IpcServer *>(param);
        self->Run();
        return 0;
    }

    void IpcServer::Run()
    {
        running_.store(true);
        while (!stop_requested_.load())
        {
            SECURITY_ATTRIBUTES security_attributes = {};
            SECURITY_DESCRIPTOR security_descriptor = {};
            SECURITY_ATTRIBUTES *pipe_security = nullptr;
            if (BuildPipeSecurityAttributes(security_attributes, security_descriptor))
            {
                pipe_security = &security_attributes;
            }

            HANDLE pipe = CreateNamedPipeW(
                pipe_name_.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,
                sizeof(IpcResponse),
                sizeof(IpcCommand),
                1000,
                pipe_security);
            if (pipe == INVALID_HANDLE_VALUE)
            {
                break;
            }

            const BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (!connected)
            {
                CloseHandle(pipe);
                continue;
            }

            IpcCommand command = {};
            IpcResponse response = {};
            PrepareResponse(response);

            DWORD bytes_read = 0;
            if (!ReadFile(pipe, &command, sizeof(command), &bytes_read, nullptr) || bytes_read != sizeof(command))
            {
                response.status = IPC_STATUS_PIPE_ERROR;
                SetIpcMessage(response, "failed to read command");
            }
            else
            {
                HandleCommand(command, response);
            }

            DWORD bytes_written = 0;
            WriteFile(pipe, &response, sizeof(response), &bytes_written, nullptr);
            FlushFileBuffers(pipe);
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }

        running_.store(false);
    }

    void IpcServer::HandleCommand(const IpcCommand &command, IpcResponse &response)
    {
        if (command.version != kIpcVersion)
        {
            response.status = IPC_STATUS_INVALID_VERSION;
            SetIpcMessage(response, "ipc version mismatch");
            return;
        }

        std::string message;
        switch (command.type)
        {
        case IpcCommandType::Ping:
            SetIpcMessage(response, "pong");
            return;

        case IpcCommandType::Configure:
            if (!State::Instance().Configure(command.configure, message))
            {
                response.status = IPC_STATUS_INVALID_ARGUMENT;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::Start:
            if (!State::Instance().Start(message))
            {
                response.status = IPC_STATUS_INVALID_STATE;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::Stop:
            if (!State::Instance().Stop(message))
            {
                response.status = IPC_STATUS_INVALID_STATE;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::Status:
            SetIpcMessage(response, State::Instance().Status());
            return;

        case IpcCommandType::ListModules:
            if (!State::Instance().ListModules(command.list_modules.include_system_modules != 0, message))
            {
                response.status = IPC_STATUS_INTERNAL_ERROR;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::DumpModule:
            if (!State::Instance().DumpModule(command.dump_module.module_name, command.dump_module.output_directory, message))
            {
                response.status = IPC_STATUS_INTERNAL_ERROR;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::ReadMemory:
            if (!State::Instance().ReadMemory(command.read_memory.address_text, command.read_memory.size, message))
            {
                response.status = IPC_STATUS_INVALID_ARGUMENT;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::WriteMemory:
            if (!State::Instance().WriteMemory(
                    command.write_memory.address_text,
                    command.write_memory.bytes,
                    command.write_memory.size,
                    message))
            {
                response.status = IPC_STATUS_INVALID_ARGUMENT;
                SetIpcMessage(response, message);
                return;
            }
            SetIpcMessage(response, message);
            return;

        case IpcCommandType::Shutdown:
            State::Instance().Stop(message);
            stop_requested_.store(true);
            SetIpcMessage(response, "server stopping");
            return;

        case IpcCommandType::Invalid:
        default:
            response.status = IPC_STATUS_INVALID_COMMAND;
            SetIpcMessage(response, "unknown command");
            return;
        }
    }
}
