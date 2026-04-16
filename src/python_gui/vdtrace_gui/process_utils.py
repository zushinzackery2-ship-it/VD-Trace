from __future__ import annotations

import ctypes
from ctypes import wintypes


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

TH32CS_SNAPTHREAD = 0x00000004
THREAD_QUERY_LIMITED_INFORMATION = 0x0800
INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value


class THREADENTRY32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ThreadID", wintypes.DWORD),
        ("th32OwnerProcessID", wintypes.DWORD),
        ("tpBasePri", wintypes.LONG),
        ("tpDeltaPri", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
    ]


kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.Thread32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(THREADENTRY32)]
kernel32.Thread32First.restype = wintypes.BOOL
kernel32.Thread32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(THREADENTRY32)]
kernel32.Thread32Next.restype = wintypes.BOOL
kernel32.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenThread.restype = wintypes.HANDLE
kernel32.GetThreadTimes.argtypes = [
    wintypes.HANDLE,
    ctypes.POINTER(wintypes.FILETIME),
    ctypes.POINTER(wintypes.FILETIME),
    ctypes.POINTER(wintypes.FILETIME),
    ctypes.POINTER(wintypes.FILETIME),
]
kernel32.GetThreadTimes.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL


def _filetime_to_int(value: wintypes.FILETIME) -> int:
    return (value.dwHighDateTime << 32) | value.dwLowDateTime


def guess_main_thread_id(pid: int) -> int:
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        return 0
    try:
        entry = THREADENTRY32()
        entry.dwSize = ctypes.sizeof(THREADENTRY32)
        found = False
        earliest = 0
        best_thread = 0
        ok = kernel32.Thread32First(snapshot, ctypes.byref(entry))
        while ok:
            if entry.th32OwnerProcessID == pid:
                thread_handle = kernel32.OpenThread(
                    THREAD_QUERY_LIMITED_INFORMATION,
                    False,
                    entry.th32ThreadID,
                )
                if thread_handle:
                    try:
                        creation = wintypes.FILETIME()
                        exit_time = wintypes.FILETIME()
                        kernel_time = wintypes.FILETIME()
                        user_time = wintypes.FILETIME()
                        if kernel32.GetThreadTimes(
                            thread_handle,
                            ctypes.byref(creation),
                            ctypes.byref(exit_time),
                            ctypes.byref(kernel_time),
                            ctypes.byref(user_time),
                        ):
                            timestamp = _filetime_to_int(creation)
                            if not found or timestamp < earliest:
                                found = True
                                earliest = timestamp
                                best_thread = int(entry.th32ThreadID)
                    finally:
                        kernel32.CloseHandle(thread_handle)
            ok = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
        return best_thread
    finally:
        kernel32.CloseHandle(snapshot)
