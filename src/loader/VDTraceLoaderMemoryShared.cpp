#include "pch.h"
#include "loader/VDTraceLoaderRuntime.hpp"
#include "loader/VDTraceLoaderMemoryRuntimeInternal.h"

#include <limits>

namespace VDTraceLoader::MemoryRuntime
{
    namespace
    {
        constexpr DWORD kSharedSessionIdleTimeoutMs = 120000;
        constexpr DWORD kSharedSessionPollTimeoutMs = 1000;

        struct SharedMemorySession
        {
            HANDLE MappingHandle = nullptr;
            HANDLE RequestEvent = nullptr;
            HANDLE ReplyEvent = nullptr;
            VDTraceLoaderMemoryIpc::SharedMemoryBlock* Block = nullptr;
            std::uint32_t MaxTransferSize = 0;
        };

        void CloseSharedMemorySession(SharedMemorySession& session)
        {
            if (session.Block != nullptr)
            {
                UnmapViewOfFile(session.Block);
                session.Block = nullptr;
            }
            if (session.MappingHandle != nullptr)
            {
                CloseHandle(session.MappingHandle);
                session.MappingHandle = nullptr;
            }
            if (session.RequestEvent != nullptr)
            {
                CloseHandle(session.RequestEvent);
                session.RequestEvent = nullptr;
            }
            if (session.ReplyEvent != nullptr)
            {
                CloseHandle(session.ReplyEvent);
                session.ReplyEvent = nullptr;
            }
        }

        void CopyReplyToSharedBlock(
            VDTraceLoaderMemoryIpc::SharedMemoryBlock& block,
            const VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply)
        {
            block.faultAddress = reply.faultAddress;
            block.instructionPointer = reply.instructionPointer;
            block.regionBaseAddress = reply.regionBaseAddress;
            block.regionSize = reply.regionSize;
            block.transferredSize = reply.transferredSize;
            block.status = reply.status;
            block.win32Error = reply.win32Error;
            block.exceptionCode = reply.exceptionCode;
            block.pageProtect = reply.pageProtect;
            block.pageState = reply.pageState;
            block.pageType = reply.pageType;
            block.accessKind = reply.accessKind;
            block.cacheHits = reply.cacheHits;
            block.cacheMisses = reply.cacheMisses;
            block.dataSize = reply.dataSize;
        }

        bool IsPipeClientDisconnected(HANDLE pipeHandle)
        {
            if (pipeHandle == nullptr || pipeHandle == INVALID_HANDLE_VALUE)
            {
                return true;
            }

            DWORD bytesAvailable = 0;
            if (PeekNamedPipe(pipeHandle, nullptr, 0, nullptr, &bytesAvailable, nullptr))
            {
                return false;
            }

            const DWORD error = GetLastError();
            return error == ERROR_BROKEN_PIPE
                || error == ERROR_PIPE_NOT_CONNECTED
                || error == ERROR_NO_DATA
                || error == ERROR_BAD_PIPE;
        }

