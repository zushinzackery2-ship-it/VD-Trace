from __future__ import annotations

import configparser
from pathlib import Path

from .depth_filters import normalize_execution_mode
from .trace_profile import TraceProfile, default_trace_profile, normalize_backend_text


def settings_path(repo_root: Path) -> Path:
    return repo_root / "vdtrace_gui.ini"


def load_trace_profile(repo_root: Path, agent_path: str) -> TraceProfile:
    path = settings_path(repo_root)
    parser = configparser.ConfigParser()
    defaults = default_trace_profile(agent_path)
    rewrite = False
    if path.exists():
        parser.read(path, encoding="utf-8-sig")
    else:
        parser["trace"] = _profile_to_section_values(defaults)
        _write_settings(path, parser)

    section = parser["trace"] if parser.has_section("trace") else {}
    profile = TraceProfile(
        agent_path=_read_text(section, "agent_path", defaults.agent_path),
        thread_id=_read_text(section, "thread_id", defaults.thread_id),
        thread_capture=defaults.thread_capture,
        modules=_read_text(section, "modules", defaults.modules),
        output_path=_read_text(section, "output_path", defaults.output_path),
        max_events=_read_text(section, "max_events", defaults.max_events),
        backend=_read_text(section, "backend", defaults.backend).strip().upper() or defaults.backend,
        call_depth=defaults.call_depth,
        outside_call_depth_enabled=defaults.outside_call_depth_enabled,
        outside_call_depth=_read_text(section, "outside_call_depth", defaults.outside_call_depth),
        outside_execution_mode=_read_text(section, "outside_execution_mode", defaults.outside_execution_mode),
        anonymous_exec_call_depth_enabled=defaults.anonymous_exec_call_depth_enabled,
        anonymous_exec_call_depth=_read_text(section, "anonymous_exec_call_depth", defaults.anonymous_exec_call_depth),
        anonymous_exec_execution_mode=_read_text(section, "anonymous_exec_execution_mode", defaults.anonymous_exec_execution_mode),
        module_call_depths=_read_text(section, "module_call_depths", defaults.module_call_depths),
        trigger_point=_read_text(section, "trigger_point", defaults.trigger_point),
        probe_spec=_read_text(section, "probe_spec", defaults.probe_spec),
        probe_enabled=_read_bool(section, "probe_enabled", defaults.probe_enabled),
        trigger_enabled=_read_bool(section, "trigger_enabled", defaults.trigger_enabled),
        block_main_thread=_read_bool(section, "block_main_thread", defaults.block_main_thread),
        trace_outside_modules=_read_bool(section, "trace_outside_modules", defaults.trace_outside_modules),
        all_events=_read_bool(section, "all_events", defaults.all_events),
        repeat_hits=_read_bool(section, "repeat_hits", defaults.repeat_hits),
        idle_escape_enabled=defaults.idle_escape_enabled,
        idle_escape_threshold=_read_text(section, "idle_escape_threshold", defaults.idle_escape_threshold),
        enhanced_sampling=_read_bool(section, "enhanced_sampling", defaults.enhanced_sampling),
        root_stop_on_return=_read_bool(section, "root_stop_on_return", defaults.root_stop_on_return),
        async_thread_handoff=_read_bool(section, "async_thread_handoff", defaults.async_thread_handoff),
    )

    call_depth_mode = _read_text(section, "call_depth_mode", "").strip().lower()
    stored_call_depth = _read_text(section, "call_depth", defaults.call_depth)
    if call_depth_mode == "ui_v2":
        profile.call_depth = stored_call_depth
    else:
        profile.call_depth = _legacy_call_depth_to_ui(stored_call_depth, defaults.call_depth)
        rewrite = True

    has_outside_call_depth = "outside_call_depth" in section
    try:
        profile.outside_execution_mode = normalize_execution_mode(profile.outside_execution_mode)
    except ValueError:
        profile.outside_execution_mode = defaults.outside_execution_mode
        rewrite = True
    if "outside_call_depth_enabled" in section:
        profile.outside_call_depth_enabled = _read_bool(
            section,
            "outside_call_depth_enabled",
            defaults.outside_call_depth_enabled,
        )
    else:
        profile.outside_call_depth_enabled = has_outside_call_depth and bool(profile.outside_call_depth.strip())
        rewrite = True

    has_anonymous_exec_call_depth = "anonymous_exec_call_depth" in section
    try:
        profile.anonymous_exec_execution_mode = normalize_execution_mode(profile.anonymous_exec_execution_mode)
    except ValueError:
        profile.anonymous_exec_execution_mode = defaults.anonymous_exec_execution_mode
        rewrite = True
    if "anonymous_exec_call_depth_enabled" in section:
        profile.anonymous_exec_call_depth_enabled = _read_bool(
            section,
            "anonymous_exec_call_depth_enabled",
            defaults.anonymous_exec_call_depth_enabled,
        )
    else:
        profile.anonymous_exec_call_depth_enabled = has_anonymous_exec_call_depth and bool(
            profile.anonymous_exec_call_depth.strip()
        )
        rewrite = True

    if "idle_escape_enabled" in section:
        profile.idle_escape_enabled = _read_bool(section, "idle_escape_enabled", defaults.idle_escape_enabled)
    else:
        profile.idle_escape_enabled = True
        rewrite = True

    thread_capture_mode = _read_text(section, "thread_capture_mode", "").strip().lower()
    if thread_capture_mode == "ui_v3_auto_positive":
        profile.thread_capture = _read_bool(section, "thread_capture", defaults.thread_capture)
    elif "thread_capture" in section:
        profile.thread_capture = not _read_bool(section, "thread_capture", defaults.thread_capture)
        rewrite = True
    elif "auto_thread_capture" in section:
        profile.thread_capture = _read_bool(section, "auto_thread_capture", defaults.thread_capture)
        rewrite = True
    elif "manual_thread" in section:
        profile.thread_capture = not _read_bool(section, "manual_thread", not defaults.thread_capture)
        rewrite = True
    elif "auto_select_thread" in section:
        legacy_capture = _read_bool(
            section,
            "thread_capture",
            _read_bool(section, "auto_select_thread", defaults.thread_capture),
        )
        profile.thread_capture = legacy_capture if profile.trigger_enabled else not legacy_capture
        rewrite = True
    else:
        profile.thread_capture = defaults.thread_capture
        rewrite = True

    profile.backend = normalize_backend_text(profile.backend)
    if profile.backend == "PT":
        pass
    elif not profile.backend:
        profile.backend = "TF" if profile.all_events else "DR"
        rewrite = True
    elif "backend" not in section:
        profile.backend = "TF" if profile.all_events else "DR"
        rewrite = True

    if (
        not parser.has_section("trace")
        or "trigger_enabled" not in section
        or "async_thread_handoff" not in section
        or "enhanced_sampling" not in section
        or "block_main_thread" not in section
        or "thread_capture" not in section
        or "thread_capture_mode" not in section
        or "call_depth_mode" not in section
        or "outside_call_depth_enabled" not in section
        or "outside_call_depth" not in section
        or "outside_execution_mode" not in section
        or "anonymous_exec_call_depth_enabled" not in section
        or "anonymous_exec_call_depth" not in section
        or "anonymous_exec_execution_mode" not in section
        or "module_call_depths" not in section
        or "probe_enabled" not in section
        or "idle_escape_enabled" not in section
        or "idle_escape_threshold" not in section
        or "backend" not in section
        or "single_thread_focus" in section
    ):
        rewrite = True

    if rewrite:
        save_trace_profile(repo_root, profile)

    return profile


