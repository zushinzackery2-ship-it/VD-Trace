from __future__ import annotations

import argparse


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="VD-Trace Python CLI 控制端")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sessions = subparsers.add_parser("sessions", help="列出当前可用目标进程")
    sessions.add_argument("--wait-ms", type=int, default=0, help="等待目标进程出现的最长时间，默认 0。")

    load = subparsers.add_parser("load", help="拉起 Agent；优先走 Loader，可回退为直接注入")
    _add_target_options(load)
    load.add_argument("--agent", dest="agent_path", help="Agent DLL 路径；默认使用共享配置里的值。")

    modules = subparsers.add_parser("modules", help="列出目标进程模块")
    _add_target_options(modules)
    modules.add_argument("--all", action="store_true", help="包含系统模块。")

    dump = subparsers.add_parser("dump", help="执行 Dump+Fix")
    _add_target_options(dump)
    dump.add_argument("--module", required=True, help="要转储的模块名。")
    dump.add_argument("--output-dir", default=r".\dump", help=r"输出目录，默认 .\dump。")

    memory_read = subparsers.add_parser("memory-read", help="读取目标进程内存")
    _add_target_options(memory_read)
    memory_read.add_argument("--agent", dest="agent_path", help="Agent DLL 路径；默认使用共享配置里的值。")
    memory_read.add_argument("--address", required=True, help="地址，支持 0xADDR / module+0xRVA / module!0xRVA。")
    memory_read.add_argument("--size", default="64", help="读取长度，默认 64。")

    memory_write = subparsers.add_parser("memory-write", help="写入目标进程内存")
    _add_target_options(memory_write)
    memory_write.add_argument("--agent", dest="agent_path", help="Agent DLL 路径；默认使用共享配置里的值。")
    memory_write.add_argument("--address", required=True, help="地址，支持 0xADDR / module+0xRVA / module!0xRVA。")
    group = memory_write.add_mutually_exclusive_group(required=True)
    group.add_argument("--hex", dest="memory_hex", help="HEX 字节串，例如 '90 90 c3'。")
    group.add_argument("--text", dest="memory_text", help="UTF-8 文本写入。")
    group.add_argument("--utf16", dest="memory_utf16", help="UTF-16LE 文本写入。")
    group.add_argument("--u32", dest="memory_u32", help="32 位整数写入。")
    group.add_argument("--u64", dest="memory_u64", help="64 位整数写入。")

    start = subparsers.add_parser("start", help="按 GUI 语义配置并启动追踪")
    _add_target_options(start)
    _add_profile_override_args(start, include_agent=True)

    stop = subparsers.add_parser("stop", help="停止当前追踪")
    _add_target_options(stop)

    status = subparsers.add_parser("status", help="查询当前追踪状态")
    _add_target_options(status)

    config_show = subparsers.add_parser("config-show", help="显示共享配置")
    config_show.add_argument("--raw-path", action="store_true", help="只输出配置文件路径。")

    config_save = subparsers.add_parser("config-save", help="保存共享配置")
    _add_profile_override_args(config_save, include_agent=True)

    subparsers.add_parser("self-test", help="运行 CLI 自检")
    return parser


def _add_target_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--session-id", type=int, help="目标会话 ID。")
    parser.add_argument("--pid", type=int, help="目标进程 PID。")
    parser.add_argument("--wait-ms", type=int, default=1500, help="等待目标进程或 Agent 上线的最长时间。")


