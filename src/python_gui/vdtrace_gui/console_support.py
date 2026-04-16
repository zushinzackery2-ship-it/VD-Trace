from __future__ import annotations

import time
from pathlib import Path

from .control_cli import TraceCli
from .loader_controller import LoaderController
from .models import LoaderSessionSnapshot, default_agent_path
from .process_sessions import collect_direct_sessions
from .session_filter import filter_loader_sessions
from .trace_profile import TraceProfile, build_trace_config_from_profile, copy_trace_profile, normalize_backend_text
from .trace_settings import load_trace_profile, settings_path


def resolve_target(loader: LoaderController, args) -> tuple[int, LoaderSessionSnapshot | None]:
    sessions = wait_for_sessions(loader, args.wait_ms)
    if args.session_id is not None:
        for session in sessions:
            if session.session_id == args.session_id:
                return session.pid, session
        raise RuntimeError(f"没有找到会话 ID {args.session_id}。")
    if args.pid is not None:
        for session in sessions:
            if session.pid == args.pid:
                return args.pid, session
        return args.pid, None
    if len(sessions) == 1:
        return sessions[0].pid, sessions[0]
    if not sessions:
        raise RuntimeError("当前没有可用目标进程；请先启动游戏，或直接传 --pid。")
    raise RuntimeError("当前有多个可用目标进程，请显式指定 --session-id 或 --pid。")


def wait_for_sessions(loader: LoaderController, wait_ms: int) -> list[LoaderSessionSnapshot]:
    deadline = time.time() + max(0, wait_ms) / 1000.0
    sessions = _collect_sessions(loader)
    while not sessions and time.time() < deadline:
        time.sleep(0.05)
        sessions = _collect_sessions(loader)
    return sessions


def _collect_sessions(loader: LoaderController) -> list[LoaderSessionSnapshot]:
    loader_sessions = loader.snapshot_sessions()
    existing_pids = {session.pid for session in loader_sessions if session.pid > 0}
    return loader_sessions + collect_direct_sessions(existing_pids)


def ensure_agent_online(
    loader: LoaderController,
    cli: TraceCli,
    session: LoaderSessionSnapshot | None,
    pid: int,
    agent_path: str,
) -> str | None:
    if cli.ping(pid).success:
        return None
    if not agent_path.strip():
        return "Agent 路径为空。"
    if not Path(agent_path).exists():
        return f"未找到 Agent DLL: {agent_path}"
    if session is None or session.source == "direct":
        inject_result = cli.inject(pid, agent_path)
        if not inject_result.success:
            return inject_result.message
    elif session.supports_bootstrap:
        load_result = loader.send_load_request_with_timeout(session.session_id, agent_path, 1500)
        if load_result is None:
            return "发送 Agent 拉起请求超时，目标进程的 Loader 可能已卡住。"
        if not load_result:
            return "发送 Agent 拉起请求失败。"
    else:
        inject_result = cli.inject(pid, agent_path)
        if not inject_result.success:
            return inject_result.message
    if not cli.wait_until_online(pid, 5000):
        return "Agent IPC 未在 5 秒内上线。"
    return None


def agent_path_from_args(profile: TraceProfile, args) -> str:
    return (getattr(args, "agent_path", None) or profile.agent_path).strip()


def apply_profile_overrides(profile: TraceProfile, args) -> TraceProfile:
    updated = copy_trace_profile(profile)
    for key in (
        "agent_path",
        "thread_id",
        "modules",
        "output_path",
        "max_events",
        "backend",
        "call_depth",
        "module_call_depths",
        "trigger_point",
        "probe_spec",
    ):
        value = getattr(args, key, None)
        if value is not None:
            setattr(updated, key, value)
    if getattr(args, "output_auto", False):
        updated.output_path = ""
    if getattr(args, "outside_call_depth", None) is not None:
        updated.outside_call_depth = args.outside_call_depth
        updated.outside_call_depth_enabled = True
    if getattr(args, "outside_execution", None) is not None:
        updated.outside_execution_mode = str(args.outside_execution).upper()
        updated.outside_call_depth_enabled = True
    if getattr(args, "outside_call_depth_enabled", None) is not None:
        updated.outside_call_depth_enabled = args.outside_call_depth_enabled
    if getattr(args, "anonymous_call_depth", None) is not None:
        updated.anonymous_exec_call_depth = args.anonymous_call_depth
        updated.anonymous_exec_call_depth_enabled = True
    if getattr(args, "anonymous_execution", None) is not None:
        updated.anonymous_exec_execution_mode = str(args.anonymous_execution).upper()
        updated.anonymous_exec_call_depth_enabled = True
    if getattr(args, "anonymous_exec_call_depth_enabled", None) is not None:
        updated.anonymous_exec_call_depth_enabled = args.anonymous_exec_call_depth_enabled
    for key in (
        "thread_capture",
        "trigger_enabled",
        "probe_enabled",
        "block_main_thread",
        "trace_outside_modules",
        "repeat_hits",
        "idle_escape_enabled",
        "enhanced_sampling",
        "root_stop_on_return",
        "async_thread_handoff",
    ):
        value = getattr(args, key, None)
        if value is not None:
            setattr(updated, key, value)
    if updated.backend:
        updated.backend = normalize_backend_text(updated.backend) or updated.backend.strip().upper()
    if getattr(args, "trigger_point", None) is not None:
        updated.trigger_enabled = True
    if getattr(args, "probe_spec", None) is not None:
        updated.probe_enabled = True
    if getattr(args, "idle_escape_threshold", None) is not None:
        updated.idle_escape_threshold = args.idle_escape_threshold
        updated.idle_escape_enabled = True
    return updated


