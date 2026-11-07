#!/usr/bin/env python3
"""Opt-in Gothic I costume integration using ONLY a LOCAL converted Harry ASC.

Never downloads/bundles the original HP2 character. Does not accept FBX as ASC.
The conversion is a separate Blender/KrxImpExp step. No change is made until
all files and source patch anchors have been checked.
"""
from __future__ import annotations

import argparse
import math
import re
import shlex
from contextlib import nullcontext
from pathlib import Path

from tools.file_transaction import FileTransaction
from tools.install import PatchError
from tools.casting_assets import CASTING_MDS, validate_optional_animation_assets

OUTFIT = "PH_HARRY.ASC"
TEXTURES = tuple(f"PH_HARRY_TEX{i}.TGA" for i in range(4))
ARMOR_SCRIPT = Path(__file__).resolve().parents[1] / "mod" / "Items" / "ParryHarryArmor.d"


def find_case(root: Path, *parts: str) -> Path:
    found = root
    for index, name in enumerate(parts):
        if not found.is_dir():
            # Preserve ALL remaining components when a directory hasn't yet
            # been created. The old code silently skipped Meshes/Textures.
            return found.joinpath(*parts[index:])
        found = next((p for p in found.iterdir() if p.name.casefold() == name.casefold()),
                     found / name)
    return found


def validate_tga(path: Path) -> None:
    if path.stat().st_size > 32 * 1024 * 1024:
        raise PatchError(f"TGA too large: {path}")
    data = path.read_bytes()
    if len(data) < 18 or data[1] != 0 or data[2] not in (2, 10):
        raise PatchError(f"Unrecognized TGA texture: {path}")
    width = int.from_bytes(data[12:14], "little")
    height = int.from_bytes(data[14:16], "little")
    if not (4 <= width <= 4096 and 4 <= height <= 4096) or data[16] not in (24, 32):
        raise PatchError(f"Invalid TGA dimensions/depth: {path}")
    pixel_size = data[16] // 8
    offset = 18 + data[0]
    pixels = width * height
    if data[2] == 2:
        if len(data) < offset + pixels * pixel_size:
            raise PatchError(f"Truncated TGA pixel data: {path}")
    else:
        # TGA RLE packets contain 1..128 pixels; a valid header alone is not
        # evidence that Blender produced a complete image.
        written = 0
        while written < pixels:
            if offset >= len(data):
                raise PatchError(f"Truncated TGA RLE data: {path}")
            packet = data[offset]
            offset += 1
            count = (packet & 0x7F) + 1
            offset += pixel_size * (1 if packet & 0x80 else count)
            written += count
            if written > pixels or offset > len(data):
                raise PatchError(f"Invalid TGA RLE packet: {path}")


def _asc_blocks(text: str, tag: str) -> list[str]:
    """Extract brace-balanced ASE/ASC blocks without counting quoted braces."""
    blocks = []
    for match in re.finditer(r"\*" + tag + r"\s*\{", text, re.IGNORECASE):
        start = match.end()
        depth = 1
        # Quoted filenames and line comments can contain literal braces.
        for token in re.finditer(r'"[^"\r\n]*"|//[^\r\n]*|[{}]', text[start:]):
            if token.group() == "{":
                depth += 1
            elif token.group() == "}":
                depth -= 1
                if depth == 0:
                    blocks.append(text[start:start + token.start()])
                    break
        else:
            raise PatchError(f"ASC contains an unterminated *{tag} block")
    return blocks


