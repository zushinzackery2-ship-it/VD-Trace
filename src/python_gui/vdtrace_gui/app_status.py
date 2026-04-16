from __future__ import annotations


def format_trace_status_text(message: str) -> str:
    if not message.strip():
        return "未拿到追踪状态。"

    fields: dict[str, str] = {}
    for token in message.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if key and value:
            fields[key] = value

    running = fields.get("running", "0") == "1"
    parts = ["运行中" if running else "待机"]
    backend = fields.get("backend", "")
    if backend:
        parts.append(f"后端={backend.upper()}")
    auto_thread = fields.get("auto_thread", "")
    if auto_thread == "1":
        parts.append("线程=自动")
    elif auto_thread == "0":
        parts.append("线程=手动")
    focus = fields.get("focus", "")
    if focus == "queue":
        parts.append("线程模式=轮转")
    elif focus == "single":
        parts.append("线程模式=单线程")
    event_mode = fields.get("event_mode", "")
    if event_mode == "flow":
        parts.append("记录=控制流")
    elif event_mode == "full":
        parts.append("记录=全指令")
    active_thread = fields.get("active_thread", "")
    if active_thread and active_thread != "0":
        parts.append(f"活动线程={active_thread}")
    capture = fields.get("capture", "")
    if capture:
        parts.append(f"捕获={capture}")
    capture_hits = fields.get("capture_hits", "")
    if capture_hits:
        parts.append(f"触发命中={capture_hits}")
    capture_last = fields.get("capture_last", "")
    if capture_last and capture_last != "0":
        parts.append(f"最近命中线程={capture_last}")
    depth = fields.get("depth", "")
    if depth:
        parts.append(f"当前深度={depth}")
    scope = fields.get("scope", "")
    if scope == "tracked":
        parts.append("范围=仅指定模块")
    elif scope == "all":
        parts.append("范围=含模块外")
    region = fields.get("region", "")
    if region == "module":
        parts.append("当前区域=模块内")
    elif region == "outside":
        parts.append("当前区域=模块外")
    elif region == "anonymous":
        parts.append("当前区域=匿名区")
    observe = fields.get("observe", "")
    if observe == "idle":
        parts.append("观测=待命")
    elif observe == "dest":
        parts.append("观测=等目标")
    elif observe == "tail":
        parts.append("观测=等块尾")
    elif observe == "single-step":
        parts.append("观测=单步")
    elif observe == "linear-scan":
        parts.append("观测=线扫")
    elif observe == "hot-bypass":
        parts.append("观测=空转跳出")
    steps = fields.get("steps", "")
    if steps:
        parts.append(f"步数={steps}")
    events = fields.get("events", "")
    if events:
        parts.append(f"事件={events}")
    written_events = fields.get("written_events", "")
    if written_events:
        parts.append(f"已写事件={written_events}")
    if events and written_events:
        try:
            lag = int(events) - int(written_events)
        except ValueError:
            lag = 0
        if lag != 0:
            parts.append(f"落盘差={lag}")
    writing = fields.get("writing", "")
    if writing == "1":
        parts.append("写入=TRUE")
    elif writing == "0":
        parts.append("写入=FALSE")
    pending_events = fields.get("pending_events", "")
    if pending_events and pending_events != "0":
        parts.append(f"待写事件={pending_events}")
    pending_write_events = fields.get("pending_write_events", "")
    if pending_write_events and pending_write_events != "0":
        parts.append(f"待写事件(文件)={pending_write_events}")
    pending_write_bytes = fields.get("pending_write_bytes", "")
    if pending_write_bytes and pending_write_bytes != "0":
        parts.append(f"待写字节={pending_write_bytes}")
    dropped_events_total = fields.get("dropped_events_total", "")
    if dropped_events_total and dropped_events_total != "0":
        parts.append(f"已丢事件={dropped_events_total}")
    dropped_write_events = fields.get("dropped_write_events", "")
    if dropped_write_events and dropped_write_events != "0":
        parts.append(f"已丢事件(文件)={dropped_write_events}")
    accounted_events = fields.get("accounted_events", "")
    if accounted_events:
        parts.append(f"已核对={accounted_events}")
    event_gap = fields.get("event_gap", "")
    if event_gap and event_gap != "0":
        parts.append(f"计数差={event_gap}")
    call_limit = fields.get("call_limit", "")
    if call_limit:
        parts.append(f"层级上限={call_limit}")
    root_stop = fields.get("root_stop", "")
    if root_stop == "1":
        parts.append("停止=根返回")
    elif root_stop == "0":
        parts.append("停止=手动")
    module_rule_count = fields.get("depth_module_rules", "")
    if module_rule_count and module_rule_count != "0":
        parts.append(f"模块规则={module_rule_count}")
    outside_limit = fields.get("depth_outside", "")
    if outside_limit:
        parts.append(f"模块外层级={outside_limit}")
    anonymous_limit = fields.get("depth_anon", "")
    if anonymous_limit:
        parts.append(f"匿名页层级={anonymous_limit}")
    probes = fields.get("probes", "")
    if probes:
        parts.append(f"观测器={probes}")
    hits = fields.get("hits", "")
    if hits:
        parts.append(f"命中策略={hits}")
    idle_escape = fields.get("idle_escape", "")
    if idle_escape:
        parts.append(f"空转跳出={idle_escape}")
    hot_streak = fields.get("hot_streak", "")
    if hot_streak and hot_streak != "0":
        parts.append(f"热重复={hot_streak}")
    dup_suppressed = fields.get("dup_suppressed", "")
    if dup_suppressed and dup_suppressed != "0":
        parts.append(f"重复边压制={dup_suppressed}")
    outside_suppressed = fields.get("outside_suppressed", "")
    if outside_suppressed and outside_suppressed != "0":
        parts.append(f"范围外静默={outside_suppressed}")
    hot_bypass_count = fields.get("hot_bypass_count", "")
    if hot_bypass_count and hot_bypass_count != "0":
        parts.append(f"空转跳出次数={hot_bypass_count}")
    if scope == "tracked" and outside_limit:
        parts.append("模块外记录=关")
    if hits == "first" and event_mode == "flow" and root_stop == "0":
        parts.append("会话=有限边集")
    trigger = fields.get("trigger", "")
    if trigger:
        parts.append(f"触发={trigger}")
    return " | ".join(parts)
