from __future__ import annotations

import threading
from collections.abc import Callable

from .control_cli import TraceCli
from .models import CommandResult
from .loader_controller import LoaderController
from .models import LoaderSessionSnapshot
from .trace_profile import TraceProfile, build_trace_config_from_profile
from .ui_state import UiVariables
from .view import VdtraceMainView


def build_trace_config_from_vars(variables: UiVariables):
    profile = TraceProfile(
        agent_path="",
        thread_id=variables.thread_var.get().strip() or "0",
        thread_capture=variables.thread_capture_var.get(),
        modules=variables.modules_var.get().strip(),
        output_path=variables.output_var.get().strip(),
        max_events=variables.max_events_var.get().strip() or "0",
        backend=variables.backend_var.get().strip() or "DR",
        call_depth=variables.call_depth_var.get().strip() or "0",
        outside_call_depth_enabled=variables.outside_call_depth_enabled_var.get(),
        outside_call_depth=variables.outside_call_depth_var.get().strip() or "3",
        outside_execution_mode=variables.outside_execution_mode_var.get().strip() or "EDGE",
        anonymous_exec_call_depth_enabled=variables.anonymous_exec_call_depth_enabled_var.get(),
        anonymous_exec_call_depth=variables.anonymous_exec_call_depth_var.get().strip() or "3",
        anonymous_exec_execution_mode=variables.anonymous_exec_execution_mode_var.get().strip() or "EDGE",
        module_call_depths=variables.module_call_depths_var.get().strip(),
        trigger_point=variables.trigger_var.get().strip(),
        probe_spec=variables.probe_var.get().strip(),
        probe_enabled=variables.probe_enabled_var.get(),
        trigger_enabled=variables.trigger_enabled_var.get(),
        block_main_thread=variables.block_main_thread_var.get(),
        trace_outside_modules=not variables.module_scope_var.get(),
        all_events=(variables.backend_var.get().strip().upper() == "TF"),
        repeat_hits=variables.repeat_var.get(),
        idle_escape_enabled=variables.idle_escape_enabled_var.get(),
        idle_escape_threshold=variables.idle_escape_threshold_var.get().strip() or "32",
        enhanced_sampling=variables.sample_var.get(),
        root_stop_on_return=variables.root_stop_var.get(),
        async_thread_handoff=variables.handoff_var.get(),
    )
    return build_trace_config_from_profile(profile)


class UiTaskRunner:
    def __init__(self, view: VdtraceMainView, log_status_var) -> None:
        self._view = view
        self._log_status_var = log_status_var
        self.busy = False
        self._pending: list[tuple[object, object, bool]] = []
        self._lock = threading.Lock()

    def dispatch(self, on_done, result, release_busy: bool) -> None:
        with self._lock:
            self._pending.append((on_done, result, release_busy))

    def run_background(self, busy_text: str, worker, on_done) -> bool:
        if self.busy:
            return False
        self.busy = True
        self._view.set_actions_enabled(False)
        self._log_status_var.set(busy_text)

        def runner() -> None:
            try:
                self.dispatch(on_done, worker(), True)
            except Exception as exc:
                self.dispatch(on_done, exc, True)

        threading.Thread(target=runner, daemon=True).start()
        return True

    def drain(self) -> None:
        with self._lock:
            pending = self._pending[:]
            self._pending.clear()
        for on_done, result, release_busy in pending:
            if release_busy:
                self.busy = False
                self._view.set_actions_enabled(True)
            on_done(result)


