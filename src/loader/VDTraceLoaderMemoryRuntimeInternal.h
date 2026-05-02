#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "loader/VDTraceLoaderIpcMemory.h"

namespace VDTraceLoader::MemoryRuntime
{
    struct MemoryRegionCacheEntry
    {
        std::uintptr_t baseAddress;
        std::uintptr_t endAddress;
        DWORD protect;
        DWORD state;
        DWORD type;
    };

    struct MemoryAccessContext
    {
        bool active = false;
        VDTraceLoaderMemoryIpc::AccessKind accessKind = VDTraceLoaderMemoryIpc::AccessKind::Invalid;
        std::uint64_t requestId = 0;
        std::uintptr_t address = 0;
        std::uint32_t size = 0;
    };

    inline constexpr std::size_t kMaxRegionCacheEntries = 256;

    extern std::mutex gMemoryRegionCacheMutex;
    extern std::vector<MemoryRegionCacheEntry> gMemoryRegionCache;
    extern thread_local MemoryAccessContext gMemoryAccessContext;
    extern thread_local VDTraceLoaderMemoryIpc::MemoryReplyPayload* gMemoryAccessReply;

    std::string FormatHex(std::uint64_t value);
    bool IsReadableProtection(DWORD protect);
    bool IsWritableProtection(DWORD protect);
    void PopulateRegionInfo(std::uintptr_t address, VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    void InvalidateRegionCache(std::uintptr_t address);
    bool QueryRegionCached(
        std::uintptr_t address,
        MemoryRegionCacheEntry& entry,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    bool ValidateRange(
        std::uintptr_t address,
        std::uint32_t size,
        VDTraceLoaderMemoryIpc::AccessKind accessKind,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    void AppendMemoryAccessLog(const char* phase, const VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    LONG CALLBACK MemoryAccessVectoredHandler(EXCEPTION_POINTERS* exceptionPointers);
    bool ExecuteProtectedRead(
        std::uintptr_t address,
        void* destination,
        std::uint32_t size,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    bool ExecuteProtectedWrite(
        std::uintptr_t address,
        const void* source,
        std::uint32_t size,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    bool PrepareMemoryReply(
        std::uint64_t requestId,
        std::uint64_t address,
        std::uint32_t size,
        VDTraceLoaderMemoryIpc::AccessKind accessKind,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    void FinalizeFailure(VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    void HandleMemoryReadRequest(
        const VDTraceLoaderMemoryIpc::MemoryReadRequestPayload& request,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    void HandleMemoryWriteRequest(
        const VDTraceLoaderMemoryIpc::MemoryWriteRequestPayload& request,
        VDTraceLoaderMemoryIpc::MemoryReplyPayload& reply);
    void SendBadReply(HANDLE pipeHandle, VDTraceLoaderMemoryIpc::MessageKind replyKind);
    void ProcessSharedMemoryConnectRequest(
        HANDLE pipeHandle,
        const VDTraceLoaderMemoryIpc::SharedMemoryConnectRequestPayload& request);
    void ProcessMemoryPipeClient(HANDLE pipeHandle);
    DWORD WINAPI MemoryIpcThreadProc(void*);
}
