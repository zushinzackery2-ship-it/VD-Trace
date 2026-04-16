from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .view import VdtraceMainView


def build_session_page(view: "VdtraceMainView", parent: ttk.Frame) -> None:
    layout = ttk.Frame(parent)
    layout.pack(fill=tk.BOTH, expand=True)
    layout.columnconfigure(0, weight=1)
    layout.rowconfigure(1, weight=1)

    picker = ttk.LabelFrame(layout, text="目标进程", style="Card.TLabelframe", padding=14)
    picker.grid(row=0, column=0, sticky=tk.EW)
    ttk.Label(picker, text="目标列表", style="Body.TLabel").grid(row=0, column=0, sticky=tk.W)
    view.session_combo = ttk.Combobox(picker, textvariable=view.vars.session_var, state="readonly", width=58)
    view.session_combo.grid(row=0, column=1, sticky=tk.EW, padx=(8, 8))
    view.refresh_button = ttk.Button(picker, text="刷新", command=view.callbacks.refresh_sessions)
    view.refresh_button.grid(row=0, column=2)
    ttk.Label(picker, text="Agent DLL", style="Body.TLabel").grid(row=1, column=0, sticky=tk.W, pady=(12, 0))
    ttk.Entry(picker, textvariable=view.vars.agent_var).grid(row=1, column=1, sticky=tk.EW, padx=(8, 8), pady=(12, 0))
    view.load_button = ttk.Button(picker, text="加载 Agent", style="Accent.TButton", command=view.callbacks.load_agent)
    view.load_button.grid(row=1, column=2, pady=(12, 0))
    ttk.Label(picker, text="转储模块", style="Body.TLabel").grid(row=2, column=0, sticky=tk.W, pady=(12, 0))
    view.dump_module_combo = ttk.Combobox(picker, textvariable=view.vars.dump_module_var, width=58, state="disabled")
    view.dump_module_combo.grid(row=2, column=1, sticky=tk.EW, padx=(8, 8), pady=(12, 0))
    view.dump_button = ttk.Button(picker, text="Dump+Fix", command=view.callbacks.dump_module)
    view.dump_button.grid(row=2, column=2, pady=(12, 0))
    picker.columnconfigure(1, weight=1)
    view.session_combo.bind("<<ComboboxSelected>>", lambda _event: view.callbacks.apply_selected_session())

    details = ttk.LabelFrame(layout, text="目标详情", style="Card.TLabelframe", padding=14)
    details.grid(row=1, column=0, sticky=tk.NSEW, pady=(12, 0))
    details.columnconfigure(1, weight=1)
    ttk.Label(details, text="PID", style="Body.TLabel").grid(row=0, column=0, sticky=tk.NW)
    ttk.Label(details, textvariable=view.vars.pid_var, style="Value.TLabel").grid(row=0, column=1, sticky=tk.W, padx=(10, 0))
    ttk.Label(details, text="进程路径", style="Body.TLabel").grid(row=1, column=0, sticky=tk.NW, pady=(12, 0))
    view._wrap_label(details, textvariable=view.vars.session_path_var, style="Body.TLabel").grid(row=1, column=1, sticky=tk.EW, padx=(10, 0), pady=(12, 0))
    ttk.Label(details, text="连接方式", style="Body.TLabel").grid(row=2, column=0, sticky=tk.NW, pady=(12, 0))
    view._wrap_label(details, textvariable=view.vars.session_status_var, style="Muted.TLabel").grid(row=2, column=1, sticky=tk.EW, padx=(10, 0), pady=(12, 0))
    ttk.Label(details, text="转储说明", style="Body.TLabel").grid(row=3, column=0, sticky=tk.NW, pady=(12, 0))
    view._wrap_label(details, textvariable=view.vars.dump_status_var, style="Muted.TLabel").grid(row=3, column=1, sticky=tk.EW, padx=(10, 0), pady=(12, 0))
    tip = (
        "先在这里选目标进程。\n"
        "Agent 装载和 Dump+Fix 都放在本页。\n"
        "Dump+Fix 默认输出到 .\\dump：原始 *_dump_raw，修正版 *_dump_fix。"
    )
    view._wrap_label(details, text=tip, style="Muted.TLabel").grid(row=4, column=0, columnspan=2, sticky=tk.EW, pady=(16, 0))


