from __future__ import annotations

from pathlib import Path

from .models import is_auto_output_path
from .trace_profile import TraceProfile
from .trace_settings import load_trace_profile, save_trace_profile, settings_path
from .ui_state import UiVariables


def load_settings(repo_root: Path, variables: UiVariables) -> None:
    profile = load_trace_profile(repo_root, variables.agent_var.get())
    variables.agent_var.set(profile.agent_path)
    variables.thread_var.set(profile.thread_id)
    variables.modules_var.set(profile.modules)
    variables.output_var.set("" if is_auto_output_path(profile.output_path, 0) else profile.output_path)
    variables.max_events_var.set(profile.max_events)
    variables.backend_var.set(profile.backend)
    variables.call_depth_var.set(profile.call_depth)
    variables.outside_call_depth_enabled_var.set(profile.outside_call_depth_enabled)
    variables.outside_call_depth_var.set(profile.outside_call_depth)
    variables.outside_execution_mode_var.set(profile.outside_execution_mode)
    variables.anonymous_exec_call_depth_enabled_var.set(profile.anonymous_exec_call_depth_enabled)
    variables.anonymous_exec_call_depth_var.set(profile.anonymous_exec_call_depth)
    variables.anonymous_exec_execution_mode_var.set(profile.anonymous_exec_execution_mode)
    variables.module_call_depths_var.set(profile.module_call_depths)
    variables.trigger_var.set(profile.trigger_point)
    variables.probe_var.set(profile.probe_spec)
    variables.probe_enabled_var.set(profile.probe_enabled)
    variables.trigger_enabled_var.set(profile.trigger_enabled)
    variables.thread_capture_var.set(profile.thread_capture)
    variables.block_main_thread_var.set(profile.block_main_thread)
    variables.module_scope_var.set(not profile.trace_outside_modules)
    variables.all_events_var.set(profile.all_events)
    variables.repeat_var.set(profile.repeat_hits)
    variables.idle_escape_enabled_var.set(profile.idle_escape_enabled)
    variables.idle_escape_threshold_var.set(profile.idle_escape_threshold)
    variables.sample_var.set(profile.enhanced_sampling)
    variables.root_stop_var.set(profile.root_stop_on_return)
    variables.handoff_var.set(profile.async_thread_handoff)


def save_settings(repo_root: Path, variables: UiVariables) -> None:
    profile = TraceProfile(
        agent_path=variables.agent_var.get().strip(),
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
    save_trace_profile(repo_root, profile)


__all__ = [
    "load_settings",
    "save_settings",
    "settings_path",
]
