"""Write and read Gothic I VDFS archives (.vdf / .mod) without GothicVDFS.exe.

Layout (ZenGin VDFS, version 0x50):
  header (296 bytes): comment[256] padded with 0x1A, signature[16]
      "PSVDSC_V2.00\\r\\n\\r\\n", u32 entry_count, u32 file_count,
      u32 DOS timestamp, u32 data_size, u32 catalog_offset (296), u32 version
  catalog: entry_count entries of 80 bytes: name[64] (upper case, space
      padded), u32 offset (file: absolute data offset; directory: index of
      its first child entry), u32 size, u32 type (0x80000000 directory,
      0x40000000 last entry of its directory block), u32 attributes
  data: file contents
Children of one directory form one contiguous block; blocks are laid out
breadth-first, the root block first, each block sorted by name.
"""
from __future__ import annotations

import datetime as dt
import struct
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

SIGNATURE_G1 = b"PSVDSC_V2.00\r\n\r\n"
SIGNATURE_G2 = b"PSVDSC_V2.00\n\r\n\r"  # Union.vdf, SystemPack.vdf; read only
HEADER_SIZE = 296
ENTRY_SIZE = 80
NAME_SIZE = 64
VERSION = 0x50
TYPE_DIRECTORY = 0x80000000
TYPE_LAST = 0x40000000
# Original Piranha Bytes G1 archives (textures_patch.VDF) use 0x2 for every entry.
ATTRIBUTE_G1 = 0x2


class VdfError(ValueError):
    pass


@dataclass
class _Node:
    name: str
    children: dict[str, "_Node"] = field(default_factory=dict)
    data: bytes | None = None

    @property
    def is_dir(self) -> bool:
        return self.data is None


def _dos_time(moment: dt.datetime) -> int:
    year = min(max(moment.year, 1980), 2107)
    date = ((year - 1980) << 9) | (moment.month << 5) | moment.day
    time = (moment.hour << 11) | (moment.minute << 5) | (moment.second // 2)
    return (date << 16) | time


def _virtual_parts(path: str) -> list[str]:
    parts = [part for part in PurePosixPath(path.replace("\\", "/")).parts]
    if not parts or any(part in ("", ".", "..", "/") or ":" in part for part in parts):
        raise VdfError(f"Unsafe virtual path: {path!r}")
    upper = [part.upper() for part in parts]
    for part in upper:
        try:
            encoded = part.encode("ascii")
        except UnicodeEncodeError as exc:
            raise VdfError(f"Non-ASCII virtual path: {path!r}") from exc
        if len(encoded) > NAME_SIZE:
            raise VdfError(f"Path component longer than {NAME_SIZE} bytes: {part}")
    return upper


def build_vdf(files: dict[str, bytes], *, comment: str = "",
              timestamp: dt.datetime | None = None) -> bytes:
    """Return a complete archive for {virtual path: content}."""
    if not files:
        raise VdfError("An archive needs at least one file")
    root = _Node("")
    for path, data in files.items():
        node = root
        *folders, leaf = _virtual_parts(path)
        for folder in folders:
            child = node.children.setdefault(folder, _Node(folder))
            if not child.is_dir:
                raise VdfError(f"{folder} is both a file and a directory")
            node = child
        if leaf in node.children:
            raise VdfError(f"Duplicate virtual path: {path}")
        node.children[leaf] = _Node(leaf, data=bytes(data))

    # Breadth-first catalog: each directory's children are one sorted block.
    entries: list[list] = []  # [node, offset_or_child_index, flags]
    def append_block(node: _Node) -> int:
        start = len(entries)
        names = sorted(node.children)
        for index, name in enumerate(names):
            child = node.children[name]
            flags = (TYPE_DIRECTORY if child.is_dir else 0) | (TYPE_LAST if index == len(names) - 1 else 0)
            entries.append([child, 0, flags])
        return start

    append_block(root)
    cursor = 0
    while cursor < len(entries):
        node = entries[cursor][0]
        if node.is_dir:
            if not node.children:
                raise VdfError(f"Empty directory: {node.name}")
            entries[cursor][1] = append_block(node)
        cursor += 1

    catalog_size = len(entries) * ENTRY_SIZE
    data_offset = HEADER_SIZE + catalog_size
    blobs: list[bytes] = []
    for entry in entries:
        node = entry[0]
        if not node.is_dir:
            entry[1] = data_offset
            blobs.append(node.data)
            data_offset += len(node.data)
    data_size = sum(len(blob) for blob in blobs)
    file_count = len(blobs)

    encoded_comment = comment.encode("ascii", "replace")[:NAME_SIZE * 4]
    header = encoded_comment.ljust(256, b"\x1a") + SIGNATURE_G1 + struct.pack(
        "<6I", len(entries), file_count, _dos_time(timestamp or dt.datetime.now()),
        data_size, HEADER_SIZE, VERSION)
    catalog = bytearray()
    for node, offset, flags in entries:
        size = 0 if node.is_dir else len(node.data)
        catalog += node.name.encode("ascii").ljust(NAME_SIZE, b" ")
        catalog += struct.pack("<4I", offset, size, flags, ATTRIBUTE_G1)
    return header + bytes(catalog) + b"".join(blobs)


def read_vdf(data: bytes) -> dict[str, bytes]:
    """Parse an archive into {VIRTUAL/PATH: content}, validating its structure."""
    if len(data) < HEADER_SIZE or data[256:272] not in (SIGNATURE_G1, SIGNATURE_G2):
        raise VdfError("Not a Gothic VDFS archive")
    count, file_count, _, data_size, catalog_offset, version = struct.unpack_from("<6I", data, 272)
    if version != VERSION or catalog_offset != HEADER_SIZE:
        raise VdfError("Unsupported VDFS header")
    if catalog_offset + count * ENTRY_SIZE > len(data):
        raise VdfError("Truncated VDFS catalog")
    raw = []
    for index in range(count):
        base = catalog_offset + index * ENTRY_SIZE
        name = data[base:base + NAME_SIZE].rstrip(b" \0").decode("ascii")
        offset, size, flags, _ = struct.unpack_from("<4I", data, base + NAME_SIZE)
        raw.append((name, offset, size, flags))
    result: dict[str, bytes] = {}
    visited: set[int] = set()

    def walk(start: int, prefix: str) -> None:
        index = start
        while True:
            if index >= count or index in visited:
                raise VdfError("Corrupt VDFS directory block")
            visited.add(index)
            name, offset, size, flags = raw[index]
            path = f"{prefix}{name}"
            if flags & TYPE_DIRECTORY:
                walk(offset, path + "/")
            else:
                if offset + size > len(data):
                    raise VdfError(f"File data outside archive: {path}")
                result[path] = data[offset:offset + size]
            if flags & TYPE_LAST:
                return
            index += 1

    walk(0, "")
    if len(visited) != count or len(result) != file_count:
        raise VdfError("VDFS catalog counts do not match its entries")
    if sum(len(blob) for blob in result.values()) != data_size:
        raise VdfError("VDFS data size does not match its files")
    return result


def write_vdf_from_stage(stage: Path, manifest: list[str], output: Path, *, comment: str) -> Path:
    """Pack the manifest's files (relative to stage) and verify by reading back."""
    files = {item: (Path(stage) / item).read_bytes() for item in manifest}
    archive = build_vdf(files, comment=comment)
    back = read_vdf(archive)
    expected = {"/".join(_virtual_parts(path)): data for path, data in files.items()}
    if back != expected:
        raise VdfError("VDFS read-back does not match the packed files")
    Path(output).write_bytes(archive)
    return Path(output)
