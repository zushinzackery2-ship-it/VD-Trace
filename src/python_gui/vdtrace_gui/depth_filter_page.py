from __future__ import annotations

import tkinter as tk
from tkinter import messagebox, ttk
from typing import TYPE_CHECKING

from .depth_filters import (
    ModuleDepthRule,
    build_depth_filter_summary,
    format_ui_depth_label,
    normalize_execution_mode,
    normalize_ui_depth_value,
    parse_module_depth_rules,
    serialize_module_depth_rules,
)
from .depth_filter_page_layout import build_depth_filter_page

if TYPE_CHECKING:
    from .view import VdtraceMainView


class DepthFilterController:
    def __init__(self, view: "VdtraceMainView") -> None:
        self.view = view
        self.module_name_var = tk.StringVar(master=view.root)
        self.module_depth_var = tk.StringVar(master=view.root, value=view.vars.call_depth_var.get() or "3")
        self.module_mode_var = tk.StringVar(master=view.root, value="EDGE")
        self.tree: ttk.Treeview | None = None
        self.module_entry: ttk.Entry | None = None
        self.depth_spinbox: ttk.Spinbox | None = None
        self.mode_combo: ttk.Combobox | None = None
        self.outside_check: ttk.Checkbutton | None = None
        self.outside_mode_combo: ttk.Combobox | None = None
        self.anonymous_check: ttk.Checkbutton | None = None
        self.anonymous_mode_combo: ttk.Combobox | None = None
        self.idle_escape_check: ttk.Checkbutton | None = None
        self.idle_escape_spinbox: ttk.Spinbox | None = None
        self.add_button: ttk.Button | None = None
        self.remove_button: ttk.Button | None = None
        self.clear_button: ttk.Button | None = None

        self.view.vars.call_depth_var.trace_add("write", lambda *_args: self.refresh_summary())
        self.view.vars.outside_call_depth_enabled_var.trace_add("write", lambda *_args: self.apply_widget_states())
        self.view.vars.outside_call_depth_var.trace_add("write", lambda *_args: self.refresh_summary())
        self.view.vars.outside_execution_mode_var.trace_add("write", lambda *_args: self.refresh_summary())
        self.view.vars.anonymous_exec_call_depth_enabled_var.trace_add("write", lambda *_args: self.apply_widget_states())
        self.view.vars.anonymous_exec_call_depth_var.trace_add("write", lambda *_args: self.refresh_summary())
        self.view.vars.anonymous_exec_execution_mode_var.trace_add("write", lambda *_args: self.refresh_summary())
        self.view.vars.idle_escape_enabled_var.trace_add("write", lambda *_args: self.apply_widget_states())
        self.view.vars.idle_escape_threshold_var.trace_add("write", lambda *_args: self.refresh_summary())
        self.view.vars.module_call_depths_var.trace_add("write", lambda *_args: self._reload_from_vars())

    def build(self, parent: ttk.Frame) -> None:
        build_depth_filter_page(self, parent)

    def apply_widget_states(self) -> None:
        busy = self.view.busy
        if self.outside_check is not None:
            self.outside_check.configure(state="normal" if not busy else "disabled")
        if self.anonymous_check is not None:
            self.anonymous_check.configure(state="normal" if not busy else "disabled")
        if self.view.outside_call_depth_spinbox is not None:
            state = "normal" if self.view.vars.outside_call_depth_enabled_var.get() and not busy else "disabled"
            self.view.outside_call_depth_spinbox.configure(state=state)
        if self.outside_mode_combo is not None:
            state = "readonly" if self.view.vars.outside_call_depth_enabled_var.get() and not busy else "disabled"
            self.outside_mode_combo.configure(state=state)
        if self.view.anonymous_exec_call_depth_spinbox is not None:
            state = "normal" if self.view.vars.anonymous_exec_call_depth_enabled_var.get() and not busy else "disabled"
            self.view.anonymous_exec_call_depth_spinbox.configure(state=state)
        if self.anonymous_mode_combo is not None:
            state = "readonly" if self.view.vars.anonymous_exec_call_depth_enabled_var.get() and not busy else "disabled"
            self.anonymous_mode_combo.configure(state=state)
        if self.idle_escape_check is not None:
            self.idle_escape_check.configure(state="normal" if not busy else "disabled")
        if self.idle_escape_spinbox is not None:
            state = "normal" if self.view.vars.idle_escape_enabled_var.get() and not busy else "disabled"
            self.idle_escape_spinbox.configure(state=state)
        if self.module_entry is not None:
            self.module_entry.configure(state="normal" if not busy else "disabled")
        if self.depth_spinbox is not None:
            self.depth_spinbox.configure(state="normal" if not busy else "disabled")
        if self.mode_combo is not None:
            self.mode_combo.configure(state="readonly" if not busy else "disabled")
        if self.add_button is not None:
            self.add_button.configure(state="normal" if not busy else "disabled")
        if self.remove_button is not None:
            self.remove_button.configure(state="normal" if not busy else "disabled")
        if self.clear_button is not None:
            self.clear_button.configure(state="normal" if not busy else "disabled")
        self.refresh_summary()

    def refresh_summary(self) -> None:
        try:
            self.view.vars.depth_filter_summary_var.set(
                build_depth_filter_summary(
                    self.view.vars.call_depth_var.get(),
                    self.view.vars.outside_call_depth_enabled_var.get(),
                    self.view.vars.outside_call_depth_var.get(),
                    self.view.vars.outside_execution_mode_var.get(),
                    self.view.vars.anonymous_exec_call_depth_enabled_var.get(),
                    self.view.vars.anonymous_exec_call_depth_var.get(),
                    self.view.vars.anonymous_exec_execution_mode_var.get(),
                    self.view.vars.module_call_depths_var.get(),
                    self.view.vars.idle_escape_enabled_var.get(),
                    self.view.vars.idle_escape_threshold_var.get(),
                )
            )
        except ValueError as exc:
            self.view.vars.depth_filter_summary_var.set(f"过滤器规则有误：{exc}")

    def _reload_from_vars(self) -> None:
        if self.tree is None:
            self.refresh_summary()
            return
        try:
            rules = parse_module_depth_rules(self.view.vars.module_call_depths_var.get())
        except ValueError:
            rules = []
        self._populate_tree(rules)
        self.refresh_summary()

    def _populate_tree(self, rules: list[ModuleDepthRule]) -> None:
        if self.tree is None:
            return
        current = self.tree.selection()
        current_module = self.tree.item(current[0], "values")[0] if current else ""
        for item in self.tree.get_children():
            self.tree.delete(item)
        replacement_id = ""
        for rule in rules:
            item_id = self.tree.insert(
                "",
                tk.END,
                values=(rule.module_name, format_ui_depth_label(rule.ui_depth), normalize_execution_mode(rule.execution_mode)),
            )
            if current_module and current_module.lower() == rule.module_name.lower():
                replacement_id = item_id
        if replacement_id:
            self.tree.selection_set(replacement_id)

    def _current_rules(self) -> list[ModuleDepthRule]:
        return parse_module_depth_rules(self.view.vars.module_call_depths_var.get())

    def _set_rules(self, rules: list[ModuleDepthRule]) -> None:
        self.view.vars.module_call_depths_var.set(serialize_module_depth_rules(rules))

    def _on_tree_select(self, _event) -> None:
        if self.tree is None:
            return
        selected = self.tree.selection()
        if not selected:
            return
        module_name = str(self.tree.item(selected[0], "values")[0])
        for rule in self._current_rules():
            if rule.module_name.lower() == module_name.lower():
                self.module_name_var.set(rule.module_name)
                self.module_depth_var.set(rule.ui_depth)
                self.module_mode_var.set(normalize_execution_mode(rule.execution_mode))
                return

    def _add_or_update_rule(self) -> None:
        module_name = self.module_name_var.get().strip()
        if not module_name:
            messagebox.showerror("VD-Trace", "模块名不能为空。")
            return

        try:
            depth_value = normalize_ui_depth_value(self.module_depth_var.get())
            mode_value = normalize_execution_mode(self.module_mode_var.get())
            rules = self._current_rules()
        except ValueError as exc:
            messagebox.showerror("VD-Trace", str(exc))
            return

        updated = False
        for rule in rules:
            if rule.module_name.lower() == module_name.lower():
                rule.module_name = module_name
                rule.ui_depth = depth_value
                rule.execution_mode = mode_value
                updated = True
                break
        if not updated:
            rules.append(ModuleDepthRule(module_name=module_name, ui_depth=depth_value, execution_mode=mode_value))
        self._set_rules(rules)
        self.module_name_var.set("")
        self.module_depth_var.set(self.view.vars.call_depth_var.get() or "3")
        self.module_mode_var.set("EDGE")

    def _remove_selected_rule(self) -> None:
        if self.tree is None:
            return
        selected = self.tree.selection()
        if not selected:
            return
        selected_module = str(self.tree.item(selected[0], "values")[0]).lower()
        rules = [rule for rule in self._current_rules() if rule.module_name.lower() != selected_module]
        self._set_rules(rules)

    def _clear_rules(self) -> None:
        self._set_rules([])
