#!/usr/bin/env python3
"""Generate a Gothic I-compatible low-poly wand as 3DS + TGA using stdlib.

If the original HP2 wand OBJ was legally acquired for private tests, preserve
its geometry and UVs. Otherwise build an original tapered wooden wand. This
code never commits, uploads or distributes the commercial model.

Output: PH_WAND.3DS, PH_WAND.TGA in user's _work/Data/Meshes and Textures.
"""
from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

NAME = "PH_WAND"
TEXTURE = "PH_WAND.TGA"
MAX_VERTICES = 60000
MAX_FACES = 60000


class WandError(RuntimeError):
    pass


def chunk(identifier: int, payload: bytes) -> bytes:
    return struct.pack("<HI", identifier, 6 + len(payload)) + payload


def cstr(value: str) -> bytes:
    return value.encode("ascii") + b"\x00"


def read_obj(path: Path) -> tuple[list[tuple[float, float, float]],
                                  list[tuple[float, float]],
                                  list[tuple[int, int, int]]]:
    if path.stat().st_size > 4 * 1024 * 1024:
        raise WandError("Original wand OBJ is unexpectedly large")
    src: list[tuple[float, float, float]] = []
    uv: list[tuple[float, float]] = []
    verts: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []
    faces: list[tuple[int, int, int]] = []
    mapping: dict[tuple[int, int], int] = {}
    for line in path.read_text("utf-8", errors="replace").splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v" and len(parts) >= 4:
            src.append(tuple(float(n) for n in parts[1:4]))
        elif parts[0] == "vt" and len(parts) >= 3:
            uv.append((float(parts[1]), float(parts[2])))
        elif parts[0] == "f" and len(parts) >= 4:
            polygon: list[int] = []
            for token in parts[1:]:
                values = token.split("/")
                v = int(values[0])
                t = int(values[1]) if len(values) > 1 and values[1] else 0
                vid = v - 1 if v > 0 else len(src) + v
                uid = t - 1 if t > 0 else len(uv) + t if t < 0 else -1
                if not (0 <= vid < len(src)):
                    raise WandError("Wand OBJ has invalid vertex index")
                if uid != -1 and not (0 <= uid < len(uv)):
                    raise WandError("Wand OBJ has invalid UV index")
                key = (vid, uid)
                if key not in mapping:
                    mapping[key] = len(verts)
                    verts.append(src[vid])
                    uvs.append(uv[uid] if uid >= 0 else (0.0, 0.0))
                polygon.append(mapping[key])
            for i in range(1, len(polygon) - 1):
                faces.append((polygon[0], polygon[i], polygon[i + 1]))
        if len(verts) > MAX_VERTICES or len(faces) > MAX_FACES:
            raise WandError("Wand OBJ exceeds 3DS limits")
    if len(verts) < 3 or not faces:
        raise WandError("Wand OBJ has no usable triangles")

    # Align the longest axis to Gothic socket Z; retain silhouette.
    extents = [max(p[a] for p in verts) - min(p[a] for p in verts) for a in range(3)]
    main = extents.index(max(extents))
    if extents[main] <= 1e-7:
        raise WandError("Wand OBJ geometry is degenerate")
    lateral = [axis for axis in range(3) if axis != main]
    means = [sum(p[a] for p in verts) / len(verts) for a in range(3)]
    bottom = min(p[main] for p in verts)
    scale = 34.0 / extents[main]
    aligned = [
        (
            (p[lateral[0]] - means[lateral[0]]) * scale,
            (p[lateral[1]] - means[lateral[1]]) * scale,
            (p[main] - bottom) * scale - 5.0,
        ) for p in verts
    ]
    # Permuting (x, y, z) -> (x, z, y) is a reflection; reverse face order
    # to retain outward-facing normals and avoid an invisible backface-culled
    # wand when the original OBJ's longest axis happens to be Y.
    if main == 1:
        faces = [(a, c, b) for a, b, c in faces]
    return aligned, uvs, faces