def validate_asc(data: bytes) -> None:
    """Validate mesh/skin structure, not just the presence of marker strings.

    The soft-skin layout follows Kerrax's AscFileSaver/AscFileLoader:
    object name, vertex count, then one influence-count/bone/weight row per
    vertex. This remains a structural check, not a ZenGin compilation test.
    """
    text = data.decode("latin-1")
    if not re.match(r"\s*\*3DSMAX_ASCIIEXPORT\s+\d+", text, re.IGNORECASE):
        raise PatchError("PH_HARRY.ASC has no ASCII export header")
    nodes = _asc_blocks(text, "GEOMOBJECT") + _asc_blocks(text, "HELPEROBJECT")
    names = set()
    meshes = {}
    for node in nodes:
        match = re.search(r'\*NODE_NAME\s+"([^"\r\n]+)"', node, re.IGNORECASE)
        if not match:
            raise PatchError("ASC object is missing *NODE_NAME")
        name = match.group(1).casefold()
        if name in names:
            raise PatchError(f"ASC has duplicate object name: {match.group(1)}")
        names.add(name)
        mesh_blocks = _asc_blocks(node, "MESH_SOFTSKIN") + _asc_blocks(node, "MESH")
        if not mesh_blocks:
            continue
        if len(mesh_blocks) != 1:
            raise PatchError(f"ASC object has ambiguous mesh data: {name}")
        mesh = mesh_blocks[0]
        counts = {}
        for field in ("NUMVERTEX", "NUMFACES"):
            declared = re.findall(r"\*MESH_" + field + r"\s+(\d+)", mesh, re.IGNORECASE)
            if len(declared) != 1:
                raise PatchError(f"ASC mesh lacks one *MESH_{field}: {name}")
            counts[field] = int(declared[0])
        if not (3 <= counts["NUMVERTEX"] <= 200_000 and 1 <= counts["NUMFACES"] <= 400_000):
            raise PatchError(f"ASC mesh has no usable triangles: {name}")
        vertices = re.findall(r"\*MESH_VERTEX\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)", mesh, re.IGNORECASE)
        if len(vertices) != counts["NUMVERTEX"]:
            raise PatchError(f"ASC vertex count does not match mesh: {name}")
        try:
            positions = {int(i): tuple(float(v) for v in (x, y, z)) for i, x, y, z in vertices}
        except ValueError as exc:
            raise PatchError(f"ASC mesh has invalid vertex coordinates: {name}") from exc
        if set(positions) != set(range(counts["NUMVERTEX"])) or len(positions) != len(vertices):
            raise PatchError(f"ASC mesh has invalid or duplicate vertex indices: {name}")
        if not all(math.isfinite(v) for p in positions.values() for v in p):
            raise PatchError(f"ASC mesh has non-finite coordinates: {name}")
        faces = re.findall(r"\*MESH_FACE\s+(\d+)\s*:\s*A:\s*(\d+)\s+B:\s*(\d+)\s+C:\s*(\d+)", mesh, re.IGNORECASE)
        if (len(faces) != counts["NUMFACES"] or
                {int(f[0]) for f in faces} != set(range(counts["NUMFACES"]))):
            raise PatchError(f"ASC face count/indices do not match mesh: {name}")
        for _, *indices in faces:
            ids = [int(i) for i in indices]
            if len(set(ids)) != 3 or any(i not in positions for i in ids):
                raise PatchError(f"ASC face references invalid vertices: {name}")
            a, b, c = (positions[i] for i in ids)
            ab, ac = ([b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)])
            cross = (ab[1] * ac[2] - ab[2] * ac[1],
                     ab[2] * ac[0] - ab[0] * ac[2],
                     ab[0] * ac[1] - ab[1] * ac[0])
            if not any(abs(v) > 1e-12 for v in cross):
                raise PatchError(f"ASC face is a degenerate triangle: {name}")
        meshes[name] = counts["NUMVERTEX"]
    if not meshes or "bip01" not in names:
        raise PatchError("ASC needs an actual mesh and a Bip01 skeleton")
    skins = {}
    for block in _asc_blocks(text, "MESH_SOFTSKINVERTS"):
        rows = [line.strip() for line in block.splitlines() if line.strip() and not line.lstrip().startswith("//")]
        if len(rows) < 3:
            raise PatchError("ASC has incomplete soft-skin data")
        name = rows[0].strip('"').casefold()
        try:
            count = int(rows[1])
        except ValueError as exc:
            raise PatchError("ASC soft-skin vertex count is invalid") from exc
        if name in skins or name not in meshes or count != meshes[name] or len(rows) != count + 2:
            raise PatchError(f"ASC soft-skin vertex count/object does not match mesh: {name}")
        for index, row in enumerate(rows[2:]):
            try:
                values = shlex.split(row)
                influences = int(values[0])
                if influences < 1 or len(values) != 1 + 2 * influences:
                    raise ValueError("incomplete influence list")
                bones = [bone.casefold() for bone in values[1::2]]
                weights = [float(weight) for weight in values[2::2]]
                if (len(set(bones)) != len(bones) or
                        any(bone not in names or not bone.startswith("bip01") for bone in bones) or
                        any(not math.isfinite(weight) or weight <= 0 or weight > 1 for weight in weights) or
                        abs(sum(weights) - 1.0) > 0.02):
                    raise ValueError("invalid bone or non-normalized weights")
            except (ValueError, IndexError) as exc:
                raise PatchError(f"ASC invalid skin weights for {name} vertex {index}: {exc}") from exc
        skins[name] = count
    if set(skins) != set(meshes):
        raise PatchError("Every Harry mesh must have complete Gothic skin weights")


