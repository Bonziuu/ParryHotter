#!/usr/bin/env python3
"""Prepare original HP2 wand in a PRIVATE folder, or an original substitute.

Download URL points to the separate small "Wand" asset, not Harry's broom.
The original copyrighted OBJ/PNG never leaves the user's PC after import.
"""
from __future__ import annotations

import os
import shutil
import urllib.error
import urllib.request
import zipfile
import zlib
from pathlib import Path

from tools.wand_mesh import WandError, generate_wand
from tools.visual_install import find_case

WAND_URL = ("https://models.spriters-resource.com/media/assets/289/"
            "291995.zip?updated=1755498297")
WAND_PAGE = ("https://models.spriters-resource.com/pc_computer/"
             "harrypotterthechamberofsecrets/asset/291995/")
WAND_FILES = frozenset(("skwandmesh.obj", "skwandtex0.png"))


def valid_zip(path: Path) -> bool:
    try:
        if not zipfile.is_zipfile(path):
            return False
        with zipfile.ZipFile(path) as archive:
            if len(archive.infolist()) > 40:
                return False
            names = {p.filename.replace("\\", "/").split("/")[-1].lower()
                     for p in archive.infolist()}
            return WAND_FILES <= names
    except (OSError, zipfile.BadZipFile):
        return False


def find_local_wand_zip(private: Path) -> Path | None:
    roots = [private, Path.home() / "Downloads"]
    if os.environ.get("USERPROFILE"):
        roots.append(Path(os.environ["USERPROFILE"]) / "Downloads")
    for root in roots:
        if not root.is_dir():
            continue
        for pattern in ("291995*.zip", "*Wand*.zip", "*wand*.zip"):
            for p in root.glob(pattern):
                if valid_zip(p):
                    return p
    return None


def download_wand_zip(path: Path) -> None:
    temporary = path.with_suffix(".zip.download")
    req = urllib.request.Request(
        WAND_URL,
        headers={"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
                 "Referer": WAND_PAGE},
    )
    try:
        with urllib.request.urlopen(req, timeout=20) as response, temporary.open("wb") as target:
            size = 0
            while buf := response.read(65536):
                size += len(buf)
                if size > 2 * 1024 * 1024:
                    raise WandError("Wand ZIP exceeds the size limit")
                target.write(buf)
        if not valid_zip(temporary):
            raise WandError("Wand archive is not a valid HP2 OBJ/PNG ZIP")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def extract_wand(path: Path, target: Path) -> tuple[Path, Path]:
    target.mkdir(parents=True, exist_ok=True)
    files = {}
    with zipfile.ZipFile(path) as archive:
        for item in archive.infolist():
            name = item.filename.replace("\\", "/").split("/")[-1].lower()
            if name in WAND_FILES:
                if name in files or item.file_size > 4 * 1024 * 1024:
                    raise WandError("Duplicate or overly large HP2 wand source")
                files[name] = item
        if set(files) != WAND_FILES:
            raise WandError("Wand ZIP must contain skWandmesh.obj and skWandTex0.png")
        for name, member in files.items():
            with archive.open(member) as src, (target / name).open("wb") as dst:
                shutil.copyfileobj(src, dst)
    return target / "skwandmesh.obj", target / "skwandtex0.png"


def prepare_wand(game: Path, *, allow_download: bool = True) -> bool:
    """Generate wand; return True if imported from original HP2 geometry.

    Never prevents installing the main mod if the optional resource is offline.
    """
    game = game.expanduser().resolve()
    if not find_case(game, "System").is_dir():
        raise WandError("Missing Gothic System directory")
    private = game / "ParryHotter-private" / "HP2-Wand-original"
    private.mkdir(parents=True, exist_ok=True)
    archive = find_local_wand_zip(private)
    if archive is None and allow_download:
        cached = private / "291995.zip"
        if valid_zip(cached):
            archive = cached
        else:
            try:
                download_wand_zip(cached)
                archive = cached
            except (OSError, urllib.error.URLError, TimeoutError, WandError) as exc:
                print(f"Original HP2 wand unavailable; using our own wand: {exc}")

    obj = png = None
    if archive is not None:
        try:
            obj, png = extract_wand(archive, private)
        except (OSError, zipfile.BadZipFile, WandError) as exc:
            print(f"Original wand could not be unpacked, using original substitute: {exc}")
    meshes = find_case(game, "_work", "Data", "Meshes")
    textures = find_case(game, "_work", "Data", "Textures")
    target_mesh = meshes / "PH_WAND.3DS"
    target_texture = textures / "PH_WAND.TGA"
    try:
        from_original = generate_wand(target_mesh, target_texture, obj, png)
    except (OSError, WandError, ValueError, UnicodeError, zlib.error) as exc:
        print(f"Original wand conversion failed, using own model: {exc}")
        from_original = generate_wand(target_mesh, target_texture)
    print(f"Wand mesh: {target_mesh}")
    print(f"Wand texture: {target_texture}")
    print("Source: private HP2 PC original" if from_original
          else "Source: original Parry Hotter low-poly substitute")
    return from_original


def main() -> int:
    import argparse
    parser = argparse.ArgumentParser(description="Prepare HP2-like Gothic 1 wand")
    parser.add_argument("--game", type=Path,
                        default=Path(r"C:\Programy\Steam\steamapps\common\Gothic"))
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    prepare_wand(args.game, allow_download=not args.offline)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