def run_cli_self_test(repo_root: Path) -> int:
    profile = TraceProfile(
        agent_path=str(default_agent_path(repo_root)),
        thread_id="77",
        thread_capture=True,
        modules="UnityPlayer.dll",
        call_depth="3",
        outside_call_depth_enabled=True,
        outside_call_depth="2",
        outside_execution_mode="TF",
        anonymous_exec_call_depth_enabled=True,
        anonymous_exec_call_depth="1",
        anonymous_exec_execution_mode="EDGE",
        module_call_depths="GameAssembly.dll:0:TF",
        probe_spec="UnityPlayer.dll!0x10->reg:rcx:key|ptr:rsp+0x20:16:buf;step@UnityPlayer.dll!0x20 steps=8 exit=return-or-leave",
        probe_enabled=True,
        trigger_point="UnityPlayer.dll!0x20",
        trigger_enabled=True,
        block_main_thread=True,
        trace_outside_modules=True,
        backend="TF",
        repeat_hits=True,
        idle_escape_enabled=True,
        idle_escape_threshold="32",
        enhanced_sampling=True,
        root_stop_on_return=False,
    )
    config = build_trace_config_from_profile(profile)
    if (
        not config.auto_select_thread
        or not config.block_main_thread
        or config.backend != "TF"
        or config.control_flow_only
        or "backend=tf" not in " ".join(config.cli_args(4242)).lower()
        or "outside=1:tf" not in config.depth_filter_spec
        or "module=GameAssembly.dll:all:tf" not in config.depth_filter_spec
        or config.hot_bypass_threshold != 32
        or "step@UnityPlayer.dll!0x20 steps=8 exit=return-or-leave" not in config.probe_spec
    ):
        print("[fail] CLI 自检失败：配置构建结果不符合预期。")
        return 1
    legacy_pt = copy_trace_profile(profile)
    legacy_pt.backend = "PT"
    try:
        build_trace_config_from_profile(legacy_pt)
    except ValueError as exc:
        if "backup/pt" not in str(exc):
            print("[fail] CLI 自检失败：旧 PT 配置没有返回明确错误。")
            return 1
    else:
        print("[fail] CLI 自检失败：旧 PT 配置不应继续成功启动。")
        return 1
    loaded = load_trace_profile(repo_root, profile.agent_path)
    if not loaded.agent_path.strip():
        print("[fail] CLI 自检失败：共享配置读取为空。")
        return 1
    if settings_path(repo_root).name.lower() != "vdtrace_gui.ini":
        print("[fail] CLI 自检失败：共享配置路径异常。")
        return 1
    filtered_sessions = filter_loader_sessions(
        [
            LoaderSessionSnapshot(session_id=1, pid=300, process_path=r"F:\Program Files\Endfield Game\PlatformProcess.exe", hello_received=True),
            LoaderSessionSnapshot(session_id=2, pid=200, process_path=r"F:\Program Files\Endfield Game\Endfield.exe", hello_received=True),
            LoaderSessionSnapshot(session_id=3, pid=100, process_path="dummy.exe", hello_received=True),
        ]
    )
    if [session.pid for session in filtered_sessions] != [200]:
        print("[fail] CLI 自检失败：Loader 会话过滤结果不符合预期。")
        return 1
    print("[ok] CLI 自检通过。")
    return 0
