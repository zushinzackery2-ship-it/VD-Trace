from __future__ import annotations

import struct
import threading
import time
from dataclasses import dataclass, field
from typing import Dict, List

from .models import (
    LOADER_MAGIC,
    LOADER_MAX_PATH_CHARS,
    LOADER_MAX_TEXT_CHARS,
    LOADER_MSG_AGENT_HELLO,
    LOADER_MSG_AGENT_LOG,
    LOADER_MSG_LOAD_DLL_REPLY,
    LOADER_MSG_LOAD_DLL_REQUEST,
    LOADER_PIPE_NAME,
    LoaderSessionSnapshot,
)
from .session_filter import filter_loader_sessions, is_allowed_loader_process_path
from .win32pipe import (
    PipeError,
    close_handle,
    connect_to_pipe,
    create_pipe_server,
    current_process_id,
    read_exact,
    peek_named_pipe,
    wait_for_pipe_client,
    write_all,
)


HEADER_STRUCT = struct.Struct("<IIII")
LOAD_DLL_REPLY_STRUCT = struct.Struct(f"<II{LOADER_MAX_PATH_CHARS * 2}s{LOADER_MAX_TEXT_CHARS * 2}s")


def _decode_utf16_text(raw: bytes) -> str:
    text = raw.decode("utf-16-le", errors="ignore")
    return text.split("\x00", 1)[0]


def _decode_ansi_text(raw: bytes) -> str:
    text = raw.split(b"\x00", 1)[0]
    return text.decode("utf-8", errors="ignore") or text.decode("mbcs", errors="ignore")


def _encode_utf16_buffer(text: str, chars: int) -> bytes:
    encoded = text.encode("utf-16-le")
    return encoded[: (chars - 1) * 2].ljust(chars * 2, b"\x00")


@dataclass(slots=True)
class _LoaderSessionState:
    snapshot: LoaderSessionSnapshot
    handle: int
    send_lock: threading.Lock = field(default_factory=threading.Lock)