        bool PrepareSharedMemoryReply(
            const VDTraceLoaderMemoryIpc::SharedMemoryBlock& block,
            std::uint32_t maxTransferSize,
            VDTraceLoaderMemoryIpc::AccessKind accessKind,
            VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply)
        {
            reply = {};
            reply.requestId = block.requestId;
            reply.address = block.address;
            reply.requestedSize = block.requestedSize;
            reply.accessKind = static_cast<std::uint32_t>(accessKind);

            if (block.requestedSize == 0)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::InvalidParameter);
                reply.win32Error = ERROR_INVALID_PARAMETER;
                return false;
            }
            if (block.requestedSize > maxTransferSize || block.requestedSize > VDTRACE_LOADER_SHARED_MEMORY_BYTES)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::SizeTooLarge);
                reply.win32Error = ERROR_INVALID_PARAMETER;
                return false;
            }

            const auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
            if (static_cast<std::uintptr_t>(block.address) > maxAddress - static_cast<std::uintptr_t>(block.requestedSize))
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::AddressOverflow);
                reply.win32Error = ERROR_ARITHMETIC_OVERFLOW;
                return false;
            }
            return true;
        }

        bool OpenSharedMemorySession(
            const VDTraceLoaderMemoryIpc::SharedMemoryConnectRequestPayload& request,
            SharedMemorySession& session,
            VDTraceLoaderMemoryIpc::SharedMemoryConnectReplyPayload& reply)
        {
            auto safeRequest = request;
            safeRequest.mappingName[VDTRACE_LOADER_SHARED_NAME_CHARS - 1] = L'\0';
            safeRequest.requestEventName[VDTRACE_LOADER_SHARED_NAME_CHARS - 1] = L'\0';
            safeRequest.replyEventName[VDTRACE_LOADER_SHARED_NAME_CHARS - 1] = L'\0';

            session = {};
            reply = {};
            reply.requestId = safeRequest.requestId;
            reply.sessionId = safeRequest.sessionId;
            reply.maxTransferSize = (std::min)(
                safeRequest.maxTransferSize,
                static_cast<std::uint32_t>(VDTRACE_LOADER_SHARED_MEMORY_BYTES));

            if (reply.maxTransferSize == 0)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::InvalidParameter);
                reply.win32Error = ERROR_INVALID_PARAMETER;
                return false;
            }

            session.MappingHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, safeRequest.mappingName);
            if (session.MappingHandle == nullptr)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::QueryFailed);
                reply.win32Error = GetLastError();
                return false;
            }

            session.Block = static_cast<VDTraceLoaderMemoryIpc::SharedMemoryBlock*>(
                MapViewOfFile(session.MappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(VDTraceLoaderMemoryIpc::SharedMemoryBlock)));
            if (session.Block == nullptr)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::QueryFailed);
                reply.win32Error = GetLastError();
                CloseSharedMemorySession(session);
                return false;
            }

            session.RequestEvent = OpenEventW(SYNCHRONIZE, FALSE, safeRequest.requestEventName);
            if (session.RequestEvent == nullptr)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::QueryFailed);
                reply.win32Error = GetLastError();
                CloseSharedMemorySession(session);
                return false;
            }

            session.ReplyEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, safeRequest.replyEventName);
            if (session.ReplyEvent == nullptr)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::QueryFailed);
                reply.win32Error = GetLastError();
                CloseSharedMemorySession(session);
                return false;
            }

            if (session.Block->magic != VDTraceLoaderMemoryIpc::kSharedMemoryMagic
                || session.Block->version != VDTraceLoaderMemoryIpc::kSharedMemoryVersion)
            {
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::BadMessage);
                reply.win32Error = ERROR_INVALID_DATA;
                CloseSharedMemorySession(session);
                return false;
            }

            session.MaxTransferSize = reply.maxTransferSize;
            session.Block->maxTransferSize = reply.maxTransferSize;
            reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::Success);
            return true;
        }

        bool ProcessSharedMemoryRequest(SharedMemorySession& session)
        {
            auto& block = *session.Block;
            MemoryBarrier();

            const auto command = static_cast<VDTraceLoaderMemoryIpc::SharedMemoryCommand>(block.command);
            VDTraceLoaderMemoryIpc::MemoryReplyPayload reply = {};

            if (command == VDTraceLoaderMemoryIpc::SharedMemoryCommand::Close)
            {
                reply.requestId = block.requestId;
                reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::Success);
                CopyReplyToSharedBlock(block, reply);
                MemoryBarrier();
                SetEvent(session.ReplyEvent);
                return false;
            }

            if (command == VDTraceLoaderMemoryIpc::SharedMemoryCommand::Read)
            {
                if (PrepareSharedMemoryReply(block, session.MaxTransferSize, VDTraceLoaderMemoryIpc::AccessKind::Read, reply)
                    && ValidateRange(static_cast<std::uintptr_t>(block.address), block.requestedSize, VDTraceLoaderMemoryIpc::AccessKind::Read, reply)
                    && ExecuteProtectedRead(static_cast<std::uintptr_t>(block.address), block.replyData, block.requestedSize, reply))
                {
                    reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::Success);
                    reply.transferredSize = block.requestedSize;
                    reply.dataSize = block.requestedSize;
                    PopulateRegionInfo(static_cast<std::uintptr_t>(block.address), reply);
                }
                else
                {
                    FinalizeFailure(reply);
                }

                CopyReplyToSharedBlock(block, reply);
                MemoryBarrier();
                SetEvent(session.ReplyEvent);
                return true;
            }

            if (command == VDTraceLoaderMemoryIpc::SharedMemoryCommand::Write)
            {
                if (PrepareSharedMemoryReply(block, session.MaxTransferSize, VDTraceLoaderMemoryIpc::AccessKind::Write, reply)
                    && ValidateRange(static_cast<std::uintptr_t>(block.address), block.requestedSize, VDTraceLoaderMemoryIpc::AccessKind::Write, reply)
                    && ExecuteProtectedWrite(static_cast<std::uintptr_t>(block.address), block.requestData, block.requestedSize, reply))
                {
                    reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::Success);
                    reply.transferredSize = block.requestedSize;
                    PopulateRegionInfo(static_cast<std::uintptr_t>(block.address), reply);
                }
                else
                {
                    FinalizeFailure(reply);
                }

                CopyReplyToSharedBlock(block, reply);
                MemoryBarrier();
                SetEvent(session.ReplyEvent);
                return true;
            }

            reply.requestId = block.requestId;
            reply.address = block.address;
            reply.requestedSize = block.requestedSize;
            reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::BadMessage);
            reply.win32Error = ERROR_INVALID_DATA;
            CopyReplyToSharedBlock(block, reply);
            MemoryBarrier();
            SetEvent(session.ReplyEvent);
            return true;
        }
    }

    void ProcessSharedMemoryConnectRequest(
        HANDLE pipeHandle,
        const VDTraceLoaderMemoryIpc::SharedMemoryConnectRequestPayload& request)
    {
        SharedMemorySession session = {};
        VDTraceLoaderMemoryIpc::SharedMemoryConnectReplyPayload reply = {};
        const bool opened = OpenSharedMemorySession(request, session, reply);

        VDTraceLoaderMemoryIpc::SendSharedMemoryConnectReply(
            pipeHandle,
            GetCurrentProcessId(),
            reply);

        if (!opened)
        {
            AppendRuntimeLog(
                "memory-ipc shared session open failed status=" + std::to_string(reply.status)
                + " win32=" + std::to_string(reply.win32Error));
            return;
        }

        AppendRuntimeLog(
            "memory-ipc shared session opened clientPid=" + std::to_string(request.clientPid)
            + " maxTransfer=" + std::to_string(session.MaxTransferSize));

        std::size_t requestCount = 0;
        ULONGLONG lastActivityTick = GetTickCount64();
        for (;;)
        {
            const DWORD waitResult = WaitForSingleObject(session.RequestEvent, kSharedSessionPollTimeoutMs);
            if (waitResult == WAIT_TIMEOUT)
            {
                if (IsPipeClientDisconnected(pipeHandle))
                {
                    AppendRuntimeLog(
                        "memory-ipc shared session client disconnected requests=" + std::to_string(requestCount));
                    break;
                }

                if ((GetTickCount64() - lastActivityTick) >= kSharedSessionIdleTimeoutMs)
                {
                    AppendRuntimeLog(
                        "memory-ipc shared session idle timeout requests=" + std::to_string(requestCount));
                    break;
                }

                continue;
            }

            if (waitResult != WAIT_OBJECT_0)
            {
                AppendRuntimeLog(
                    "memory-ipc shared session wait failed requests=" + std::to_string(requestCount)
                    + " error=" + std::to_string(GetLastError()));
                break;
            }

            requestCount++;
            lastActivityTick = GetTickCount64();
            if (!ProcessSharedMemoryRequest(session))
            {
                break;
            }
        }

        CloseSharedMemorySession(session);
        AppendRuntimeLog(
            "memory-ipc shared session closed requests=" + std::to_string(requestCount));
    }
}
