"""Dress the bare legs of the converted Harry model in dark school trousers.

In motion the legs swing out of the robe's side slit. The converted texture
paints them in skin colour, so a pale leg showed through the robe in game.
Only the texels that leg faces map to are recoloured. Their shading is kept.
Hands and the waist band are skipped, and so are the shoes, which use another
texture.

The local, private texture is changed in place. The original is kept as
`<name>.parryhotter-original-legs` and every run recomputes the result from
it, so the step is idempotent. Pure Python: no imaging library is required.
"""
from __future__ import annotations

import re
from pathlib import Path

LEG_BONES = ("thigh", "calf")
TROUSERS_RGB = (54, 54, 60)
BACKUP_SUFFIX = ".parryhotter-original-legs"


class TrousersError(ValueError):
    pass


# --------------------------------------------------------------------- TGA
def read_tga(data: bytes) -> tuple[int, int, int, bool, list[list[int]]]:
    """Return width, height, bytes per pixel, top-origin flag and BGR(A) rows.

    Rows are returned top to bottom whatever the file's origin.
    """
    if len(data) < 18 or data[1] != 0 or data[2] not in (2, 10):
        raise TrousersError("Unsupported TGA type")
    width = int.from_bytes(data[12:14], "little")
    height = int.from_bytes(data[14:16], "little")
    if data[16] not in (24, 32) or not (1 <= width <= 4096 and 1 <= height <= 4096):
        raise TrousersError("Unsupported TGA depth or size")
    size = data[16] // 8
    top = bool(data[17] & 0x20)
    offset = 18 + data[0]
    total = width * height * size
    if data[2] == 2:
        flat = bytearray(data[offset:offset + total])
    else:
        flat = bytearray()
        while len(flat) < total:
            if offset >= len(data):
                raise TrousersError("Truncated TGA RLE data")
            packet = data[offset]
            offset += 1
            count = (packet & 0x7F) + 1
            if packet & 0x80:
                flat += data[offset:offset + size] * count
                offset += size
            else:
                flat += data[offset:offset + size * count]
                offset += size * count
    if len(flat) < total:
        raise TrousersError("Truncated TGA pixel data")
    stride = width * size
    rows = [list(flat[y * stride:(y + 1) * stride]) for y in range(height)]
    if not top:
        rows.reverse()
    return width, height, size, top, rows


def write_tga(width: int, height: int, size: int, rows: list[list[int]]) -> bytes:
    """Uncompressed, top-left origin TGA (which Gothic reads)."""
    header = bytearray(18)
    header[2] = 2
    header[12:14] = width.to_bytes(2, "little")
    header[14:16] = height.to_bytes(2, "little")
    header[16] = size * 8
    header[17] = 0x20 | (8 if size == 4 else 0)
    return bytes(header) + b"".join(bytes(row) for row in rows)


