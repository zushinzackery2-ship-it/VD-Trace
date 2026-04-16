from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import TYPE_CHECKING

from .memory_codec import memory_write_mode_options
from .view_pages import create_log_view

if TYPE_CHECKING:
    from .view import VdtraceMainView


class MemoryPageController:
    def __init__(self, view: "VdtraceMainView") -> None:
        self.view = view
        self.address_entry: ttk.Entry | None = None
        self.size_entry: ttk.Entry | None = None
        self.read_button: ttk.Button | None = None
        self.mode_combo: ttk.Combobox | None = None
        self.write_entry: ttk.Entry | None = None
        self.write_button: ttk.Button | None = None

    def build(self, parent: ttk.Frame) -> None:
        layout = ttk.Frame(parent)
        layout.pack(fill=tk.BOTH, expand=True)
        layout.columnconfigure(0, weight=1)
        layout.rowconfigure(1, weight=1)

        controls = ttk.LabelFrame(layout, text="内存", style="Card.TLabelframe", padding=12)
        controls.grid(row=0, column=0, sticky=tk.EW)
        controls.columnconfigure(1, weight=1)
        controls.columnconfigure(5, weight=1)

        ttk.Label(controls, text="地址", style="Body.TLabel").grid(row=0, column=0, sticky=tk.W)
        self.address_entry = ttk.Entry(controls, textvariable=self.view.vars.memory_address_var)
        self.address_entry.grid(row=0, column=1, sticky=tk.EW, padx=(8, 12))
        ttk.Label(controls, text="长度", style="Body.TLabel").grid(row=0, column=2, sticky=tk.W)
        self.size_entry = ttk.Entry(controls, textvariable=self.view.vars.memory_size_var, width=10)
        self.size_entry.grid(row=0, column=3, sticky=tk.W, padx=(8, 12))
        self.read_button = ttk.Button(controls, text="读取", command=self.view.callbacks.read_memory)
        self.read_button.grid(row=0, column=4, sticky=tk.W)

        ttk.Label(controls, text="写入模式", style="Body.TLabel").grid(row=1, column=0, sticky=tk.W, pady=(12, 0))
        self.mode_combo = ttk.Combobox(
            controls,
            textvariable=self.view.vars.memory_write_mode_var,
            values=memory_write_mode_options(),
            state="readonly",
            width=10,
        )
        self.mode_combo.grid(row=1, column=1, sticky=tk.W, padx=(8, 12), pady=(12, 0))
        ttk.Label(controls, text="写入值", style="Body.TLabel").grid(row=1, column=2, sticky=tk.W, pady=(12, 0))
        self.write_entry = ttk.Entry(controls, textvariable=self.view.vars.memory_write_value_var)
        self.write_entry.grid(row=1, column=3, columnspan=2, sticky=tk.EW, padx=(8, 12), pady=(12, 0))
        self.write_button = ttk.Button(controls, text="写入", command=self.view.callbacks.write_memory)
        self.write_button.grid(row=1, column=5, sticky=tk.W, pady=(12, 0))

        self.view._wrap_label(
            controls,
            text="地址支持 0xADDR / module+0xRVA / module!0xRVA。写入模式支持 HEX / TEXT / UTF16 / U32 / U64。",
            style="Muted.TLabel",
        ).grid(row=2, column=0, columnspan=6, sticky=tk.EW, pady=(12, 0))

        result = ttk.LabelFrame(layout, text="结果", style="Card.TLabelframe", padding=8)
        result.grid(row=1, column=0, sticky=tk.NSEW, pady=(12, 0))
        toolbar = ttk.Frame(result)
        toolbar.pack(fill=tk.X, pady=(0, 6))
        self.view._wrap_label(toolbar, textvariable=self.view.vars.memory_status_var, style="Muted.TLabel").pack(
            side=tk.LEFT,
            fill=tk.X,
            expand=True,
        )
        ttk.Button(toolbar, text="带入追踪选中", command=self.view.callbacks.memory_from_trace).pack(side=tk.RIGHT)
        ttk.Button(toolbar, text="带入日志选中", command=self.view.callbacks.memory_from_log).pack(side=tk.RIGHT, padx=(0, 8))
        self.view.memory_text = create_log_view(result)

        self.apply_widget_states()

    def apply_widget_states(self) -> None:
        state = "normal" if not self.view.busy else "disabled"
        readonly_state = "readonly" if not self.view.busy else "disabled"
        if self.address_entry is not None:
            self.address_entry.configure(state=state)
        if self.size_entry is not None:
            self.size_entry.configure(state=state)
        if self.read_button is not None:
            self.read_button.configure(state=state)
        if self.mode_combo is not None:
            self.mode_combo.configure(state=readonly_state)
        if self.write_entry is not None:
            self.write_entry.configure(state=state)
        if self.write_button is not None:
            self.write_button.configure(state=state)