def original_wand() -> tuple[list[tuple[float, float, float]],
                            list[tuple[float, float]],
                            list[tuple[int, int, int]]]:
    """Create an original tapered wand (not copied from the HP2 asset)."""
    n = 10
    rings = [
        (-6.0, 0.55), (0.0, 0.67), (3.0, 0.76),
        (7.0, 0.55), (18.0, 0.36), (30.0, 0.14), (34.0, 0.04)
    ]
    vertices = []
    uv = []
    for i, (height, radius) in enumerate(rings):
        for side in range(n):
            angle = 2.0 * math.pi * side / n
            vertices.append((radius * math.cos(angle),
                             radius * math.sin(angle), height))
            uv.append((side / (n - 1), i / (len(rings) - 1)))
    faces = []
    for ring in range(len(rings) - 1):
        for side in range(n):
            a = ring * n + side
            b = ring * n + (side + 1) % n
            c = (ring + 1) * n + (side + 1) % n
            d = (ring + 1) * n + side
            faces.extend(((a, b, c), (a, c, d)))
    return vertices, uv, faces


def write_3ds(path: Path, vertices, uvs, faces) -> None:
    if not (3 <= len(vertices) <= MAX_VERTICES and 1 <= len(faces) <= MAX_FACES):
        raise WandError("Invalid 3DS mesh size")
    if len(vertices) != len(uvs):
        raise WandError("UV and vertex counts differ")
    material = chunk(0xAFFF,
        chunk(0xA000, cstr("PH_WAND_WOOD"))
        + chunk(0xA020, chunk(0x0011, bytes((127, 77, 41))))
        + chunk(0xA200, chunk(0xA300, cstr(TEXTURE)))
    )
    vertex_block = chunk(
        0x4110, struct.pack("<H", len(vertices)) +
        b"".join(struct.pack("<fff", *p) for p in vertices)
    )
    group = chunk(0x4130, cstr("PH_WAND_WOOD") +
                  struct.pack("<H", len(faces)) +
                  b"".join(struct.pack("<H", i) for i in range(len(faces))))
    face_block = chunk(
        0x4120, struct.pack("<H", len(faces)) +
        b"".join(struct.pack("<HHHH", a, b, c, 7) for a, b, c in faces)
        + group
    )
    texture_coords = chunk(
        0x4140, struct.pack("<H", len(uvs)) +
        b"".join(struct.pack("<ff", u, 1.0 - v) for u, v in uvs)
    )
    local_mesh = chunk(0x4100, vertex_block + face_block + texture_coords)
    named_object = chunk(0x4000, cstr(NAME) + local_mesh)
    path.write_bytes(chunk(0x4D4D, chunk(0x3D3D, material + named_object)))


def write_tga(path: Path, pixels: list[tuple[int, int, int]], width: int, height: int) -> None:
    if len(pixels) != width * height:
        raise WandError("Texture pixel count differs from dimensions")
    header = struct.pack("<BBBHHBHHHHBB",
        0, 0, 2, 0, 0, 0, 0, 0, width, height, 24, 0x20)
    with path.open("wb") as dst:
        dst.write(header)
        for red, green, blue in pixels:
            dst.write(bytes((blue, green, red)))


def fallback_texture(path: Path) -> None:
    pixels = []
    for y in range(64):
        for x in range(64):
            # Subtle original woodgrain with deterministic variation.
            grain = 18 * math.sin(0.3 * x + 0.07 * y + 2.0 * math.sin(y / 7.0))
            noise = ((x * 19 + y * 31) % 17) - 8
            shade = int(max(-24, min(24, grain + noise)))
            pixels.append((max(0, 123 + shade),
                           max(0, 75 + int(shade * 0.7)),
                           max(0, 44 + int(shade * 0.4))))
    write_tga(path, pixels, 64, 64)


