#!/usr/bin/env python3
"""Update only the DLL in an existing local ParryHotter.mod, with backups."""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import io
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.download_hud import EXPECTED_NAME, HudDownloadError, fetch_verified_hud
from tools.file_transaction import FileTransaction
from tools.vdf import VdfError, build_vdf, read_vdf
from tools.verify_dll import DllError, inspect_dll

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")
DLL_ENTRY = "SYSTEM/AUTORUN/PARRYHOTTERHUD-G1.DLL"
DAT_ENTRY = "_WORK/DATA/SCRIPTS/_COMPILED/GOTHIC.DAT"
MAX_MOD_BYTES = 512 * 1024 * 1024


class UpdateError(RuntimeError):
    pass


@dataclass(frozen=True)
class UpdateResult:
    archive: Path
    dll: Path
    sha256: str
    changed: bool
    backups: tuple[Path, ...]


def ensure_game_closed() -> None:
    """Do not terminate the game or replace its mounted archive."""
    if sys.platform != "win32":
        return
    tasklist = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "tasklist.exe"
    try:
        result = subprocess.run(
            [str(tasklist), "/FO", "CSV", "/NH"], capture_output=True,
            text=True, errors="replace", timeout=20,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise UpdateError("Nie mozna sprawdzic, czy Gothic jest zamkniety.") from exc
    if result.returncode or not result.stdout.strip():
        raise UpdateError("Nie mozna odczytac listy procesow. Aktualizacja przerwana.")
    try:
        rows = [row for row in csv.reader(io.StringIO(result.stdout), strict=True) if row]
    except csv.Error as exc:
        raise UpdateError("Nieprawidlowy wynik listowania procesow. Aktualizacja przerwana.") from exc
    if not rows or any(len(row) < 2 or not row[1].isdigit() for row in rows):
        raise UpdateError("Nieprawidlowy wynik listowania procesow. Aktualizacja przerwana.")
    for row in rows:
        if row[0].casefold() in {"gothic.exe", "gothicmod.exe"}:
            raise UpdateError("Zamknij Gothic i ponownie uruchom aktualizator. Gra nie zostala zamknieta automatycznie.")


def keep_backup(source: Path) -> Path:
    """Create a persistent sibling backup without overwriting any old one."""
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
    for number in range(1000):
        backup = source.with_name(f"{source.name}.parryhotter-backup-{stamp}-{number}")
        try:
            target = backup.open("xb")
        except FileExistsError:
            continue
        try:
            with target, source.open("rb") as stream:
                shutil.copyfileobj(stream, target)
                target.flush()
                os.fsync(target.fileno())
            shutil.copystat(source, backup)
        except BaseException:
            # Only the incomplete file created by this attempt may be removed.
            backup.unlink(missing_ok=True)
            raise
        return backup
    raise UpdateError(f"Nie mozna utworzyc kolejnej kopii zapasowej: {source}")


def update_hud(game: Path, project: Path = ROOT) -> UpdateResult:
    game = game.expanduser().resolve()
    project = project.expanduser().resolve()
    archive = game / "Data" / "ModVDF" / "ParryHotter.mod"
    cached = project / ".parryhotter" / "build" / EXPECTED_NAME
    if not (game / "System").is_dir() or not archive.is_file():
        raise UpdateError(f"Brak zainstalowanego ParryHotter.mod: {archive}. Najpierw zbuduj mod.")
    ensure_game_closed()

    with FileTransaction(project) as project_files, FileTransaction(game) as game_files:
        project_files.check_file(cached)
        game_files.check_file(archive)
        if archive.stat().st_size > MAX_MOD_BYTES:
            raise UpdateError("ParryHotter.mod przekracza limit aktualizatora (512 MB).")
        old_archive = archive.read_bytes()
        old_cached = cached.read_bytes() if cached.is_file() else None
        try:
            files = read_vdf(old_archive)
            if DLL_ENTRY not in files or b"ph_union_loaded" not in files.get(DAT_ENTRY, b"").lower():
                raise UpdateError("Archiwum nie zawiera aktualnego moda Parry Hotter z wtyczka Union.")
            inspect_dll(files[DLL_ENTRY])
        except (VdfError, DllError, UnicodeError) as exc:
            raise UpdateError(f"Nieprawidlowy istniejacy ParryHotter.mod: {exc}") from exc

        print("Pobieram najnowsza DLL z dev013-latest i sprawdzam SHA-256 oraz PE32...")
        with tempfile.TemporaryDirectory(prefix="ph-dll-update-") as temporary:
            downloaded = fetch_verified_hud(Path(temporary) / EXPECTED_NAME, tag="dev013-latest")
            new_dll = downloaded.read_bytes()
            report = inspect_dll(new_dll)

        mod_changed = files[DLL_ENTRY] != new_dll
        cache_changed = old_cached != new_dll
        new_archive = old_archive
        if mod_changed:
            expected = dict(files)
            expected[DLL_ENTRY] = new_dll
            comment = old_archive[:256].rstrip(b"\x1a\0").decode("ascii", "replace")
            new_archive = build_vdf(expected, comment=comment)
            if read_vdf(new_archive) != expected:
                raise UpdateError("Weryfikacja archiwum nie powiodla sie. Pliki gry nie zostaly zmienione.")

        # The download can take a while. Re-check before creating backups or
        # changing anything, including changes made by another updater.
        ensure_game_closed()
        if archive.read_bytes() != old_archive or (cached.read_bytes() if cached.is_file() else None) != old_cached:
            raise UpdateError("Pliki zmienily sie w trakcie pobierania. Uruchom aktualizator ponownie.")
        backups = []
        if mod_changed:
            backups.append(keep_backup(archive))
        if cache_changed and cached.is_file():
            backups.append(keep_backup(cached))
        for backup in backups:
            print(f"Kopia zapasowa: {backup}")
        if cache_changed:
            project_files.write_bytes(cached, new_dll)
        if mod_changed:
            game_files.write_bytes(archive, new_archive)
        # Verify the committed archive too; a failure rolls back both roots.
        if read_vdf(archive.read_bytes())[DLL_ENTRY] != new_dll or cached.read_bytes() != new_dll:
            raise UpdateError("Weryfikacja zapisanych plikow nie powiodla sie. Przywracam poprzednia wersje.")
    return UpdateResult(archive, cached, report.sha256,
                        mod_changed or cache_changed, tuple(backups))


def launch_starter(game: Path) -> None:
    game = game.expanduser().resolve()
    starter = game / "System" / "GothicStarter_mod.exe"
    if not starter.is_file():
        raise UpdateError(f"DLL zaktualizowana, ale brak GothicStarter_mod.exe: {starter}")
    subprocess.Popen([str(starter)], cwd=str(starter.parent))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Automatyczna aktualizacja DLL Parry Hotter z kopia zapasowa")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--project", type=Path, default=ROOT)
    parser.add_argument("--run", action="store_true", help="Po aktualizacji otworz GothicStarter_mod")
    args = parser.parse_args(argv)
    try:
        result = update_hud(args.game, args.project)
        print("Zaktualizowano DLL w modzie." if result.changed else "Masz juz aktualna DLL.")
        print(f"SHA-256: {result.sha256}")
        print("W GothicStarter wybierz Parry Hotter (PLAY). Reparse Scripts nie jest potrzebny.")
        if args.run:
            launch_starter(args.game)
    except (UpdateError, HudDownloadError, DllError, VdfError, OSError, subprocess.SubprocessError) as exc:
        parser.exit(1, f"BLAD: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
