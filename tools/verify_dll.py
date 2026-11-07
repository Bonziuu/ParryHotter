"""Verify a built Parry Hotter Union plugin before it is installed or packaged.

Checks, without Windows tools, that the file is a 32-bit (i386, PE32) DLL
that Gothic I can load and that it carries the current build marker.
Prints its SHA-256 and the DLLs it imports, so an unexpected runtime
dependency (for example vcruntime140.dll) is visible before a game test.

    python -m tools.verify_dll .parryhotter/build/ParryHotterHUD-G1.dll
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

BUILD_MARKER = b"PH_BUILD_013_TEN_SLOTS_PROJECTILE_INPUT_CAST_ANIM"
IMAGE_FILE_MACHINE_I386 = 0x014C
IMAGE_FILE_DLL = 0x2000
PE32_MAGIC = 0x010B
IMPORT_DIRECTORY = 1


class DllError(ValueError):
    """The file is not a loadable, current Gothic I plugin."""


@dataclass(frozen=True)
class DllReport:
    sha256: str
    size: int
    imports: tuple[str, ...]


def _sections(data: bytes, offset: int, count: int) -> list[tuple[int, int, int, int]]:
    sections = []
    for index in range(count):
        start = offset + index * 40
        if start + 40 > len(data):
            raise DllError("Truncated PE section table")
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, start + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer, raw_size))
    return sections


def _file_offset(sections, rva: int) -> int:
    for virtual_address, span, raw_pointer, raw_size in sections:
        if virtual_address <= rva < virtual_address + span and rva - virtual_address < raw_size:
            return raw_pointer + rva - virtual_address
    raise DllError(f"RVA 0x{rva:x} is outside every section")


def _c_string(data: bytes, offset: int) -> str:
    end = data.find(b"\0", offset)
    if end < 0:
        raise DllError("Unterminated import name")
    return data[offset:end].decode("ascii", "replace")


def inspect_dll(data: bytes, marker: bytes = BUILD_MARKER) -> DllReport:
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise DllError("Missing MZ header")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise DllError("Missing PE signature")
    machine, section_count = struct.unpack_from("<HH", data, pe + 4)
    optional_size, characteristics = struct.unpack_from("<HH", data, pe + 20)
    if machine != IMAGE_FILE_MACHINE_I386:
        raise DllError(f"Machine 0x{machine:04x} is not i386; Gothic I needs a Win32 build")
    if not characteristics & IMAGE_FILE_DLL:
        raise DllError("PE file is not a DLL")
    optional = pe + 24
    if optional + 96 > len(data) or struct.unpack_from("<H", data, optional)[0] != PE32_MAGIC:
        raise DllError("Optional header is not PE32")
    directory_count = struct.unpack_from("<I", data, optional + 92)[0]
    sections = _sections(data, optional + optional_size, section_count)
    imports: list[str] = []
    if directory_count > IMPORT_DIRECTORY:
        rva, size = struct.unpack_from("<II", data, optional + 96 + IMPORT_DIRECTORY * 8)
        if rva and size:
            descriptor = _file_offset(sections, rva)
            while True:
                if descriptor + 20 > len(data):
                    raise DllError("Truncated import directory")
                fields = struct.unpack_from("<IIIII", data, descriptor)
                if not any(fields):
                    break
                imports.append(_c_string(data, _file_offset(sections, fields[3])))
                descriptor += 20
    if marker not in data:
        raise DllError(f"Build marker {marker.decode()} is missing: this is an old or foreign DLL")
    return DllReport(hashlib.sha256(data).hexdigest(), len(data), tuple(imports))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dll", type=Path)
    args = parser.parse_args(argv)
    try:
        report = inspect_dll(args.dll.read_bytes())
    except (OSError, DllError) as error:
        print(f"FAIL: {args.dll}: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {args.dll} is a PE32 i386 DLL with {BUILD_MARKER.decode()}")
    print(f"SHA-256: {report.sha256}")
    print(f"Size: {report.size} bytes")
    print("Imports: " + (", ".join(report.imports) or "(none)"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
