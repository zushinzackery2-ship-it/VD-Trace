import argparse
import struct
from pathlib import Path


STREAM_TYPE_THREAD_LIST = 3
STREAM_TYPE_MODULE_LIST = 4
STREAM_TYPE_EXCEPTION = 6
STREAM_TYPE_SYSTEM_INFO = 7


def read_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def read_u64(data, offset):
    return struct.unpack_from("<Q", data, offset)[0]


def read_minidump_string(data, rva):
    if rva == 0:
        return ""
    length = read_u32(data, rva)
    raw = data[rva + 4:rva + 4 + length]
    return raw.decode("utf-16-le", errors="replace")


def parse_directories(data):
    signature = data[:4]
    if signature != b"MDMP":
        raise ValueError("not a minidump")

    stream_count = read_u32(data, 8)
    directory_rva = read_u32(data, 12)
    directories = {}
    for index in range(stream_count):
        entry_offset = directory_rva + index * 12
        stream_type = read_u32(data, entry_offset)
        size = read_u32(data, entry_offset + 4)
        rva = read_u32(data, entry_offset + 8)
        directories[stream_type] = (size, rva)
    return directories


def parse_modules(data, directories):
    modules = []
    if STREAM_TYPE_MODULE_LIST not in directories:
        return modules

    _, rva = directories[STREAM_TYPE_MODULE_LIST]
    count = read_u32(data, rva)
    cursor = rva + 4
    for _ in range(count):
        base = read_u64(data, cursor)
        size = read_u32(data, cursor + 8)
        name_rva = read_u32(data, cursor + 20)
        name = read_minidump_string(data, name_rva)
        modules.append(
            {
                "base": base,
                "end": base + size,
                "size": size,
                "name": name,
            }
        )
        cursor += 108

    modules.sort(key=lambda item: item["base"])
    return modules


def parse_threads(data, directories):
    threads = {}
    if STREAM_TYPE_THREAD_LIST not in directories:
        return threads

    _, rva = directories[STREAM_TYPE_THREAD_LIST]
    count = read_u32(data, rva)
    cursor = rva + 4
    for _ in range(count):
        thread_id = read_u32(data, cursor)
        teb = read_u64(data, cursor + 16)
        stack_start = read_u64(data, cursor + 24)
        stack_size = read_u32(data, cursor + 32)
        stack_rva = read_u32(data, cursor + 36)
        context_size = read_u32(data, cursor + 40)
        context_rva = read_u32(data, cursor + 44)
        threads[thread_id] = {
            "thread_id": thread_id,
            "teb": teb,
            "stack_start": stack_start,
            "stack_size": stack_size,
            "stack_rva": stack_rva,
            "context_size": context_size,
            "context_rva": context_rva,
        }
        cursor += 48
    return threads


def parse_exception(data, directories):
    if STREAM_TYPE_EXCEPTION not in directories:
        return None

    _, rva = directories[STREAM_TYPE_EXCEPTION]
    thread_id = read_u32(data, rva)
    code = read_u32(data, rva + 8)
    flags = read_u32(data, rva + 12)
    address = read_u64(data, rva + 24)
    parameter_count = read_u32(data, rva + 32)
    parameters = []
    params_offset = rva + 40
    for index in range(min(parameter_count, 15)):
        parameters.append(read_u64(data, params_offset + index * 8))
    context_size = read_u32(data, rva + 160)
    context_rva = read_u32(data, rva + 164)
    return {
        "thread_id": thread_id,
        "code": code,
        "flags": flags,
        "address": address,
        "parameters": parameters,
        "context_size": context_size,
        "context_rva": context_rva,
    }