class LoaderController:
    def __init__(self) -> None:
        self._stop_event = threading.Event()
        self._sessions: Dict[int, _LoaderSessionState] = {}
        self._session_lock = threading.Lock()
        self._log_lines: List[str] = []
        self._log_lock = threading.Lock()
        self._sessions_revision = 1
        self._logs_revision = 1
        self._next_session_id = 1
        self._accept_thread = threading.Thread(target=self._accept_loop, daemon=True)

    def start(self) -> None:
        self._accept_thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        poke = connect_to_pipe(LOADER_PIPE_NAME, 20)
        close_handle(poke)

    @property
    def sessions_revision(self) -> int:
        return self._sessions_revision

    @property
    def logs_revision(self) -> int:
        return self._logs_revision

    def snapshot_sessions(self) -> List[LoaderSessionSnapshot]:
        with self._session_lock:
            sessions = [
                state.snapshot
                for state in self._sessions.values()
                if state.snapshot.connected and state.snapshot.hello_received
            ]
        return filter_loader_sessions(sessions)

    def snapshot_logs(self) -> str:
        with self._log_lock:
            return "\r\n".join(self._log_lines)

    def clear_logs(self) -> None:
        with self._log_lock:
            self._log_lines.clear()
            self._logs_revision += 1

    def send_load_request(self, session_id: int, dll_path: str) -> bool:
        with self._session_lock:
            state = self._sessions.get(session_id)
        if state is None:
            return False
        if not is_allowed_loader_process_path(state.snapshot.process_path):
            self._append_log(f"[Loader] 已拒绝向非主目标进程发送加载请求: pid={state.snapshot.pid} path={state.snapshot.process_path}")
            return False
        header = HEADER_STRUCT.pack(
            LOADER_MAGIC,
            LOADER_MSG_LOAD_DLL_REQUEST,
            HEADER_STRUCT.size + LOADER_MAX_PATH_CHARS * 2,
            current_process_id(),
        )
        payload = _encode_utf16_buffer(dll_path, LOADER_MAX_PATH_CHARS)
        try:
            with state.send_lock:
                write_all(state.handle, header + payload)
            return True
        except PipeError:
            return False

    def send_load_request_with_timeout(self, session_id: int, dll_path: str, timeout_ms: int) -> bool | None:
        result: dict[str, bool] = {}
        finished = threading.Event()

        def worker() -> None:
            result["ok"] = self.send_load_request(session_id, dll_path)
            finished.set()

        threading.Thread(target=worker, daemon=True).start()
        if not finished.wait(timeout_ms / 1000.0):
            return None
        return result.get("ok", False)

    def _append_log(self, line: str) -> None:
        with self._log_lock:
            self._log_lines.append(line)
            if len(self._log_lines) > 2048:
                del self._log_lines[: len(self._log_lines) - 2048]
            self._logs_revision += 1

    def _accept_loop(self) -> None:
        self._append_log("Python GUI Loader 控制器已开始监听命名管道。")
        while not self._stop_event.is_set():
            handle = None
            try:
                handle = create_pipe_server(LOADER_PIPE_NAME)
                if not wait_for_pipe_client(handle):
                    close_handle(handle)
                    continue
                session_id = self._next_session_id
                self._next_session_id += 1
                state = _LoaderSessionState(
                    snapshot=LoaderSessionSnapshot(session_id=session_id),
                    handle=handle,
                )
                with self._session_lock:
                    self._sessions[session_id] = state
                    self._sessions_revision += 1
                threading.Thread(
                    target=self._session_loop,
                    args=(state,),
                    daemon=True,
                ).start()
                handle = None
            except PipeError as exc:
                self._append_log(f"[Loader] 管道监听失败: {exc}")
                close_handle(handle)

    def _session_loop(self, state: _LoaderSessionState) -> None:
        try:
            while not self._stop_event.is_set():
                if peek_named_pipe(state.handle) < HEADER_STRUCT.size:
                    time.sleep(0.015)
                    continue
                header = read_exact(state.handle, HEADER_STRUCT.size)
                magic, kind, size, pid = HEADER_STRUCT.unpack(header)
                if magic != LOADER_MAGIC or size < HEADER_STRUCT.size:
                    break
                payload = read_exact(state.handle, size - HEADER_STRUCT.size)
                if kind == LOADER_MSG_AGENT_HELLO:
                    process_path = _decode_utf16_text(payload[: LOADER_MAX_PATH_CHARS * 2])
                    protocol_version, feature_flags = struct.unpack_from("<II", payload, LOADER_MAX_PATH_CHARS * 2)
                    state.snapshot.pid = pid
                    state.snapshot.process_path = process_path
                    state.snapshot.protocol_version = protocol_version
                    state.snapshot.feature_flags = feature_flags
                    state.snapshot.hello_received = True
                    self._append_log(f"[已连接] {state.snapshot.display_name}")
                    with self._session_lock:
                        self._sessions_revision += 1
                    continue
                if kind == LOADER_MSG_AGENT_LOG:
                    self._append_log(f"[Loader] pid={pid} {_decode_ansi_text(payload[:LOADER_MAX_TEXT_CHARS])}")
                    continue
                if kind == LOADER_MSG_LOAD_DLL_REPLY:
                    status, win32_error, dll_path_raw, text_raw = LOAD_DLL_REPLY_STRUCT.unpack(payload)
                    dll_path = _decode_utf16_text(dll_path_raw)
                    text = _decode_utf16_text(text_raw)
                    if status == 0:
                        self._append_log(f"[加载成功] pid={pid} 路径={dll_path}")
                    else:
                        self._append_log(f"[加载失败] pid={pid} 错误={win32_error} 路径={dll_path} 文本={text}")
        except PipeError:
            pass
        finally:
            state.snapshot.connected = False
            if state.snapshot.hello_received:
                self._append_log(f"[已断开] {state.snapshot.display_name}")
            close_handle(state.handle)
            with self._session_lock:
                self._sessions.pop(state.snapshot.session_id, None)
                self._sessions_revision += 1
