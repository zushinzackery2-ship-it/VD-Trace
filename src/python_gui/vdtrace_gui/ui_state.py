from __future__ import annotations

import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


@dataclass(slots=True)
class UiVariables:
    active_target_var: tk.StringVar
    pid_var: tk.StringVar
    session_path_var: tk.StringVar
    session_status_var: tk.StringVar
    agent_var: tk.StringVar
    dump_module_var: tk.StringVar
    thread_var: tk.StringVar
    thread_capture_var: tk.BooleanVar
    module_scope_var: tk.BooleanVar
    modules_var: tk.StringVar
    output_var: tk.StringVar
    max_events_var: tk.StringVar
    backend_var: tk.StringVar
    call_depth_var: tk.StringVar
    outside_call_depth_enabled_var: tk.BooleanVar
    outside_call_depth_var: tk.StringVar
    outside_execution_mode_var: tk.StringVar
    anonymous_exec_call_depth_enabled_var: tk.BooleanVar
    anonymous_exec_call_depth_var: tk.StringVar
    anonymous_exec_execution_mode_var: tk.StringVar
    module_call_depths_var: tk.StringVar
    depth_filter_summary_var: tk.StringVar
    trigger_var: tk.StringVar
    probe_var: tk.StringVar
    probe_enabled_var: tk.BooleanVar
    trigger_enabled_var: tk.BooleanVar
    block_main_thread_var: tk.BooleanVar
    all_events_var: tk.BooleanVar
    repeat_var: tk.BooleanVar
    idle_escape_enabled_var: tk.BooleanVar
    idle_escape_threshold_var: tk.StringVar
    sample_var: tk.BooleanVar
    root_stop_var: tk.BooleanVar
    handoff_var: tk.BooleanVar
    session_var: tk.StringVar
    dump_status_var: tk.StringVar
    trace_status_var: tk.StringVar
    preview_status_var: tk.StringVar
    log_status_var: tk.StringVar
    memory_address_var: tk.StringVar
    memory_size_var: tk.StringVar
    memory_status_var: tk.StringVar
    memory_write_mode_var: tk.StringVar
    memory_write_value_var: tk.StringVar


def create_ui_variables(root: tk.Misc, agent_path: Path) -> UiVariables:
    return UiVariables(
        active_target_var=tk.StringVar(master=root, value="当前目标：还没有选中目标进程。"),
        pid_var=tk.StringVar(master=root),
        session_path_var=tk.StringVar(master=root, value="等待目标进程..."),
        session_status_var=tk.StringVar(master=root, value="等待 Loader 管道连接目标进程。"),
        agent_var=tk.StringVar(master=root, value=str(agent_path)),
        dump_module_var=tk.StringVar(master=root),
        thread_var=tk.StringVar(master=root, value="0"),
        thread_capture_var=tk.BooleanVar(master=root, value=False),
        module_scope_var=tk.BooleanVar(master=root, value=True),
        modules_var=tk.StringVar(master=root, value="UnityPlayer.dll"),
        output_var=tk.StringVar(master=root, value=""),
        max_events_var=tk.StringVar(master=root, value="0"),
        backend_var=tk.StringVar(master=root, value="DR"),
        call_depth_var=tk.StringVar(master=root, value="3"),
        outside_call_depth_enabled_var=tk.BooleanVar(master=root, value=False),
        outside_call_depth_var=tk.StringVar(master=root, value="3"),
        outside_execution_mode_var=tk.StringVar(master=root, value="EDGE"),
        anonymous_exec_call_depth_enabled_var=tk.BooleanVar(master=root, value=False),
        anonymous_exec_call_depth_var=tk.StringVar(master=root, value="3"),
        anonymous_exec_execution_mode_var=tk.StringVar(master=root, value="EDGE"),
        module_call_depths_var=tk.StringVar(master=root, value=""),
        depth_filter_summary_var=tk.StringVar(master=root, value="默认=向下2层"),
        trigger_var=tk.StringVar(master=root),
        probe_var=tk.StringVar(master=root),
        probe_enabled_var=tk.BooleanVar(master=root, value=False),
        trigger_enabled_var=tk.BooleanVar(master=root, value=True),
        block_main_thread_var=tk.BooleanVar(master=root, value=False),
        all_events_var=tk.BooleanVar(master=root, value=False),
        repeat_var=tk.BooleanVar(master=root, value=False),
        idle_escape_enabled_var=tk.BooleanVar(master=root, value=True),
        idle_escape_threshold_var=tk.StringVar(master=root, value="32"),
        sample_var=tk.BooleanVar(master=root, value=False),
        root_stop_var=tk.BooleanVar(master=root, value=False),
        handoff_var=tk.BooleanVar(master=root, value=True),
        session_var=tk.StringVar(master=root),
        dump_status_var=tk.StringVar(master=root, value="等待真实模块列表；Dump+Fix 默认输出到 .\\dump。"),
        trace_status_var=tk.StringVar(master=root, value="等待追踪启动。"),
        preview_status_var=tk.StringVar(master=root, value="等待追踪输出文件生成。"),
        log_status_var=tk.StringVar(master=root, value="这里会显示 Loader 连接日志和控制命令结果。"),
        memory_address_var=tk.StringVar(master=root),
        memory_size_var=tk.StringVar(master=root, value="64"),
        memory_status_var=tk.StringVar(master=root, value="等待读取内存。"),
        memory_write_mode_var=tk.StringVar(master=root, value="HEX"),
        memory_write_value_var=tk.StringVar(master=root),
    )


@dataclass(slots=True)
class UiCallbacks:
    refresh_sessions: Callable[[], None]
    apply_selected_session: Callable[[], None]
    load_agent: Callable[[], None]
    dump_module: Callable[[], None]
    start_trace: Callable[[], None]
    stop_trace: Callable[[], None]
    copy_trace: Callable[[], None]
    clear_log: Callable[[], None]
    read_memory: Callable[[], None]
    write_memory: Callable[[], None]
    memory_from_trace: Callable[[], None]
    memory_from_log: Callable[[], None]
