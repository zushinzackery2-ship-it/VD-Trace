import argparse
import ctypes
import ctypes.wintypes as wt
from pathlib import Path
import time


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)


LPVOID = wt.LPVOID
SIZE_T = ctypes.c_size_t

DEBUG_ONLY_THIS_PROCESS = 0x00000002
CREATE_SUSPENDED = 0x00000004
DBG_CONTINUE = 0x00010002
INFINITE = 0xFFFFFFFF

CREATE_PROCESS_DEBUG_EVENT = 3
EXIT_PROCESS_DEBUG_EVENT = 5
LOAD_DLL_DEBUG_EVENT = 6


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [
        ("cb", wt.DWORD),
        ("lpReserved", wt.LPWSTR),
        ("lpDesktop", wt.LPWSTR),
        ("lpTitle", wt.LPWSTR),
        ("dwX", wt.DWORD),
        ("dwY", wt.DWORD),
        ("dwXSize", wt.DWORD),
        ("dwYSize", wt.DWORD),
        ("dwXCountChars", wt.DWORD),
        ("dwYCountChars", wt.DWORD),
        ("dwFillAttribute", wt.DWORD),
        ("dwFlags", wt.DWORD),
        ("wShowWindow", wt.WORD),
        ("cbReserved2", wt.WORD),
        ("lpReserved2", ctypes.POINTER(ctypes.c_byte)),
        ("hStdInput", wt.HANDLE),
        ("hStdOutput", wt.HANDLE),
        ("hStdError", wt.HANDLE),
    ]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("hProcess", wt.HANDLE),
        ("hThread", wt.HANDLE),
        ("dwProcessId", wt.DWORD),
        ("dwThreadId", wt.DWORD),
    ]


class EXCEPTION_DEBUG_INFO(ctypes.Structure):
    _fields_ = [
        ("ExceptionRecord", ctypes.c_byte * 152),
        ("dwFirstChance", wt.DWORD),
    ]


class CREATE_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [
        ("hFile", wt.HANDLE),
        ("hProcess", wt.HANDLE),
        ("hThread", wt.HANDLE),
        ("lpBaseOfImage", LPVOID),
        ("dwDebugInfoFileOffset", wt.DWORD),
        ("nDebugInfoSize", wt.DWORD),
        ("lpThreadLocalBase", LPVOID),
        ("lpStartAddress", LPVOID),
        ("lpImageName", LPVOID),
        ("fUnicode", wt.WORD),
    ]


class EXIT_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("dwExitCode", wt.DWORD)]


class LOAD_DLL_DEBUG_INFO(ctypes.Structure):
    _fields_ = [
        ("hFile", wt.HANDLE),
        ("lpBaseOfDll", LPVOID),
        ("dwDebugInfoFileOffset", wt.DWORD),
        ("nDebugInfoSize", wt.DWORD),
        ("lpImageName", LPVOID),
        ("fUnicode", wt.WORD),
    ]


class DEBUG_EVENT_UNION(ctypes.Union):
    _fields_ = [
        ("Exception", EXCEPTION_DEBUG_INFO),
        ("CreateProcessInfo", CREATE_PROCESS_DEBUG_INFO),
        ("ExitProcess", EXIT_PROCESS_DEBUG_INFO),
        ("LoadDll", LOAD_DLL_DEBUG_INFO),
    ]


class DEBUG_EVENT(ctypes.Structure):
    _fields_ = [
        ("dwDebugEventCode", wt.DWORD),
        ("dwProcessId", wt.DWORD),
        ("dwThreadId", wt.DWORD),
        ("u", DEBUG_EVENT_UNION),
    ]


kernel32.CreateProcessW.argtypes = [
    wt.LPCWSTR,
    wt.LPWSTR,
    LPVOID,
    LPVOID,
    wt.BOOL,
    wt.DWORD,
    LPVOID,
    wt.LPCWSTR,
    ctypes.POINTER(STARTUPINFOW),
    ctypes.POINTER(PROCESS_INFORMATION),
]
kernel32.CreateProcessW.restype = wt.BOOL

kernel32.WaitForDebugEvent.argtypes = [ctypes.POINTER(DEBUG_EVENT), wt.DWORD]
kernel32.WaitForDebugEvent.restype = wt.BOOL

