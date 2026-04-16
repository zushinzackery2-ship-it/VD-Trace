from __future__ import annotations

from dataclasses import dataclass


_MODE_CAPTURE = "capture"
_MODE_STEP = "step"
_MODE_WRITE = "write"
_EXIT_DEFAULTS = {
    _MODE_STEP: "return-or-leave",
    _MODE_WRITE: "return",
}
_EXIT_OPTIONS = ("return", "leave", "return-or-leave")


@dataclass(slots=True)
class ObserverRule:
    mode: str
    hit: str
    content: str = ""
    steps: str = ""
    exit_kind: str = ""


def observer_mode_options() -> tuple[str, ...]:
    return (_MODE_CAPTURE, _MODE_STEP, _MODE_WRITE)


def observer_exit_options() -> tuple[str, ...]:
    return _EXIT_OPTIONS


def normalize_observer_mode(value: str) -> str:
    lowered = value.strip().lower()
    if lowered not in observer_mode_options():
        raise ValueError("观测模式无效。")
    return lowered


def normalize_observer_exit(mode: str, value: str) -> str:
    normalized_mode = normalize_observer_mode(mode)
    if normalized_mode == _MODE_CAPTURE:
        return ""

    lowered = value.strip().lower() or _EXIT_DEFAULTS[normalized_mode]
    if lowered not in _EXIT_OPTIONS:
        raise ValueError("退出条件无效。")
    return lowered


def normalize_observer_steps(mode: str, value: str) -> str:
    normalized_mode = normalize_observer_mode(mode)
    if normalized_mode == _MODE_CAPTURE:
        return ""

    text = value.strip() or "256"
    try:
        parsed = int(text, 0)
    except ValueError as exc:
        raise ValueError("步数必须是正整数。") from exc
    if parsed <= 0:
        raise ValueError("步数必须是正整数。")
    return str(parsed)


def parse_observer_rules(text: str) -> list[ObserverRule]:
    rules: list[ObserverRule] = []
    for raw_rule in [item.strip() for item in text.split(";") if item.strip()]:
        lowered = raw_rule.lower()
        if lowered.startswith("step@") or lowered.startswith("write@"):
            rules.append(_parse_local_rule(raw_rule))
            continue
        rules.append(_parse_capture_rule(raw_rule))
    return rules


def serialize_observer_rules(rules: list[ObserverRule]) -> str:
    rendered: list[str] = []
    for rule in rules:
        mode = normalize_observer_mode(rule.mode)
        hit = rule.hit.strip()
        if not hit:
            continue
        if mode == _MODE_CAPTURE:
            content = rule.content.strip()
            if content:
                rendered.append(f"{hit}->{content}")
            continue
        steps = normalize_observer_steps(mode, rule.steps)
        exit_kind = normalize_observer_exit(mode, rule.exit_kind)
        if mode == _MODE_STEP:
            rendered.append(f"step@{hit} steps={steps} exit={exit_kind}")
            continue
        content = rule.content.strip()
        if content:
            rendered.append(f"write@{hit} watch={content} steps={steps} exit={exit_kind}")
    return "; ".join(rendered)


def build_observer_summary(enabled: bool, text: str) -> str:
    if not enabled:
        return "观测器未启用。"
    try:
        rules = parse_observer_rules(text)
    except ValueError as exc:
        return f"观测器规则有误：{exc}"
    if not rules:
        return "观测器已启用，但还没有规则。"
    capture_count = sum(1 for rule in rules if rule.mode == _MODE_CAPTURE)
    step_count = sum(1 for rule in rules if rule.mode == _MODE_STEP)
    write_count = sum(1 for rule in rules if rule.mode == _MODE_WRITE)
    parts = [f"共 {len(rules)} 条"]
    if capture_count:
        parts.append(f"capture {capture_count}")
    if step_count:
        parts.append(f"step {step_count}")
    if write_count:
        parts.append(f"write {write_count}")
    return " | ".join(parts)


def observer_rule_columns(rule: ObserverRule) -> tuple[str, str, str, str, str]:
    return (
        normalize_observer_mode(rule.mode),
        rule.hit.strip(),
        rule.content.strip() or "-",
        rule.steps.strip() or "-",
        rule.exit_kind.strip() or "-",
    )


def _parse_capture_rule(text: str) -> ObserverRule:
    if "->" not in text:
        raise ValueError("capture 规则缺少 '->'。")
    hit, content = [item.strip() for item in text.split("->", 1)]
    if not hit or not content:
        raise ValueError("capture 规则的命中点或内容为空。")
    return ObserverRule(mode=_MODE_CAPTURE, hit=hit, content=content)


def _parse_local_rule(text: str) -> ObserverRule:
    tokens = text.split()
    if not tokens or "@" not in tokens[0]:
        raise ValueError("局部观测规则语法无效。")

    head = tokens[0]
    mode = _MODE_STEP if head.lower().startswith("step@") else _MODE_WRITE
    hit = head[head.find("@") + 1 :].strip()
    if not hit:
        raise ValueError("局部观测规则缺少命中点。")

    content = ""
    steps = ""
    exit_kind = ""
    for token in tokens[1:]:
        if "=" not in token:
            raise ValueError("局部观测规则参数必须使用 key=value。")
        key, value = [item.strip() for item in token.split("=", 1)]
        lowered_key = key.lower()
        if lowered_key == "watch":
            content = value
            continue
        if lowered_key == "steps":
            steps = value
            continue
        if lowered_key == "exit":
            exit_kind = value
            continue
        raise ValueError("存在未知的局部观测参数。")

    if mode == _MODE_WRITE and not content.strip():
        raise ValueError("write 规则缺少 watch。")

    return ObserverRule(
        mode=mode,
        hit=hit,
        content=content,
        steps=normalize_observer_steps(mode, steps),
        exit_kind=normalize_observer_exit(mode, exit_kind),
    )
