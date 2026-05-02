from __future__ import annotations

from tkinter import messagebox
from typing import TYPE_CHECKING

from .cli_bridge import build_trace_config_from_vars
from .memory_codec import encode_memory_write_bytes, parse_memory_size
from .models import default_output_path, is_auto_output_path

if TYPE_CHECKING:
    from .app import VdtraceGuiApp


def load_agent(app: "VdtraceGuiApp") -> None:
    session = app._selected_session_or_warn()
    if session is None:
        return

    def worker():
        agent_path = app.vars.agent_var.get().strip()
        return app.loader.send_load_request_with_timeout(session.session_id, agent_path, 1500)

    def on_done(result) -> None:
        if isinstance(result, Exception):
            app._log(f"[load] [fail] {result}")
            messagebox.showerror("VD-Trace", f"加载 Agent 时发生异常：{result}")
        elif hasattr(result, "success"):
            app._log(f"[load] {'[ok]' if result.success else '[fail]'} {result.message}")
            if not result.success:
                messagebox.showerror("VD-Trace", result.message)
            else:
                app.root.after(600, lambda: app.dump_support.refresh_modules(True))
        elif result is None:
            app._log("[load] [fail] 发送 Loader 加载请求超时。")
            messagebox.showerror("VD-Trace", "发送 Loader 加载请求超时，目标进程的 Loader 可能已卡住。")
        elif not result:
            app._log("[load] [fail] 发送 Loader 加载请求失败。")
            messagebox.showerror("VD-Trace", "发送 Loader 加载请求失败。")
        else:
            app._log("[load] [ok] 已发送 Agent 加载请求。")
            app.root.after(600, lambda: app.dump_support.refresh_modules(True))

    app.task_runner.run_background("正在加载 Agent...", worker, on_done)


def dump_module(app: "VdtraceGuiApp") -> None:
    session = app._selected_session_or_warn()
    if session is None:
        return
    module_name = app.vars.dump_module_var.get().strip()
    if not module_name:
        messagebox.showerror("VD-Trace", "转储模块名不能为空。")
        return

    def worker():
        return app.dump_support.run_dump(session, module_name)

    def on_done(result) -> None:
        if isinstance(result, Exception):
            app._log(f"[dump] [fail] {result}")
            messagebox.showerror("VD-Trace", f"执行 Dump+Fix 时发生异常：{result}")
            return
        if result[0] == "error":
            app._log(f"[dump] [fail] {result[1]}")
            messagebox.showerror("VD-Trace", result[1])
            return
        modules_result, dump_result = result[1], result[2]
        app.dump_support.apply_modules_from_result(session.pid, modules_result)
        app._log(f"[dump] {'[ok]' if dump_result.success else '[fail]'} {dump_result.message}")
        if dump_result.success:
            app.dump_support.set_status(
                f"Dump+Fix 完成：{module_name}，输出目录 {app.dump_support.default_output_directory()}。"
            )
        else:
            app.dump_support.set_status(f"Dump+Fix 失败：{module_name}。")
            messagebox.showerror("VD-Trace", dump_result.message)

    app.task_runner.run_background("正在执行模块 Dump+Fix...", worker, on_done)


def start_trace(app: "VdtraceGuiApp") -> None:
    session = app._selected_session_or_warn()
    if session is None:
        return
    try:
        config = build_trace_config_from_vars(app.vars)
    except ValueError as exc:
        messagebox.showerror("VD-Trace", str(exc) or "线程 ID、事件上限、过滤器或观测器规则格式无效。")
        return

    def worker():
        online_error = app.dump_support.ensure_agent_online(session)
        if online_error:
            return ("error", online_error)
        if is_auto_output_path(config.output_path, session.pid):
            config.output_path = default_output_path(session.pid)
        if config.auto_select_thread:
            config.thread_id = 0
        configure = app.cli.configure(session.pid, config)
        if not configure.success:
            return ("failed", configure, None)
        return ("ok", configure, app.cli.start(session.pid))

    def on_done(result) -> None:
        if isinstance(result, Exception):
            app._log(f"[start] [fail] {result}")
            messagebox.showerror("VD-Trace", f"启动追踪时发生异常：{result}")
            return
        if result[0] == "error":
            app._log(f"[start] [fail] {result[1]}")
            messagebox.showerror("VD-Trace", result[1])
            return
        configure, start = result[1], result[2]
        app._log(f"[configure] {'[ok]' if configure.success else '[fail]'} {configure.message}")
        if start is None:
            return
        app.vars.output_var.set(config.output_path)
        app._log(f"[start] {'[ok]' if start.success else '[fail]'} {start.message}")
        if start.success:
            app.trace_preview.begin_round(app.vars.agent_var.get(), config.output_path, app.view.set_trace_text)
            app._set_trace_running(True)
            app._set_trace_writing(False)
            trigger = config.trigger_point
            app.vars.trace_status_var.set("运行中 | 正在等待首批状态刷新。")
            if trigger and config.auto_select_thread and config.block_main_thread:
                app.vars.preview_status_var.set(f"已启动，当前屏蔽主线程，等待其他线程命中触发点：{trigger}")
            elif trigger and config.auto_select_thread:
                app.vars.preview_status_var.set(f"已启动，等待首个线程命中触发点：{trigger}")
            elif trigger:
                if config.thread_id == 0:
                    app.vars.preview_status_var.set(f"已启动，当前固定主线程等待命中触发点：{trigger}")
                else:
                    app.vars.preview_status_var.set(f"已启动，当前固定线程 {config.thread_id} 等待命中触发点：{trigger}")
            elif config.thread_id == 0:
                app.vars.preview_status_var.set("已启动，当前直接追主线程。")
            else:
                app.vars.preview_status_var.set(f"已启动，当前直接追线程 {config.thread_id}。")

    app.task_runner.run_background("正在配置并启动追踪...", worker, on_done)


