#include "pch.h"
#include "control/ipc_client/VDTraceControlSupport.h"
#include "control/ipc_client/VDTraceControlSupportInternal.h"

#include <TlHelp32.h>

namespace vdtrace::tools::detail
{
    std::wstring BuildTimestampSuffix()
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);

        wchar_t buffer[32] = {};
        swprintf_s(
            buffer,
            L"%04u%02u%02u-%02u%02u%02u",
            static_cast<unsigned>(now.wYear),
            static_cast<unsigned>(now.wMonth),
            static_cast<unsigned>(now.wDay),
            static_cast<unsigned>(now.wHour),
            static_cast<unsigned>(now.wMinute),
            static_cast<unsigned>(now.wSecond));
        return buffer;
    }

    std::string NarrowUtf8(const std::wstring &text)
    {
        if (text.empty())
        {
            return {};
        }

        const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
        {
            return {};
        }

        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }
        return result;
    }

    bool GetThreadCreationTime(HANDLE thread_handle, FILETIME &creation_time)
    {
        FILETIME exit_time = {};
        FILETIME kernel_time = {};
        FILETIME user_time = {};
        return GetThreadTimes(thread_handle, &creation_time, &exit_time, &kernel_time, &user_time) != FALSE;
    }
}

namespace vdtrace::tools
{
    std::wstring GetExecutableDirectory()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return L".";
        }

        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return L".";
            }
        }

        buffer.resize(length);
        const size_t slash = buffer.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L".";
        }

        return buffer.substr(0, slash);
    }

    std::wstring BuildDefaultAgentPath()
    {
        return GetExecutableDirectory() + L"\\VDTraceAgent.dll";
    }

    std::wstring BuildDefaultOutputPath(DWORD pid)
    {
        return L".\\traces\\VDTrace-" + std::to_wstring(pid) + L"-" + detail::BuildTimestampSuffix() + L".log";
    }

    std::wstring BuildDefaultDumpOutputDirectory()
    {
        return L".\\dump";
    }

    bool ResolveFullPath(const std::wstring &input, std::wstring &output)
    {
        DWORD length = GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
        if (length == 0)
        {
            return false;
        }

        std::wstring buffer(length, L'\0');
        const DWORD written = GetFullPathNameW(input.c_str(), length, buffer.data(), nullptr);
        if (written == 0)
        {
            return false;
        }

        buffer.resize(written);
        output = buffer;
        return true;
    }

    bool GuessMainThreadId(DWORD pid, DWORD &thread_id, std::wstring &error)
    {
        thread_id = 0;
        error.clear();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            error = L"无法枚举线程。";
            return false;
        }

        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);

        bool found = false;
        FILETIME earliest_time = {};
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID != pid)
                {
                    continue;
                }

                HANDLE thread_handle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
                if (thread_handle == nullptr)
                {
                    continue;
                }

                FILETIME creation_time = {};
                if (detail::GetThreadCreationTime(thread_handle, creation_time))
                {
                    if (!found || CompareFileTime(&creation_time, &earliest_time) < 0)
                    {
                        earliest_time = creation_time;
                        thread_id = entry.th32ThreadID;
                        found = true;
                    }
                }

                CloseHandle(thread_handle);
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        if (!found)
        {
            error = L"没有找到可用线程。";
        }

        return found;
    }

    bool WaitForPipeReady(DWORD pid, DWORD timeout_ms)
    {
        const std::wstring pipe_name = BuildPipeName(pid);
        return WaitNamedPipeW(pipe_name.c_str(), timeout_ms) != FALSE;
    }
}
