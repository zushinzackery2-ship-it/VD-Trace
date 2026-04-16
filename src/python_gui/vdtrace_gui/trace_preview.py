from __future__ import annotations

from pathlib import Path


class TracePreviewBuffer:
    _TAIL_LINE_LIMIT = 4096
    _TAIL_READ_CHUNK = 65536

    def __init__(self) -> None:
        self.trace_path = ""
        self.trace_signature = (-1, -1)
        self.trace_line_count = 0
        self.awaiting_new_data = False

    def begin_round(self, agent_path: str, output_path: str, replace_text) -> None:
        trace_path = self._resolved_path(agent_path, output_path)
        signature = self._path_signature(Path(trace_path)) if trace_path else (-1, -1)
        self._reset(trace_path, signature, replace_text, True)

    def refresh(self, agent_path: str, output_path: str, trigger_point: str, replace_text) -> str:
        trace_path = self._resolved_path(agent_path, output_path)
        if trace_path != self.trace_path:
            self._reset(trace_path, (-1, -1), replace_text, True)
        if not trace_path:
            return "还没填写输出路径。"

        path = Path(trace_path)
        if not path.exists():
            return f"等待输出文件生成: {path.name}"

        signature = self._path_signature(path)
        if signature == self.trace_signature:
            return self._status_text(path.name, trigger_point)

        preview_text, preview_lines = self._read_tail_text(path)
        self.trace_path = trace_path
        self.trace_signature = signature
        self.trace_line_count = preview_lines
        self.awaiting_new_data = preview_lines == 0
        replace_text(preview_text)
        return self._status_text(path.name, trigger_point)

    def _reset(self, trace_path: str, signature: tuple[int, int], replace_text, awaiting_new_data: bool) -> None:
        self.trace_path = trace_path
        self.trace_signature = signature
        self.trace_line_count = 0
        self.awaiting_new_data = awaiting_new_data
        replace_text("")

    def _status_text(self, file_name: str, trigger_point: str) -> str:
        if self.trace_line_count == 0 and self.awaiting_new_data:
            return (
                f"输出文件已创建，正在等待命中触发点：{trigger_point}"
                if trigger_point
                else f"输出文件已创建，正在等待第一条追踪事件：{file_name}"
            )
        return f"当前显示日志末端 {self.trace_line_count} 行，输出文件 {file_name}。"

    @classmethod
    def _read_tail_text(cls, path: Path) -> tuple[str, int]:
        tail = cls._read_tail_bytes(path)
        if not tail:
            return "", 0
        text = tail.decode("utf-8", errors="replace")
        lines = text.splitlines()
        if len(lines) > cls._TAIL_LINE_LIMIT:
            lines = lines[-cls._TAIL_LINE_LIMIT:]
        return "\n".join(lines), len(lines)

    @classmethod
    def _read_tail_bytes(cls, path: Path) -> bytes:
        with path.open("rb") as handle:
            handle.seek(0, 2)
            position = handle.tell()
            chunks: list[bytes] = []
            newline_count = 0
            while position > 0 and newline_count <= cls._TAIL_LINE_LIMIT:
                read_size = min(cls._TAIL_READ_CHUNK, position)
                position -= read_size
                handle.seek(position)
                chunk = handle.read(read_size)
                if not chunk:
                    break
                chunks.append(chunk)
                newline_count += chunk.count(b"\n")
        return b"".join(reversed(chunks))

    @staticmethod
    def _path_signature(path: Path) -> tuple[int, int]:
        stat = path.stat()
        return stat.st_size, getattr(stat, "st_mtime_ns", int(stat.st_mtime * 1_000_000_000))

    @staticmethod
    def _resolved_path(agent_path: str, output_path: str) -> str:
        output = output_path.strip()
        if not output:
            return ""
        path = Path(output)
        if path.is_absolute():
            return str(path)
        agent_dir = Path(agent_path).expanduser().resolve().parent
        return str((agent_dir / path).resolve())
