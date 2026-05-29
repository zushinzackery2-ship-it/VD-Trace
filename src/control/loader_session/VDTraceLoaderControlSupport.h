#ifndef VDTRACE_LOADER_CONTROL_SUPPORT_H
#define VDTRACE_LOADER_CONTROL_SUPPORT_H

#include <Windows.h>

#include <string>

namespace vdtrace::tools
{
    struct LoaderSession
    {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        DWORD pid = 0;
        std::wstring process_path;
        uint32_t protocol_version = 0;
        uint32_t feature_flags = 0;
    };

    bool WaitForAnyLoaderSession(DWORD timeout_ms, LoaderSession &session, std::wstring &error);
    bool WaitForLoaderSession(DWORD pid, DWORD timeout_ms, LoaderSession &session, std::wstring &error);
    bool WaitForLoaderSessionByPath(const std::wstring &process_path, DWORD timeout_ms, LoaderSession &session, std::wstring &error);
    bool SendLoaderBootstrapRequest(LoaderSession &session, const std::wstring &dll_path, DWORD timeout_ms, std::wstring &reply_text, std::wstring &error);
    void CloseLoaderSession(LoaderSession &session);
}

#endif
