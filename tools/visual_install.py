#!/usr/bin/env python3
"""Opt-in Gothic I costume integration using ONLY a LOCAL converted Harry ASC.

Never downloads/bundles the original HP2 character. Does not accept FBX as ASC.
The conversion is a separate Blender/KrxImpExp step. No change is made until
all files and source patch anchors have been checked.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from tools.install import PatchError

OUTFIT = "PH_HARRY.ASC"
TEXTURES = tuple(f"PH_HARRY_TEX{i}.TGA" for i in range(4))
ARMOR_SCRIPT = Path(__file__).resolve().parents[1] / "mod" / "Items" / "ParryHarryArmor.d"


def find_case(root: Path, *parts: str) -> Path:
    found = root
    for name in parts:
        if not found.is_dir():
            return found / name
        found = next((p for p in found.iterdir() if p.name.casefold() == name.casefold()),
                     found / name)
    return found


def validate_tga(path: Path) -> None:
    data = path.read_bytes()
    if len(data) < 18 + 128 or data[2] not in (2, 10):
        raise PatchError(f"Unrecognized TGA texture: {path}")
    width = int.from_bytes(data[12:14], "little")
    height = int.from_bytes(data[14:16], "little")
    if not (4 <= width <= 4096 and 4 <= height <= 4096) or data[16] not in (24, 32):
        raise PatchError(f"Invalid TGA dimensions/depth: {path}")
    if len(data) > 32 * 1024 * 1024:
        raise PatchError(f"TGA too large: {path}")


def validate_converted_assets(root: Path) -> tuple[Path, ...]:
    asc = root / OUTFIT
    files = (asc,) + tuple(root / name for name in TEXTURES)
    for path in files:
        if not path.is_file():
            raise PatchError(f"Converted Harry outfit missing: {path}")
    data = asc.read_bytes()
    if len(data) < 256 or len(data) > 12 * 1024 * 1024:
        raise PatchError("ASC has an unexpected file size")
    if b"*GEOMOBJECT" not in data.upper() or b"*MESH" not in data.upper():
        raise PatchError("PH_HARRY.ASC is not a Gothic/3ds Max ASCII mesh")
    # A static ASCII mesh may render in a T-pose. Gothic armor needs the
    # actual skin-weight map exported by KrxImpExp (vertex -> Bip01 bones).
    upper = data.upper()
    if b"*MESH_SOFTSKINVERTS" not in upper or b"BIP01" not in upper:
        raise PatchError(
            "PH_HARRY.ASC lacks Gothic skin weights (*MESH_SOFTSKINVERTS/Bip01). "
            "Export an animated model mesh, not a static object."
        )
    # Reference textures by exact safe names, not arbitrary absolute paths.
    for match in re.findall(rb'\*BITMAP\s+"([^"]+)"', data, re.IGNORECASE):
        name = match.replace(b"\\", b"/").split(b"/")[-1].decode("latin-1")
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


def select_visual_armor(hero: str) -> str:
    match = re.search(r"(?is)\bINSTANCE\s+PC_Hero\s*\(\s*NPC_DEFAULT\s*\)\s*\{", hero)
    if not match:
        raise PatchError("PC_Hero missing in Polish Gothic sources")
    following = re.search(r"(?im)^\s*INSTANCE\s+PC_Hero_\w+\s*\(", hero[match.end():])
    end = match.end() + following.start() if following else len(hero)
    body = hero[match.start():end]
    pattern = r"(?is)(Mdl_SetVisualBody\s*\(\s*self\s*,[^;]*?,\s*)(KDF_ARMOR_L|PH_ARMOR_HARRY|-1)(\s*\)\s*;)"
    matches = list(re.finditer(pattern, body))
    if len(matches) != 1:
        raise PatchError("Expected one supported Mdl_SetVisualBody in PC_Hero")
    arm = matches[0]
    if arm.group(2).upper() == "PH_ARMOR_HARRY":
        return hero
    body = body[:arm.start()] + arm.group(1) + "PH_ARMOR_HARRY" + arm.group(3) + body[arm.end():]
    return hero[:match.start()] + body + hero[end:]


def install_visual(game: Path) -> bool:
    """Install outfit only if all local converted assets exist.

    Future re-running the setup should call this after installing PL scripts.
    Returns False if no completed converted ASC is present yet.
    """
    game = game.expanduser().resolve()
    converted = game / "ParryHotter-private" / "HP2-Harry-converted"
    if not (converted / OUTFIT).is_file():
        return False

    source_assets = validate_converted_assets(converted)
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
        hero: select_visual_armor(hero.read_bytes().decode("latin-1")).encode("latin-1"),
        source_mds: register_mesh(source_mds.read_bytes().decode("latin-1")).encode("latin-1"),
        content / "Items" / "ParryHarryArmor.d": ARMOR_SCRIPT.read_bytes(),
    }
    # Avoid accidentally replacing an existing unrelated mod's assets.
    for source in source_assets:
        target = (anims if source.name.upper().endswith(".ASC") else textures) / source.name
        if target.is_file() and target.read_bytes() != source.read_bytes():
            backup = target.with_name(target.name + ".parryhotter-original")
            if not backup.exists():
                raise PatchError(
                    f"Refusing to overwrite an unrelated existing file: {target}. "
                    "Rename it or prepare a backup before retrying"
                )
        updates[target] = source.read_bytes()

    # All validation is complete. Each preexisting file is backed up and
    # changes are written with a temporary suffix before replacement.
    for path, data in updates.items():
        if path.is_file() and path.read_bytes() == data:
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.is_file():
            backup = path.with_name(path.name + ".parryhotter-original")
            if not backup.exists():
                backup.write_bytes(path.read_bytes())
        temporary = path.with_name(path.name + ".parryhotter.tmp")
        temporary.write_bytes(data)
        temporary.replace(path)

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
