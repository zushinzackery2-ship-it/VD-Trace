from __future__ import annotations

import subprocess
import time
from pathlib import Path

from .models import CommandResult, TraceConfig


class TraceCli:
    def __init__(self, ctl_path: Path, workdir: Path) -> None:
        self._ctl_path = ctl_path
        self._workdir = workdir

    def run(self, args: list[str]) -> CommandResult:
        command = [str(self._ctl_path), *args]
        completed = subprocess.run(
            command,
            cwd=self._workdir,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        output = (completed.stdout or completed.stderr).strip()
        success = output.startswith("[ok] ")
        message = output[5:] if success else output[7:] if output.startswith("[fail] ") else output
        return CommandResult(success=success, message=message or "命令没有输出。", raw_output=output)

    def ping(self, pid: int) -> CommandResult:
        return self.run(["ping", str(pid)])

    def status(self, pid: int) -> CommandResult:
        return self.run(["status", str(pid)])

    def start(self, pid: int) -> CommandResult:
        return self.run(["start", str(pid)])

    def stop(self, pid: int) -> CommandResult:
        return self.run(["stop", str(pid)])

    def modules(self, pid: int, include_system_modules: bool = False) -> CommandResult:
        args = ["modules", str(pid)]
        if include_system_modules:
            args.append("all")
        return self.run(args)

    def dump_module(self, pid: int, module_name: str, output_directory: str = r".\dump") -> CommandResult:
        return self.run(["dump", str(pid), module_name, output_directory])

    def read_memory(self, pid: int, address_text: str, size: int = 64) -> CommandResult:
        return self.run(["read", str(pid), address_text, str(size)])

    def write_memory(self, pid: int, address_text: str, data: bytes) -> CommandResult:
        return self.run(["write", str(pid), address_text, data.hex()])

    def inject(self, pid: int, agent_path: str) -> CommandResult:
        return self.run(["inject", str(pid), agent_path])

    def configure(self, pid: int, config: TraceConfig) -> CommandResult:
        return self.run(config.cli_args(pid))

    def wait_until_online(self, pid: int, timeout_ms: int) -> bool:
        deadline = time.time() + timeout_ms / 1000.0
        while time.time() < deadline:
            if self.ping(pid).success:
                return True
            time.sleep(0.1)
        return self.ping(pid).success
