import argparse
import ctypes
import ctypes.wintypes as wt
import time
from dataclasses import dataclass
from pathlib import Path


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
MAX_PATH = 260


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD),
        ("cntUsage", wt.DWORD),
        ("th32ProcessID", wt.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wt.DWORD),
        ("cntThreads", wt.DWORD),
        ("th32ParentProcessID", wt.DWORD),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", wt.DWORD),
        ("szExeFile", wt.WCHAR * MAX_PATH),
    ]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD),
        ("th32ModuleID", wt.DWORD),
        ("th32ProcessID", wt.DWORD),
        ("GlblcntUsage", wt.DWORD),
        ("ProccntUsage", wt.DWORD),
        ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wt.DWORD),
        ("hModule", wt.HMODULE),
        ("szModule", wt.WCHAR * 256),
        ("szExePath", wt.WCHAR * MAX_PATH),
    ]


kernel32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.Process32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
kernel32.Process32FirstW.restype = wt.BOOL
kernel32.Process32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
kernel32.Process32NextW.restype = wt.BOOL
kernel32.Module32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32FirstW.restype = wt.BOOL
kernel32.Module32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32NextW.restype = wt.BOOL
kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.CloseHandle.restype = wt.BOOL


@dataclass
class ProcessInfo:
    pid: int
    name: str


def enumerate_processes() -> list[ProcessInfo]:
    result: list[ProcessInfo] = []
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        return result
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
            return result
        while True:
            result.append(ProcessInfo(pid=entry.th32ProcessID, name=entry.szExeFile))
            if not kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                break
        return result
    finally:
        kernel32.CloseHandle(snapshot)


def enumerate_modules(pid: int) -> list[str]:
    result: list[str] = []
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == INVALID_HANDLE_VALUE:
        return result
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        if not kernel32.Module32FirstW(snapshot, ctypes.byref(entry)):
            return result
        while True:
            result.append(entry.szModule)
            if not kernel32.Module32NextW(snapshot, ctypes.byref(entry)):
                break
        return result
    finally:
        kernel32.CloseHandle(snapshot)


def main() -> int:
    parser = argparse.ArgumentParser(description="Poll Endfield process/module state")
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--interval", type=float, default=5.0)
    args = parser.parse_args()

    watched_names = {"Endfield.exe", "PlatformProcess.exe"}
    seen: set[int] = set()
    deadline = time.time() + args.seconds
    while time.time() < deadline:
        print(f"[tick] {time.strftime('%H:%M:%S')}", flush=True)
        processes = [p for p in enumerate_processes() if p.name in watched_names]
        if not processes:
            print("  no target processes", flush=True)
        for process in sorted(processes, key=lambda p: (p.name.lower(), p.pid)):
            modules = {name.lower() for name in enumerate_modules(process.pid)}
            prefix = "  new" if process.pid not in seen else "  pid"
            seen.add(process.pid)
            print(
                f"{prefix}={process.pid} name={process.name} "
                f"unity={'unityplayer.dll' in modules} "
                f"gameasm={'gameassembly.dll' in modules} "
                f"endfieldbase={'endfieldbase.dll' in modules}",
                flush=True,
            )
        time.sleep(args.interval)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
