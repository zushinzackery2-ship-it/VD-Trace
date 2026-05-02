from __future__ import annotations

import argparse
import sys
import tkinter as tk
from pathlib import Path

from vdtrace_gui.app import VdtraceGuiApp
from vdtrace_gui.cli_bridge import build_trace_config_from_vars
from vdtrace_gui.models import LoaderSessionSnapshot, default_agent_path, default_ctl_path, repo_root_from_file
from vdtrace_gui import session_filter
from vdtrace_gui.session_filter import filter_loader_sessions
from vdtrace_gui.trace_profile import default_trace_profile


def _widget_state(widget) -> str:
    return str(widget.cget("state"))


def _run_gui_self_test(app: VdtraceGuiApp) -> None:
    view = app.view
    vars = app.vars

    if (
        view.thread_entry is None
        or view.block_main_thread_check is None
        or view.root_stop_check is None
        or view.depth_filter_controller is None
        or view.memory_controller is None
        or view.memory_text is None
        or view.outside_call_depth_spinbox is None
        or view.anonymous_exec_call_depth_spinbox is None
    ):
        raise RuntimeError("GUI self-test failed: trace controls were not constructed.")

    def apply_controls() -> None:
        view._apply_trigger_controls()
        view._apply_thread_controls()
        app.root.update_idletasks()

    vars.trigger_enabled_var.set(True)
    vars.thread_capture_var.set(True)
    vars.block_main_thread_var.set(False)
    vars.root_stop_var.set(False)
    apply_controls()
    if _widget_state(view.thread_entry) != "disabled":
        raise RuntimeError("GUI self-test failed: auto-thread + trigger should disable thread entry.")
    if _widget_state(view.block_main_thread_check) != "normal":
        raise RuntimeError("GUI self-test failed: auto-thread + trigger should enable block-main-thread.")
    if _widget_state(view.root_stop_check) != "normal":
        raise RuntimeError("GUI self-test failed: auto-thread + trigger should keep root-stop editable.")

    vars.block_main_thread_var.set(True)
    vars.root_stop_var.set(False)
    apply_controls()
    if _widget_state(view.block_main_thread_check) != "normal":
        raise RuntimeError("GUI self-test failed: auto-thread + trigger should keep block-main-thread editable.")
    if _widget_state(view.root_stop_check) != "normal":
        raise RuntimeError("GUI self-test failed: block-main-thread mode should keep root-stop editable.")

    vars.trigger_enabled_var.set(True)
    vars.thread_capture_var.set(False)
    vars.block_main_thread_var.set(True)
    vars.root_stop_var.set(False)
    apply_controls()
    if _widget_state(view.thread_entry) != "normal":
        raise RuntimeError("GUI self-test failed: manual-thread + trigger should enable thread entry.")
    if _widget_state(view.block_main_thread_check) != "disabled":
        raise RuntimeError("GUI self-test failed: manual-thread + trigger should disable block-main-thread.")
    if _widget_state(view.root_stop_check) != "normal":
        raise RuntimeError("GUI self-test failed: manual-thread + trigger should keep root-stop editable.")

    vars.trigger_enabled_var.set(False)
    vars.thread_capture_var.set(True)
    apply_controls()
    if _widget_state(view.thread_entry) != "disabled":
        raise RuntimeError("GUI self-test failed: no-trigger auto mode should disable thread entry.")
    if _widget_state(view.block_main_thread_check) != "disabled":
        raise RuntimeError("GUI self-test failed: no-trigger auto mode should disable block-main-thread.")

    vars.trigger_enabled_var.set(False)
    vars.thread_capture_var.set(False)
    apply_controls()
    if _widget_state(view.thread_entry) != "normal":
        raise RuntimeError("GUI self-test failed: no-trigger manual mode should enable thread entry.")
    if _widget_state(view.block_main_thread_check) != "disabled":
        raise RuntimeError("GUI self-test failed: no-trigger manual mode should keep block-main-thread disabled.")

    app._set_trace_running(True)
    app._set_trace_writing(True)
    if _widget_state(view.stop_button) != "disabled":
        raise RuntimeError("GUI self-test failed: writing state should disable stop button.")
    app._set_trace_writing(False)
    if _widget_state(view.stop_button) != "normal":
        raise RuntimeError("GUI self-test failed: non-writing running state should enable stop button.")
    app._set_trace_running(False)

    vars.outside_call_depth_enabled_var.set(True)
    view.depth_filter_controller.apply_widget_states()
    if _widget_state(view.outside_call_depth_spinbox) != "normal":
        raise RuntimeError("GUI self-test failed: enabled outside-depth override should unlock its spinbox.")

    vars.anonymous_exec_call_depth_enabled_var.set(True)
    view.depth_filter_controller.apply_widget_states()
    if _widget_state(view.anonymous_exec_call_depth_spinbox) != "normal":
        raise RuntimeError("GUI self-test failed: enabled anonymous-depth override should unlock its spinbox.")
    vars.backend_var.set("TF")
    built_config = build_trace_config_from_vars(vars)
    if built_config.backend != "TF" or built_config.control_flow_only:
        raise RuntimeError("GUI self-test failed: backend selection should build TF trace config.")
    vars.backend_var.set("DR")

    default_profile = default_trace_profile(vars.agent_var.get())
    if default_profile.backend.strip().upper() != "DR":
        raise RuntimeError("GUI self-test failed: backend template should default to DR.")
    if not default_profile.idle_escape_enabled:
        raise RuntimeError("GUI self-test failed: idle-escape template should default to enabled.")
    if default_profile.idle_escape_threshold.strip() != "32":
        raise RuntimeError("GUI self-test failed: idle-escape template threshold should default to 32.")

    current_idle_escape = vars.idle_escape_threshold_var.get().strip()
    if not vars.idle_escape_enabled_var.get():
        raise RuntimeError("GUI self-test failed: loaded idle-escape setting should stay enabled.")
    if not current_idle_escape.isdigit() or int(current_idle_escape) < 0:
        raise RuntimeError("GUI self-test failed: loaded idle-escape threshold should stay a non-negative integer.")

    filtered_sessions = filter_loader_sessions(session_filter.SELF_TEST_SESSIONS)
    if [session.pid for session in filtered_sessions] != session_filter.SELF_TEST_EXPECTED_PIDS:
        raise RuntimeError("GUI self-test failed: loader session filtering should keep all connected sessions.")

    app.selected_session = LoaderSessionSnapshot(session_id=1, pid=4242, process_path="dummy.exe")
    app.session_lookup = {"dummy": LoaderSessionSnapshot(session_id=1, pid=4242, process_path="dummy.exe")}
    vars.session_var.set("dummy")
    vars.output_var.set(r".\traces\VDTrace-4242-20260410-000000.log")
    app._apply_selected_session()
    if vars.output_var.get().strip() != r".\traces\VDTrace-4242-20260410-000000.log":
        raise RuntimeError("GUI self-test failed: same-session refresh should not clear auto output path.")

    preview_dir = app.repo_root / "obj" / "gui_self_test"
    preview_dir.mkdir(parents=True, exist_ok=True)
    preview_path = preview_dir / "preview.log"
    try:
        preview_lines = [f"line-{index:04d}" for index in range(5000)]
        preview_path.write_text("\n".join(preview_lines) + "\n", encoding="utf-8")
        vars.output_var.set(str(preview_path))
        app._refresh_trace_preview()
        preview_text = view.trace_text.get("1.0", "end-1c")
        rendered_lines = preview_text.splitlines()
        if len(rendered_lines) != 4096:
            raise RuntimeError("GUI self-test failed: trace preview should keep exactly the last 4096 lines.")
        if rendered_lines[0] != preview_lines[-4096] or rendered_lines[-1] != preview_lines[-1]:
            raise RuntimeError("GUI self-test failed: trace preview should show the latest 4096 lines only.")
    finally:
        if preview_path.exists():
            preview_path.unlink()


def main() -> int:
    parser = argparse.ArgumentParser(description="VD-Trace Python 中文控制端")
    parser.add_argument("--self-test", action="store_true", help="启动窗口后短暂延时再自动关闭。")
    args = parser.parse_args()

    repo_root = repo_root_from_file(__file__)
    root = tk.Tk()
    app = VdtraceGuiApp(
        root=root,
        repo_root=repo_root,
        ctl_path=default_ctl_path(repo_root),
        agent_path=default_agent_path(repo_root),
    )
    if args.self_test:
        _run_gui_self_test(app)
        root.after(250, root.destroy)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
