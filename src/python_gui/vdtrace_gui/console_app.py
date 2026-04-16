from __future__ import annotations

from pathlib import Path

from .app_status import format_trace_status_text
from .console_args import build_parser
from .control_cli import TraceCli
from .console_support import agent_path_from_args, apply_profile_overrides, ensure_agent_online, resolve_target, run_cli_self_test
from .console_support import wait_for_sessions
from .loader_controller import LoaderController
from .memory_codec import encode_memory_write_bytes, parse_memory_size
from .models import LoaderSessionSnapshot, default_agent_path, default_ctl_path, default_output_path
from .models import is_auto_output_path
from .trace_profile import TraceProfile, build_trace_config_from_profile, format_trace_profile
from .trace_settings import load_trace_profile, save_trace_profile, settings_path


def run_cli(repo_root: Path, argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    profile = load_trace_profile(repo_root, str(default_agent_path(repo_root)))
    ctl_path = default_ctl_path(repo_root)
    if not ctl_path.exists():
        print(f"[fail] 未找到控制器: {ctl_path}")
        return 1
    cli = TraceCli(ctl_path, repo_root)

    if args.command == "config-show":
        if args.raw_path:
            print(settings_path(repo_root))
            return 0
        print(settings_path(repo_root))
        print(format_trace_profile(profile))
        return 0

    if args.command == "config-save":
        updated = apply_profile_overrides(profile, args)
        save_trace_profile(repo_root, updated)
        print(f"[ok] 已保存共享配置: {settings_path(repo_root)}")
        print(format_trace_profile(updated))
        return 0

    if args.command == "self-test":
        return run_cli_self_test(repo_root)

    loader = LoaderController()
    loader.start()
    try:
        if args.command == "sessions":
            return _handle_sessions(loader, args.wait_ms)

        try:
            pid, session = resolve_target(loader, args)
        except RuntimeError as exc:
            print(f"[fail] {exc}")
            return 1
        if args.command == "load":
            return _handle_load(loader, cli, session, pid, agent_path_from_args(profile, args))
        if args.command == "modules":
            return _handle_modules(loader, cli, session, pid, agent_path_from_args(profile, args), args.all)
        if args.command == "dump":
            return _handle_dump(loader, cli, session, pid, agent_path_from_args(profile, args), args.module, args.output_dir)
        if args.command == "memory-read":
            return _handle_memory_read(loader, cli, session, pid, agent_path_from_args(profile, args), args.address, args.size)
        if args.command == "memory-write":
            return _handle_memory_write(loader, cli, session, pid, agent_path_from_args(profile, args), args)
        if args.command == "status":
            return _handle_status(cli, pid)
        if args.command == "stop":
            return _print_result("[stop]", cli.stop(pid))
        if args.command == "start":
            updated = apply_profile_overrides(profile, args)
            return _handle_start(loader, cli, session, pid, updated)
        parser.error(f"未知命令: {args.command}")
        return 2
    finally:
        loader.stop()


def _handle_sessions(loader: LoaderController, wait_ms: int) -> int:
    sessions = wait_for_sessions(loader, wait_ms)
    if not sessions:
        print("[fail] 当前没有可用目标进程。")
        return 1
    for session in sessions:
        print(
            f"session_id={session.session_id} pid={session.pid} source={session.source} supports_bootstrap={'true' if session.supports_bootstrap else 'false'}"
        )
        print(f"  path={session.process_path}")
        print(f"  status={session.capability_text}")
    return 0


def _handle_load(loader: LoaderController, cli: TraceCli, session: LoaderSessionSnapshot | None, pid: int, agent_path: str) -> int:
    error = ensure_agent_online(loader, cli, session, pid, agent_path)
    if error:
        print(f"[fail] {error}")
        return 1
    print(f"[ok] Agent IPC 已上线: pid={pid}")
    return 0


def _handle_modules(
    loader: LoaderController,
    cli: TraceCli,
    session: LoaderSessionSnapshot | None,
    pid: int,
    agent_path: str,
    include_system_modules: bool,
) -> int:
    error = ensure_agent_online(loader, cli, session, pid, agent_path)
    if error:
        print(f"[fail] {error}")
        return 1
    result = cli.modules(pid, include_system_modules=include_system_modules)
    return _print_result("[modules]", result)


def _handle_dump(
    loader: LoaderController,
    cli: TraceCli,
    session: LoaderSessionSnapshot | None,
    pid: int,
    agent_path: str,
    module_name: str,
    output_dir: str,
) -> int:
    error = ensure_agent_online(loader, cli, session, pid, agent_path)
    if error:
        print(f"[fail] {error}")
        return 1
    modules_result = cli.modules(pid, include_system_modules=False)
    dump_result = cli.dump_module(pid, module_name, output_dir)
    code = _print_result("[modules]", modules_result)
    dump_code = _print_result("[dump]", dump_result)
    return dump_code if dump_code != 0 else code


def _handle_memory_read(
    loader: LoaderController,
    cli: TraceCli,
    session: LoaderSessionSnapshot | None,
    pid: int,
    agent_path: str,
    address_text: str,
    size_text: str,
) -> int:
    error = ensure_agent_online(loader, cli, session, pid, agent_path)
    if error:
        print(f"[fail] {error}")
        return 1
    try:
        size = parse_memory_size(size_text)
    except ValueError as exc:
        print(f"[fail] {exc}")
        return 1
    return _print_result("[memory-read]", cli.read_memory(pid, address_text, size))


def _handle_memory_write(
    loader: LoaderController,
    cli: TraceCli,
    session: LoaderSessionSnapshot | None,
    pid: int,
    agent_path: str,
    args,
) -> int:
    error = ensure_agent_online(loader, cli, session, pid, agent_path)
    if error:
        print(f"[fail] {error}")
        return 1

    mode = "HEX"
    value = args.memory_hex
    if args.memory_text is not None:
        mode = "TEXT"
        value = args.memory_text
    elif args.memory_utf16 is not None:
        mode = "UTF16"
        value = args.memory_utf16
    elif args.memory_u32 is not None:
        mode = "U32"
        value = args.memory_u32
    elif args.memory_u64 is not None:
        mode = "U64"
        value = args.memory_u64

    try:
        data = encode_memory_write_bytes(mode, value)
    except ValueError as exc:
        print(f"[fail] {exc}")
        return 1
    return _print_result("[memory-write]", cli.write_memory(pid, args.address, data))


def _handle_status(cli: TraceCli, pid: int) -> int:
    result = cli.status(pid)
    if not result.success:
        return _print_result("[status]", result)
    print("[status] " + format_trace_status_text(result.message))
    print(result.message)
    return 0


def _handle_start(
    loader: LoaderController,
    cli: TraceCli,
    session: LoaderSessionSnapshot | None,
    pid: int,
    profile: TraceProfile,
) -> int:
    error = ensure_agent_online(loader, cli, session, pid, profile.agent_path)
    if error:
        print(f"[fail] {error}")
        return 1
    try:
        config = build_trace_config_from_profile(profile)
    except ValueError as exc:
        print(f"[fail] {exc}")
        return 1
    if is_auto_output_path(config.output_path, pid):
        config.output_path = default_output_path(pid)
    configure = cli.configure(pid, config)
    configure_code = _print_result("[configure]", configure)
    if configure_code != 0:
        return configure_code
    start = cli.start(pid)
    start_code = _print_result("[start]", start)
    if start.success:
        print(f"output_path={config.output_path}")
    return start_code


def _print_result(prefix: str, result) -> int:
    tag = "[ok]" if result.success else "[fail]"
    print(f"{prefix} {tag} {result.message}")
    return 0 if result.success else 1
