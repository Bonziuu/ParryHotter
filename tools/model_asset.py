#!/usr/bin/env python3
"""Prepare the ordinary HP2 (PC 2002) Harry model for PRIVATE local conversion.

Does not publish any game assets, edit the Gothic model or claim an FBX is a
valid Gothic animated ASC. The original model needs rigging and ASC export.
"""
from __future__ import annotations

import argparse
import shutil
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")
MODEL_SOURCE = "https://models.spriters-resource.com/media/assets/292/295618.zip"
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


def prepare_model(game_dir: Path, model_archive: Path | None = None) -> Path:
    """Save extracted sources outside repository and game resource folders."""
    game_dir = game_dir.expanduser().resolve()
    if not (game_dir / "System").is_dir():
        raise ModelAssetError(f"Nie znaleziono Gothic/System: {game_dir}")
    private_dir = game_dir / "ParryHotter-private" / "HP2-Harry-original"
    private_dir.mkdir(parents=True, exist_ok=True)
    zip_path = model_archive or private_dir / "295618.zip"

    if model_archive is None and not zip_path.exists():
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Import private Harry HP2 model archive")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--zip", type=Path, help="ZIP downloaded manually from The Models Resource")
    args = parser.parse_args()
    try:
        prepare_model(args.game, args.zip)
    except (ModelAssetError, OSError, zipfile.BadZipFile) as exc:
        print("Nie przygotowano modelu: " + str(exc), file=sys.stderr)
        print("Strona modelu: " + MODEL_PAGE, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
