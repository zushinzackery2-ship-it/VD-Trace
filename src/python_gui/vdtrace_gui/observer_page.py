from __future__ import annotations

import tkinter as tk
from tkinter import messagebox, ttk
from typing import TYPE_CHECKING

from .observer_rules import (
    ObserverRule,
    build_observer_summary,
    normalize_observer_exit,
    normalize_observer_mode,
    normalize_observer_steps,
    observer_exit_options,
    observer_mode_options,
    observer_rule_columns,
    parse_observer_rules,
    serialize_observer_rules,
)

if TYPE_CHECKING:
    from .view import VdtraceMainView


class ObserverController:
    def __init__(self, view: "VdtraceMainView") -> None:
        self.view = view
        self.mode_var = tk.StringVar(master=view.root, value="capture")
        self.hit_var = tk.StringVar(master=view.root)
        self.content_var = tk.StringVar(master=view.root)
        self.steps_var = tk.StringVar(master=view.root, value="256")
        self.exit_var = tk.StringVar(master=view.root, value="return-or-leave")
        self.summary_var = tk.StringVar(master=view.root, value="观测器未启用。")
        self.tree: ttk.Treeview | None = None
        self.mode_combo: ttk.Combobox | None = None
        self.hit_entry: ttk.Entry | None = None
        self.content_entry: ttk.Entry | None = None
        self.steps_combo: ttk.Combobox | None = None
        self.exit_combo: ttk.Combobox | None = None
        self.content_label: ttk.Label | None = None
        self.add_button: ttk.Button | None = None
        self.remove_button: ttk.Button | None = None
        self.clear_button: ttk.Button | None = None

        self.mode_var.trace_add("write", lambda *_args: self._sync_editor_mode())
        self.view.vars.probe_var.trace_add("write", lambda *_args: self._reload_from_vars())
        self.view.vars.probe_enabled_var.trace_add("write", lambda *_args: self.apply_widget_states())

    def build(self, parent: ttk.Frame) -> None:
        layout = ttk.Frame(parent)
        layout.pack(fill=tk.BOTH, expand=True)
        layout.columnconfigure(0, weight=1)
        layout.rowconfigure(1, weight=1)

        summary = ttk.LabelFrame(layout, text="规则", style="Card.TLabelframe", padding=12)
        summary.grid(row=0, column=0, sticky=tk.EW)
        summary.columnconfigure(1, weight=1)
        view = self.view
        view.probe_check = ttk.Checkbutton(
            summary,
            text="启用观测器",
            variable=view.vars.probe_enabled_var,
            command=view._apply_probe_controls,
        )
        view.probe_check.grid(row=0, column=0, sticky=tk.W)
        view._wrap_label(summary, textvariable=self.summary_var, style="Value.TLabel").grid(row=0, column=1, sticky=tk.W, padx=(12, 0))

        editor = ttk.Frame(summary)
        editor.grid(row=1, column=0, columnspan=2, sticky=tk.EW, pady=(10, 0))
        editor.columnconfigure(3, weight=2)
        editor.columnconfigure(5, weight=3)
        ttk.Label(editor, text="模式", style="Body.TLabel").grid(row=0, column=0, sticky=tk.W)
        self.mode_combo = ttk.Combobox(editor, textvariable=self.mode_var, state="readonly", values=observer_mode_options(), width=10)
        self.mode_combo.grid(row=0, column=1, sticky=tk.W, padx=(8, 12))
        ttk.Label(editor, text="命中点", style="Body.TLabel").grid(row=0, column=2, sticky=tk.W)
        self.hit_entry = ttk.Entry(editor, textvariable=self.hit_var)
        self.hit_entry.grid(row=0, column=3, sticky=tk.EW, padx=(8, 12))
        self.content_label = ttk.Label(editor, text="capture/watch", style="Body.TLabel")
        self.content_label.grid(row=0, column=4, sticky=tk.W)
        self.content_entry = ttk.Entry(editor, textvariable=self.content_var)
        self.content_entry.grid(row=0, column=5, sticky=tk.EW, padx=(8, 12))
        ttk.Label(editor, text="步数", style="Body.TLabel").grid(row=0, column=6, sticky=tk.W)
        self.steps_combo = ttk.Combobox(editor, textvariable=self.steps_var, values=("64", "128", "256", "512", "1024"), width=8)
        self.steps_combo.grid(row=0, column=7, sticky=tk.W, padx=(8, 12))
        ttk.Label(editor, text="退出", style="Body.TLabel").grid(row=0, column=8, sticky=tk.W)
        self.exit_combo = ttk.Combobox(editor, textvariable=self.exit_var, state="readonly", values=observer_exit_options(), width=16)
        self.exit_combo.grid(row=0, column=9, sticky=tk.W, padx=(8, 12))

        actions = ttk.Frame(editor)
        actions.grid(row=0, column=10, sticky=tk.E)
        self.add_button = ttk.Button(actions, text="添加/更新", command=self._add_or_update_rule)
        self.add_button.pack(side=tk.LEFT)
        self.remove_button = ttk.Button(actions, text="移除选中", command=self._remove_selected_rule)
        self.remove_button.pack(side=tk.LEFT, padx=(8, 0))
        self.clear_button = ttk.Button(actions, text="清空规则", command=self._clear_rules)
        self.clear_button.pack(side=tk.LEFT, padx=(8, 0))

        tree_frame = ttk.LabelFrame(layout, text="规则列表", style="Card.TLabelframe", padding=14)
        tree_frame.grid(row=1, column=0, sticky=tk.NSEW, pady=(12, 0))
        tree_frame.columnconfigure(0, weight=1)
        tree_frame.rowconfigure(0, weight=1)
        self.tree = ttk.Treeview(
            tree_frame,
            columns=("mode", "hit", "content", "steps", "exit"),
            show="headings",
            height=11,
        )
        self.tree.heading("mode", text="模式")
        self.tree.heading("hit", text="命中点")
        self.tree.heading("content", text="内容")
        self.tree.heading("steps", text="步数")
        self.tree.heading("exit", text="退出")
        self.tree.column("mode", anchor=tk.W, stretch=False, width=90)
        self.tree.column("hit", anchor=tk.W, stretch=True, width=220)
        self.tree.column("content", anchor=tk.W, stretch=True, width=320)
        self.tree.column("steps", anchor=tk.W, stretch=False, width=90)
        self.tree.column("exit", anchor=tk.W, stretch=False, width=130)
        scroll = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.grid(row=0, column=0, sticky=tk.NSEW)
        scroll.grid(row=0, column=1, sticky=tk.NS)
        self.tree.bind("<<TreeviewSelect>>", self._on_tree_select)

        self._reload_from_vars()
        self.apply_widget_states()

    def apply_widget_states(self) -> None:
        enabled = self.view.vars.probe_enabled_var.get() and not self.view.busy
        if self.view.probe_check is not None:
            self.view.probe_check.configure(state=tk.NORMAL if not self.view.busy else tk.DISABLED)
        for widget in (
            self.mode_combo,
            self.hit_entry,
            self.content_entry,
            self.steps_combo,
            self.exit_combo,
            self.add_button,
            self.remove_button,
            self.clear_button,
        ):
            if widget is not None:
                widget.configure(state="normal" if enabled else "disabled")
        if self.mode_combo is not None and enabled:
            self.mode_combo.configure(state="readonly")
        if self.exit_combo is not None and enabled and normalize_observer_mode(self.mode_var.get()) != "capture":
            self.exit_combo.configure(state="readonly")
        self._sync_editor_mode()
        self.summary_var.set(build_observer_summary(self.view.vars.probe_enabled_var.get(), self.view.vars.probe_var.get()))

    def _reload_from_vars(self) -> None:
        if self.tree is None:
            self.summary_var.set(build_observer_summary(self.view.vars.probe_enabled_var.get(), self.view.vars.probe_var.get()))
            return
        try:
            rules = parse_observer_rules(self.view.vars.probe_var.get())
        except ValueError:
            rules = []
        current = self.tree.selection()
        current_key = self.tree.item(current[0], "values")[:2] if current else ()
        for item in self.tree.get_children():
            self.tree.delete(item)
        replacement = ""
        for rule in rules:
            values = observer_rule_columns(rule)
            item_id = self.tree.insert("", tk.END, values=values)
            if current_key and tuple(values[:2]) == tuple(current_key):
                replacement = item_id
        if replacement:
            self.tree.selection_set(replacement)
        self.summary_var.set(build_observer_summary(self.view.vars.probe_enabled_var.get(), self.view.vars.probe_var.get()))

    def _current_rules(self) -> list[ObserverRule]:
        return parse_observer_rules(self.view.vars.probe_var.get())

    def _set_rules(self, rules: list[ObserverRule]) -> None:
        self.view.vars.probe_var.set(serialize_observer_rules(rules))

    def _sync_editor_mode(self) -> None:
        mode = normalize_observer_mode(self.mode_var.get())
        enabled = self.view.vars.probe_enabled_var.get() and not self.view.busy
        if self.content_label is not None:
            self.content_label.configure(text="capture" if mode == "capture" else "watch" if mode == "write" else "内容")
        if self.content_entry is not None:
            self.content_entry.configure(state="normal" if enabled and mode != "step" else "disabled")
        if self.steps_combo is not None:
            self.steps_combo.configure(state="normal" if enabled and mode != "capture" else "disabled")
        if self.exit_combo is not None:
            state = "readonly" if enabled and mode != "capture" else "disabled"
            self.exit_combo.configure(state=state)
        if mode == "capture":
            self.steps_var.set("")
            self.exit_var.set("")
        elif mode == "step":
            if not self.steps_var.get().strip():
                self.steps_var.set("256")
            if not self.exit_var.get().strip():
                self.exit_var.set("return-or-leave")
        else:
            if not self.steps_var.get().strip():
                self.steps_var.set("256")
            if not self.exit_var.get().strip():
                self.exit_var.set("return")

    def _on_tree_select(self, _event) -> None:
        if self.tree is None:
            return
        selected = self.tree.selection()
        if not selected:
            return
        values = self.tree.item(selected[0], "values")
        if not values:
            return
        self.mode_var.set(str(values[0]))
        self.hit_var.set(str(values[1]))
        self.content_var.set("" if str(values[2]) == "-" else str(values[2]))
        self.steps_var.set("" if str(values[3]) == "-" else str(values[3]))
        self.exit_var.set("" if str(values[4]) == "-" else str(values[4]))
        self._sync_editor_mode()

    def _add_or_update_rule(self) -> None:
        try:
            rule = self._build_editor_rule()
            rules = self._current_rules()
        except ValueError as exc:
            messagebox.showerror("VD-Trace", str(exc))
            return

        updated = False
        for index, current in enumerate(rules):
            if current.mode == rule.mode and current.hit.lower() == rule.hit.lower():
                rules[index] = rule
                updated = True
                break
        if not updated:
            rules.append(rule)
        self._set_rules(rules)
        self._reset_editor()

    def _remove_selected_rule(self) -> None:
        if self.tree is None:
            return
        selected = self.tree.selection()
        if not selected:
            return
        values = self.tree.item(selected[0], "values")
        if not values:
            return
        selected_mode = str(values[0]).lower()
        selected_hit = str(values[1]).lower()
        rules = [
            rule
            for rule in self._current_rules()
            if not (rule.mode == selected_mode and rule.hit.lower() == selected_hit)
        ]
        self._set_rules(rules)

    def _clear_rules(self) -> None:
        self._set_rules([])
        self._reset_editor()

    def _reset_editor(self) -> None:
        self.mode_var.set("capture")
        self.hit_var.set("")
        self.content_var.set("")
        self.steps_var.set("256")
        self.exit_var.set("return-or-leave")
        self._sync_editor_mode()

    def _build_editor_rule(self) -> ObserverRule:
        mode = normalize_observer_mode(self.mode_var.get())
        hit = self.hit_var.get().strip()
        if not hit:
            raise ValueError("命中点不能为空。")
        if mode == "capture":
            content = self.content_var.get().strip()
            if not content:
                raise ValueError("capture 内容不能为空。")
            return ObserverRule(mode=mode, hit=hit, content=content)
        if mode == "step":
            return ObserverRule(
                mode=mode,
                hit=hit,
                steps=normalize_observer_steps(mode, self.steps_var.get()),
                exit_kind=normalize_observer_exit(mode, self.exit_var.get()),
            )
        content = self.content_var.get().strip()
        if not content:
            raise ValueError("write watch 不能为空。")
        return ObserverRule(
            mode=mode,
            hit=hit,
            content=content,
            steps=normalize_observer_steps(mode, self.steps_var.get()),
            exit_kind=normalize_observer_exit(mode, self.exit_var.get()),
        )