def parse_x64_context(data, context_rva):
    if context_rva == 0:
        return {}

    return {
        "context_flags": read_u32(data, context_rva + 48),
        "eflags": read_u32(data, context_rva + 68),
        "rax": read_u64(data, context_rva + 120),
        "rcx": read_u64(data, context_rva + 128),
        "rdx": read_u64(data, context_rva + 136),
        "rbx": read_u64(data, context_rva + 144),
        "rsp": read_u64(data, context_rva + 152),
        "rbp": read_u64(data, context_rva + 160),
        "rsi": read_u64(data, context_rva + 168),
        "rdi": read_u64(data, context_rva + 176),
        "r8": read_u64(data, context_rva + 184),
        "r9": read_u64(data, context_rva + 192),
        "r10": read_u64(data, context_rva + 200),
        "r11": read_u64(data, context_rva + 208),
        "r12": read_u64(data, context_rva + 216),
        "r13": read_u64(data, context_rva + 224),
        "r14": read_u64(data, context_rva + 232),
        "r15": read_u64(data, context_rva + 240),
        "rip": read_u64(data, context_rva + 248),
    }


def find_module(modules, address):
    for module in modules:
        if module["base"] <= address < module["end"]:
            return module
    return None


def format_address(modules, address):
    module = find_module(modules, address)
    if module is None:
        return f"0x{address:x}"
    return f"{module['name']}+0x{address - module['base']:x}"


def collect_stack_candidates(data, thread, modules, rsp, limit):
    if thread is None or rsp == 0:
        return []

    stack_start = thread["stack_start"]
    stack_end = stack_start + thread["stack_size"]
    if not (stack_start <= rsp < stack_end):
        return []

    offset = rsp - stack_start
    cursor = thread["stack_rva"] + offset
    remaining = thread["stack_rva"] + thread["stack_size"] - cursor
    count = min(limit, remaining // 8)
    results = []
    for index in range(count):
        value = read_u64(data, cursor + index * 8)
        module = find_module(modules, value)
        if module is None:
            continue
        results.append(
            {
                "slot": rsp + index * 8,
                "value": value,
                "text": f"{module['name']}+0x{value - module['base']:x}",
            }
        )
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump_path")
    parser.add_argument("--stack-limit", type=int, default=256)
    args = parser.parse_args()

    data = Path(args.dump_path).read_bytes()
    directories = parse_directories(data)
    modules = parse_modules(data, directories)
    threads = parse_threads(data, directories)
    exception = parse_exception(data, directories)

    print(f"dump={args.dump_path}")
    print(f"module_count={len(modules)} thread_count={len(threads)}")

    interesting = [
        module for module in modules
        if any(token in module["name"].lower() for token in ("qt5webenginecore", "platformprocess", "unityplayer", "vdtrace"))
    ]
    for module in interesting:
        print(
            "module="
            f"{module['name']} "
            f"base=0x{module['base']:x} "
            f"size=0x{module['size']:x}"
        )

    if exception is None:
        print("exception=missing")
        return

    print(
        "exception="
        f"code=0x{exception['code']:08x} "
        f"flags=0x{exception['flags']:08x} "
        f"thread={exception['thread_id']} "
        f"address={format_address(modules, exception['address'])}"
    )
    if exception["parameters"]:
        print(
            "exception.params="
            + ", ".join(f"0x{value:x}" for value in exception["parameters"])
        )

    thread = threads.get(exception["thread_id"])
    context = parse_x64_context(data, exception["context_rva"])
    if context:
        print(
            "context="
            f"rip={format_address(modules, context['rip'])} "
            f"rsp=0x{context['rsp']:x} "
            f"rbp=0x{context['rbp']:x} "
            f"rax=0x{context['rax']:x} "
            f"rbx=0x{context['rbx']:x} "
            f"rcx=0x{context['rcx']:x} "
            f"rdx=0x{context['rdx']:x}"
        )

    if thread is not None:
        print(
            "thread="
            f"id={thread['thread_id']} "
            f"teb=0x{thread['teb']:x} "
            f"stack=0x{thread['stack_start']:x}-0x{thread['stack_start'] + thread['stack_size']:x}"
        )

    stack_candidates = collect_stack_candidates(data, thread, modules, context.get("rsp", 0), args.stack_limit)
    print("stack_candidates:")
    for item in stack_candidates[:80]:
        print(
            f"  [0x{item['slot']:x}] -> 0x{item['value']:x} "
            f"({item['text']})"
        )


if __name__ == "__main__":
    main()