def stop_trace(app: "VdtraceGuiApp") -> None:
    pid = app._selected_pid()
    if pid == 0:
        messagebox.showerror("VD-Trace", "PID 不能为空。")
        return

    def on_done(result) -> None:
        if isinstance(result, Exception):
            app._log(f"[stop] [fail] {result}")
            messagebox.showerror("VD-Trace", f"停止追踪时发生异常：{result}")
        else:
            if result.success:
                app._set_trace_running(False)
                app._set_trace_writing(False)
                app.vars.trace_status_var.set("待机 | 追踪已停止。")
            app._log(f"[stop] {'[ok]' if result.success else '[fail]'} {result.message}")

    app.task_runner.run_background("正在停止追踪...", lambda: app.cli.stop(pid), on_done)


def read_memory(app: "VdtraceGuiApp") -> None:
    session = app._selected_session_or_warn()
    if session is None:
        return

    address_text = app.vars.memory_address_var.get().strip()
    if not address_text:
        messagebox.showerror("VD-Trace", "内存地址不能为空。")
        return

    try:
        size = parse_memory_size(app.vars.memory_size_var.get())
    except ValueError as exc:
        messagebox.showerror("VD-Trace", str(exc))
        return

    def worker():
        online_error = app.dump_support.ensure_agent_online(session)
        if online_error:
            return ("error", online_error)
        return ("ok", app.cli.read_memory(session.pid, address_text, size))

    def on_done(result) -> None:
        if isinstance(result, Exception):
            app.vars.memory_status_var.set(f"读取内存失败：{result}")
            messagebox.showerror("VD-Trace", f"读取内存时发生异常：{result}")
            return
        if result[0] == "error":
            app.vars.memory_status_var.set(result[1])
            app._log(f"[memory-read] [fail] {result[1]}")
            messagebox.showerror("VD-Trace", result[1])
            return
        read_result = result[1]
        app.view.set_memory_text(read_result.message if read_result.success else "")
        app.vars.memory_status_var.set("读取完成。" if read_result.success else f"读取失败：{read_result.message}")
        app._log(f"[memory-read] {'[ok]' if read_result.success else '[fail]'} {read_result.message}")
        if not read_result.success:
            messagebox.showerror("VD-Trace", read_result.message)

    app.task_runner.run_background("正在读取内存...", worker, on_done)


def write_memory(app: "VdtraceGuiApp") -> None:
    session = app._selected_session_or_warn()
    if session is None:
        return

    address_text = app.vars.memory_address_var.get().strip()
    if not address_text:
        messagebox.showerror("VD-Trace", "内存地址不能为空。")
        return

    try:
        data = encode_memory_write_bytes(app.vars.memory_write_mode_var.get(), app.vars.memory_write_value_var.get())
    except ValueError as exc:
        messagebox.showerror("VD-Trace", str(exc))
        return

    def worker():
        online_error = app.dump_support.ensure_agent_online(session)
        if online_error:
            return ("error", online_error)
        return ("ok", app.cli.write_memory(session.pid, address_text, data))

    def on_done(result) -> None:
        if isinstance(result, Exception):
            app.vars.memory_status_var.set(f"写入内存失败：{result}")
            messagebox.showerror("VD-Trace", f"写入内存时发生异常：{result}")
            return
        if result[0] == "error":
            app.vars.memory_status_var.set(result[1])
            app._log(f"[memory-write] [fail] {result[1]}")
            messagebox.showerror("VD-Trace", result[1])
            return
        write_result = result[1]
        app.view.set_memory_text(write_result.message if write_result.success else "")
        app.vars.memory_status_var.set("写入完成。" if write_result.success else f"写入失败：{write_result.message}")
        app._log(f"[memory-write] {'[ok]' if write_result.success else '[fail]'} {write_result.message}")
        if not write_result.success:
            messagebox.showerror("VD-Trace", write_result.message)

    app.task_runner.run_background("正在写入内存...", worker, on_done)


def clear_logs(app: "VdtraceGuiApp") -> None:
    app.command_lines.clear()
    app.loader.clear_logs()
    app.view.set_log_text("")
    app.logs_revision = app.loader.logs_revision
    app.vars.log_status_var.set("日志已清空。")
