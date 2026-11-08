"""Install the Polish System scripts (menu, camera, music, effects).

tools/setup.py installs only _work/Data/Scripts/Content from the pinned
Unified-PL sources, so dialogues become Polish but the main menu, compiled
from _work/Data/Scripts/System/Menu.src, stays in the base game language.
Parry Hotter does not modify any System script, so the whole directory is
replaced from the same pinned archive after a full backup.

    py -3 -m tools.polish_system --game "C:\\Programy\\Steam\\steamapps\\common\\Gothic"
"""
from __future__ import annotations

import argparse
import datetime as dt
import filecmp
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

from tools.install import PatchError

REQUIRED_SYSTEM_FILES = (
    "menu.src",
    "menu/menu_main.d",
    "menu/menu_opt.d",
    "camera.src",
    "music.src",
    "sfx.src",
    "particlefx.src",
    "visualfx.src",
    "_intern/menu.d",
)
MIN_SYSTEM_FILES = 25


def _find_case(root: Path, *parts: str) -> Path:
    current = root
    for part in parts:
        candidate = current / part
        if not candidate.exists() and current.is_dir():
            candidate = next((p for p in current.iterdir() if p.name.lower() == part.lower()), candidate)
        current = candidate
    return current


def unpack_polish_system(zip_path: Path, destination: Path) -> int:
    """Extract only _work/Data/Scripts/System, rejecting unexpected paths."""
    count = 0
    prefix = "/_work/data/scripts/system/"
    names: set[str] = set()
    with zipfile.ZipFile(zip_path) as archive:
        for item in archive.infolist():
            filename = item.filename.replace("\\", "/")
            idx = filename.lower().find(prefix)
            if idx < 0 or item.is_dir():
                continue
            rel = filename[idx + len(prefix):]
            parts = Path(rel).parts
            if not parts or any(p in ("", ".", "..") for p in parts) or ":" in rel:
                raise PatchError(f"Unsafe ZIP entry: {filename}")
            target = destination.joinpath(*parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            names.add("/".join(parts).lower())
            count += 1
    missing = [name for name in REQUIRED_SYSTEM_FILES if name not in names]
    if missing:
        raise PatchError("Polish System scripts missing: " + ", ".join(missing))
    if count < MIN_SYSTEM_FILES:
        raise PatchError("Suspiciously few System script files in the Polish source archive")
    return count


def _same_tree(left: Path, right: Path) -> bool:
    comparison = filecmp.dircmp(left, right)
    if comparison.left_only or comparison.right_only or comparison.funny_files:
        return False
    _, mismatch, errors = filecmp.cmpfiles(left, right, comparison.common_files, shallow=False)
    if mismatch or errors:
        return False
    return all(_same_tree(left / name, right / name) for name in comparison.common_dirs)


def install_polish_system(game: Path, *, zip_path: Path | None = None) -> Path | None:
    """Replace the System scripts; return the backup path, or None if already Polish."""
    game = Path(game).expanduser().resolve()
    scripts = _find_case(game, "_work", "Data", "Scripts")
    if not (scripts / "Content").is_dir() and not _find_case(scripts, "Content").is_dir():
        raise PatchError(f"Gothic 1 ModKit scripts not found: {scripts}")
    system = _find_case(scripts, "System")

    with tempfile.TemporaryDirectory(prefix="parryhotter_system_") as temp_name:
        temp = Path(temp_name)
        archive = zip_path
        if archive is None:
            from tools.setup import download_polish_scripts
            archive = temp / "polish_scripts.zip"
            print("Pobieram polskie skrypty Gothic 1 z GitHub (Unified-PL)...")
            download_polish_scripts(archive)
        stage = temp / "System"
        stage.mkdir()
        count = unpack_polish_system(archive, stage)

        if system.is_dir() and _same_tree(stage, system):
            print("Polskie skrypty System (menu) sa juz zainstalowane.")
            return None

        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        backup = scripts / f"System_ParryHotter_Backup_{stamp}"
        if backup.exists():
            raise PatchError(f"Backup directory already exists: {backup}")
        had_system = system.is_dir()
        try:
            if had_system:
                system.rename(backup)
            try:
                shutil.copytree(stage, system)
            except Exception:
                if system.exists():
                    shutil.rmtree(system)
                if had_system:
                    backup.rename(system)
                raise
        except PatchError:
            raise
        except Exception as exc:
            raise PatchError(f"Cannot install Polish System scripts: {exc}") from exc

    print(f"Zainstalowano {count} polskich skryptow System (menu, kamera, muzyka, efekty).")
    if had_system:
        print(f"Kopia zapasowa poprzednich skryptow System: {backup}")
        return backup
    return system


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Install Polish Gothic I System scripts (menu)")
    parser.add_argument("--game", type=Path, default=Path(r"C:\Programy\Steam\steamapps\common\Gothic"))
    parser.add_argument("--zip", dest="zip_path", type=Path, help="Offline test with Unified-PL ZIP")
    args = parser.parse_args(argv)
    try:
        install_polish_system(args.game, zip_path=args.zip_path)
    except (PatchError, OSError, zipfile.BadZipFile) as exc:
        print(f"BLAD: {exc}", file=sys.stderr)
        return 1
    print("W GothicStarter_mod zaznacz Reparse Scripts, aby skompilowac polskie menu.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