def png_to_tga(source: Path, destination: Path) -> None:
    """Read 8-bit non-interlaced grayscale/RGB/palette/RGBA PNG using stdlib."""
    raw = source.read_bytes()
    if len(raw) > 8 * 1024 * 1024 or not raw.startswith(b"\x89PNG\r\n\x1a\n"):
        raise WandError("Wand texture is not a supported PNG")
    offset = 8
    data = b""
    palette = b""
    transparency = b""
    width = height = color_type = depth = interlace = None
    while offset + 12 <= len(raw):
        n = int.from_bytes(raw[offset:offset+4], "big")
        name = raw[offset+4:offset+8]
        block = raw[offset+8:offset+8+n]
        offset += 12+n
        if len(block) != n:
            raise WandError("Truncated PNG texture")
        if name == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", block
            )
        elif name == b"PLTE":
            palette = block
        elif name == b"tRNS":
            transparency = block
        elif name == b"IDAT":
            data += block
        elif name == b"IEND":
            break
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
    if (width is None or not 1 <= width <= 2048 or not 1 <= height <= 2048
            or depth != 8 or color_type not in channels or interlace != 0):
        raise WandError("PNG format not supported by stdlib converter")
    bpp = channels[color_type]
    stride = width * bpp
    decoder = zlib.decompressobj()
    inflated = decoder.decompress(data, (stride + 1) * height + 1)
    if decoder.unconsumed_tail or len(inflated) != (stride + 1) * height:
        raise WandError("PNG is corrupt or excessively large")
    rows = []
    pos = 0
    last = bytearray(stride)
    for _ in range(height):
        kind = inflated[pos]
        row = bytearray(inflated[pos+1:pos+1+stride])
        pos += stride + 1
        if kind not in range(5):
            raise WandError("Unsupported PNG row filter")
        for i in range(stride):
            left = row[i-bpp] if i >= bpp else 0
            up = last[i]
            upper_left = last[i-bpp] if i >= bpp else 0
            if kind == 1:
                row[i] = (row[i] + left) & 255
            elif kind == 2:
                row[i] = (row[i] + up) & 255
            elif kind == 3:
                row[i] = (row[i] + (left+up)//2) & 255
            elif kind == 4:
                p = left+up-upper_left
                da, db, dc = abs(p-left), abs(p-up), abs(p-upper_left)
                guess = left if da <= db and da <= dc else up if db <= dc else upper_left
                row[i] = (row[i] + guess) & 255
        last = row
        for col in range(width):
            px = row[col*bpp:(col+1)*bpp]
            if color_type in (0, 4):
                rows.append((px[0], px[0], px[0]))
            elif color_type in (2, 6):
                rows.append(tuple(px[:3]))
            else:
                idx = px[0]
                if (idx+1)*3 > len(palette):
                    raise WandError("PNG palette index is invalid")
                rows.append(tuple(palette[idx*3:idx*3+3]))
    write_tga(destination, rows, width, height)


def generate_wand(mesh_path: Path, texture_path: Path,
                  original_obj: Path | None = None,
                  original_png: Path | None = None) -> bool:
    """True if the authentic HP2 OBJ was used, False for original fallback."""
    mesh_path.parent.mkdir(parents=True, exist_ok=True)
    texture_path.parent.mkdir(parents=True, exist_ok=True)
    from_hp2 = original_obj is not None and original_obj.is_file()
    geom = read_obj(original_obj) if from_hp2 else original_wand()
    # Convert onto temp files first; keep previous valid files if a converter
    # raises an exception.
    tmp_3ds = mesh_path.with_suffix(".3ds.parryhotter.tmp")
    tmp_tga = texture_path.with_suffix(".tga.parryhotter.tmp")
    try:
        write_3ds(tmp_3ds, *geom)
        if from_hp2 and original_png is not None and original_png.is_file():
            png_to_tga(original_png, tmp_tga)
        else:
            fallback_texture(tmp_tga)
        if tmp_3ds.stat().st_size < 128 or tmp_tga.stat().st_size < 24:
            raise WandError("Generated wand files are too small")
        tmp_3ds.replace(mesh_path)
        tmp_tga.replace(texture_path)
    finally:
        tmp_3ds.unlink(missing_ok=True)
        tmp_tga.unlink(missing_ok=True)
    return from_hp2
