#!/usr/bin/env python3
"""Install a validated local Harry ASC/texture bundle into Gothic source files.

This imports converted assets only. It neither downloads models nor compiles or
launches Gothic. Run Reparse Scripts and rebuild the local .mod afterwards.
"""
from __future__ import annotations

import argparse
import stat
import tempfile
import zipfile
from pathlib import Path

from tools.casting_assets import (
    CASTING_ASSET_NAMES, CASTING_MAX_BYTES, animation_rig_signature, upgrade_casting_mds,
    validate_optional_animation_assets,
)
from tools.file_transaction import FileTransaction
from tools.visual_install import (
    ARMOR_SCRIPT, OUTFIT, TEXTURES, PatchError, find_case, install_visual,
    register_armor_source, register_mesh, select_visual_armor,
    validate_converted_assets,
)

ASSET_NAMES = (OUTFIT,) + TEXTURES
MAX_ASSET_BYTES = {OUTFIT: 12 * 1024 * 1024,
                   **{name: 32 * 1024 * 1024 for name in TEXTURES},
                   **CASTING_MAX_BYTES}
MAX_ZIP_MEMBERS = 512


def _zip_parts(info: zipfile.ZipInfo) -> list[str]:
    name = info.filename.replace("\\", "/")
    parts = name.rstrip("/").split("/")
    if (not name or name.startswith("/") or "\x00" in name or
            any(part in ("", ".", "..") or ":" in part for part in parts) or
            stat.S_ISLNK(info.external_attr >> 16)):
        raise PatchError(f"Unsafe bundle archive entry: {info.filename}")
    return parts


def _read_assets(bundle: Path, destination: Path) -> None:
    expected = {name.casefold(): name for name in ASSET_NAMES + CASTING_ASSET_NAMES}
    if bundle.is_dir():
        members = {}
        for source in bundle.iterdir():
            canonical = expected.get(source.name.casefold())
            if canonical is None:
                continue
            if canonical in members:
                raise PatchError(f"Duplicate converted model asset: {canonical}")
            if source.is_symlink() or not source.is_file():
                raise PatchError(f"Bundle asset must be a regular file: {source.name}")
            if source.stat().st_size > MAX_ASSET_BYTES[canonical]:
                raise PatchError(f"Bundle asset exceeds its size limit: {canonical}")
            members[canonical] = source
        missing = set(ASSET_NAMES) - members.keys()
        if missing:
            raise PatchError("Converted model bundle missing: " + ", ".join(sorted(missing)))
        for canonical, source in members.items():
            with source.open("rb") as stream:
                data = stream.read(MAX_ASSET_BYTES[canonical] + 1)
            if len(data) > MAX_ASSET_BYTES[canonical]:
                raise PatchError(f"Bundle asset exceeds its size limit: {canonical}")
            (destination / canonical).write_bytes(data)
        return

    if not bundle.is_file() or not zipfile.is_zipfile(bundle):
        raise PatchError(f"Expected a converted-assets directory or ZIP: {bundle}")
    with zipfile.ZipFile(bundle) as archive:
        entries = archive.infolist()
        if len(entries) > MAX_ZIP_MEMBERS:
            raise PatchError("Converted model ZIP has too many entries")
        members = {}
        for info in entries:
            parts = _zip_parts(info)
            if info.is_dir():
                continue
            canonical = expected.get(parts[-1].casefold())
            if canonical is None:
                continue # Safe documentation/preview files are not extracted.
            if canonical in members:
                raise PatchError(f"Duplicate converted model asset: {canonical}")
            if info.file_size > MAX_ASSET_BYTES[canonical]:
                raise PatchError(f"Bundle asset exceeds its size limit: {canonical}")
            members[canonical] = info
        missing = set(ASSET_NAMES) - members.keys()
        if missing:
            raise PatchError("Converted model bundle missing: " + ", ".join(sorted(missing)))
        for canonical, info in members.items():
            with archive.open(info) as stream, (destination / canonical).open("wb") as output:
                total = 0
                while block := stream.read(min(65536, MAX_ASSET_BYTES[canonical] - total + 1)):
                    total += len(block)
                    if total > MAX_ASSET_BYTES[canonical]:
                        raise PatchError(f"Bundle asset exceeds its size limit: {canonical}")
                    output.write(block)


def validate_harry_bundle(bundle: Path) -> None:
    """Check a converted ZIP/directory without changing any game files."""
    bundle = Path(bundle).expanduser().resolve()
    with tempfile.TemporaryDirectory(prefix="parry-harry-check-") as directory:
        staged = Path(directory)
        _read_assets(bundle, staged)
        if upgrade_casting_mds(staged):
            print("Upgraded PH_HARRY_CAST.MDS: overlay now declares the human skeleton (fixes ani-bbox error).")
        validate_converted_assets(staged)
        validate_optional_animation_assets(staged)


def _safe_game_path(game: Path, path: Path, *, file: bool = False) -> None:
    if not path.resolve().is_relative_to(game):
        raise PatchError(f"Game path escapes the selected installation: {path}")
    for parent in path.parents:
        if parent == game:
            break
        if parent.exists() and not parent.is_dir():
            raise PatchError(f"Game directory path is occupied by a file: {parent}")
    if file and (path.is_symlink() or (path.exists() and not path.is_file())):
        raise PatchError(f"Game file path is not a regular file: {path}")


