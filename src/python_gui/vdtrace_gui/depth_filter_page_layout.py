from __future__ import annotations

import tkinter as tk
from tkinter import ttk

from .depth_filters import execution_mode_options


def build_depth_filter_page(controller, parent: ttk.Frame) -> None:
    layout = ttk.Frame(parent)
    layout.pack(fill=tk.BOTH, expand=True)
    layout.columnconfigure(0, weight=4)
    layout.columnconfigure(1, weight=6)
    layout.rowconfigure(1, weight=1)

    _build_summary_section(controller, layout)
    _build_global_rules_section(controller, layout)
    _build_module_rules_section(controller, layout)

    controller._reload_from_vars()
    controller.apply_widget_states()


def _build_summary_section(controller, layout: ttk.Frame) -> None:
    summary = ttk.LabelFrame(layout, text="规则总览", style="Card.TLabelframe", padding=12)
    summary.grid(row=0, column=0, columnspan=2, sticky=tk.EW)
    controller.view._wrap_label(
        summary,
        textvariable=controller.view.vars.depth_filter_summary_var,
        style="Value.TLabel",
    ).pack(anchor=tk.W, fill=tk.X)


def _build_global_rules_section(controller, layout: ttk.Frame) -> None:
    global_rules = ttk.LabelFrame(layout, text="区域规则", style="Card.TLabelframe", padding=12)
    global_rules.grid(row=1, column=0, sticky=tk.NSEW, padx=(0, 6))
    global_rules.columnconfigure(1, weight=1)

    ttk.Label(global_rules, text="范围", style="Body.TLabel").grid(row=0, column=0, sticky=tk.W)
    ttk.Label(global_rules, text="层级", style="Body.TLabel").grid(row=0, column=1, sticky=tk.W, padx=(8, 0))
    ttk.Label(global_rules, text="模式", style="Body.TLabel").grid(row=0, column=2, sticky=tk.W, padx=(8, 0))

    ttk.Label(global_rules, text="默认", style="Body.TLabel").grid(row=1, column=0, sticky=tk.W, pady=(8, 0))
    ttk.Spinbox(
        global_rules,
        from_=0,
        to=64,
        increment=1,
        textvariable=controller.view.vars.call_depth_var,
        width=10,
    ).grid(row=1, column=1, sticky=tk.W, padx=(8, 0), pady=(8, 0))
    ttk.Label(global_rules, text="EDGE", style="Body.TLabel").grid(row=1, column=2, sticky=tk.W, padx=(8, 0), pady=(8, 0))

    controller.outside_check = ttk.Checkbutton(
        global_rules,
        text="模块外",
        variable=controller.view.vars.outside_call_depth_enabled_var,
        command=controller.apply_widget_states,
    )
    controller.outside_check.grid(row=2, column=0, sticky=tk.W, pady=(12, 0))
    controller.view.outside_call_depth_spinbox = ttk.Spinbox(
        global_rules,
        from_=0,
        to=64,
        increment=1,
        textvariable=controller.view.vars.outside_call_depth_var,
        width=10,
    )
    controller.view.outside_call_depth_spinbox.grid(row=2, column=1, sticky=tk.W, padx=(8, 0), pady=(12, 0))
    controller.outside_mode_combo = ttk.Combobox(
        global_rules,
        textvariable=controller.view.vars.outside_execution_mode_var,
        values=execution_mode_options(),
        state="readonly",
        width=10,
    )
    controller.outside_mode_combo.grid(row=2, column=2, sticky=tk.W, padx=(8, 0), pady=(12, 0))

    controller.anonymous_check = ttk.Checkbutton(
        global_rules,
        text="匿名页",
        variable=controller.view.vars.anonymous_exec_call_depth_enabled_var,
        command=controller.apply_widget_states,
    )
    controller.anonymous_check.grid(row=3, column=0, sticky=tk.W, pady=(10, 0))
    controller.view.anonymous_exec_call_depth_spinbox = ttk.Spinbox(
        global_rules,
        from_=0,
        to=64,
        increment=1,
        textvariable=controller.view.vars.anonymous_exec_call_depth_var,
        width=10,
    )
    controller.view.anonymous_exec_call_depth_spinbox.grid(row=3, column=1, sticky=tk.W, padx=(8, 0), pady=(10, 0))
    controller.anonymous_mode_combo = ttk.Combobox(
        global_rules,
        textvariable=controller.view.vars.anonymous_exec_execution_mode_var,
        values=execution_mode_options(),
        state="readonly",
        width=10,
    )
    controller.anonymous_mode_combo.grid(row=3, column=2, sticky=tk.W, padx=(8, 0), pady=(10, 0))

    controller.view._wrap_label(global_rules, text="0=ALL，1=SINGLE，2=向下一层。", style="Muted.TLabel").grid(
        row=4,
        column=0,
        columnspan=3,
        sticky=tk.W,
        pady=(12, 0),
    )
    controller.idle_escape_check = ttk.Checkbutton(
        global_rules,
        text="空转跳出",
        variable=controller.view.vars.idle_escape_enabled_var,
        command=controller.apply_widget_states,
    )
    controller.idle_escape_check.grid(row=5, column=0, sticky=tk.W, pady=(12, 0))
    controller.idle_escape_spinbox = ttk.Spinbox(
        global_rules,
        from_=0,
        to=65535,
        increment=1,
        textvariable=controller.view.vars.idle_escape_threshold_var,
        width=10,
    )
    controller.idle_escape_spinbox.grid(row=5, column=1, sticky=tk.W, padx=(8, 0), pady=(12, 0))
    controller.view._wrap_label(
        global_rules,
        text="默认 32；只在首次命中模式下用于热空转跳出。",
        style="Muted.TLabel",
    ).grid(row=6, column=0, columnspan=3, sticky=tk.W, pady=(8, 0))


