from __future__ import annotations

import ctypes
from ctypes import wintypes


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
PIPE_ACCESS_DUPLEX = 0x00000003
PIPE_TYPE_BYTE = 0x00000000
PIPE_READMODE_BYTE = 0x00000000
PIPE_WAIT = 0x00000000
PIPE_UNLIMITED_INSTANCES = 255


kernel32.CreateNamedPipeW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPVOID,
]
kernel32.CreateNamedPipeW.restype = wintypes.HANDLE

kernel32.ConnectNamedPipe.argtypes = [wintypes.HANDLE, wintypes.LPVOID]
kernel32.ConnectNamedPipe.restype = wintypes.BOOL

kernel32.CreateFileW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPVOID,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.HANDLE,
]
kernel32.CreateFileW.restype = wintypes.HANDLE

kernel32.WaitNamedPipeW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
kernel32.WaitNamedPipeW.restype = wintypes.BOOL

kernel32.ReadFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
kernel32.ReadFile.restype = wintypes.BOOL

kernel32.WriteFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPCVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
kernel32.WriteFile.restype = wintypes.BOOL

kernel32.FlushFileBuffers.argtypes = [wintypes.HANDLE]
kernel32.FlushFileBuffers.restype = wintypes.BOOL

kernel32.DisconnectNamedPipe.argtypes = [wintypes.HANDLE]
kernel32.DisconnectNamedPipe.restype = wintypes.BOOL

kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL

kernel32.GetCurrentProcessId.argtypes = []
kernel32.GetCurrentProcessId.restype = wintypes.DWORD
kernel32.PeekNamedPipe.argtypes = [
    wintypes.HANDLE,
    wintypes.LPVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    ctypes.POINTER(wintypes.DWORD),
    ctypes.POINTER(wintypes.DWORD),
]
kernel32.PeekNamedPipe.restype = wintypes.BOOL


class PipeError(OSError):
    pass


def _raise_last_error(prefix: str) -> PipeError:
    error = ctypes.get_last_error()
    raise PipeError(error, f"{prefix} failed", None, error)


def current_process_id() -> int:
    return int(kernel32.GetCurrentProcessId())


def create_pipe_server(pipe_name: str) -> int:
    handle = kernel32.CreateNamedPipeW(
        pipe_name,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        4096,
        4096,
        0,
        None,
    )
    if handle == INVALID_HANDLE_VALUE:
        _raise_last_error("CreateNamedPipeW")
    return int(handle)


def wait_for_pipe_client(handle: int) -> bool:
    ok = kernel32.ConnectNamedPipe(wintypes.HANDLE(handle), None)
    if ok:
        return True
    error = ctypes.get_last_error()
    return error == 535


def connect_to_pipe(pipe_name: str, timeout_ms: int) -> int | None:
    if not kernel32.WaitNamedPipeW(pipe_name, timeout_ms):
        return None
    handle = kernel32.CreateFileW(
        pipe_name,
        GENERIC_READ | GENERIC_WRITE,
        0,
        None,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        None,
    )
    if handle == INVALID_HANDLE_VALUE:
        return None
    return int(handle)


def close_pipe(handle: int | None) -> None:
    if handle in (None, 0, INVALID_HANDLE_VALUE):
        return
    kernel32.FlushFileBuffers(wintypes.HANDLE(handle))
    kernel32.DisconnectNamedPipe(wintypes.HANDLE(handle))
    kernel32.CloseHandle(wintypes.HANDLE(handle))


def close_handle(handle: int | None) -> None:
    if handle in (None, 0, INVALID_HANDLE_VALUE):
        return
    kernel32.CloseHandle(wintypes.HANDLE(handle))


def peek_named_pipe(handle: int) -> int:
    available = wintypes.DWORD()
    ok = kernel32.PeekNamedPipe(
        wintypes.HANDLE(handle),
        None,
        0,
        None,
        ctypes.byref(available),
        None,
    )
    if not ok:
        _raise_last_error("PeekNamedPipe")
    return int(available.value)


def read_exact(handle: int, size: int) -> bytes:
    buffer = ctypes.create_string_buffer(size)
    view = memoryview(buffer)
    total = 0
    while total < size:
        chunk = ctypes.c_void_p(ctypes.addressof(buffer) + total)
        read = wintypes.DWORD()
        ok = kernel32.ReadFile(
            wintypes.HANDLE(handle),
            chunk,
            size - total,
            ctypes.byref(read),
            None,
        )
        if not ok or read.value == 0:
            _raise_last_error("ReadFile")
        total += int(read.value)
    return bytes(view[:size])


def write_all(handle: int, data: bytes) -> None:
    if not data:
        return
    total = 0
    raw = ctypes.create_string_buffer(data, len(data))
    while total < len(data):
        wrote = wintypes.DWORD()
        ok = kernel32.WriteFile(
            wintypes.HANDLE(handle),
            ctypes.c_void_p(ctypes.addressof(raw) + total),
            len(data) - total,
            ctypes.byref(wrote),
            None,
        )
        if not ok or wrote.value == 0:
            _raise_last_error("WriteFile")
        total += int(wrote.value)
