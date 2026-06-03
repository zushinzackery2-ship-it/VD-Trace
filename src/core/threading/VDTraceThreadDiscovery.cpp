#include "pch.h"
#include "core/runtime/VDTraceInternal.h"
#include "core/threading/VDTraceThreadEnum.h"

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

        bool found = false;
        FILETIME earliest_time = {};

        if (!EnumerateProcessThreads(GetCurrentProcessId(), [&](const THREADENTRY32 &entry)
        {
            HANDLE thread_handle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
            if (thread_handle == nullptr)
            {
                return;
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
        }))
        {
            error = L"无法枚举当前进程线程。";
            return false;
        }

        if (!found)
        {
            error = L"没有找到可用线程。";
        }

        return found;
    }
}