def save_trace_profile(repo_root: Path, profile: TraceProfile) -> None:
    parser = configparser.ConfigParser()
    parser["trace"] = _profile_to_section_values(profile)
    _write_settings(settings_path(repo_root), parser)


def _profile_to_section_values(profile: TraceProfile) -> dict[str, str]:
    return {
        "agent_path": profile.agent_path.strip(),
        "thread_id": profile.thread_id.strip() or "0",
        "thread_capture_mode": "ui_v3_auto_positive",
        "thread_capture": _write_bool(profile.thread_capture),
        "modules": profile.modules.strip(),
        "output_path": profile.output_path.strip(),
        "max_events": profile.max_events.strip() or "0",
        "backend": normalize_backend_text(profile.backend) or "DR",
        "call_depth_mode": "ui_v2",
        "call_depth": profile.call_depth.strip() or "0",
        "outside_call_depth_enabled": _write_bool(profile.outside_call_depth_enabled),
        "outside_call_depth": profile.outside_call_depth.strip() or "3",
        "outside_execution_mode": normalize_execution_mode(profile.outside_execution_mode),
        "anonymous_exec_call_depth_enabled": _write_bool(profile.anonymous_exec_call_depth_enabled),
        "anonymous_exec_call_depth": profile.anonymous_exec_call_depth.strip() or "3",
        "anonymous_exec_execution_mode": normalize_execution_mode(profile.anonymous_exec_execution_mode),
        "module_call_depths": profile.module_call_depths.strip(),
        "trigger_point": profile.trigger_point.strip(),
        "probe_spec": profile.probe_spec.strip(),
        "probe_enabled": _write_bool(profile.probe_enabled),
        "trigger_enabled": _write_bool(profile.trigger_enabled),
        "block_main_thread": _write_bool(profile.block_main_thread),
        "trace_outside_modules": _write_bool(profile.trace_outside_modules),
        "all_events": _write_bool((normalize_backend_text(profile.backend) or "DR") == "TF"),
        "repeat_hits": _write_bool(profile.repeat_hits),
        "idle_escape_enabled": _write_bool(profile.idle_escape_enabled),
        "idle_escape_threshold": profile.idle_escape_threshold.strip() or "32",
        "enhanced_sampling": _write_bool(profile.enhanced_sampling),
        "root_stop_on_return": _write_bool(profile.root_stop_on_return),
        "async_thread_handoff": _write_bool(profile.async_thread_handoff),
    }


