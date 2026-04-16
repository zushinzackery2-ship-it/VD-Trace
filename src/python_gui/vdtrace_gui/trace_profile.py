from __future__ import annotations

from dataclasses import dataclass, replace

from .depth_filters import build_depth_filter_spec, ui_depth_to_runtime_text
from .models import TraceConfig


@dataclass(slots=True)
class TraceProfile:
    agent_path: str
    thread_id: str = "0"
    thread_capture: bool = True
    modules: str = "UnityPlayer.dll"
    output_path: str = ""
    max_events: str = "0"
    backend: str = "DR"
    call_depth: str = "3"
    outside_call_depth_enabled: bool = False
    outside_call_depth: str = "3"
    outside_execution_mode: str = "EDGE"
    anonymous_exec_call_depth_enabled: bool = False
    anonymous_exec_call_depth: str = "3"
    anonymous_exec_execution_mode: str = "EDGE"
    module_call_depths: str = ""
    trigger_point: str = "GameAssembly.dll+0x3498AE0"
    probe_spec: str = ""
    probe_enabled: bool = False
    trigger_enabled: bool = True
    block_main_thread: bool = False
    trace_outside_modules: bool = False
    all_events: bool = False
    repeat_hits: bool = False
    idle_escape_enabled: bool = True
    idle_escape_threshold: str = "32"
    enhanced_sampling: bool = False
    root_stop_on_return: bool = True
    async_thread_handoff: bool = True


def default_trace_profile(agent_path: str) -> TraceProfile:
    return TraceProfile(agent_path=agent_path)


def copy_trace_profile(profile: TraceProfile) -> TraceProfile:
    return replace(profile)


def normalize_backend_text(text: str) -> str:
    backend = text.strip().upper() or "DR"
    if backend in ("DR", "TF", "PT"):
        return backend
    return ""


def validate_probe_spec_text(text: str) -> str:
    stripped = text.strip()
    if not stripped:
        raise ValueError("已启用观测器，但规则为空。")

    for rule in [item.strip() for item in stripped.split(";") if item.strip()]:
        lowered = rule.lower()
        if lowered.startswith("step@") or lowered.startswith("write@"):
            tokens = rule.split()
            if not tokens or "@" not in tokens[0]:
                raise ValueError("观测器规则语法无效。")
            is_write = lowered.startswith("write@")
            has_steps = False
            has_watch = False
            for token in tokens[1:]:
                if "=" not in token:
                    raise ValueError("观测器规则语法无效：参数必须使用 key=value。")
                key, value = [item.strip() for item in token.split("=", 1)]
                key = key.lower()
                if key == "steps":
                    try:
                        parsed = int(value, 0)
                    except ValueError as exc:
                        raise ValueError("观测器规则语法无效：steps 不是正整数。") from exc
                    if parsed <= 0:
                        raise ValueError("观测器规则语法无效：steps 不是正整数。")
                    has_steps = True
                elif key == "exit":
                    if value.lower() not in ("return", "leave", "return-or-leave", "return_or_leave"):
                        raise ValueError("观测器规则语法无效：exit 只支持 return / leave / return-or-leave。")
                elif key == "watch":
                    if not is_write:
                        raise ValueError("step 规则不能带 watch。")
                    watches = [item.strip() for item in value.split("|") if item.strip()]
                    if not watches or len(watches) > 4:
                        raise ValueError("write 规则需要 1 到 4 个 watch。")
                    for watch in watches:
                        parts = [item.strip() for item in watch.split(":")]
                        if len(parts) < 2 or len(parts) > 3:
                            raise ValueError("write watch 语法无效。")
                        try:
                            size = int(parts[1], 0)
                        except ValueError as exc:
                            raise ValueError("write watch 大小无效。") from exc
                        if size <= 0 or size > 32:
                            raise ValueError("write watch 大小必须在 1 到 32 之间。")
                    has_watch = True
                else:
                    raise ValueError(f"未知的观测器参数：{key}")
            if not has_steps:
                raise ValueError("step/write 规则必须显式配置 steps。")
            if is_write and not has_watch:
                raise ValueError("write 规则必须显式配置 watch。")
            continue

        if "->" not in rule:
            raise ValueError("观测器规则语法无效：capture 规则缺少 '->'。")
        hit_text, capture_text = [item.strip() for item in rule.split("->", 1)]
        if not hit_text or not capture_text:
            raise ValueError("观测器规则语法无效：命中点或 capture 为空。")
        captures = [item.strip() for item in capture_text.split("|") if item.strip()]
        if not captures:
            raise ValueError("观测器规则语法无效：没有 capture。")
        if len(captures) > 4:
            raise ValueError("单个 capture 规则最多只能配置 4 个 capture。")
        for capture in captures:
            parts = [item.strip() for item in capture.split(":")]
            if len(parts) < 2:
                raise ValueError("capture 语法无效。")
            kind = parts[0].lower()
            if kind == "reg":
                if len(parts) > 3:
                    raise ValueError("reg capture 语法无效。")
            elif kind == "mem":
                if len(parts) < 3 or len(parts) > 4:
                    raise ValueError("mem capture 语法无效。")
            elif kind == "ptr":
                if len(parts) < 3 or len(parts) > 4:
                    raise ValueError("ptr capture 语法无效。")
            else:
                raise ValueError(f"未知的 capture 类型：{parts[0]}")
    return stripped