def validate_converted_assets(root: Path) -> tuple[Path, ...]:
    asc = root / OUTFIT
    files = (asc,) + tuple(root / name for name in TEXTURES)
    for path in files:
        if not path.is_file():
            raise PatchError(f"Converted Harry outfit missing: {path}")
    data = asc.read_bytes()
    if len(data) < 256 or len(data) > 12 * 1024 * 1024:
        raise PatchError("ASC has an unexpected file size")
    validate_asc(data)
    # Reference textures by exact safe names, not arbitrary absolute paths.
    for match in re.findall(rb'\*BITMAP\s+"([^"]+)"', data, re.IGNORECASE):
        name = match.decode("latin-1")
        if name.upper() not in TEXTURES:
            raise PatchError(f"ASC references unknown texture: {name}")
    if not re.search(rb"\*BITMAP\s+", data, re.IGNORECASE):
        raise PatchError("ASC has no texture assignment")
    for tex in files[1:]:
        validate_tga(tex)
    return files


def register_mesh(mds: str) -> str:
    if re.search(r'(?i)registerMesh\s*\(\s*"PH_HARRY\.ASC"\s*\)', mds):
        return mds
    # Only humans are registered, never monsters or NPC scripts.
    if "registerMesh" not in mds or "aniEnum" not in mds:
        raise PatchError("HUMANS.MDS does not look like a human model script")
    match = re.search(r"(?im)^[ \t]*aniEnum\b", mds)
    if not match:
        raise PatchError("HUMANS.MDS aniEnum anchor missing")
    newline = "\r\n" if "\r\n" in mds else "\n"
    return mds[:match.start()] + '    registerMesh ("PH_HARRY.ASC")' + newline + mds[match.start():]


def register_armor_source(src: str) -> str:
    if re.search(r"(?im)^\s*ITEMS\\PARRYHARRYARMOR\.D\s*$", src):
        return src
    match = re.search(r"(?im)^[ \t]*ITEMS\\ARMOR\.D[ \t]*\r?$", src)
    if not match:
        raise PatchError("Gothic.src ITEMS\\ARMOR.D anchor missing")
    newline = "\r\n" if "\r\n" in src else "\n"
    return src[:match.end()] + newline + "ITEMS\\PARRYHARRYARMOR.D" + src[match.end():]


def select_visual_armor(hero: str, *, casting: bool = False) -> str:
    match = re.search(r"(?is)\bINSTANCE\s+PC_Hero\s*\(\s*NPC_DEFAULT\s*\)\s*\{", hero)
    if not match:
        raise PatchError("PC_Hero missing in Polish Gothic sources")
    following = re.search(r"(?im)^\s*INSTANCE\s+PC_Hero_\w+\s*\(", hero[match.end():])
    end = match.end() + following.start() if following else len(hero)
    body = hero[match.start():end]
    # A complete Harry mesh includes its own head. Leaving Hum_Head_Pony
    # active would draw a second Gothic head above the costume.
    pattern = (r'(?is)(Mdl_SetVisualBody\s*\(\s*self\s*,\s*"[^"]*"\s*,\s*[^,;]+,\s*[^,;]+,\s*)'
               r'"[^"]*"(\s*,\s*[^,;]+,\s*[^,;]+,\s*)'
               r'(KDF_ARMOR_L|PH_ARMOR_HARRY|-1)(\s*\)\s*;)')
    matches = list(re.finditer(pattern, body))
    if len(matches) != 1:
        raise PatchError("Expected one supported Mdl_SetVisualBody in PC_Hero")
    arm = matches[0]
    body = (body[:arm.start()] + arm.group(1) + '""' + arm.group(2)
            + "PH_ARMOR_HARRY" + arm.group(4) + body[arm.end():])
    overlay = re.compile(r'Mdl_ApplyOverlayMds\s*\(\s*self\s*,\s*"PH_HARRY_CAST\.MDS"\s*\)\s*;', re.IGNORECASE)
    present = list(overlay.finditer(body))
    if not casting and present:
        raise PatchError("Harry casting overlay is enabled but its animation sources are missing")
    if casting:
        mage = list(re.finditer(r'Mdl_ApplyOverlayMds\s*\(\s*self\s*,\s*"Humans_Mage\.mds"\s*\)\s*;', body, re.IGNORECASE))
        visual = re.search(r'Mdl_SetVisualBody\s*\([^;]+;', body, re.IGNORECASE)
        anchor = max([visual.end()] + [match.end() for match in mage])
        if len(present) != 1 or present[0].start() < anchor:
            # Remove only our own calls, then put one after the base mage
            # overlay and body initialization. Other NPCs are outside body.
            body = overlay.sub("", body)
            mage = list(re.finditer(r'Mdl_ApplyOverlayMds\s*\(\s*self\s*,\s*"Humans_Mage\.mds"\s*\)\s*;', body, re.IGNORECASE))
            visual = re.search(r'Mdl_SetVisualBody\s*\([^;]+;', body, re.IGNORECASE)
            anchor = max([visual.end()] + [match.end() for match in mage])
            newline = "\r\n" if "\r\n" in body else "\n"
            body = (body[:anchor] + newline + f'    Mdl_ApplyOverlayMds(self, "{CASTING_MDS}");'
                    + body[anchor:])
    return hero[:match.start()] + body + hero[end:]


