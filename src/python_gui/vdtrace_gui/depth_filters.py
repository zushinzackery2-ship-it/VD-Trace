from __future__ import annotations

from dataclasses import dataclass


def execution_mode_options() -> tuple[str, ...]:
    return ("EDGE", "TF")


def normalize_execution_mode(text: str) -> str:
    lowered = text.strip().lower()
    if lowered == "tf":
        return "TF"
    if lowered in ("", "edge"):
        return "EDGE"
    raise ValueError("执行模式只支持 EDGE 或 TF。")


def runtime_execution_mode_text(text: str) -> str:
    return normalize_execution_mode(text).lower()


@dataclass(slots=True)
class ModuleDepthRule:
    module_name: str
    ui_depth: str
    execution_mode: str = "EDGE"


def normalize_ui_depth_value(text: str) -> str:
    value = int(text.strip() or "0")
    if value < 0:
        raise ValueError("追踪层级必须是非负整数。")
    return str(value)


def ui_depth_to_runtime_text(text: str) -> str:
    value = int(normalize_ui_depth_value(text))
    if value == 0:
        return "all"
    if value == 1:
        return "single"
    return str(value - 1)


def parse_module_depth_rules(text: str) -> list[ModuleDepthRule]:
    rules: list[ModuleDepthRule] = []
    seen_modules: set[str] = set()
    for raw_token in [item.strip() for item in text.replace(";", ",").split(",")]:
        if not raw_token:
            continue
        parts = [item.strip() for item in raw_token.split(":")]
        if len(parts) not in (2, 3):
            raise ValueError("模块过滤规则格式无效，使用 模块名:层级[:模式]。")
        module_name = parts[0]
        if not module_name:
            raise ValueError("模块过滤规则缺少模块名。")
        lowered = module_name.lower()
        if lowered in seen_modules:
            raise ValueError(f"模块过滤规则重复：{module_name}")
        seen_modules.add(lowered)
        rules.append(
            ModuleDepthRule(
                module_name=module_name,
                ui_depth=normalize_ui_depth_value(parts[1]),
                execution_mode=normalize_execution_mode(parts[2] if len(parts) == 3 else "EDGE"),
            )
        )
    return rules


def serialize_module_depth_rules(rules: list[ModuleDepthRule]) -> str:
    tokens: list[str] = []
    for rule in rules:
        tokens.append(
            f"{rule.module_name}:{normalize_ui_depth_value(rule.ui_depth)}:{normalize_execution_mode(rule.execution_mode)}"
        )
    return ",".join(tokens)


def format_ui_depth_label(text: str) -> str:
    value = int(normalize_ui_depth_value(text))
    if value == 0:
        return "不限(ALL)"
    if value == 1:
        return "同层(SINGLE)"
    return f"向下{value - 1}层"


def format_rule_label(ui_depth: str, execution_mode: str) -> str:
    return f"{format_ui_depth_label(ui_depth)} / {normalize_execution_mode(execution_mode)}"


def build_depth_filter_spec(
    outside_enabled: bool,
    outside_ui_depth: str,
    outside_execution_mode: str,
    anonymous_enabled: bool,
    anonymous_ui_depth: str,
    anonymous_execution_mode: str,
    module_rules_text: str,
) -> str:
    tokens: list[str] = []
    if outside_enabled:
        tokens.append(
            f"outside={ui_depth_to_runtime_text(outside_ui_depth)}:{runtime_execution_mode_text(outside_execution_mode)}"
        )
    if anonymous_enabled:
        tokens.append(
            f"anon={ui_depth_to_runtime_text(anonymous_ui_depth)}:{runtime_execution_mode_text(anonymous_execution_mode)}"
        )
    for rule in parse_module_depth_rules(module_rules_text):
        tokens.append(
            "module="
            f"{rule.module_name}:{ui_depth_to_runtime_text(rule.ui_depth)}:{runtime_execution_mode_text(rule.execution_mode)}"
        )
    return ",".join(tokens)


def build_depth_filter_summary(
    default_ui_depth: str,
    outside_enabled: bool,
    outside_ui_depth: str,
    outside_execution_mode: str,
    anonymous_enabled: bool,
    anonymous_ui_depth: str,
    anonymous_execution_mode: str,
    module_rules_text: str,
    idle_escape_enabled: bool,
    idle_escape_threshold: str,
) -> str:
    parts = [f"默认={format_ui_depth_label(default_ui_depth)}"]
    if outside_enabled:
        parts.append(f"模块外={format_rule_label(outside_ui_depth, outside_execution_mode)}")
    if anonymous_enabled:
        parts.append(f"匿名页={format_rule_label(anonymous_ui_depth, anonymous_execution_mode)}")
    module_rules = parse_module_depth_rules(module_rules_text)
    if module_rules:
        tf_count = sum(1 for rule in module_rules if normalize_execution_mode(rule.execution_mode) == "TF")
        if tf_count == 0:
            parts.append(f"模块规则={len(module_rules)}条")
        else:
            parts.append(f"模块规则={len(module_rules)}条/{tf_count}个TF")
    parts.append(f"空转跳出={idle_escape_threshold.strip() or '32'}" if idle_escape_enabled else "空转跳出=关闭")
    return " | ".join(parts)
