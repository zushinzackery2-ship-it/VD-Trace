import argparse
import ctypes
import ctypes.util
import ctypes.wintypes as wt
import time
from pathlib import Path

import pefile


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

DEBUG_ONLY_THIS_PROCESS = 0x00000002
DBG_CONTINUE = 0x00010002
CREATE_PROCESS_DEBUG_EVENT = 3
EXIT_PROCESS_DEBUG_EVENT = 5
LOAD_DLL_DEBUG_EVENT = 6
EXCEPTION_DEBUG_EVENT = 1
EXCEPTION_BREAKPOINT = 0x80000003
EXCEPTION_SINGLE_STEP = 0x80000004


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


class EXCEPTION_RECORD64(ctypes.Structure):
    _fields_ = [
        ("ExceptionCode", wt.DWORD),
        ("ExceptionFlags", wt.DWORD),
        ("ExceptionRecord", wt.LPVOID),
        ("ExceptionAddress", wt.LPVOID),
        ("NumberParameters", wt.DWORD),
        ("__unusedAlignment", wt.DWORD),
        ("ExceptionInformation", ctypes.c_ulonglong * 15),
    ]


class EXCEPTION_DEBUG_INFO(ctypes.Structure):
    _fields_ = [
        ("ExceptionRecord", EXCEPTION_RECORD64),
        ("dwFirstChance", wt.DWORD),
    ]


class CREATE_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [
        ("hFile", wt.HANDLE),
        ("hProcess", wt.HANDLE),
        ("hThread", wt.HANDLE),
        ("lpBaseOfImage", wt.LPVOID),
        ("dwDebugInfoFileOffset", wt.DWORD),
        ("nDebugInfoSize", wt.DWORD),
        ("lpThreadLocalBase", wt.LPVOID),
        ("lpStartAddress", wt.LPVOID),
        ("lpImageName", wt.LPVOID),
        ("fUnicode", wt.WORD),
    ]


class EXIT_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("dwExitCode", wt.DWORD)]


class LOAD_DLL_DEBUG_INFO(ctypes.Structure):
    _fields_ = [
        ("hFile", wt.HANDLE),
        ("lpBaseOfDll", wt.LPVOID),
        ("dwDebugInfoFileOffset", wt.DWORD),
        ("nDebugInfoSize", wt.DWORD),
        ("lpImageName", wt.LPVOID),
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


class CONTEXT64(ctypes.Structure):
    _fields_ = [
        ("P1Home", ctypes.c_ulonglong),
        ("P2Home", ctypes.c_ulonglong),
        ("P3Home", ctypes.c_ulonglong),
        ("P4Home", ctypes.c_ulonglong),
        ("P5Home", ctypes.c_ulonglong),
        ("P6Home", ctypes.c_ulonglong),
        ("ContextFlags", wt.DWORD),
        ("MxCsr", wt.DWORD),
        ("SegCs", wt.WORD),
        ("SegDs", wt.WORD),
        ("SegEs", wt.WORD),
        ("SegFs", wt.WORD),
        ("SegGs", wt.WORD),
        ("SegSs", wt.WORD),
        ("EFlags", wt.DWORD),
        ("Dr0", ctypes.c_ulonglong),
        ("Dr1", ctypes.c_ulonglong),
        ("Dr2", ctypes.c_ulonglong),
        ("Dr3", ctypes.c_ulonglong),
        ("Dr6", ctypes.c_ulonglong),
        ("Dr7", ctypes.c_ulonglong),
        ("Rax", ctypes.c_ulonglong),
        ("Rcx", ctypes.c_ulonglong),
        ("Rdx", ctypes.c_ulonglong),
        ("Rbx", ctypes.c_ulonglong),
        ("Rsp", ctypes.c_ulonglong),
        ("Rbp", ctypes.c_ulonglong),
        ("Rsi", ctypes.c_ulonglong),
        ("Rdi", ctypes.c_ulonglong),
        ("R8", ctypes.c_ulonglong),
        ("R9", ctypes.c_ulonglong),
        ("R10", ctypes.c_ulonglong),
        ("R11", ctypes.c_ulonglong),
        ("R12", ctypes.c_ulonglong),
        ("R13", ctypes.c_ulonglong),
        ("R14", ctypes.c_ulonglong),
        ("R15", ctypes.c_ulonglong),
        ("Rip", ctypes.c_ulonglong),
        ("FltSave", ctypes.c_byte * 512),
        ("VectorRegister", ctypes.c_byte * (26 * 16)),
        ("VectorControl", ctypes.c_ulonglong),
        ("DebugControl", ctypes.c_ulonglong),
        ("LastBranchToRip", ctypes.c_ulonglong),
        ("LastBranchFromRip", ctypes.c_ulonglong),
        ("LastExceptionToRip", ctypes.c_ulonglong),
        ("LastExceptionFromRip", ctypes.c_ulonglong),
    ]


kernel32.CreateProcessW.argtypes = [
    wt.LPCWSTR, wt.LPWSTR, wt.LPVOID, wt.LPVOID, wt.BOOL, wt.DWORD,
    wt.LPVOID, wt.LPCWSTR, ctypes.POINTER(STARTUPINFOW), ctypes.POINTER(PROCESS_INFORMATION)
]
kernel32.CreateProcessW.restype = wt.BOOL
kernel32.WaitForDebugEvent.argtypes = [ctypes.POINTER(DEBUG_EVENT), wt.DWORD]
kernel32.WaitForDebugEvent.restype = wt.BOOL
kernel32.ContinueDebugEvent.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD]
kernel32.ContinueDebugEvent.restype = wt.BOOL
kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, wt.LPCVOID, wt.LPVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.ReadProcessMemory.restype = wt.BOOL
kernel32.WriteProcessMemory.argtypes = [wt.HANDLE, wt.LPVOID, wt.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.WriteProcessMemory.restype = wt.BOOL
kernel32.FlushInstructionCache.argtypes = [wt.HANDLE, wt.LPCVOID, ctypes.c_size_t]
kernel32.FlushInstructionCache.restype = wt.BOOL
kernel32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.POINTER(CONTEXT64)]
kernel32.GetThreadContext.restype = wt.BOOL
kernel32.SetThreadContext.argtypes = [wt.HANDLE, ctypes.POINTER(CONTEXT64)]
kernel32.SetThreadContext.restype = wt.BOOL
kernel32.OpenThread.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel32.OpenThread.restype = wt.HANDLE


