#include "pch.h"
#include "control/loader_session/VDTraceLoaderControlSupport.h"
#include "control/loader_session/VDTraceLoaderControlIpc.h"

namespace vdtrace::tools
{
    namespace
    {
        std::wstring DecodeUtf16Text(const wchar_t *buffer, size_t count)
        {
            const size_t length = wcsnlen_s(buffer, count);
            return std::wstring(buffer, buffer + length);
        }

        std::wstring NormalizePathText(const std::wstring &path)
        {
            if (path.empty())
            {
                return {};
            }

            return std::filesystem::path(path).lexically_normal().wstring();
        }

        bool EqualsInsensitive(const std::wstring &left, const std::wstring &right)
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t index = 0; index < left.size(); ++index)
            {
                if (towlower(left[index]) != towlower(right[index]))
                {
                    return false;
                }
            }

            return true;
        }

        bool ReadLoaderMessage(HANDLE pipe, std::vector<std::uint8_t> &buffer)
        {
            return loader_ipc::ReadMessage(pipe, buffer);
        }

        HANDLE CreateOverlappedLoaderServer()
        {
            SECURITY_DESCRIPTOR descriptor = {};
            if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION))
            {
                return INVALID_HANDLE_VALUE;
            }

            if (!SetSecurityDescriptorDacl(&descriptor, TRUE, nullptr, FALSE))
            {
                return INVALID_HANDLE_VALUE;
            }

            SECURITY_ATTRIBUTES attributes = {};
            attributes.nLength = sizeof(attributes);
            attributes.lpSecurityDescriptor = &descriptor;
            attributes.bInheritHandle = FALSE;

            return CreateNamedPipeW(
                VDTRACE_LOADER_CONTROL_PIPE_NAME,
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                4096,
                4096,
                0,
                &attributes);
        }

        bool WaitForLoaderSessionInternal(
            DWORD expected_pid,
            const std::wstring *expected_process_path,
            DWORD timeout_ms,
            LoaderSession &session,
            std::wstring &error)
        {
            error.clear();
            CloseLoaderSession(session);
            const std::wstring normalized_expected_path = expected_process_path != nullptr
                ? NormalizePathText(*expected_process_path)
                : std::wstring();

            const ULONGLONG deadline = GetTickCount64() + timeout_ms;
            while (GetTickCount64() < deadline)
            {
                HANDLE server = CreateOverlappedLoaderServer();
                if (server == INVALID_HANDLE_VALUE)
                {
                    error = L"创建 Loader 控制管道失败。";
                    return false;
                }

                const DWORD remaining = static_cast<DWORD>(std::min<ULONGLONG>(deadline - GetTickCount64(), 0xFFFFFFFFull));
                OVERLAPPED overlapped = {};
                overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (overlapped.hEvent == nullptr)
                {
                    CloseHandle(server);
                    error = L"创建 Loader 等待事件失败。";
                    return false;
                }

                const BOOL connected = ConnectNamedPipe(server, &overlapped);
                DWORD wait_result = WAIT_TIMEOUT;
                if (connected)
                {
                    wait_result = WAIT_OBJECT_0;
                }
                else
                {
                    const DWORD last_error = GetLastError();
                    if (last_error == ERROR_PIPE_CONNECTED)
                    {
                        wait_result = WAIT_OBJECT_0;
                    }
                    else if (last_error == ERROR_IO_PENDING)
                    {
                        wait_result = WaitForSingleObject(overlapped.hEvent, remaining);
                    }
                }

                CloseHandle(overlapped.hEvent);
                if (wait_result != WAIT_OBJECT_0)
                {
                    CancelIo(server);
                    loader_ipc::ClosePipe(server);
                    continue;
                }

                std::vector<std::uint8_t> buffer;
                bool matched = false;
                while (ReadLoaderMessage(server, buffer))
                {
                    if (buffer.size() < sizeof(loader_ipc::MessageHeader))
                    {
                        break;
                    }

                    const auto *header = reinterpret_cast<const loader_ipc::MessageHeader *>(buffer.data());
                    const auto kind = static_cast<loader_ipc::MessageKind>(header->kind);
                    if (kind != loader_ipc::MessageKind::AgentHello)
                    {
                        continue;
                    }

                    if (buffer.size() < sizeof(loader_ipc::MessageHeader) + sizeof(loader_ipc::AgentHelloPayload))
                    {
                        break;
                    }

                    const auto *payload = reinterpret_cast<const loader_ipc::AgentHelloPayload *>(buffer.data() + sizeof(loader_ipc::MessageHeader));
                    const std::wstring reported_process_path = NormalizePathText(DecodeUtf16Text(payload->processPath, VDTRACE_LOADER_CONTROL_MAX_PATH_CHARS));

                    const bool pid_matched = expected_pid == 0 || header->pid == expected_pid;
                    const bool path_matched = expected_process_path == nullptr || EqualsInsensitive(reported_process_path, normalized_expected_path);
                    if (!pid_matched || !path_matched)
                    {
                        break;
                    }

                    session.pipe = server;
                    session.pid = header->pid;
                    session.process_path = reported_process_path;
                    session.protocol_version = payload->protocolVersion;
                    session.feature_flags = payload->featureFlags;
                    matched = true;
                    break;
                }

                if (matched)
                {
                    return true;
                }

                loader_ipc::ClosePipe(server);
            }

            error = L"等待目标进程 Loader 会话超时。";
            return false;
        }
    }

    bool WaitForLoaderSession(DWORD pid, DWORD timeout_ms, LoaderSession &session, std::wstring &error)
    {
        return WaitForLoaderSessionInternal(pid, nullptr, timeout_ms, session, error);
    }

    bool WaitForAnyLoaderSession(DWORD timeout_ms, LoaderSession &session, std::wstring &error)
    {
        return WaitForLoaderSessionInternal(0, nullptr, timeout_ms, session, error);
    }

    bool WaitForLoaderSessionByPath(const std::wstring &process_path, DWORD timeout_ms, LoaderSession &session, std::wstring &error)
    {
        return WaitForLoaderSessionInternal(0, &process_path, timeout_ms, session, error);
    }

    bool SendLoaderBootstrapRequest(LoaderSession &session, const std::wstring &dll_path, DWORD timeout_ms, std::wstring &reply_text, std::wstring &error)
    {
        reply_text.clear();
        error.clear();
        if (session.pipe == INVALID_HANDLE_VALUE)
        {
            error = L"Loader 会话未建立。";
            return false;
        }

        if (!loader_ipc::SendLoadDllRequest(session.pipe, GetCurrentProcessId(), dll_path))
        {
            error = L"发送 Loader 加载请求失败。";
            return false;
        }

        const ULONGLONG deadline = GetTickCount64() + timeout_ms;
        std::vector<std::uint8_t> buffer;
        while (GetTickCount64() < deadline)
        {
            if (!ReadLoaderMessage(session.pipe, buffer))
            {
                error = L"等待 Loader 加载结果失败。";
                return false;
            }

            if (buffer.size() < sizeof(loader_ipc::MessageHeader))
            {
                continue;
            }

            const auto *header = reinterpret_cast<const loader_ipc::MessageHeader *>(buffer.data());
            const auto kind = static_cast<loader_ipc::MessageKind>(header->kind);
            if (kind == loader_ipc::MessageKind::AgentLog)
            {
                continue;
            }

            if (kind != loader_ipc::MessageKind::LoadDllReply
                || buffer.size() < sizeof(loader_ipc::MessageHeader) + sizeof(loader_ipc::LoadDllReplyPayload))
            {
                continue;
            }

            const auto *payload = reinterpret_cast<const loader_ipc::LoadDllReplyPayload *>(buffer.data() + sizeof(loader_ipc::MessageHeader));
            reply_text = DecodeUtf16Text(payload->text, VDTRACE_LOADER_CONTROL_MAX_TEXT_CHARS);
            if (payload->status == 0)
            {
                return true;
            }

            error = L"Loader 拉起目标 DLL 失败。";
            if (!reply_text.empty())
            {
                error += L" ";
                error += reply_text;
            }
            if (payload->win32Error != 0)
            {
                error += L" (error=" + std::to_wstring(payload->win32Error) + L")";
            }
            return false;
        }

        error = L"等待 Loader 加载结果超时。";
        return false;
    }

    void CloseLoaderSession(LoaderSession &session)
    {
        if (session.pipe != INVALID_HANDLE_VALUE)
        {
            loader_ipc::ClosePipe(session.pipe);
        }
        session = {};
        session.pipe = INVALID_HANDLE_VALUE;
    }
}