def _build_module_rules_section(controller, layout: ttk.Frame) -> None:
    modules = ttk.LabelFrame(layout, text="模块规则", style="Card.TLabelframe", padding=12)
    modules.grid(row=1, column=1, sticky=tk.NSEW, padx=(6, 0))
    modules.columnconfigure(0, weight=1)
    modules.rowconfigure(1, weight=1)

    editor = ttk.Frame(modules)
    editor.grid(row=0, column=0, sticky=tk.EW)
    editor.columnconfigure(1, weight=1)
    ttk.Label(editor, text="模块名", style="Body.TLabel").grid(row=0, column=0, sticky=tk.W)
    controller.module_entry = ttk.Entry(editor, textvariable=controller.module_name_var)
    controller.module_entry.grid(row=0, column=1, sticky=tk.EW, padx=(8, 12))
    ttk.Label(editor, text="层级", style="Body.TLabel").grid(row=0, column=2, sticky=tk.W)
    controller.depth_spinbox = ttk.Spinbox(
        editor,
        from_=0,
        to=64,
        increment=1,
        textvariable=controller.module_depth_var,
        width=10,
    )
    controller.depth_spinbox.grid(row=0, column=3, sticky=tk.W, padx=(8, 12))
    ttk.Label(editor, text="模式", style="Body.TLabel").grid(row=0, column=4, sticky=tk.W)
    controller.mode_combo = ttk.Combobox(
        editor,
        textvariable=controller.module_mode_var,
        values=execution_mode_options(),
        state="readonly",
        width=10,
    )
    controller.mode_combo.grid(row=0, column=5, sticky=tk.W, padx=(8, 12))
    controller.add_button = ttk.Button(editor, text="添加/更新", command=controller._add_or_update_rule)
    controller.add_button.grid(row=0, column=6, sticky=tk.W)

    tree_container = ttk.Frame(modules)
    tree_container.grid(row=1, column=0, sticky=tk.NSEW, pady=(12, 0))
    tree_container.columnconfigure(0, weight=1)
    tree_container.rowconfigure(0, weight=1)
    controller.tree = ttk.Treeview(tree_container, columns=("module", "depth", "mode"), show="headings", height=10)
    controller.tree.heading("module", text="模块")
    controller.tree.heading("depth", text="层级")
    controller.tree.heading("mode", text="模式")
    controller.tree.column("module", anchor=tk.W, stretch=True, width=280)
    controller.tree.column("depth", anchor=tk.W, stretch=False, width=120)
    controller.tree.column("mode", anchor=tk.W, stretch=False, width=90)
    y_scroll = ttk.Scrollbar(tree_container, orient=tk.VERTICAL, command=controller.tree.yview)
    controller.tree.configure(yscrollcommand=y_scroll.set)
    controller.tree.grid(row=0, column=0, sticky=tk.NSEW)
    y_scroll.grid(row=0, column=1, sticky=tk.NS)
    controller.tree.bind("<<TreeviewSelect>>", controller._on_tree_select)

    actions = ttk.Frame(modules)
    actions.grid(row=2, column=0, sticky=tk.EW, pady=(12, 0))
    controller.remove_button = ttk.Button(actions, text="移除选中", command=controller._remove_selected_rule)
    controller.remove_button.pack(side=tk.LEFT)
    controller.clear_button = ttk.Button(actions, text="清空规则", command=controller._clear_rules)
    controller.clear_button.pack(side=tk.LEFT, padx=(8, 0))
