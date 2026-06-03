#pragma once

#include <TlHelp32.h>
#include <Windows.h>

namespace vdtrace
{
    template<typename Callback>
    bool EnumerateProcessThreads(DWORD pid, Callback &&fn)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID == pid)
                {
                    fn(entry);
                }
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return true;
    }
}
