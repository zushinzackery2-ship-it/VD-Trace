from __future__ import annotations

import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import messagebox

from .cli_bridge import DumpModuleSupport, TraceCli, UiTaskRunner
from .app_actions import clear_logs, dump_module, load_agent, read_memory, start_trace, stop_trace, write_memory
from .app_status import format_trace_status_text
from .loader_controller import LoaderController
from .models import LoaderSessionSnapshot, default_output_path, is_auto_output_path
from .settings_store import load_settings, save_settings
from .trace_preview import TracePreviewBuffer
from .ui_state import UiCallbacks, create_ui_variables
from .view import VdtraceMainView


class VdtraceGuiApp:
    def __init__(self, root: tk.Tk, repo_root: Path, ctl_path: Path, agent_path: Path) -> None:
        self.root = root
        self.repo_root = repo_root
        self.cli = TraceCli(ctl_path=ctl_path, workdir=repo_root)
        self.loader = LoaderController()
        self.loader.start()
        self.command_lines: list[str] = []
        self.session_lookup: dict[str, LoaderSessionSnapshot] = {}
        self.selected_session: LoaderSessionSnapshot | None = None
        self.sessions_revision = 0
        self.logs_revision = 0
        self.trace_preview = TracePreviewBuffer()
        self.trace_running = False
        self.trace_writing = False
        self.status_poll_pending = False
        self.next_status_poll_at = 0.0
        self.next_session_poll_at = 0.0
        self.vars = create_ui_variables(root, agent_path)
        load_settings(repo_root, self.vars)
        self.view = VdtraceMainView(
            root=root,
            variables=self.vars,
            callbacks=UiCallbacks(
                refresh_sessions=lambda: self._refresh_sessions(True),
                apply_selected_session=self._apply_selected_session,
                load_agent=lambda: self._load_agent(),
                dump_module=lambda: self._dump_module(),
                start_trace=lambda: self._start_trace(),
                stop_trace=lambda: self._stop_trace(),
                copy_trace=self._copy_trace_preview,
                clear_log=lambda: self._clear_logs(),
                read_memory=lambda: self._read_memory(),
                write_memory=lambda: self._write_memory(),
                memory_from_trace=lambda: self._memory_from_trace(),
                memory_from_log=lambda: self._memory_from_log(),
            ),
        )
        self.task_runner = UiTaskRunner(self.view, self.vars.log_status_var)
        self.dump_support = DumpModuleSupport(
            cli=self.cli,
            loader=self.loader,
            variables=self.vars,
            view=self.view,
            dispatch=self.task_runner.dispatch,
            selected_pid=self._selected_pid,
        )
        self.view.set_trace_running(False)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.bind("<Configure>", self._handle_resize)
        self.root.after(250, self._poll)

    def _log(self, line: str) -> None:
        self.command_lines.append(line)
        if len(self.command_lines) > 512:
            self.command_lines = self.command_lines[-512:]
        self._refresh_loader_logs(force=True)

    def _set_trace_running(self, running: bool) -> None:
        self.trace_running = running
        self.view.set_trace_running(running)

    def _set_trace_writing(self, writing: bool) -> None:
        self.trace_writing = writing
        self.view.set_trace_writing(writing)

    def _refresh_sessions(self, force: bool) -> None:
        now = time.monotonic()
        if not force and now < self.next_session_poll_at and self.sessions_revision == self.loader.sessions_revision:
            return
        selected_pid = self.selected_session.pid if self.selected_session else 0
        sessions = self.loader.snapshot_sessions()
        self.session_lookup = {item.display_name: item for item in sessions}
        if self.view.session_combo is not None:
            self.view.session_combo["values"] = list(self.session_lookup.keys())
        if selected_pid:
            for snapshot in sessions:
                if snapshot.pid == selected_pid:
                    self.vars.session_var.set(snapshot.display_name)
                    self.selected_session = snapshot
                    break
        elif sessions:
            self.vars.session_var.set(sessions[0].display_name)
            self.selected_session = sessions[0]
        else:
            self.selected_session = None
            self.vars.session_var.set("")
        self._apply_selected_session()
        self.sessions_revision = self.loader.sessions_revision
        self.next_session_poll_at = now + 1.0
        self.vars.log_status_var.set(f"在线目标 {len(sessions)} 个，控制输出 {len(self.command_lines)} 条。")

    def _apply_selected_session(self) -> None:
        previous_pid = self.selected_session.pid if self.selected_session is not None else 0
        self.selected_session = self.session_lookup.get(self.vars.session_var.get())
        if self.selected_session is None:
            self._set_trace_running(False)
            self._set_trace_writing(False)
            self.vars.active_target_var.set("当前目标：还没有选中目标进程。")
            self.vars.pid_var.set("")
            self.vars.session_path_var.set("当前没有选中的目标进程。")
            self.vars.session_status_var.set("等待目标进程...")
            self.vars.trace_status_var.set("等待追踪启动。")
            self.dump_support.reset_selection()
            return
        self.next_status_poll_at = 0.0
        pid = self.selected_session.pid
        self.vars.active_target_var.set(f"当前目标：[{pid}] {self.selected_session.process_path}")
        self.vars.pid_var.set(str(pid))
        self.vars.session_path_var.set(self.selected_session.process_path)
        self.vars.session_status_var.set(self.selected_session.capability_text)
        current_output = self.vars.output_var.get().strip()
        if pid != previous_pid and (not current_output or is_auto_output_path(current_output, previous_pid)):
            self.vars.output_var.set("")
        self.dump_support.on_session_selected()
        self.dump_support.refresh_modules(True)

    def _refresh_loader_logs(self, force: bool = False) -> None:
        if not force and self.logs_revision == self.loader.logs_revision:
            return
        log_parts = []
        loader_text = self.loader.snapshot_logs()
        if loader_text:
            log_parts.append(loader_text)
        if self.command_lines:
            log_parts.append("\r\n".join(self.command_lines))
        self.view.set_log_text("\r\n".join(log_parts))
        self.logs_revision = self.loader.logs_revision
        self.vars.log_status_var.set(f"在线目标 {len(self.session_lookup)} 个，控制输出 {len(self.command_lines)} 条。")

    def _refresh_trace_preview(self) -> None:
        self.vars.preview_status_var.set(
            self.trace_preview.refresh(
                self.vars.agent_var.get(),
                self.vars.output_var.get(),
                self.vars.trigger_var.get().strip() if self.vars.trigger_enabled_var.get() else "",
                self.view.set_trace_text,
            )
        )

    def _selected_pid(self) -> int:
        return int(self.vars.pid_var.get() or "0")

    def _handle_resize(self, _event: tk.Event) -> None:
        self.view.update_wraplengths()

    def _selected_session_or_warn(self) -> LoaderSessionSnapshot | None:
        if self.selected_session is None or self.selected_session.pid == 0:
            messagebox.showerror("VD-Trace", "当前没有选中的目标进程。")
            return None
        return self.selected_session

    def _poll_trace_status(self) -> None:
        pid = self._selected_pid()
        if pid == 0 or self.task_runner.busy or self.status_poll_pending or time.monotonic() < self.next_status_poll_at:
            return
        self.status_poll_pending = True

        def runner() -> None:
            self.task_runner.dispatch(self._complete_trace_status, (pid, self.cli.status(pid)), False)

        threading.Thread(target=runner, daemon=True).start()

    def _complete_trace_status(self, payload) -> None:
        self.status_poll_pending = False
        self.next_status_poll_at = time.monotonic() + 1.0
        pid, result = payload
        if pid == self._selected_pid():
            self._set_trace_running(result.success and "running=1" in result.message)
            self._set_trace_writing(result.success and "writing=1" in result.message)
            self.vars.trace_status_var.set(format_trace_status_text(result.message))

    def _load_agent(self) -> None:
        load_agent(self)

    def _dump_module(self) -> None:
        dump_module(self)

    def _start_trace(self) -> None:
        start_trace(self)

    def _stop_trace(self) -> None:
        stop_trace(self)

    def _read_memory(self) -> None:
        read_memory(self)

    def _write_memory(self) -> None:
        write_memory(self)

    def _memory_from_trace(self) -> None:
        address = self.view.extract_trace_address()
        if address:
            self.vars.memory_address_var.set(address)
            self.vars.memory_status_var.set(f"已带入追踪地址：{address}")
            self.view.select_page("memory")
        else:
            self.vars.memory_status_var.set("追踪页当前没有可带入的地址。")

    def _memory_from_log(self) -> None:
        address = self.view.extract_log_address()
        if address:
            self.vars.memory_address_var.set(address)
            self.vars.memory_status_var.set(f"已带入日志地址：{address}")
            self.view.select_page("memory")
        else:
            self.vars.memory_status_var.set("日志页当前没有可带入的地址。")

    def _copy_trace_preview(self) -> None:
        copied = self.view.copy_trace_text()
        if copied > 0:
            self.vars.preview_status_var.set(f"已复制 {copied} 个字符到剪贴板。")
        else:
            self.vars.preview_status_var.set("当前没有可复制的追踪内容。")

    def _clear_logs(self) -> None:
        clear_logs(self)

    def _poll(self) -> None:
        self.task_runner.drain()
        self._poll_trace_status()
        self.dump_support.refresh_modules(False)
        self._refresh_sessions(False)
        self._refresh_loader_logs()
        self._refresh_trace_preview()
        self.root.after(250, self._poll)

    def _on_close(self) -> None:
        try:
            save_settings(self.repo_root, self.vars)
        finally:
            self.loader.stop()
            self.root.destroy()
