#ifndef VDTRACE_LOADER_CONTROL_IPC_H
#define VDTRACE_LOADER_CONTROL_IPC_H

#ifndef WINHTTP_REDIRECT_PROXY_PIPE_NAME
#define WINHTTP_REDIRECT_PROXY_PIPE_NAME L"\\\\.\\pipe\\WinHttpRedirectProxyControl"
#endif

#ifndef WINHTTP_REDIRECT_PROXY_MAX_PATH_CHARS
#define WINHTTP_REDIRECT_PROXY_MAX_PATH_CHARS 1024
#endif

#ifndef WINHTTP_REDIRECT_PROXY_MAX_TEXT_CHARS
#define WINHTTP_REDIRECT_PROXY_MAX_TEXT_CHARS 256
#endif

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace vdtrace::tools::loader_ipc
{
    inline constexpr std::uint32_t kMaxMessageSize = 1 * 1024 * 1024;
    inline constexpr std::uint32_t kMagic = 0x58525057;
    inline constexpr std::uint32_t kProtocolVersion = 1;
    inline constexpr std::uint32_t kFeatureBootstrapAfterLoad = 0x1;

    enum class MessageKind : std::uint32_t
    {
        Invalid = 0,
        AgentHello = 1,
        AgentLog = 2,
        LoadDllRequest = 3,
        LoadDllReply = 4,
    };

    struct MessageHeader
    {
        std::uint32_t magic;
        std::uint32_t kind;
        std::uint32_t size;
        std::uint32_t pid;
    };

    struct AgentHelloPayload
    {
        wchar_t processPath[WINHTTP_REDIRECT_PROXY_MAX_PATH_CHARS];
        std::uint32_t protocolVersion;
        std::uint32_t featureFlags;
    };

    struct LoadDllRequestPayload
    {
        wchar_t dllPath[WINHTTP_REDIRECT_PROXY_MAX_PATH_CHARS];
    };

    struct LoadDllReplyPayload
    {
        std::uint32_t status;
        std::uint32_t win32Error;
        wchar_t dllPath[WINHTTP_REDIRECT_PROXY_MAX_PATH_CHARS];
        wchar_t text[WINHTTP_REDIRECT_PROXY_MAX_TEXT_CHARS];
    };

    inline bool WriteAll(HANDLE pipe, const void *buffer, DWORD size)
    {
        const auto *bytes = static_cast<const std::uint8_t *>(buffer);
        DWORD total_written = 0;
        while (total_written < size)
        {
            DWORD bytes_written = 0;
            if (!WriteFile(pipe, bytes + total_written, size - total_written, &bytes_written, nullptr) || bytes_written == 0)
            {
                return false;
            }

            total_written += bytes_written;
        }

        return true;
    }

    inline bool ReadAll(HANDLE pipe, void *buffer, DWORD size)
    {
        auto *bytes = static_cast<std::uint8_t *>(buffer);
        DWORD total_read = 0;
        while (total_read < size)
        {
            DWORD bytes_read = 0;
            if (!ReadFile(pipe, bytes + total_read, size - total_read, &bytes_read, nullptr) || bytes_read == 0)
            {
                return false;
            }

            total_read += bytes_read;
        }

        return true;
    }

    inline bool WriteMessage(HANDLE pipe, MessageKind kind, std::uint32_t pid, const void *payload, std::uint32_t payload_size)
    {
        MessageHeader header = {};
        header.magic = kMagic;
        header.kind = static_cast<std::uint32_t>(kind);
        header.size = static_cast<std::uint32_t>(sizeof(header) + payload_size);
        header.pid = pid;

        if (!WriteAll(pipe, &header, static_cast<DWORD>(sizeof(header))))
        {
            return false;
        }

        if (payload == nullptr || payload_size == 0)
        {
            return true;
        }

        return WriteAll(pipe, payload, payload_size);
    }

    inline bool ReadMessage(HANDLE pipe, std::vector<std::uint8_t> &buffer)
    {
        buffer.resize(sizeof(MessageHeader));
        if (!ReadAll(pipe, buffer.data(), static_cast<DWORD>(sizeof(MessageHeader))))
        {
            return false;
        }

        const auto *header = reinterpret_cast<const MessageHeader *>(buffer.data());
        if (header->magic != kMagic || header->size < sizeof(MessageHeader) || header->size > kMaxMessageSize)
        {
            return false;
        }

        buffer.resize(header->size);
        const DWORD payload_size = static_cast<DWORD>(header->size - sizeof(MessageHeader));
        if (payload_size == 0)
        {
            return true;
        }

        return ReadAll(pipe, buffer.data() + sizeof(MessageHeader), payload_size);
    }

    template <std::size_t Count>
    inline void CopyWideText(wchar_t (&destination)[Count], const std::wstring &text)
    {
        wcsncpy_s(destination, Count, text.c_str(), _TRUNCATE);
    }

    inline bool SendLoadDllRequest(HANDLE pipe, std::uint32_t pid, const std::wstring &dll_path)
    {
        LoadDllRequestPayload payload = {};
        CopyWideText(payload.dllPath, dll_path);
        return WriteMessage(pipe, MessageKind::LoadDllRequest, pid, &payload, static_cast<std::uint32_t>(sizeof(payload)));
    }

    inline void ClosePipe(HANDLE &pipe)
    {
        if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE)
        {
            pipe = INVALID_HANDLE_VALUE;
            return;
        }

        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
}

#endif