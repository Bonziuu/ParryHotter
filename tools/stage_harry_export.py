#!/usr/bin/env python3
"""Stage a locally exported Harry Gothic ASC and 4 TGAs without source changes.

When the user exports PH_HARRY.ASC next to the original FBX via KrxImpExp,
normalize its material names and prepare the private output folder expected
by visual_install.py. Nothing is published.
"""
from __future__ import annotations

import argparse
import re
import tempfile
from pathlib import Path

from tools.install import PatchError
from tools.visual_install import OUTFIT, TEXTURES, validate_converted_assets

DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")


def rewrite_bitmaps(source: bytes) -> bytes:
    def replace(match: re.Match[bytes]) -> bytes:
        raw = match.group(2)
        base = raw.replace(b"\\", b"/").split(b"/")[-1].lower()
        normalized = re.fullmatch(rb"(?:skharrytex|ph_harry_tex)([0-3])\.(?:png|tga|tex)", base)
        if not normalized:
            raise PatchError(f"Exported ASC references an unexpected bitmap: {raw!r}")
        index = int(normalized.group(1))
        return match.group(1) + f"PH_HARRY_TEX{index}.TGA".encode("ascii") + match.group(3)

    regex = rb'(\*BITMAP\s+")([^"]+)(")'
    result, count = re.subn(regex, replace, source, flags=re.IGNORECASE)
    if not count:
        raise PatchError("Exported Harry ASC contains no *BITMAP texture references")
    return result


def stage_export(game: Path) -> bool:
    """True only if the user already exported a real ASC locally."""
    game = game.expanduser().resolve()
    source_dir = game / "ParryHotter-private" / "HP2-Harry-original"
    source_asc = source_dir / OUTFIT
    if not source_asc.is_file():
        return False
    for prerequisite in (source_dir / "skharrymesh.fbx", source_dir / "ParryHotter_VISUAL_FIRST.blend"):
        if prerequisite.is_file() and source_asc.stat().st_mtime + 2 < prerequisite.stat().st_mtime:
            raise PatchError(f"PH_HARRY.ASC is older than {prerequisite.name}; export the reviewed scene again")
    dst_dir = game / "ParryHotter-private" / "HP2-Harry-converted"
    with tempfile.TemporaryDirectory(prefix="ph_asc_", dir=source_dir) as tmpdir:
        stage = Path(tmpdir)
        (stage / OUTFIT).write_bytes(rewrite_bitmaps(source_asc.read_bytes()))
        for name in TEXTURES:
            asset = source_dir / name
            if not asset.is_file():
                raise PatchError(
                    f"Brakuje tekstury {name}. Uruchom konwersje PNG -> TGA w Blenderze"
                )
            (stage / name).write_bytes(asset.read_bytes())
        validate_converted_assets(stage)
        dst_dir.mkdir(parents=True, exist_ok=True)
        for file in stage.iterdir():
            destination = dst_dir / file.name
            if destination.is_file() and destination.read_bytes() != file.read_bytes():
                backup = destination.with_name(destination.name + ".parryhotter-original")
                if not backup.exists():
                    backup.write_bytes(destination.read_bytes())
            temp_target = destination.with_name(destination.name + ".parryhotter.tmp")
            temp_target.write_bytes(file.read_bytes())
            temp_target.replace(destination)
    print(f"Poprawny gotowy ASC + tekstury lokalnie: {dst_dir}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Stage local Harry ASC export and normalize textures")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    args = parser.parse_args()
    try:
        if not stage_export(args.game):
            print("Brak PH_HARRY.ASC obok oryginalnego FBX; Blender/KrxImpExp ASC export still needed.")
    except (PatchError, OSError) as exc:
        parser.exit(1, f"BLAD: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
