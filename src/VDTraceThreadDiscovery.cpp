#include "pch.h"
#include "VDTraceInternal.h"

#include <TlHelp32.h>

namespace vdtrace
{
    namespace
    {
        bool GetThreadCreationTime(HANDLE thread_handle, FILETIME &creation_time)
        {
            FILETIME exit_time = {};
            FILETIME kernel_time = {};
            FILETIME user_time = {};
            return GetThreadTimes(thread_handle, &creation_time, &exit_time, &kernel_time, &user_time) != FALSE;
        }
    }

    bool GuessCurrentProcessMainThread(DWORD &thread_id, std::wstring &error)
    {
        thread_id = 0;
        error.clear();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            error = L"无法枚举当前进程线程。";
            return false;
        }

        bool found = false;
        FILETIME earliest_time = {};
        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID != GetCurrentProcessId())
                {
                    continue;
                }

                HANDLE thread_handle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
                if (thread_handle == nullptr)
                {
                    continue;
                }

                FILETIME creation_time = {};
                if (GetThreadCreationTime(thread_handle, creation_time)
                    && (!found || CompareFileTime(&creation_time, &earliest_time) < 0))
                {
                    earliest_time = creation_time;
                    thread_id = entry.th32ThreadID;
                    found = true;
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
}
