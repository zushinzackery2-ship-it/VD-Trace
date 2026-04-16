import argparse
from pathlib import Path

import capstone
import pefile


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Quick PE RVA disassembler")
    parser.add_argument("image", type=Path, help="PE image path")
    parser.add_argument("--rva", type=lambda x: int(x, 0), help="RVA to disassemble from")
    parser.add_argument("--count", type=int, default=48, help="Maximum instruction count")
    parser.add_argument("--bytes", type=lambda x: int(x, 0), default=0x100, dest="size", help="Maximum byte window")
    parser.add_argument("--entry", action="store_true", help="Use image entry RVA")
    parser.add_argument("--export-ordinal", type=int, help="Use export ordinal")
    parser.add_argument("--exports", action="store_true", help="List exports")
    parser.add_argument("--imports", action="store_true", help="List imports")
    return parser


def find_export_rva(pe: pefile.PE, ordinal: int) -> int | None:
    if not hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        return None
    for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if symbol.ordinal == ordinal:
            return symbol.address
    return None


def list_exports(pe: pefile.PE) -> None:
    if not hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        print("no exports")
        return
    for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = symbol.name.decode("utf-8", errors="ignore") if symbol.name else "<NONAME>"
        print(f"ordinal={symbol.ordinal} rva=0x{symbol.address:x} name={name}")


def list_imports(pe: pefile.PE) -> None:
    if not hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
        print("no imports")
        return
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        dll_name = entry.dll.decode("utf-8", errors="ignore")
        print(f"[{dll_name}]")
        for imp in entry.imports:
            if imp.name is not None:
                name = imp.name.decode("utf-8", errors="ignore")
            else:
                name = f"ordinal_{imp.ordinal}"
            print(f"  0x{imp.address:x} {name}")


def disasm(pe: pefile.PE, image_path: Path, rva: int, size: int, count: int) -> None:
    file_offset = pe.get_offset_from_rva(rva)
    image_base = pe.OPTIONAL_HEADER.ImageBase
    data = image_path.read_bytes()
    window = data[file_offset:file_offset + size]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = False
    print(f"image={image_path}")
    print(f"image_base=0x{image_base:x}")
    print(f"rva=0x{rva:x}")
    print(f"va=0x{image_base + rva:x}")
    print(f"file_offset=0x{file_offset:x}")
    for index, insn in enumerate(md.disasm(window, image_base + rva)):
        bytes_text = insn.bytes.hex(" ")
        print(f"0x{insn.address:x}: {bytes_text:<32} {insn.mnemonic} {insn.op_str}".rstrip())
        if index + 1 >= count:
            break


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    pe = pefile.PE(str(args.image), fast_load=False)
    pe.parse_data_directories()

    if args.exports:
        list_exports(pe)
        return 0

    if args.imports:
        list_imports(pe)
        return 0

    rva = args.rva
    if args.entry:
        rva = pe.OPTIONAL_HEADER.AddressOfEntryPoint
    if args.export_ordinal is not None:
        rva = find_export_rva(pe, args.export_ordinal)
    if rva is None:
        raise SystemExit("missing target rva")

    disasm(pe, args.image, rva, args.size, args.count)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