def _read_text(section, key: str, fallback: str) -> str:
    value = section.get(key, fallback)
    return fallback if value is None else str(value)


def _read_bool(section, key: str, fallback: bool) -> bool:
    value = _read_text(section, key, _write_bool(fallback)).strip().lower()
    if value in ("1", "true", "yes", "on"):
        return True
    if value in ("0", "false", "no", "off"):
        return False
    return fallback


def _write_bool(value: bool) -> str:
    return "true" if value else "false"


def _write_settings(path: Path, parser: configparser.ConfigParser) -> None:
    path.write_text(_render_settings(parser), encoding="utf-8")


def _legacy_call_depth_to_ui(text: str, fallback: str) -> str:
    lowered = text.strip().lower()
    if lowered == "all":
        return "0"
    if lowered == "same" or lowered == "single":
        return "1"

    try:
        value = int(lowered or "0")
    except ValueError:
        return fallback

    return str(max(0, value) + 1)


def _render_settings(parser: configparser.ConfigParser) -> str:
    section = parser["trace"]
    lines = [
        "; VD-Trace Python control settings",
        "; GUI 和 CLI 共用这一份配置",
        "; trigger_point supports 0xADDRESS or module+0xRVA or module!0xRVA",
        "; probe_spec stores observer rules: capture uses hit->capture|capture ; step uses step@hit steps=256 exit=return-or-leave ; write uses write@hit watch=addr:size:label|... steps=256 exit=return",
        "; backend supports DR / TF; legacy PT entries stay visible but start will fail with an explicit error",
        "; call_depth_mode=ui_v2 means: 0=all, 1=single, 2=follow one more layer, 3=follow two more layers, etc.",
        "; outside_call_depth / anonymous_exec_call_depth reuse the same numbering as call_depth; outside_execution_mode / anonymous_exec_execution_mode choose EDGE or TF",
        "; module_call_depths format: ModuleA.dll:3:TF,ModuleB.dll:1:EDGE  (same UI numbering; 0=all, 1=single)",
        "; thread_capture_mode=ui_v3_auto_positive means: thread_capture=true is the positive '自动线程捕获' meaning",
        "; thread_capture=true means: with trigger -> auto capture the first hit thread; without trigger -> direct main-thread trace",
        "; thread_capture=false means: enable manual thread_id input; with trigger it becomes a fixed waiting thread",
        "; block_main_thread=true means: with trigger+thread_capture=true, main-thread hits are ignored and capture keeps waiting for another thread",
        "; output_path keeps custom paths as-is; the legacy .\\traces\\VDTrace.log template auto-upgrades to a timestamped file on start",
        "; idle_escape_threshold controls hot empty-spin escape in first-hit mode; 0 disables it",
        "; enhanced_sampling adds rare-path buffer snapshots for cross-module call/return pairs",
        "[trace]",
    ]
    for key in (
        "agent_path",
        "thread_id",
        "thread_capture_mode",
        "thread_capture",
        "modules",
        "output_path",
        "max_events",
        "backend",
        "call_depth_mode",
        "call_depth",
        "outside_call_depth_enabled",
        "outside_call_depth",
        "outside_execution_mode",
        "anonymous_exec_call_depth_enabled",
        "anonymous_exec_call_depth",
        "anonymous_exec_execution_mode",
        "module_call_depths",
        "trigger_point",
        "probe_spec",
        "probe_enabled",
        "trigger_enabled",
        "block_main_thread",
        "trace_outside_modules",
        "all_events",
        "repeat_hits",
        "idle_escape_enabled",
        "idle_escape_threshold",
        "enhanced_sampling",
        "root_stop_on_return",
        "async_thread_handoff",
    ):
        lines.append(f"{key} = {section.get(key, '')}")
    return "\n".join(lines) + "\n"