def read_c_string(process, address, limit=512):
    if not address:
        return ""
    buf = ctypes.create_string_buffer(limit)
    got = ctypes.c_size_t()
    if not kernel32.ReadProcessMemory(process, ctypes.c_void_p(address), buf, limit, ctypes.byref(got)):
        return ""
    return buf.raw.split(b"\x00", 1)[0].decode("mbcs", errors="ignore")


def patch_byte(process, address, value):
    data = (ctypes.c_ubyte * 1)(value)
    written = ctypes.c_size_t()
    if not kernel32.WriteProcessMemory(process, ctypes.c_void_p(address), data, 1, ctypes.byref(written)):
        raise ctypes.WinError(ctypes.get_last_error())
    kernel32.FlushInstructionCache(process, ctypes.c_void_p(address), 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("--workdir", type=Path, default=None)
    parser.add_argument("--seconds", type=int, default=30)
    args = parser.parse_args()

    local_kernel32 = pefile.PE(r"C:\Windows\System32\kernel32.dll")
    loadlib_rva = next(sym.address for sym in local_kernel32.DIRECTORY_ENTRY_EXPORT.symbols if sym.name == b"LoadLibraryA")

    startup = STARTUPINFOW()
    startup.cb = ctypes.sizeof(startup)
    proc = PROCESS_INFORMATION()
    cmd = ctypes.create_unicode_buffer(f"\"{args.exe}\"")
    workdir = str(args.workdir if args.workdir else args.exe.parent)
    if not kernel32.CreateProcessW(None, cmd, None, None, False, DEBUG_ONLY_THIS_PROCESS, None, workdir, ctypes.byref(startup), ctypes.byref(proc)):
        raise ctypes.WinError(ctypes.get_last_error())

    print(f"[start] pid={proc.dwProcessId}", flush=True)
    event = DEBUG_EVENT()
    deadline = time.time() + args.seconds
    bp_addr = None
    bp_orig = None
    rearm = False

    while True:
        remaining = deadline - time.time()
        if remaining <= 0:
            print("[timeout]", flush=True)
            break
        if not kernel32.WaitForDebugEvent(ctypes.byref(event), int(remaining * 1000)):
            print("[timeout]", flush=True)
            break

        code = event.dwDebugEventCode
        if code == LOAD_DLL_DEBUG_EVENT and bp_addr is None:
            info = event.u.LoadDll
            base = ctypes.cast(info.lpBaseOfDll, ctypes.c_void_p).value
            if base and read_c_string(proc.hProcess, 0) == "":
                pass
            # kernel32 is loaded very early; use base if image name unavailable
            # pick first base in system32\\kernel32.dll by matching RVA range of exports
            if base and bp_addr is None:
                # heuristic: first system dll after ntdll with exportable kernel32 range
                # test by reading prospective byte
                candidate = base + loadlib_rva
                buf = ctypes.create_string_buffer(1)
                got = ctypes.c_size_t()
                if kernel32.ReadProcessMemory(proc.hProcess, ctypes.c_void_p(candidate), buf, 1, ctypes.byref(got)):
                    bp_addr = candidate
                    bp_orig = buf.raw[0]
                    patch_byte(proc.hProcess, bp_addr, 0xCC)
                    print(f"[hook] LoadLibraryA=0x{bp_addr:x}", flush=True)
            if info.hFile:
                kernel32.CloseHandle(info.hFile)
        elif code == EXCEPTION_DEBUG_EVENT and bp_addr is not None:
            exc = event.u.Exception.ExceptionRecord
            addr = ctypes.cast(exc.ExceptionAddress, ctypes.c_void_p).value
            if exc.ExceptionCode == EXCEPTION_BREAKPOINT and addr == bp_addr:
                thread = kernel32.OpenThread(0x1F03FF, False, event.dwThreadId)
                ctx = CONTEXT64()
                ctx.ContextFlags = 0x100001
                if kernel32.GetThreadContext(thread, ctypes.byref(ctx)):
                    name = read_c_string(proc.hProcess, ctx.Rcx)
                    print(f"[LoadLibraryA] tid={event.dwThreadId} rcx=0x{ctx.Rcx:x} name={name}", flush=True)
                    patch_byte(proc.hProcess, bp_addr, bp_orig)
                    ctx.Rip = bp_addr
                    ctx.EFlags |= 0x100
                    kernel32.SetThreadContext(thread, ctypes.byref(ctx))
                    rearm = True
                kernel32.CloseHandle(thread)
            elif exc.ExceptionCode == EXCEPTION_SINGLE_STEP and rearm:
                patch_byte(proc.hProcess, bp_addr, 0xCC)
                rearm = False
        elif code == EXIT_PROCESS_DEBUG_EVENT:
            print(f"[exit] code={event.u.ExitProcess.dwExitCode} (0x{event.u.ExitProcess.dwExitCode:08x})", flush=True)
            kernel32.ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE)
            break

        kernel32.ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE)


if __name__ == "__main__":
    main()
