#!/usr/bin/env python3
"""Prepare the ordinary HP2 (PC 2002) Harry model for PRIVATE local conversion.

Does not publish any game assets, edit the Gothic model or claim an FBX is a
valid Gothic animated ASC. The original model needs rigging and ASC export.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")
MODEL_SOURCE = "https://models.spriters-resource.com/media/assets/292/295618.zip?updated=1755499167"
MODEL_PAGE = (
    "https://models.spriters-resource.com/pc_computer/"
    "harrypotterthechamberofsecrets/asset/295618/"
)
MAX_DOWNLOAD_SIZE = 12 * 1024 * 1024
MAX_FILE_SIZE = 15 * 1024 * 1024
EXPECTED = {
    "skharrymesh.fbx",
    "skharrymesh.obj",
    "skharrytex0.png",
    "skharrytex1.png",
    "skharrytex2.png",
    "skharrytex3.png",
}


class ModelAssetError(RuntimeError):
    pass


def download_asset(target: Path) -> None:
    request = urllib.request.Request(
        MODEL_SOURCE,
        headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
            "Referer": MODEL_PAGE,
            "Accept": "application/zip,application/octet-stream,*/*",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=40) as response:
            written = 0
            with target.open("wb") as out:
                while True:
                    chunk = response.read(256 * 1024)
                    if not chunk:
                        break
                    written += len(chunk)
                    if written > MAX_DOWNLOAD_SIZE:
                        raise ModelAssetError("Archive exceeds 12 MB limit")
                    out.write(chunk)
    except (TimeoutError, urllib.error.URLError) as exc:
        raise ModelAssetError("Automatyczne pobieranie nie powiodlo sie: " + str(exc)) from exc
    if not zipfile.is_zipfile(target):
        raise ModelAssetError(
            "Zrodlo nie zwrocilo ZIP (mozliwa ochrona strony). "
            "Pobierz ZIP w przegladarce i uruchom ponownie z --zip."
        )



def discover_model_zip(private_dir: Path) -> Path | None:
    """Reuse a downloaded ordinary HP2 Harry ZIP before making web requests.

    Only inspect likely archive names in the user's Downloads/our folder.
    Never search or copy the rest of their personal files.
    """
    roots = [private_dir, ROOT, Path.home() / "Downloads"]
    if os.environ.get("USERPROFILE"):
        roots.append(Path(os.environ["USERPROFILE"]) / "Downloads")
    candidates = []
    for root in roots:
        if not root.is_dir():
            continue
        for pattern in ("295618*.zip", "*Harry*Potter*.zip", "*harry*.zip"):
            candidates.extend(root.glob(pattern))
    seen = set()
    for path in sorted(candidates, key=lambda item: item.stat().st_mtime, reverse=True):
        if path in seen:
            continue
        seen.add(path)
        try:
            if not zipfile.is_zipfile(path):
                continue
            with zipfile.ZipFile(path) as archive:
                names = {p.filename.replace("\\", "/").split("/")[-1].lower()
                         for p in archive.infolist()}
            if EXPECTED <= names:
                return path
        except (OSError, zipfile.BadZipFile):
            continue
    return None



def prepare_model(game_dir: Path, model_archive: Path | None = None) -> Path:
    """Save extracted sources outside repository and game resource folders."""
    game_dir = game_dir.expanduser().resolve()
    if not (game_dir / "System").is_dir():
        raise ModelAssetError(f"Nie znaleziono Gothic/System: {game_dir}")
    private_dir = game_dir / "ParryHotter-private" / "HP2-Harry-original"
    private_dir.mkdir(parents=True, exist_ok=True)
    cached_zip = private_dir / "295618.zip"
    zip_path = model_archive or (
        cached_zip if cached_zip.is_file() else discover_model_zip(private_dir)
    )
    if zip_path is None:
        # Try downloading automatically if no suitable local archive exists.
        zip_path = cached_zip
        download_asset(zip_path)
    if not zipfile.is_zipfile(zip_path):
        raise ModelAssetError(f"Nieprawidlowy ZIP: {zip_path}")

    members: dict[str, zipfile.ZipInfo] = {}
    with zipfile.ZipFile(zip_path) as archive:
        for item in archive.infolist():
            # Only known filenames; no archive paths are trusted.
            name = item.filename.replace("\\", "/").split("/")[-1].lower()
            if name in EXPECTED:
                if item.file_size > MAX_FILE_SIZE:
                    raise ModelAssetError(f"Plik jest za duzy: {name}")
                if name in members:
                    raise ModelAssetError(f"Duplikat modelu lub tekstury: {name}")
                members[name] = item
        missing = EXPECTED - members.keys()
        if missing:
            raise ModelAssetError(
                "To nie jest zwykly model Harry z HP2. Brakuje: "
                + ", ".join(sorted(missing))
            )
        for name, member in members.items():
            with archive.open(member, "r") as infile, (private_dir / name).open("wb") as outfile:
                shutil.copyfileobj(infile, outfile)
    print(f"Przygotowano model i tekstury HP2 w: {private_dir}")
    print("Model FBX jest tylko materialem do konwersji w Blenderze.")
    print("NIE zmieniono wygladu bohatera w Gothic 1: potrzebny jest retarget i ASC.")
    return private_dir



def prepare_blender_project(private_dir: Path) -> Path | None:
    """Build a .blend project if a Blender executable is available locally.

    The output is intentionally NOT marked as a Gothic-ready ASC.
    """
    blender_path = shutil.which("blender")
    if blender_path is None:
        # Official Blender installations on Windows are usually versioned.
        root = Path(r"C:\Program Files\Blender Foundation")
        if root.is_dir():
            candidates = sorted(root.glob("Blender */blender.exe"), reverse=True)
            blender_path = str(candidates[0]) if candidates else None
    if blender_path is None:
        print("Blender nie jest zainstalowany. FBX zachowany do konwersji.")
        return None

    output = private_dir / "Harry_HP2_to_Gothic_PREP.blend"
    command = [
        blender_path,
        "--background",
        "--python",
        str(ROOT / "tools" / "blender_prepare.py"),
        "--",
        str(private_dir),
        str(output),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=240)
    except (OSError, subprocess.TimeoutExpired) as exc:
        print(f"Blender nie przygotowal sceny: {exc}")
        return None
    if result.returncode != 0 or not output.exists():
        print("Nie udalo sie zaimportowac modelu do Blendera:")
        print((result.stderr or result.stdout)[-1200:])
        return None
    print(f"Scena Blendera gotowa do retargetingu: {output}")
    return output


def prepare_visual_first(private_dir: Path) -> Path | None:
    """Visual-only attempt once a Gothic human skeleton .blend is available.

    The original Harry appearance and robe are kept. No HP2 combat animations
    or collision are transferred. Output is a PREVIEW, not compiled Gothic ASC.
    """
    game_dir = private_dir.parent.parent
    gothic_reference = game_dir / "ParryHotter-private" / "GothicHuman.blend"
    source = private_dir / "skharrymesh.fbx"
    if not gothic_reference.is_file():
        print(
            "Brak GothicHuman.blend (szkielet z HUM_BODY_NAKED0.ASC). "
            "Harry FBX zachowany; nie podmieniam wizualu w Gothic."
        )
        return None
    if not source.is_file():
        return None

    blender_path = shutil.which("blender")
    if blender_path is None:
        root = Path(r"C:\Program Files\Blender Foundation")
        if root.is_dir():
            candidates = sorted(root.glob("Blender */blender.exe"), reverse=True)
            blender_path = str(candidates[0]) if candidates else None
    if blender_path is None:
        print("Blender nie znaleziony — nie mozna zalozyc stroju Harry'ego na rig Gothic.")
        return None

    target = private_dir / "ParryHotter_VISUAL_FIRST.blend"
    try:
        command = [
            blender_path, "--background", "--python",
            str(ROOT / "tools" / "blender_visual_first.py"), "--",
            str(source), str(gothic_reference), str(target),
        ]
        result = subprocess.run(command, capture_output=True, text=True, timeout=240)
    except (OSError, subprocess.TimeoutExpired) as exc:
        print(f"Nie udalo sie uruchomic konwersji wygladu: {exc}")
        return None
    if result.returncode or not target.is_file():
        print("Przygotowanie wygladu Harry'ego nie powiodlo sie:")
        print((result.stderr or result.stdout)[-1200:])
        return None
    print(f"Przygotowano wizual Harry + peleryna: {target}")
    print("To podglad do sprawdzenia w Blenderze, NIE gotowy model do Gothic 1.")
    return target


def main() -> int:
    parser = argparse.ArgumentParser(description="Import private Harry HP2 model archive")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--zip", type=Path, help="ZIP downloaded manually from The Models Resource")
    args = parser.parse_args()
    try:
        private_dir = prepare_model(args.game, args.zip)
        prepare_blender_project(private_dir)
        prepare_visual_first(private_dir)
    except (ModelAssetError, OSError, zipfile.BadZipFile) as exc:
        print("Nie przygotowano modelu: " + str(exc), file=sys.stderr)
        print("Strona modelu: " + MODEL_PAGE, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