class DumpModuleSupport:
    def __init__(
        self,
        cli: TraceCli,
        loader: LoaderController,
        variables: UiVariables,
        view: VdtraceMainView,
        dispatch: Callable[[object, object, bool], None],
        selected_pid: Callable[[], int],
    ) -> None:
        self.cli = cli
        self.loader = loader
        self.vars = variables
        self.view = view
        self.dispatch = dispatch
        self.selected_pid = selected_pid
        self.pending = False
        self.modules_pid = 0
        self.set_modules([])
        self.set_status(r"等待真实模块列表；Dump+Fix 默认输出到 .\dump。")

    @staticmethod
    def default_output_directory() -> str:
        return r".\dump"

    @staticmethod
    def parse_modules_result(result: CommandResult) -> list[str]:
        if not result.success:
            return []
        return [line.strip() for line in result.message.splitlines() if line.strip()]

    def set_status(self, text: str) -> None:
        self.vars.dump_status_var.set(text)

    def set_modules(self, modules: list[str]) -> None:
        values: list[str] = []
        for module in modules:
            text = module.strip()
            if text and text not in values:
                values.append(text)
        self.view.set_dump_modules(values)
        current = self.vars.dump_module_var.get().strip()
        if current in values:
            self.vars.dump_module_var.set(current)
        elif values:
            self.vars.dump_module_var.set(values[0])
        else:
            self.vars.dump_module_var.set("")

    def reset_selection(self) -> None:
        self.modules_pid = 0
        self.set_modules([])
        self.set_status(r"当前没有选中目标进程；Dump+Fix 需要先拿到真实模块列表。")

    def on_session_selected(self) -> None:
        self.set_modules([])
        self.set_status(r"等待 Agent 上线后刷新真实模块列表。")

    def refresh_modules(self, force: bool = False) -> None:
        pid = self.selected_pid()
        if pid == 0 or self.pending:
            return
        if not force and self.modules_pid == pid:
            return
        if not self.cli.ping(pid).success:
            self.modules_pid = 0
            self.set_modules([])
            self.set_status(r"等待 Agent 上线后刷新真实模块列表。")
            return
        self.pending = True

        def runner() -> None:
            self.dispatch(self._complete_refresh, (pid, self.cli.modules(pid)), False)

        threading.Thread(target=runner, daemon=True).start()

    def apply_modules_from_result(self, pid: int, result: CommandResult) -> list[str]:
        modules = self.parse_modules_result(result)
        if modules:
            self.modules_pid = pid
            self.set_modules(modules)
        return modules

    def ensure_agent_online(self, session: LoaderSessionSnapshot) -> str | None:
        if self.cli.ping(session.pid).success:
            return None
        agent_path = self.vars.agent_var.get().strip()
        if session.source == "direct" or not session.supports_bootstrap:
            inject_result = self.cli.inject(session.pid, agent_path)
            if not inject_result.success:
                return inject_result.message
        else:
            load_result = self.loader.send_load_request_with_timeout(session.session_id, agent_path, 1500)
            if load_result is None:
                return "发送 Agent 拉起请求超时，目标进程的 Loader 可能已卡住。"
            if not load_result:
                return "发送 Agent 拉起请求失败。"
        if not self.cli.wait_until_online(session.pid, 5000):
            return "Agent IPC 未在 5 秒内上线。"
        return None

    def run_dump(self, session: LoaderSessionSnapshot, module_name: str):
        online_error = self.ensure_agent_online(session)
        if online_error:
            return ("error", online_error)
        return (
            "ok",
            self.cli.modules(session.pid, include_system_modules=False),
            self.cli.dump_module(session.pid, module_name, self.default_output_directory()),
        )

    def _complete_refresh(self, payload) -> None:
        self.pending = False
        pid, result = payload
        if pid != self.selected_pid():
            return
        modules = self.parse_modules_result(result)
        if modules:
            self.modules_pid = pid
            self.set_modules(modules)
            self.set_status(
                f"Agent 已上线，共发现 {len(modules)} 个模块；Dump+Fix 默认输出到 .\\dump，原始文件 *_dump_raw，修正版 *_dump_fix。"
            )
            return
        self.modules_pid = 0
        self.set_modules([])
        if result.success:
            self.set_status(r"未拿到真实模块列表：枚举结果为空。")
        else:
            detail = result.message.strip() or "模块枚举失败。"
            self.set_status(f"未拿到真实模块列表：{detail}")
