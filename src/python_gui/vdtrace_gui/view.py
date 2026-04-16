from __future__ import annotations

import re
import tkinter as tk
from tkinter import ttk

from .depth_filter_page import DepthFilterController
from .memory_page import MemoryPageController
from .observer_page import ObserverController
from .ui_state import UiCallbacks, UiVariables
from .view_pages import build_log_page, build_preview_page, build_session_page, build_trace_page


class VdtraceMainView:
    def __init__(self, root: tk.Tk, variables: UiVariables, callbacks: UiCallbacks) -> None:
        self.root = root
        self.vars = variables
        self.callbacks = callbacks
        self.wrap_labels: list[ttk.Label] = []
        self.trace_text: tk.Text | None = None
        self.log_text: tk.Text | None = None
        self.memory_text: tk.Text | None = None
        self.session_combo: ttk.Combobox | None = None
        self.notebook: ttk.Notebook | None = None
        self.page_ids: dict[str, ttk.Frame] = {}
        self.refresh_button: ttk.Button | None = None
        self.load_button: ttk.Button | None = None
        self.dump_button: ttk.Button | None = None
        self.start_button: ttk.Button | None = None
        self.stop_button: ttk.Button | None = None
        self.trigger_entry: ttk.Entry | None = None
        self.trigger_check: ttk.Checkbutton | None = None
        self.output_entry: ttk.Entry | None = None
        self.backend_combo: ttk.Combobox | None = None
        self.probe_check: ttk.Checkbutton | None = None
        self.module_entry: ttk.Entry | None = None
        self.thread_entry: ttk.Entry | None = None
        self.thread_capture_check: ttk.Checkbutton | None = None
        self.block_main_thread_check: ttk.Checkbutton | None = None
        self.root_stop_check: ttk.Checkbutton | None = None
        self.sample_check: ttk.Checkbutton | None = None
        self.handoff_check: ttk.Checkbutton | None = None
        self.outside_call_depth_spinbox: ttk.Spinbox | None = None
        self.anonymous_exec_call_depth_spinbox: ttk.Spinbox | None = None
        self.dump_module_combo: ttk.Combobox | None = None
        self.depth_filter_controller: DepthFilterController | None = None
        self.observer_controller: ObserverController | None = None
        self.memory_controller: MemoryPageController | None = None
        self.busy = False
        self.trace_running = False
        self.trace_writing = False
        self.dump_ready = False
        self._log_snapshot = ""
        self._trace_snapshot = ""

        self._configure_style()
        self._build_ui()
        self.update_wraplengths()
        self._apply_button_states()

    def _configure_style(self) -> None:
        self.root.title("VD-Trace 控制台")
        self._fit_window_to_screen()
        self.root.minsize(720, 520)
        self.root.configure(bg="#f6f1ea")
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure(".", background="#f6f1ea", foreground="#2f2d2a", fieldbackground="#fcf9f4")
        style.configure("Header.TFrame", background="#fffaf2")
        style.configure("Card.TLabelframe", background="#fffaf2")
        style.configure("Card.TLabelframe.Label", background="#fffaf2", foreground="#2f2d2a", font=("Microsoft YaHei UI", 11, "bold"))
        style.configure("Accent.TButton", background="#ba5c32", foreground="#ffffff")
        style.configure("Body.TLabel", background="#fffaf2", foreground="#2f2d2a")
        style.configure("Muted.TLabel", background="#fffaf2", foreground="#72685d")
        style.configure("Value.TLabel", background="#fffaf2", foreground="#2f2d2a", font=("Microsoft YaHei UI", 11, "bold"))

    def _fit_window_to_screen(self) -> None:
        screen_width = max(1, self.root.winfo_screenwidth())
        screen_height = max(1, self.root.winfo_screenheight())
        width = min(960, max(780, int(screen_width * 0.72)))
        height = min(660, max(560, int(screen_height * 0.72)))
        x = max(0, (screen_width - width) // 2)
        y = max(0, (screen_height - height) // 2)
        self.root.geometry(f"{width}x{height}+{x}+{y}")

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=12)
        outer.pack(fill=tk.BOTH, expand=True)

        header = ttk.Frame(outer, style="Header.TFrame", padding=(16, 12))
        header.pack(fill=tk.X)
        ttk.Label(header, text="VD-Trace 控制台", font=("Microsoft YaHei UI", 18, "bold"), background="#fffaf2").pack(anchor=tk.W)
        self._wrap_label(header, text="会话、策略、过滤器、观测器、追踪、日志分开显示。", style="Muted.TLabel").pack(anchor=tk.W, fill=tk.X, pady=(4, 0))

        target_strip = ttk.Frame(outer, style="Header.TFrame", padding=(12, 8))
        target_strip.pack(fill=tk.X, pady=(10, 0))
        ttk.Label(target_strip, text="当前目标", style="Body.TLabel").pack(side=tk.LEFT)
        self._wrap_label(target_strip, textvariable=self.vars.active_target_var, style="Value.TLabel").pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(10, 0))

        self.notebook = ttk.Notebook(outer)
        self.notebook.pack(fill=tk.BOTH, expand=True, pady=(12, 0))
        self._add_page("session", "会话", self._build_session_page)
        self._add_page("trace", "策略", self._build_trace_page)
        self._add_page("depth", "过滤器", self._build_depth_page)
        self._add_page("fine", "观测器", self._build_fine_page)
        self._add_page("memory", "内存", self._build_memory_page)
        self._add_page("preview", "追踪", self._build_preview_page)
        self._add_page("log", "日志", self._build_log_page)

    def _add_page(self, key: str, title: str, builder: callable) -> None:
        if self.notebook is None:
            return
        page = ttk.Frame(self.notebook, padding=12)
        self.page_ids[key] = page
        self.notebook.add(page, text=title)
        builder(page)

    def _build_session_page(self, parent: ttk.Frame) -> None:
        build_session_page(self, parent)

    def _build_trace_page(self, parent: ttk.Frame) -> None:
        build_trace_page(self, parent)

    def _build_depth_page(self, parent: ttk.Frame) -> None:
        self.depth_filter_controller = DepthFilterController(self)
        self.depth_filter_controller.build(parent)

    def _build_fine_page(self, parent: ttk.Frame) -> None:
        self.observer_controller = ObserverController(self)
        self.observer_controller.build(parent)

    def _build_memory_page(self, parent: ttk.Frame) -> None:
        self.memory_controller = MemoryPageController(self)
        self.memory_controller.build(parent)

    def _apply_thread_controls(self) -> None:
        if self.thread_entry is None:
            return

        capture_mode_active = self.vars.trigger_enabled_var.get() and self.vars.thread_capture_var.get()
        allow_manual_thread = not self.vars.thread_capture_var.get() and not self.busy
        state = "normal" if allow_manual_thread else "disabled"
        self.thread_entry.configure(state=state)
        if self.block_main_thread_check is not None:
            self.block_main_thread_check.configure(state="normal" if capture_mode_active and not self.busy else "disabled")
        if self.root_stop_check is not None:
            self.root_stop_check.configure(state="normal" if not self.busy else "disabled")

    def _apply_probe_controls(self) -> None:
        if self.observer_controller is not None:
            self.observer_controller.apply_widget_states()
        elif self.probe_check is not None:
            self.probe_check.configure(state=tk.NORMAL if not self.busy else tk.DISABLED)

    def _apply_module_scope_controls(self) -> None:
        if self.module_entry is not None:
            self.module_entry.configure(state="normal" if self.vars.module_scope_var.get() and not self.busy else "disabled")

    def _build_preview_page(self, parent: ttk.Frame) -> None:
        build_preview_page(self, parent)

    def _build_log_page(self, parent: ttk.Frame) -> None:
        build_log_page(self, parent)

    def _wrap_label(self, parent: tk.Misc, text: str = "", textvariable: tk.StringVar | None = None, style: str = "Body.TLabel") -> ttk.Label:
        label = ttk.Label(parent, text=text, textvariable=textvariable, style=style, justify=tk.LEFT)
        self.wrap_labels.append(label)
        return label

    def update_wraplengths(self) -> None:
        for label in self.wrap_labels:
            parent_width = max(label.master.winfo_width(), label.winfo_width())
            label.configure(wraplength=max(220, parent_width - 24))

    def set_log_text(self, text: str) -> None:
        self._replace_text_widget(self.log_text, text, "_log_snapshot")

    def set_trace_text(self, text: str) -> None:
        self._replace_text_widget(self.trace_text, text, "_trace_snapshot")

    def set_memory_text(self, text: str) -> None:
        self._replace_text_widget(self.memory_text, text, None)

    def set_dump_modules(self, modules: list[str]) -> None:
        values: list[str] = []
        for module in modules:
            text = module.strip()
            if text and text not in values:
                values.append(text)
        if self.dump_module_combo is None:
            return
        self.dump_module_combo["values"] = values
        self.dump_module_combo.configure(state="readonly" if values else "disabled")
        self.dump_ready = bool(values)
        self._apply_button_states()

    def copy_trace_text(self) -> int:
        return self._copy_text_widget(self.trace_text)

    def set_actions_enabled(self, enabled: bool) -> None:
        self.busy = not enabled
        self.root.configure(cursor="" if enabled else "watch")
        self._apply_button_states()

    def set_trace_running(self, running: bool) -> None:
        self.trace_running = running
        self._apply_button_states()

    def set_trace_writing(self, writing: bool) -> None:
        self.trace_writing = writing
        self._apply_button_states()

    def select_page(self, key: str) -> None:
        if self.notebook is None:
            return
        page = self.page_ids.get(key)
        if page is not None:
            self.notebook.select(page)

    def extract_trace_address(self) -> str:
        return self._extract_address_from_widget(self.trace_text)

    def extract_log_address(self) -> str:
        return self._extract_address_from_widget(self.log_text)

    def _apply_button_states(self) -> None:
        idle = not self.busy
        if self.refresh_button is not None:
            self.refresh_button.configure(state=tk.NORMAL if idle else tk.DISABLED)
        if self.load_button is not None:
            self.load_button.configure(state=tk.NORMAL if idle and not self.trace_running else tk.DISABLED)
        if self.dump_button is not None:
            self.dump_button.configure(state=tk.NORMAL if idle and self.dump_ready else tk.DISABLED)
        if self.start_button is not None:
            self.start_button.configure(state=tk.NORMAL if idle and not self.trace_running else tk.DISABLED)
        if self.stop_button is not None:
            self.stop_button.configure(state=tk.NORMAL if idle and self.trace_running and not self.trace_writing else tk.DISABLED)
        if self.thread_capture_check is not None:
            self.thread_capture_check.configure(state=tk.NORMAL if idle else tk.DISABLED)
        self._apply_backend_controls()
        self._apply_thread_controls()
        self._apply_probe_controls()
        self._apply_module_scope_controls()
        self._apply_trigger_controls()
        if self.depth_filter_controller is not None:
            self.depth_filter_controller.apply_widget_states()
        if self.memory_controller is not None:
            self.memory_controller.apply_widget_states()

    def _apply_trigger_controls(self) -> None:
        if self.trigger_entry is not None:
            trigger_state = tk.NORMAL if self.vars.trigger_enabled_var.get() and not self.busy else tk.DISABLED
            self.trigger_entry.configure(state=trigger_state)
        if self.trigger_check is not None:
            self.trigger_check.configure(state=tk.NORMAL if not self.busy else tk.DISABLED)
        self._apply_thread_controls()

    def _apply_backend_controls(self) -> None:
        if self.backend_combo is not None:
            self.backend_combo.configure(state="readonly" if not self.busy else "disabled")

        if self.sample_check is not None:
            self.sample_check.configure(state=tk.NORMAL if not self.busy else tk.DISABLED)
        if self.handoff_check is not None:
            self.handoff_check.configure(state=tk.NORMAL if not self.busy else tk.DISABLED)

    @staticmethod
    def _is_scrolled_to_bottom(widget: tk.Text) -> bool:
        return widget.yview()[1] >= 0.999

    def _replace_text_widget(self, widget: tk.Text | None, text: str, cache_name: str | None) -> None:
        if widget is None:
            return
        if cache_name is not None and getattr(self, cache_name) == text:
            return
        at_bottom = self._is_scrolled_to_bottom(widget)
        xview = widget.xview()
        yview = widget.yview()
        widget.delete("1.0", tk.END)
        if text:
            widget.insert(tk.END, text)
        if cache_name is not None:
            setattr(self, cache_name, text)
        if at_bottom:
            widget.see(tk.END)
        else:
            widget.xview_moveto(xview[0])
            widget.yview_moveto(yview[0])

    def _copy_text_widget(self, widget: tk.Text | None) -> int:
        if widget is None:
            return 0
        try:
            text = widget.get(tk.SEL_FIRST, tk.SEL_LAST)
        except tk.TclError:
            text = widget.get("1.0", "end-1c")
        if not text:
            return 0
        self.root.clipboard_clear()
        self.root.clipboard_append(text)
        self.root.update_idletasks()
        return len(text)

    def _extract_address_from_widget(self, widget: tk.Text | None) -> str:
        if widget is None:
            return ""

        candidate_text = ""
        try:
            candidate_text = widget.get(tk.SEL_FIRST, tk.SEL_LAST).strip()
        except tk.TclError:
            index = widget.index(tk.INSERT)
            line_text = widget.get(f"{index} linestart", f"{index} lineend")
            line_column = int(float(index.split(".")[1]))
            for pattern in (
                re.compile(r"[\w.\-]+(?:!|\+)0x[0-9a-fA-F]+"),
                re.compile(r"0x[0-9a-fA-F]+"),
            ):
                for match in pattern.finditer(line_text):
                    if match.start() <= line_column <= match.end():
                        return match.group(0)
            candidate_text = line_text

        for pattern in (
            re.compile(r"[\w.\-]+(?:!|\+)0x[0-9a-fA-F]+"),
            re.compile(r"0x[0-9a-fA-F]+"),
        ):
            match = pattern.search(candidate_text)
            if match is not None:
                return match.group(0)
        return ""