def build_trace_config_from_profile(profile: TraceProfile) -> TraceConfig:
    backend = normalize_backend_text(profile.backend) or "DR"
    if backend == "PT":
        raise ValueError("PT 已迁到 backup/pt；当前主线只支持 DR / TF。")
    trigger_enabled = profile.trigger_enabled
    thread_capture_enabled = profile.thread_capture
    thread_id = _parse_non_negative_int(profile.thread_id, "线程 ID")
    max_events = _parse_non_negative_int(profile.max_events, "事件上限")
    idle_escape_threshold = _parse_non_negative_int(profile.idle_escape_threshold, "空转跳出阈值")
    auto_select_thread = trigger_enabled and thread_capture_enabled
    block_main_thread = auto_select_thread and profile.block_main_thread
    if trigger_enabled:
        if auto_select_thread:
            thread_id = 0
    elif thread_capture_enabled:
        thread_id = 0
    probe_spec = validate_probe_spec_text(profile.probe_spec) if profile.probe_enabled else ""
    depth_filter_spec = build_depth_filter_spec(
        profile.outside_call_depth_enabled,
        profile.outside_call_depth,
        profile.outside_execution_mode,
        profile.anonymous_exec_call_depth_enabled,
        profile.anonymous_exec_call_depth,
        profile.anonymous_exec_execution_mode,
        profile.module_call_depths,
    )

    return TraceConfig(
        thread_id=thread_id,
        auto_select_thread=auto_select_thread,
        block_main_thread=block_main_thread,
        modules=profile.modules.strip(),
        output_path=profile.output_path.strip(),
        max_events=max_events,
        trace_outside_modules=profile.trace_outside_modules,
        backend=backend,
        control_flow_only=backend != "TF",
        max_call_depth=ui_depth_to_runtime_text(profile.call_depth),
        depth_filter_spec=depth_filter_spec,
        hit_policy="every" if profile.repeat_hits else "first",
        hot_bypass_threshold=idle_escape_threshold if profile.idle_escape_enabled else 0,
        enhanced_sampling=profile.enhanced_sampling,
        trigger_point=profile.trigger_point.strip() if trigger_enabled else "",
        probe_spec=probe_spec,
        stop_on_root_return=profile.root_stop_on_return,
        async_thread_handoff=profile.async_thread_handoff,
    )


def format_trace_profile(profile: TraceProfile) -> str:
    depth_filter_spec = build_depth_filter_spec(
        profile.outside_call_depth_enabled,
        profile.outside_call_depth,
        profile.outside_execution_mode,
        profile.anonymous_exec_call_depth_enabled,
        profile.anonymous_exec_call_depth,
        profile.anonymous_exec_execution_mode,
        profile.module_call_depths,
    )
    parts = [
        f"agent_path={profile.agent_path}",
        f"thread_id={profile.thread_id.strip() or '0'}",
        f"thread_capture={'true' if profile.thread_capture else 'false'}",
        f"modules={profile.modules.strip() or '-'}",
        f"output_path={profile.output_path.strip() or '(auto)'}",
        f"max_events={profile.max_events.strip() or '0'}",
        f"backend={normalize_backend_text(profile.backend) or 'DR'}",
        f"call_depth={profile.call_depth.strip() or '0'}",
        f"outside_execution_mode={profile.outside_execution_mode.strip() or 'EDGE'}",
        f"anonymous_exec_execution_mode={profile.anonymous_exec_execution_mode.strip() or 'EDGE'}",
        f"trigger_enabled={'true' if profile.trigger_enabled else 'false'}",
        f"trigger_point={profile.trigger_point.strip() or '-'}",
        f"observer_enabled={'true' if profile.probe_enabled else 'false'}",
        f"observer_spec={profile.probe_spec.strip() or '-'}",
        f"trace_outside_modules={'true' if profile.trace_outside_modules else 'false'}",
        f"repeat_hits={'true' if profile.repeat_hits else 'false'}",
        f"idle_escape_enabled={'true' if profile.idle_escape_enabled else 'false'}",
        f"idle_escape_threshold={profile.idle_escape_threshold.strip() or '32'}",
        f"enhanced_sampling={'true' if profile.enhanced_sampling else 'false'}",
        f"block_main_thread={'true' if profile.block_main_thread else 'false'}",
        f"root_stop_on_return={'true' if profile.root_stop_on_return else 'false'}",
        f"async_thread_handoff={'true' if profile.async_thread_handoff else 'false'}",
        f"depth_filter_spec={depth_filter_spec or '-'}",
    ]
    return "\n".join(parts)


def _parse_non_negative_int(text: str, field_name: str) -> int:
    stripped = text.strip() or "0"
    try:
        value = int(stripped)
    except ValueError as exc:
        raise ValueError(f"{field_name}必须是非负整数。") from exc
    if value < 0:
        raise ValueError(f"{field_name}必须是非负整数。")
    return value