def install_visual(game: Path, *, transaction: FileTransaction | None = None) -> bool:
    """Install outfit only if all local converted assets exist.

    Future re-running the setup should call this after installing PL scripts.
    Returns False if no completed converted ASC is present yet.
    """
    game = game.expanduser().resolve()
    converted = game / "ParryHotter-private" / "HP2-Harry-converted"
    if not (converted / OUTFIT).is_file():
        return False

    source_assets = validate_converted_assets(converted)
    animation_assets = validate_optional_animation_assets(converted)
    content = find_case(game, "_work", "DATA", "scripts", "Content")
    anims = find_case(game, "_work", "DATA", "Anims")
    textures = find_case(game, "_work", "DATA", "Textures")
    gothic_src = find_case(content, "Gothic.src")
    hero = find_case(content, "Story", "NPC", "PC_Hero.d")
    source_mds = find_case(anims, "HUMANS.MDS")
    if not gothic_src.is_file() or not hero.is_file() or not source_mds.is_file():
        raise PatchError(
            "Model Gothic 1 needs Gothic.src, PC_Hero.d AND source HUMANS.MDS "
            "from the animation ModKit; no changes applied"
        )
    if not ARMOR_SCRIPT.is_file():
        raise PatchError("ParryHarryArmor.d is missing")

    updates = {
        gothic_src: register_armor_source(gothic_src.read_bytes().decode("latin-1")).encode("latin-1"),
        hero: select_visual_armor(hero.read_bytes().decode("latin-1"), casting=bool(animation_assets)).encode("latin-1"),
        source_mds: register_mesh(source_mds.read_bytes().decode("latin-1")).encode("latin-1"),
        find_case(content, "Items", "ParryHarryArmor.d"): ARMOR_SCRIPT.read_bytes(),
    }
    # Avoid accidentally replacing an existing unrelated mod's assets.
    for source in tuple(source_assets) + tuple(animation_assets):
        target = find_case(textures if source.name.upper().endswith(".TGA") else anims, source.name)
        if target.is_file() and target.read_bytes() != source.read_bytes():
            backup = target.with_name(target.name + ".parryhotter-original")
            if not backup.exists():
                raise PatchError(
                    f"Refusing to overwrite an unrelated existing file: {target}. "
                    "Rename it or prepare a backup before retrying"
                )
        updates[target] = source.read_bytes()

    # The importer shares its transaction so source, active assets, backups
    # and private cache all return to their previous state if any write fails.
    context = nullcontext(transaction) if transaction is not None else FileTransaction(game)
    with context as files:
        for path in updates:
            files.check_file(path)
            files.check_file(path.with_name(path.name + ".parryhotter-original"))
        for path, data in updates.items():
            if path.is_file() and path.read_bytes() == data:
                continue
            if path.is_file():
                backup = path.with_name(path.name + ".parryhotter-original")
                if not backup.exists():
                    files.copy_file(path, backup)
            files.write_bytes(path, data)

    print("Local Harry costume ASC/4 TGAs and MDS mesh registration prepared.")
    print("GothicStarter_mod: Reparse scripts; model/texture compilation may also be needed.")
    print("The engine must still verify the converted mesh and animations.")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Install privately converted Harry Gothic mesh")
    parser.add_argument("--game", type=Path, default=Path(r"C:\Programy\Steam\steamapps\common\Gothic"))
    args = parser.parse_args()
    try:
        installed = install_visual(args.game)
    except (PatchError, OSError) as exc:
        parser.exit(1, f"BLAD KONWERSJI: {exc}\n")
    if not installed:
        print("Nie ma jeszcze PH_HARRY.ASC w ParryHotter-private/HP2-Harry-converted; zachowano robe Gothic.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