def build_trace_page(view: "VdtraceMainView", parent: ttk.Frame) -> None:
    layout = ttk.Frame(parent)
    layout.pack(fill=tk.BOTH, expand=True)
    layout.columnconfigure(0, weight=5)
    layout.columnconfigure(1, weight=5)

    target = ttk.LabelFrame(layout, text="目标与输出", style="Card.TLabelframe", padding=14)
    target.grid(row=0, column=0, sticky=tk.NSEW, padx=(0, 6))
    view.thread_capture_check = ttk.Checkbutton(
        target,
        text="自动线程捕获",
        variable=view.vars.thread_capture_var,
        command=view._apply_thread_controls,
    )
    view.thread_capture_check.grid(row=0, column=0, sticky=tk.W, pady=(0, 10))
    view.thread_entry = ttk.Entry(target, textvariable=view.vars.thread_var)
    view.thread_entry.grid(row=0, column=1, sticky=tk.EW, padx=(8, 0), pady=(0, 10))
    view.module_scope_check = ttk.Checkbutton(target, text="指定模块记录", variable=view.vars.module_scope_var, command=view._apply_module_scope_controls)
    view.module_scope_check.grid(row=1, column=0, sticky=tk.W, pady=(0, 10))
    view.module_entry = ttk.Entry(target, textvariable=view.vars.modules_var)
    view.module_entry.grid(row=1, column=1, sticky=tk.EW, padx=(8, 0), pady=(0, 10))
    view.trigger_check = ttk.Checkbutton(
        target,
        text="定点触发",
        variable=view.vars.trigger_enabled_var,
        command=view._apply_trigger_controls,
    )
    view.trigger_check.grid(row=2, column=0, sticky=tk.W, pady=(0, 10))
    view.trigger_entry = ttk.Entry(target, textvariable=view.vars.trigger_var)
    view.trigger_entry.grid(row=2, column=1, sticky=tk.EW, padx=(8, 0), pady=(0, 10))
    ttk.Label(target, text="输出", style="Body.TLabel").grid(row=3, column=0, sticky=tk.W, pady=(0, 10))
    view.output_entry = ttk.Entry(target, textvariable=view.vars.output_var, state="readonly")
    view.output_entry.grid(row=3, column=1, sticky=tk.EW, padx=(8, 0), pady=(0, 10))
    target.columnconfigure(1, weight=1)
    view._apply_thread_controls()
    view._apply_trigger_controls()
    view._apply_module_scope_controls()

    policy = ttk.LabelFrame(layout, text="策略", style="Card.TLabelframe", padding=14)
    policy.grid(row=0, column=1, sticky=tk.NSEW, padx=(6, 0))
    ttk.Label(policy, text="事件上限", style="Body.TLabel").grid(row=0, column=0, sticky=tk.W)
    ttk.Entry(policy, textvariable=view.vars.max_events_var, width=12).grid(row=0, column=1, sticky=tk.W, padx=(8, 0))
    ttk.Label(policy, text="后端", style="Body.TLabel").grid(row=0, column=2, sticky=tk.W, padx=(16, 0))
    view.backend_combo = ttk.Combobox(policy, textvariable=view.vars.backend_var, state="readonly", values=("DR", "TF", "PT"), width=10)
    view.backend_combo.grid(row=0, column=3, sticky=tk.W, padx=(8, 0))
    view.backend_combo.bind("<<ComboboxSelected>>", lambda _event: view._apply_button_states())
    ttk.Checkbutton(policy, text="记录重复命中", variable=view.vars.repeat_var).grid(row=1, column=0, columnspan=2, sticky=tk.W, pady=(12, 0))
    view.block_main_thread_check = ttk.Checkbutton(policy, text="屏蔽主线程", variable=view.vars.block_main_thread_var, command=view._apply_thread_controls)
    view.block_main_thread_check.grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=(8, 0))
    view.root_stop_check = ttk.Checkbutton(policy, text="单次调用即停", variable=view.vars.root_stop_var)
    view.root_stop_check.grid(row=2, column=2, columnspan=2, sticky=tk.W, pady=(8, 0))
    view.handoff_check = ttk.Checkbutton(policy, text="异步线程追踪", variable=view.vars.handoff_var)
    view.handoff_check.grid(row=3, column=0, columnspan=2, sticky=tk.W, pady=(8, 0))
    view.sample_check = ttk.Checkbutton(policy, text="增强采样（跨模块）", variable=view.vars.sample_var)
    view.sample_check.grid(row=3, column=2, columnspan=2, sticky=tk.W, pady=(8, 0))
    policy.columnconfigure(1, weight=1)
    policy.columnconfigure(3, weight=1)
