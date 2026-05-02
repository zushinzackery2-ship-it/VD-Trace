#include "pch.h"
#include "loader/VDTraceLoaderRuntime.hpp"
#include "loader/VDTraceLoaderMemoryRuntimeInternal.h"

#include <limits>

namespace VDTraceLoader::MemoryRuntime
{
    bool PrepareMemoryReply(
        std::uint64_t requestId,
        std::uint64_t address,
        std::uint32_t size,
        VDTraceLoaderMemoryIpc::AccessKind accessKind,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply)
    {
        reply = {};
        reply.requestId = requestId;
        reply.address = address;
        reply.requestedSize = size;
        reply.accessKind = static_cast<std::uint32_t>(accessKind);

        if (size == 0)
        {
            reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::InvalidParameter);
            reply.win32Error = ERROR_INVALID_PARAMETER;
            return false;
        }

        if (size > VDTRACE_LOADER_MAX_MEMORY_BYTES)
        {
            reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::SizeTooLarge);
            reply.win32Error = ERROR_INVALID_PARAMETER;
            return false;
        }

        const auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
        if (static_cast<std::uintptr_t>(address) > maxAddress - static_cast<std::uintptr_t>(size))
        {
            reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::AddressOverflow);
            reply.win32Error = ERROR_ARITHMETIC_OVERFLOW;
            return false;
        }

        return true;
    }

    void FinalizeFailure(VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply)
    {
        if (reply.faultAddress == 0)
        {
            reply.faultAddress = reply.address;
        }

        PopulateRegionInfo(static_cast<std::uintptr_t>(reply.faultAddress), reply);
        InvalidateRegionCache(static_cast<std::uintptr_t>(reply.faultAddress));
        AppendMemoryAccessLog("fail", reply);
    }

    void HandleMemoryReadRequest(
        const VDTraceLoaderMemoryIpc::MemoryReadRequestPayload& request,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply)
    {
        if (!PrepareMemoryReply(
                request.requestId,
                request.address,
                request.size,
                VDTraceLoaderMemoryIpc::AccessKind::Read,
                reply))
        {
            FinalizeFailure(reply);
            return;
        }

        if (!ValidateRange(
                static_cast<std::uintptr_t>(request.address),
                request.size,
                VDTraceLoaderMemoryIpc::AccessKind::Read,
                reply))
        {
            FinalizeFailure(reply);
            return;
        }

        if (!ExecuteProtectedRead(
                static_cast<std::uintptr_t>(request.address),
                reply.data,
                request.size,
                reply))
        {
            FinalizeFailure(reply);
            return;
        }

        reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::Success);
        reply.transferredSize = request.size;
        reply.dataSize = request.size;
        PopulateRegionInfo(static_cast<std::uintptr_t>(request.address), reply);
    }

    void HandleMemoryWriteRequest(
        const VDTraceLoaderMemoryIpc::MemoryWriteRequestPayload& request,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply)
    {
        if (!PrepareMemoryReply(
                request.requestId,
                request.address,
                request.size,
                VDTraceLoaderMemoryIpc::AccessKind::Write,
                reply))
        {
            FinalizeFailure(reply);
            return;
        }

        if (!ValidateRange(
                static_cast<std::uintptr_t>(request.address),
                request.size,
                VDTraceLoaderMemoryIpc::AccessKind::Write,
                reply))
        {
            FinalizeFailure(reply);
            return;
        }

        if (!ExecuteProtectedWrite(
                static_cast<std::uintptr_t>(request.address),
                request.data,
                request.size,
                reply))
        {
            FinalizeFailure(reply);
            return;
        }

        reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::Success);
        reply.transferredSize = request.size;
        PopulateRegionInfo(static_cast<std::uintptr_t>(request.address), reply);
    }

    void SendBadReply(HANDLE pipeHandle, VDTraceLoaderMemoryIpc::MessageKind replyKind)
    {
        VDTraceLoaderMemoryIpc::MemoryReplyPayload reply = {};
        reply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::BadMessage);
        reply.win32Error = ERROR_INVALID_DATA;
        VDTraceLoaderMemoryIpc::SendMemoryReply(
            pipeHandle,
            replyKind,
            GetCurrentProcessId(),
            reply);
    }

    void ProcessMemoryPipeClient(HANDLE pipeHandle)
    {
        std::size_t requestCount = 0;

        for (;;)
        {
            std::vector<std::uint8_t> buffer;
            if (!VDTraceLoaderMemoryIpc::ReadMessage(pipeHandle, buffer))
            {
                const DWORD lastError = GetLastError();
                AppendRuntimeLog(
                    "memory-ipc client session end requests=" + std::to_string(requestCount)
                    + " readError=" + std::to_string(lastError));
                return;
            }

            requestCount++;

            if (buffer.size() < sizeof(VDTraceLoaderMemoryIpc::MessageHeader))
            {
                AppendRuntimeLog(
                    "memory-ipc short message requests=" + std::to_string(requestCount)
                    + " size=" + std::to_string(buffer.size()));
                return;
            }

            const auto* header = reinterpret_cast<const VDTraceLoaderMemoryIpc::MessageHeader*>(buffer.data());
            VDTraceLoaderMemoryIpc::MemoryReplyPayload reply = {};
            if (header->kind == static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::MessageKind::MemoryReadRequest))
            {
                if (buffer.size() < sizeof(VDTraceLoaderMemoryIpc::MessageHeader)
                    + sizeof(VDTraceLoaderMemoryIpc::MemoryReadRequestPayload))
                {
                    AppendRuntimeLog(
                        "memory-ipc bad read request payload requests=" + std::to_string(requestCount)
                        + " size=" + std::to_string(buffer.size()));
                    SendBadReply(pipeHandle, VDTraceLoaderMemoryIpc::MessageKind::MemoryReadReply);
                    return;
                }

                const auto* request = reinterpret_cast<const VDTraceLoaderMemoryIpc::MemoryReadRequestPayload*>(
                    buffer.data() + sizeof(VDTraceLoaderMemoryIpc::MessageHeader));
                HandleMemoryReadRequest(*request, reply);
                if (!VDTraceLoaderMemoryIpc::SendMemoryReply(
                        pipeHandle,
                        VDTraceLoaderMemoryIpc::MessageKind::MemoryReadReply,
                        GetCurrentProcessId(),
                        reply))
                {
                    AppendRuntimeLog(
                        "memory-ipc send read reply failed requests=" + std::to_string(requestCount)
                        + " error=" + std::to_string(GetLastError()));
                    return;
                }

                continue;
            }

            if (header->kind == static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::MessageKind::MemoryWriteRequest))
            {
                if (buffer.size() < sizeof(VDTraceLoaderMemoryIpc::MessageHeader)
                    + sizeof(VDTraceLoaderMemoryIpc::MemoryWriteRequestPayload))
                {
                    AppendRuntimeLog(
                        "memory-ipc bad write request payload requests=" + std::to_string(requestCount)
                        + " size=" + std::to_string(buffer.size()));
                    SendBadReply(pipeHandle, VDTraceLoaderMemoryIpc::MessageKind::MemoryWriteReply);
                    return;
                }

                const auto* request = reinterpret_cast<const VDTraceLoaderMemoryIpc::MemoryWriteRequestPayload*>(
                    buffer.data() + sizeof(VDTraceLoaderMemoryIpc::MessageHeader));
                HandleMemoryWriteRequest(*request, reply);
                if (!VDTraceLoaderMemoryIpc::SendMemoryReply(
                        pipeHandle,
                        VDTraceLoaderMemoryIpc::MessageKind::MemoryWriteReply,
                        GetCurrentProcessId(),
                        reply))
                {
                    AppendRuntimeLog(
                        "memory-ipc send write reply failed requests=" + std::to_string(requestCount)
                        + " error=" + std::to_string(GetLastError()));
                    return;
                }

                continue;
            }

            if (header->kind == static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::MessageKind::SharedMemoryConnectRequest))
            {
                if (buffer.size() < sizeof(VDTraceLoaderMemoryIpc::MessageHeader)
                    + sizeof(VDTraceLoaderMemoryIpc::SharedMemoryConnectRequestPayload))
                {
                    AppendRuntimeLog(
                        "memory-ipc bad shared-memory connect payload requests=" + std::to_string(requestCount)
                        + " size=" + std::to_string(buffer.size()));
                    VDTraceLoaderMemoryIpc::SharedMemoryConnectReplyPayload badReply = {};
                    badReply.status = static_cast<std::uint32_t>(VDTraceLoaderMemoryIpc::OperationStatus::BadMessage);
                    badReply.win32Error = ERROR_INVALID_DATA;
                    VDTraceLoaderMemoryIpc::SendSharedMemoryConnectReply(
                        pipeHandle,
                        GetCurrentProcessId(),
                        badReply);
                    return;
                }

                const auto* request = reinterpret_cast<const VDTraceLoaderMemoryIpc::SharedMemoryConnectRequestPayload*>(
                    buffer.data() + sizeof(VDTraceLoaderMemoryIpc::MessageHeader));
                ProcessSharedMemoryConnectRequest(pipeHandle, *request);
                return;
            }

            AppendRuntimeLog(
                "memory-ipc unknown message kind requests=" + std::to_string(requestCount)
                + " kind=" + std::to_string(header->kind));
            return;
        }
    }
}
