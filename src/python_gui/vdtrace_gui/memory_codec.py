from __future__ import annotations

import re
import struct


def memory_write_mode_options() -> tuple[str, ...]:
    return ("HEX", "TEXT", "UTF16", "U32", "U64")


def normalize_memory_write_mode(text: str) -> str:
    lowered = text.strip().lower()
    if lowered == "text":
        return "TEXT"
    if lowered == "utf16":
        return "UTF16"
    if lowered == "u32":
        return "U32"
    if lowered == "u64":
        return "U64"
    return "HEX"


def parse_memory_size(text: str) -> int:
    stripped = text.strip() or "64"
    try:
        size = int(stripped, 0)
    except ValueError as exc:
        raise ValueError("读取长度必须是 1 到 512 之间的整数。") from exc
    if size <= 0 or size > 512:
        raise ValueError("读取长度必须是 1 到 512 之间的整数。")
    return size


def encode_memory_write_bytes(mode: str, text: str) -> bytes:
    normalized = normalize_memory_write_mode(mode)
    raw = text.strip()
    if normalized == "HEX":
        compact = re.sub(r"[\s,\-]", "", raw)
        if not compact or (len(compact) % 2) != 0 or re.fullmatch(r"[0-9a-fA-F]+", compact) is None:
            raise ValueError("HEX 写入需要偶数个十六进制字符。")
        data = bytes.fromhex(compact)
    elif normalized == "TEXT":
        data = raw.encode("utf-8")
    elif normalized == "UTF16":
        data = raw.encode("utf-16le")
    elif normalized == "U32":
        try:
            value = int(raw, 0)
        except ValueError as exc:
            raise ValueError("U32 写入值无效。") from exc
        if value < 0 or value > 0xFFFFFFFF:
            raise ValueError("U32 写入值必须在 0 到 0xFFFFFFFF 之间。")
        data = struct.pack("<I", value)
    else:
        try:
            value = int(raw, 0)
        except ValueError as exc:
            raise ValueError("U64 写入值无效。") from exc
        if value < 0 or value > 0xFFFFFFFFFFFFFFFF:
            raise ValueError("U64 写入值必须在 0 到 0xFFFFFFFFFFFFFFFF 之间。")
        data = struct.pack("<Q", value)

    if not data:
        raise ValueError("写入内容不能为空。")
    if len(data) > 512:
        raise ValueError("写入内容不能超过 512 字节。")
    return data