def build_preview_page(view: "VdtraceMainView", parent: ttk.Frame) -> None:
    frame = ttk.LabelFrame(parent, text="追踪", style="Card.TLabelframe", padding=8)
    toolbar = ttk.Frame(frame)
    toolbar.pack(fill=tk.X, pady=(0, 6))

    status_box = ttk.Frame(toolbar)
    status_box.pack(side=tk.LEFT, fill=tk.X, expand=True)
    view._wrap_label(status_box, textvariable=view.vars.trace_status_var, style="Muted.TLabel").pack(anchor=tk.W, fill=tk.X)
    view._wrap_label(status_box, textvariable=view.vars.preview_status_var, style="Muted.TLabel").pack(anchor=tk.W, fill=tk.X, pady=(4, 0))

    button_box = ttk.Frame(toolbar)
    button_box.pack(side=tk.RIGHT)
    ttk.Button(button_box, text="到内存", command=view.callbacks.memory_from_trace).pack(side=tk.RIGHT)
    ttk.Button(button_box, text="复制", command=view.callbacks.copy_trace).pack(side=tk.RIGHT)
    view.stop_button = ttk.Button(button_box, text="停止", command=view.callbacks.stop_trace)
    view.stop_button.pack(side=tk.RIGHT, padx=(0, 8))
    view.start_button = ttk.Button(button_box, text="一键开始", style="Accent.TButton", command=view.callbacks.start_trace)
    view.start_button.pack(side=tk.RIGHT, padx=(0, 8))

    view.trace_text = create_log_view(frame)
    frame.pack(fill=tk.BOTH, expand=True)


def build_log_page(view: "VdtraceMainView", parent: ttk.Frame) -> None:
    frame, view.log_text = build_output_panel(
        view,
        parent,
        "控制日志",
        view.vars.log_status_var,
        "清空",
        view.callbacks.clear_log,
    )
    frame.pack(fill=tk.BOTH, expand=True)


def build_output_panel(
    view: "VdtraceMainView",
    parent: ttk.Frame,
    title: str,
    status_var: tk.StringVar,
    action_text: str,
    action_command,
) -> tuple[ttk.LabelFrame, tk.Text]:
    frame = ttk.LabelFrame(parent, text=title, style="Card.TLabelframe", padding=8)
    toolbar = ttk.Frame(frame)
    toolbar.pack(fill=tk.X, pady=(0, 6))
    view._wrap_label(toolbar, textvariable=status_var, style="Muted.TLabel").pack(side=tk.LEFT, fill=tk.X, expand=True)
    if title == "控制日志":
        ttk.Button(toolbar, text="到内存", command=view.callbacks.memory_from_log).pack(side=tk.RIGHT)
    ttk.Button(toolbar, text=action_text, command=action_command).pack(side=tk.RIGHT)
    return frame, create_log_view(frame)


def create_log_view(parent: ttk.LabelFrame) -> tk.Text:
    container = ttk.Frame(parent)
    container.pack(fill=tk.BOTH, expand=True)
    text = tk.Text(container, font=("Consolas", 10), bg="#fcf9f4", fg="#2f2d2a", relief=tk.FLAT, wrap="none")
    y_scroll = ttk.Scrollbar(container, orient=tk.VERTICAL, command=text.yview)
    x_scroll = ttk.Scrollbar(container, orient=tk.HORIZONTAL, command=text.xview)
    text.configure(yscrollcommand=y_scroll.set, xscrollcommand=x_scroll.set)
    text.grid(row=0, column=0, sticky=tk.NSEW)
    y_scroll.grid(row=0, column=1, sticky=tk.NS)
    x_scroll.grid(row=1, column=0, sticky=tk.EW)
    container.columnconfigure(0, weight=1)
    container.rowconfigure(0, weight=1)
    return text