def _add_profile_override_args(parser: argparse.ArgumentParser, include_agent: bool) -> None:
    if include_agent:
        parser.add_argument("--agent", dest="agent_path", help="Agent DLL 路径。")
    parser.add_argument("--thread-id", help="手动线程 ID；0 表示主线程。")
    _add_bool_pair(parser, "thread_capture", "--thread-capture", "--manual-thread", "启用自动线程捕获。", "改为手动线程。")
    parser.add_argument("--modules", help="指定模块记录列表，逗号分隔。")
    parser.add_argument("--output", dest="output_path", help="输出日志路径。")
    parser.add_argument("--output-auto", action="store_true", help="恢复自动时间戳输出路径。")
    parser.add_argument("--max-events", help="事件上限，0 表示不限。")
    parser.add_argument("--backend", choices=("dr", "tf", "DR", "TF"), help="显式选择后端：DR / TF。")
    parser.add_argument("--call-depth", help="默认追踪层级，沿用 GUI 编号：0=ALL，1=SINGLE，2=向下一层。")
    parser.add_argument("--outside-call-depth", help="模块外区域层级，设置即启用。")
    parser.add_argument("--outside-execution", choices=("edge", "tf", "EDGE", "TF"), help="模块外区域执行模式：EDGE 或 TF。")
    parser.add_argument("--no-outside-call-depth", dest="outside_call_depth_enabled", action="store_false", default=None, help="关闭模块外区域层级覆盖。")
    parser.add_argument("--anonymous-call-depth", help="匿名执行页层级，设置即启用。")
    parser.add_argument("--anonymous-execution", choices=("edge", "tf", "EDGE", "TF"), help="匿名执行页执行模式：EDGE 或 TF。")
    parser.add_argument("--no-anonymous-call-depth", dest="anonymous_exec_call_depth_enabled", action="store_false", default=None, help="关闭匿名执行页层级覆盖。")
    parser.add_argument("--module-call-depths", help="模块规则，例如 GameAssembly.dll:0:TF,UnityPlayer.dll:2:EDGE")
    parser.add_argument("--trigger", dest="trigger_point", help="触发点，支持 0x地址 / 模块+0xRVA / 模块!0xRVA。")
    _add_bool_pair(parser, "trigger_enabled", "--trigger-on", "--no-trigger", "启用触发点开关。", "关闭触发点开关。")
    parser.add_argument("--observer", "--probe", dest="probe_spec", help="观测器规则。")
    _add_bool_pair(parser, "probe_enabled", "--observer-on", "--no-observer", "启用观测器。", "关闭观测器。")
    parser.add_argument("--probe-on", dest="probe_enabled", action="store_true", default=None, help=argparse.SUPPRESS)
    parser.add_argument("--no-probe", dest="probe_enabled", action="store_false", help=argparse.SUPPRESS)
    _add_bool_pair(parser, "block_main_thread", "--block-main-thread", "--no-block-main-thread", "启用屏蔽主线程。", "关闭屏蔽主线程。")
    _add_bool_pair(parser, "trace_outside_modules", "--trace-outside-modules", "--module-scope-only", "允许继续记录外部业务模块。", "只记录指定模块。")
    parser.add_argument("--all-events", dest="backend", action="store_const", const="TF", default=None, help=argparse.SUPPRESS)
    parser.add_argument("--control-flow-only", dest="backend", action="store_const", const="DR", help=argparse.SUPPRESS)
    _add_bool_pair(parser, "repeat_hits", "--repeat-hits", "--first-hit-only", "记录重复命中。", "只记录首次命中。")
    parser.add_argument("--idle-escape-threshold", help="空转跳出阈值；0 表示关闭。")
    _add_bool_pair(parser, "idle_escape_enabled", "--idle-escape", "--no-idle-escape", "启用空转跳出。", "关闭空转跳出。")
    _add_bool_pair(parser, "enhanced_sampling", "--sample", "--no-sample", "启用增强采样。", "关闭增强采样。")
    _add_bool_pair(parser, "root_stop_on_return", "--root-stop", "--no-root-stop", "根返回自动停止。", "关闭根返回自动停止。")
    _add_bool_pair(parser, "async_thread_handoff", "--handoff", "--no-handoff", "启用异步线程追踪。", "关闭异步线程追踪。")


def _add_bool_pair(
    parser: argparse.ArgumentParser,
    dest: str,
    positive: str,
    negative: str,
    positive_help: str,
    negative_help: str,
) -> None:
    parser.add_argument(positive, dest=dest, action="store_true", default=None, help=positive_help)
    parser.add_argument(negative, dest=dest, action="store_false", help=negative_help)
