from __future__ import annotations

from pathlib import PureWindowsPath

from .models import LoaderSessionSnapshot


_PREFERRED_PROCESS_BASENAMES = (
    "endfield.exe",
)

_ALLOWED_PROCESS_BASENAMES = set(_PREFERRED_PROCESS_BASENAMES)


def process_basename(process_path: str) -> str:
    normalized = process_path.strip().replace("/", "\\")
    if not normalized:
        return ""
    return PureWindowsPath(normalized).name.lower()


def is_allowed_loader_process_path(process_path: str) -> bool:
    return process_basename(process_path) in _ALLOWED_PROCESS_BASENAMES


def should_expose_loader_session(session: LoaderSessionSnapshot) -> bool:
    return (
        session.connected
        and session.hello_received
        and is_allowed_loader_process_path(session.process_path)
    )


def _session_sort_key(session: LoaderSessionSnapshot) -> tuple[int, str, int]:
    basename = process_basename(session.process_path)
    preferred_rank = 0 if basename in _PREFERRED_PROCESS_BASENAMES else 1
    return (
        preferred_rank,
        basename,
        session.pid,
    )


def filter_loader_sessions(sessions: list[LoaderSessionSnapshot]) -> list[LoaderSessionSnapshot]:
    visible = [session for session in sessions if should_expose_loader_session(session)]
    visible.sort(key=_session_sort_key)
    return visible


SELF_TEST_SESSIONS: list[LoaderSessionSnapshot] = [
    LoaderSessionSnapshot(session_id=1, pid=300, process_path=r"C:\Game\PlatformProcess.exe", hello_received=True),
    LoaderSessionSnapshot(session_id=2, pid=200, process_path=r"C:\Game\Endfield.exe", hello_received=True),
    LoaderSessionSnapshot(session_id=3, pid=100, process_path="dummy.exe", hello_received=True),
]

SELF_TEST_EXPECTED_PIDS: list[int] = [200]