def _preflight(game: Path, assets: Path, names: tuple[str, ...]) -> tuple[Path, list[Path]]:
    system = find_case(game, "System")
    if not game.is_dir() or not system.is_dir():
        raise PatchError(f"Gothic/System directory not found: {system}")
    _safe_game_path(game, system)
    content = find_case(game, "_work", "Data", "Scripts", "Content")
    anims = find_case(game, "_work", "Data", "Anims")
    textures = find_case(game, "_work", "Data", "Textures")
    source = find_case(content, "Gothic.src")
    hero = find_case(content, "Story", "NPC", "PC_Hero.d")
    mds = find_case(anims, "HUMANS.MDS")
    for path in (source, hero, mds):
        _safe_game_path(game, path, file=True)
        if not path.is_file():
            raise PatchError(f"Source Gothic ModKit prerequisite is missing: {path}")
    # Verify every patch anchor before staging even the private asset cache.
    register_armor_source(source.read_bytes().decode("latin-1"))
    select_visual_armor(hero.read_bytes().decode("latin-1"), casting=bool(set(names) & set(CASTING_ASSET_NAMES)))
    register_mesh(mds.read_bytes().decode("latin-1"))
    if not ARMOR_SCRIPT.is_file():
        raise PatchError("ParryHarryArmor.d is missing from the project")

    converted = game / "ParryHotter-private" / "HP2-Harry-converted"
    _safe_game_path(game, converted)
    if converted.exists() and not converted.is_dir():
        raise PatchError(f"Private conversion directory is occupied by a file: {converted}")
    for name in names:
        _safe_game_path(game, converted / name, file=True)

    owned_updates = []
    changed = [source, hero, mds, find_case(content, "Items", "ParryHarryArmor.d")]
    for name in names:
        target = find_case(textures if name in TEXTURES else anims, name)
        changed.append(target)
        _safe_game_path(game, target, file=True)
        if target.is_file() and target.read_bytes() != (assets / name).read_bytes():
            if not target.with_name(target.name + ".parryhotter-original").is_file():
                previous = converted / name
                if previous.is_file() and previous.read_bytes() == target.read_bytes():
                    owned_updates.append(target)
                else:
                    raise PatchError(f"Refusing to replace an unrelated existing model asset: {target}")
    for path in changed:
        _safe_game_path(game, path, file=True)
        backup = path.with_name(path.name + ".parryhotter-original")
        _safe_game_path(game, backup, file=True)
        temporary = path.with_name(path.name + ".parryhotter.tmp")
        if temporary.exists() or temporary.is_symlink():
            raise PatchError(f"Preserve the existing setup temporary file before importing: {temporary}")
    return converted, owned_updates


def _preserve_cached_animations(game: Path, staged: Path) -> None:
    """A legacy model-only update may keep clips only for the same bind rig."""
    if validate_optional_animation_assets(staged):
        return
    converted = game / "ParryHotter-private" / "HP2-Harry-converted"
    _safe_game_path(game, converted)
    if not converted.is_dir():
        return
    for name in CASTING_ASSET_NAMES:
        _safe_game_path(game, converted / name, file=True)
    previous = validate_optional_animation_assets(converted)
    if not previous:
        return
    if (animation_rig_signature((converted / OUTFIT).read_bytes()) !=
            animation_rig_signature((staged / OUTFIT).read_bytes())):
        raise PatchError("Existing casting animations use a different bind rig; supply matching animation assets")
    for source in previous:
        (staged / source.name).write_bytes(source.read_bytes())
    upgrade_casting_mds(staged)
    validate_optional_animation_assets(staged)


def _backup_cache(path: Path, current: bytes, transaction: FileTransaction) -> None:
    for index in range(1000):
        suffix = ".parryhotter-original" if index == 0 else f".parryhotter-backup-{index}"
        candidate = path.with_name(path.name + suffix)
        if candidate.exists() or candidate.is_symlink():
            if not candidate.is_symlink() and candidate.is_file() and candidate.read_bytes() == current:
                return
            continue
        transaction.write_bytes(candidate, current)
        return
    raise PatchError(f"Too many private asset backups: {path}")


def import_harry_bundle(game: Path, bundle: Path) -> Path:
    """Validate the bundle and ModKit before changing cache or game sources."""
    game = Path(game).expanduser().resolve()
    bundle = Path(bundle).expanduser().resolve()
    with tempfile.TemporaryDirectory(prefix="parry-harry-bundle-") as directory:
        staged = Path(directory)
        _read_assets(bundle, staged)
        if upgrade_casting_mds(staged):
            print("Upgraded PH_HARRY_CAST.MDS: overlay now declares the human skeleton (fixes ani-bbox error).")
        validate_converted_assets(staged)
        _preserve_cached_animations(game, staged)
        optional = validate_optional_animation_assets(staged)
        names = ASSET_NAMES + tuple(path.name for path in optional)
        converted, owned_updates = _preflight(game, staged, names)
        with FileTransaction(game) as files:
            # A first install has no original active asset to back up. On an
            # update, recognize it only while it still matches our old cache.
            for path in owned_updates:
                files.copy_file(path, path.with_name(path.name + ".parryhotter-original"))
            for name in names:
                destination = converted / name
                data = (staged / name).read_bytes()
                if destination.is_file():
                    current = destination.read_bytes()
                    if current == data:
                        continue
                    _backup_cache(destination, current, files)
                files.write_bytes(destination, data)
            if not install_visual(game, transaction=files):
                raise PatchError("Converted model source installation did not complete")
    print("Converted Harry source files installed; Reparse Scripts and rebuild .mod next.")
    print("Gothic engine model compilation and gameplay were not performed by this importer.")
    return converted


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True, type=Path, help="Gothic 1 installation with source ModKit")
    parser.add_argument("--bundle", required=True, type=Path, help="Converted PH_HARRY.ASC/4-TGA ZIP or directory")
    args = parser.parse_args()
    try:
        import_harry_bundle(args.game, args.bundle)
    except (PatchError, OSError, zipfile.BadZipFile, RuntimeError) as exc:
        parser.exit(1, f"BLAD IMPORTU MODELU: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