kernel32.ContinueDebugEvent.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD]
kernel32.ContinueDebugEvent.restype = wt.BOOL

kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.CloseHandle.restype = wt.BOOL

kernel32.ResumeThread.argtypes = [wt.HANDLE]
kernel32.ResumeThread.restype = wt.DWORD

kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, LPVOID, LPVOID, SIZE_T, ctypes.POINTER(SIZE_T)]
kernel32.ReadProcessMemory.restype = wt.BOOL

psapi.GetMappedFileNameW.argtypes = [wt.HANDLE, LPVOID, wt.LPWSTR, wt.DWORD]
psapi.GetMappedFileNameW.restype = wt.DWORD


def read_remote_string(process_handle, address, is_unicode):
    if not address:
        return ""
    if is_unicode:
        buffer = ctypes.create_unicode_buffer(512)
        size = SIZE_T()
        if kernel32.ReadProcessMemory(process_handle, address, buffer, ctypes.sizeof(buffer), ctypes.byref(size)):
            return buffer.value
    else:
        buffer = ctypes.create_string_buffer(512)
        size = SIZE_T()
        if kernel32.ReadProcessMemory(process_handle, address, buffer, ctypes.sizeof(buffer), ctypes.byref(size)):
            return buffer.value.decode("mbcs", errors="ignore")
    return ""


def mapped_name(process_handle, base):
    buffer = ctypes.create_unicode_buffer(1024)
    length = psapi.GetMappedFileNameW(process_handle, base, buffer, 1024)
    if length:
        return buffer.value
    return ""


def main():
    parser = argparse.ArgumentParser(description="Observe module load order")
    parser.add_argument("exe", type=Path)
    parser.add_argument("--workdir", type=Path, default=None)
    parser.add_argument("--seconds", type=int, default=30)
    args = parser.parse_args()

    startup = STARTUPINFOW()
    startup.cb = ctypes.sizeof(startup)
    process = PROCESS_INFORMATION()
    command_line = f"\"{args.exe}\""
    mutable_cmd = ctypes.create_unicode_buffer(command_line)
    workdir = str(args.workdir if args.workdir else args.exe.parent)
    ok = kernel32.CreateProcessW(
        None,
        mutable_cmd,
        None,
        None,
        False,
        DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED,
        None,
        workdir,
        ctypes.byref(startup),
        ctypes.byref(process),
    )
    if not ok:
        raise ctypes.WinError(ctypes.get_last_error())

    print(f"[start] pid={process.dwProcessId}")
    kernel32.ResumeThread(process.hThread)

    event = DEBUG_EVENT()
    seen = set()
    deadline = time.time() + args.seconds
    while True:
        remaining = deadline - time.time()
        if remaining <= 0:
            print("[timeout]")
            break
        wait_ms = max(1, int(remaining * 1000))
        if not kernel32.WaitForDebugEvent(ctypes.byref(event), wait_ms):
            print("[timeout]")
            break
        code = event.dwDebugEventCode
        if code == CREATE_PROCESS_DEBUG_EVENT:
            info = event.u.CreateProcessInfo
            path = mapped_name(info.hProcess, info.lpBaseOfImage)
            print(f"[create] base=0x{ctypes.cast(info.lpBaseOfImage, ctypes.c_void_p).value:x} path={path}")
            if info.hFile:
                kernel32.CloseHandle(info.hFile)
        elif code == LOAD_DLL_DEBUG_EVENT:
            info = event.u.LoadDll
            base = ctypes.cast(info.lpBaseOfDll, ctypes.c_void_p).value
            path = mapped_name(process.hProcess, info.lpBaseOfDll)
            if not path:
                path = read_remote_string(process.hProcess, info.lpImageName, bool(info.fUnicode))
            key = (base, path)
            if key not in seen:
                seen.add(key)
                print(f"[dll] base=0x{base:x} path={path}")
            if info.hFile:
                kernel32.CloseHandle(info.hFile)
        elif code == EXIT_PROCESS_DEBUG_EVENT:
            print(f"[exit] code={event.u.ExitProcess.dwExitCode} (0x{event.u.ExitProcess.dwExitCode:08x})")
            kernel32.ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE)
            break

        kernel32.ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE)

    kernel32.CloseHandle(process.hThread)
    kernel32.CloseHandle(process.hProcess)


if __name__ == "__main__":
    main()
