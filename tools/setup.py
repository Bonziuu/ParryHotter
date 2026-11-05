#!/usr/bin/env python3
"""One-command local setup of Parry Hotter on Windows.

Downloads pinned Gothic 1 Unified-PL source scripts, keeps a full backup,
installs local Parry Hotter changes, copies the GothicStarter profile, and
optionally opens GothicStarter_mod.exe. Does NOT download game assets/models.
"""

from __future__ import annotations

import argparse
import datetime as dt
import shutil
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.install import PatchError, install

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")
POLISH_SOURCE_COMMIT = "1a65a231f83d31ff1f2b80dba25a3c4843bff4be"
POLISH_SOURCE_URL = (
    "https://github.com/VaanaCZ/gothic-1-classic-scripts/archive/"
    f"{POLISH_SOURCE_COMMIT}.zip"
)
MAX_SOURCE_ZIP_BYTES = 48 * 1024 * 1024
REQUIRED_SOURCE_FILES = (
    "gothic.src",
    "story/npc/pc_hero.d",
    "magic/b_assessmagic.d",
    "magic/spells_params.d",
    "cutscene/ou.bin",
)


def unpack_polish_content(zip_path: Path, destination: Path) -> int:
    """Extract only _work/Data/Scripts/Content, rejecting unexpected paths."""
    count = 0
    prefix = "/_work/data/scripts/content/"
    with zipfile.ZipFile(zip_path) as archive:
        for item in archive.infolist():
            filename = item.filename.replace("\\", "/")
            idx = filename.lower().find(prefix)
            if idx < 0 or item.is_dir():
                continue
            rel = filename[idx + len(prefix) :]
            parts = Path(rel).parts
            if not parts or any(p in ("", ".", "..") for p in parts):
                raise PatchError(f"Unsafe ZIP entry: {filename}")
            target = destination.joinpath(*parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            count += 1

    for rel in REQUIRED_SOURCE_FILES:
        expected = destination.joinpath(*rel.split("/"))
        if not expected.is_file():
            # GitHub's archive preserves filename case; resolve case-insensitively.
            actual = destination
            for part in rel.split("/"):
                if not actual.is_dir():
                    raise PatchError(f"Polish scripts archive missing: {rel}")
                match = next((p for p in actual.iterdir() if p.name.lower() == part), None)
                if match is None:
                    raise PatchError(f"Polish scripts archive missing: {rel}")
                actual = match
            if not actual.is_file():
                raise PatchError(f"Polish scripts archive missing: {rel}")
    if count < 100:
        raise PatchError("Suspiciously few script files in the Polish source archive")
    return count


def download_polish_scripts(destination: Path) -> None:
    request = urllib.request.Request(
        POLISH_SOURCE_URL,
        headers={"User-Agent": "ParryHotter-installer/0.2"},
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            size = 0
            with destination.open("wb") as target:
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    size += len(chunk)
                    if size > MAX_SOURCE_ZIP_BYTES:
                        raise PatchError("Polish sources ZIP exceeds size limit")
                    target.write(chunk)
    except (urllib.error.URLError, TimeoutError) as exc:
        raise PatchError(f"Could not download Polish sources: {exc}") from exc


def install_bundle(game: Path, *, zip_path: Path | None = None) -> Path:
    """Prepare, patch, then atomically switch the local Content directory."""
    game = game.expanduser().resolve()
    scripts = game / "_work" / "DATA" / "scripts"
    content = scripts / "Content"
    system = game / "System"
    starter = system / "GothicStarter_mod.exe"
    if not content.is_dir() or not (content / "Gothic.src").is_file():
        raise PatchError(f"Gothic 1 ModKit not found: {content / 'Gothic.src'}")
    if not system.is_dir():
        raise PatchError(f"Gothic/System directory not found: {system}")
    if not starter.is_file():
        raise PatchError(f"GothicStarter_mod.exe not found: {starter}")

    with tempfile.TemporaryDirectory(prefix="parryhotter_") as temp_name:
        temp = Path(temp_name)
        archive = zip_path or temp / "polish_scripts.zip"
        if zip_path is None:
            print("Pobieram polskie skrypty Gothic 1 z GitHub (Unified-PL)...")
            download_polish_scripts(archive)

        stage = temp / "Content"
        stage.mkdir()
        count = unpack_polish_content(archive, stage)
        print(f"Przygotowano {count} polskich plikow.")
        print("Dodaje zaklecia i bohatera Parry Hotter...")
        changes = install(stage)
        if len(changes) < 4:
            raise PatchError("Unexpected patch result; refusing to install")

        # The ModKit's Content source is a shared directory. Back it up in full.
        # Staging and validation are complete before the first change to the game.
        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        backup = scripts / f"Content_ParryHotter_Backup_{stamp}"
        if backup.exists():
            raise PatchError(f"Backup directory already exists: {backup}")

        ini_src = REPO_ROOT / "System" / "ParryHotter.ini"
        if not ini_src.is_file():
            raise PatchError("Missing mod launch profile: System/ParryHotter.ini")
        ini_dst = system / "ParryHotter.ini"
        ini_backup = system / f"ParryHotter_Backup_{stamp}.ini"
        replaced = False

        try:
            if ini_dst.exists():
                shutil.copy2(ini_dst, ini_backup)

            content.rename(backup)
            try:
                shutil.copytree(stage, content)
                shutil.copy2(ini_src, ini_dst)
                replaced = True
            except Exception:
                if content.exists():
                    shutil.rmtree(content)
                backup.rename(content)
                if ini_backup.exists():
                    shutil.copy2(ini_backup, ini_dst)
                elif ini_dst.exists():
                    ini_dst.unlink()
                raise
        except Exception as exc:
            if isinstance(exc, PatchError):
                raise
            raise PatchError(f"Cannot install scripts: {exc}") from exc

    if not replaced:
        raise PatchError("Installation did not finish")
    print(f"Gotowe! Kopia zapasowa starych skryptow: {backup}")
    print(f"Skrypty PL i Parry Hotter zainstalowane: {content}")
    print(f"Profil uruchamiania: {ini_dst}")
    return starter


def main() -> int:
    parser = argparse.ArgumentParser(description="Parry Hotter — Windows one-click setup")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--zip", dest="zip_path", type=Path, help="Offline test with Unified-PL ZIP")
    parser.add_argument("--run", action="store_true", help="Open GothicStarter_mod.exe after setup")
    args = parser.parse_args()

    try:
        starter = install_bundle(args.game, zip_path=args.zip_path)
    except (PatchError, OSError, zipfile.BadZipFile) as exc:
        print(f"BLAD INSTALACJI: {exc}", file=sys.stderr)
        print("Sprawdz sciezke i polaczenie internetowe; nie uruchamiaj gry.", file=sys.stderr)
        return 1

    print("W GothicStarter_mod wybierz 'Parry Hotter (DEV)' i reparse scripts.")
    print("Rozpocznij NOWA GRE. Model Harry'ego i fizyka ragdoll NIE sa jeszcze dodane.")
    if args.run:
        import subprocess
        try:
            subprocess.Popen([str(starter)], cwd=str(starter.parent))
        except OSError as exc:
            print(f"Nie udalo sie uruchomic GothicStarter: {exc}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
