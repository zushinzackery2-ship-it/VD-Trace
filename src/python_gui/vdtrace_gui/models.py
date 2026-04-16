from __future__ import annotations

from datetime import datetime
from dataclasses import dataclass
from pathlib import Path
import re


LOADER_PIPE_NAME = r"\\.\pipe\VDTraceLoaderControl"
LOADER_MAGIC = 0x5654444C
LOADER_PROTOCOL_VERSION = 1
LOADER_FEATURE_BOOTSTRAP = 0x00000001
LOADER_MAX_PATH_CHARS = 1024
LOADER_MAX_TEXT_CHARS = 256

LOADER_MSG_AGENT_HELLO = 1
LOADER_MSG_AGENT_LOG = 2
LOADER_MSG_LOAD_DLL_REQUEST = 3
LOADER_MSG_LOAD_DLL_REPLY = 4


@dataclass(slots=True)
class LoaderSessionSnapshot:
    session_id: int
    pid: int = 0
    process_path: str = ""
    source: str = "loader"
    protocol_version: int = 0
    feature_flags: int = 0
    connected: bool = True
    hello_received: bool = False

    @property
    def supports_bootstrap(self) -> bool:
        return self.protocol_version >= LOADER_PROTOCOL_VERSION and bool(
            self.feature_flags & LOADER_FEATURE_BOOTSTRAP
        )

    @property
    def display_name(self) -> str:
        if self.pid == 0:
            return f"[等待握手:{self.session_id}] Loader 尚未上报"
        if self.source == "direct":
            return f"[{self.pid}] {self.process_path} [直连]"
        return f"[{self.pid}] {self.process_path}"

    @property
    def capability_text(self) -> str:
        if self.pid == 0:
            return "等待 Loader 握手。"
        if self.source == "direct":
            return "直连 | 未使用 Loader | 可直接注入 Agent"
        support = "支持" if self.supports_bootstrap else "不支持"
        return f"在线 | 协议 v{self.protocol_version} | 自动拉起 Agent {support}"


@dataclass(slots=True)
class CommandResult:
    success: bool
    message: str
    raw_output: str = ""


@dataclass(slots=True)
class TraceConfig:
    thread_id: int
    auto_select_thread: bool
    block_main_thread: bool
    modules: str
    output_path: str
    max_events: int
    trace_outside_modules: bool
    backend: str
    control_flow_only: bool
    max_call_depth: str
    depth_filter_spec: str
    hit_policy: str
    hot_bypass_threshold: int
    enhanced_sampling: bool
    trigger_point: str
    probe_spec: str
    stop_on_root_return: bool
    async_thread_handoff: bool

    def cli_args(self, pid: int) -> list[str]:
        args = [
            "configure",
            str(pid),
            str(self.thread_id),
            self.modules if self.modules else "-",
            self.output_path,
        ]
        if self.max_events > 0:
            args.append(str(self.max_events))
        if self.trace_outside_modules:
            args.append("outside")
        args.append(f"backend={self.backend.lower()}")
        args.append(f"depth={self.max_call_depth}")
        if self.depth_filter_spec:
            args.append(f"depthfilter={self.depth_filter_spec}")
        args.append(f"hits={self.hit_policy}")
        args.append(f"idleescape={self.hot_bypass_threshold}")
        if self.enhanced_sampling:
            args.append("sample")
        if self.auto_select_thread:
            args.append("autothread")
        if self.block_main_thread:
            args.append("blockmain")
        if self.trigger_point:
            args.append(f"trigger={self.trigger_point}")
        if self.probe_spec:
            args.append(f"probe={self.probe_spec}")
        if self.stop_on_root_return:
            args.append("rootstop")
        if self.async_thread_handoff:
            args.append("handoff")
        return args


def repo_root_from_file(file_path: str) -> Path:
    return Path(file_path).resolve().parents[2]


def default_agent_path(repo_root: Path) -> Path:
    return repo_root / "bin" / "release" / "VDTraceAgent.dll"


def default_ctl_path(repo_root: Path) -> Path:
    return repo_root / "bin" / "release" / "vdtrace_ctl.exe"


def default_output_path(pid: int) -> str:
    return f".\\traces\\VDTrace-{pid}-{datetime.now().strftime('%Y%m%d-%H%M%S')}.log"


def is_auto_output_path(text: str, pid: int) -> bool:
    path = text.strip().replace("/", "\\")
    if not path:
        return True
    if re.fullmatch(r"^\.\\traces\\VDTrace\.log$", path, flags=re.IGNORECASE) is not None:
        return True
    if re.fullmatch(r"^\.\\traces\\VDTrace-\d+(?:-\d{8}-\d{6})?\.log$", path, flags=re.IGNORECASE) is not None:
        return True
    if pid <= 0:
        return False
    pattern = rf"^\.\\traces\\VDTrace-{pid}(?:-\d{{8}}-\d{{6}})?\.log$"
    return re.fullmatch(pattern, path, flags=re.IGNORECASE) is not None
