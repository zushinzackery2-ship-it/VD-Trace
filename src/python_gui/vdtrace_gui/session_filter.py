from __future__ import annotations

from .models import LoaderSessionSnapshot


def should_expose_loader_session(session: LoaderSessionSnapshot) -> bool:
    return session.connected and session.hello_received


def filter_loader_sessions(sessions: list[LoaderSessionSnapshot]) -> list[LoaderSessionSnapshot]:
    visible = [session for session in sessions if should_expose_loader_session(session)]
    visible.sort(key=lambda s: (s.pid,))
    return visible


SELF_TEST_SESSIONS: list[LoaderSessionSnapshot] = [
    LoaderSessionSnapshot(session_id=1, pid=300, process_path=r"C:\Game\PlatformProcess.exe", hello_received=True),
    LoaderSessionSnapshot(session_id=2, pid=200, process_path=r"C:\Game\Endfield.exe", hello_received=True),
    LoaderSessionSnapshot(session_id=3, pid=100, process_path="dummy.exe", hello_received=True),
    LoaderSessionSnapshot(session_id=4, pid=999, process_path="not_connected.exe", hello_received=True, connected=False),
]

SELF_TEST_EXPECTED_PIDS: list[int] = [100, 200, 300]