# --------------------------------------------------------------------- ASC
def _block(text: str, start: int) -> str:
    depth = 0
    for index in range(text.index("{", start), len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise TrousersError("Unbalanced ASC block")


def _bitmaps(text: str) -> list[str]:
    if "*MATERIAL_LIST" not in text:
        return []
    material_list = _block(text, text.index("*MATERIAL_LIST"))
    return [Path(name.replace("\\", "/")).name.upper()
            for name in re.findall(r'\*BITMAP\s+"([^"]+)"', material_list)]


def _skin_weights(text: str) -> dict[str, list[str]]:
    """Mesh name -> dominant bone (lower case) of each vertex."""
    result: dict[str, list[str]] = {}
    for match in re.finditer(r"\*MESH_SOFTSKINVERTS\s*\{", text):
        lines = _block(text, match.start()).splitlines()[1:-1]
        lines = [line.strip() for line in lines if line.strip()]
        name, count = lines[0], int(lines[1])
        dominant = []
        for line in lines[2:2 + count]:
            pairs = re.findall(r'"([^"]+)"\s+([-0-9.eE]+)', line)
            if not pairs:
                raise TrousersError(f"Vertex without skin weights in {name}")
            dominant.append(max(pairs, key=lambda pair: float(pair[1]))[0].lower())
        result[name] = dominant
    return result


def leg_triangles(asc: str) -> dict[str, list[tuple[tuple[float, float], ...]]]:
    """Texture file name -> UV triangles of faces whose vertices are all legs."""
    bitmaps = _bitmaps(asc)
    weights = _skin_weights(asc)
    found: dict[str, list[tuple[tuple[float, float], ...]]] = {}
    for match in re.finditer(r"\*GEOMOBJECT\s*\{", asc):
        geom = _block(asc, match.start())
        name = re.search(r'\*NODE_NAME\s+"([^"]+)"', geom).group(1)
        bones = weights.get(name)
        if bones is None:
            continue
        faces = re.findall(
            r"\*MESH_FACE\s+\d+:\s+A:\s+(\d+)\s+B:\s+(\d+)\s+C:\s+(\d+).*?\*MESH_MTLID\s+(\d+)", geom)
        tverts = [(float(u), float(v)) for u, v in
                  re.findall(r"\*MESH_TVERT\s+\d+\s+(\S+)\s+(\S+)\s+\S+", geom)]
        tfaces = re.findall(r"\*MESH_TFACE\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)", geom)
        if len(faces) != len(tfaces):
            raise TrousersError(f"Face/UV count mismatch in {name}")
        for (a, b, c, material), uv in zip(faces, tfaces):
            corners = (int(a), int(b), int(c))
            dominant = [bones[i] for i in corners]
            legs = sum(any(bone.endswith(leg) for leg in LEG_BONES) for bone in dominant)
            # Upper-thigh faces share a corner with the pelvis.
            if not legs or legs + sum(bone.endswith("pelvis") for bone in dominant) < 3:
                continue
            if int(material) >= len(bitmaps):
                continue
            found.setdefault(bitmaps[int(material)], []).append(
                tuple(tverts[int(i)] for i in uv))
    return found


# ------------------------------------------------------------------ recolour
def _mask(width: int, height: int, triangles, grow: int = 2) -> list[bytearray]:
    mask = [bytearray(width) for _ in range(height)]
    for triangle in triangles:
        points = [(u * width, (1.0 - v) * height) for u, v in triangle]
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        (x0, y0), (x1, y1), (x2, y2) = points
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        for y in range(max(0, int(min(ys))), min(height, int(max(ys)) + 1)):
            for x in range(max(0, int(min(xs))), min(width, int(max(xs)) + 1)):
                px, py = x + 0.5, y + 0.5
                w0 = (x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)
                w1 = (x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)
                w2 = (x0 - px) * (y1 - py) - (x1 - px) * (y0 - py)
                if area and ((w0 >= 0 and w1 >= 0 and w2 >= 0) or (w0 <= 0 and w1 <= 0 and w2 <= 0)):
                    mask[y][x] = 1
    for _ in range(grow):  # texture filtering samples beyond triangle edges
        grown = [bytearray(row) for row in mask]
        for y in range(height):
            for x in range(width):
                if not mask[y][x] and any(
                        0 <= y + dy < height and 0 <= x + dx < width and mask[y + dy][x + dx]
                        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                    grown[y][x] = 1
        mask = grown
    return mask


def dress_texture(tga: bytes, triangles) -> bytes:
    width, height, size, _, rows = read_tga(tga)
    mask = _mask(width, height, triangles)
    lum = [0.114 * rows[y][x * size] + 0.587 * rows[y][x * size + 1] + 0.299 * rows[y][x * size + 2]
           for y in range(height) for x in range(width) if mask[y][x]]
    if not lum:
        raise TrousersError("No leg texels found")
    mean = sum(lum) / len(lum) or 1.0
    red, green, blue = TROUSERS_RGB
    for y in range(height):
        for x in range(width):
            if not mask[y][x]:
                continue
            i = x * size
            shade = 0.114 * rows[y][i] + 0.587 * rows[y][i + 1] + 0.299 * rows[y][i + 2]
            factor = min(1.45, max(0.55, shade / mean))
            rows[y][i] = min(255, round(blue * factor))
            rows[y][i + 1] = min(255, round(green * factor))
            rows[y][i + 2] = min(255, round(red * factor))
    return write_tga(width, height, size, rows)


def dress_harry(asc_path: Path, textures: Path) -> list[Path]:
    """Recolour leg texels of every Harry texture; return the changed files."""
    triangles = leg_triangles(Path(asc_path).read_text(encoding="latin-1"))
    changed = []
    for name, faces in sorted(triangles.items()):
        target = _find(textures, name)
        if target is None:
            continue
        backup = target.with_name(target.name + BACKUP_SUFFIX)
        current = target.read_bytes()
        if backup.is_file():
            original = backup.read_bytes()
            if current != original and current != dress_texture(original, faces):
                # A new conversion replaced the texture: it becomes the original.
                backup.write_bytes(current)
                original = current
        else:
            backup.write_bytes(current)
            original = current
        result = dress_texture(original, faces)
        if result != current:
            target.write_bytes(result)
            changed.append(target)
    return changed


def _find(directory: Path, name: str) -> Path | None:
    for path in Path(directory).glob("*"):
        if path.name.upper() == name and path.is_file() and not path.is_symlink():
            return path
    return None
