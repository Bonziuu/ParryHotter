#!/usr/bin/env python3
"""Build ParryHotter.mod from *locally compiled* Gothic 1 scripts.

This script never redistributes the original game's data in the repository.
Requires GothicVDFS.exe from the user's Gothic 1 ModKit and the compiled DAT
created by GothicStarter_mod.exe with 'Reparse Scripts' enabled.

IMPORTANT: Does not compile Daedalus nor retarget Harry automatically.
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # BAT runs this file as a script
from tools.vdf import VdfError, write_vdf_from_stage
from tools.generate_hud_textures import generate as generate_hud_textures
from tools.download_hud import HudDownloadError, fetch_verified_hud, validate_hud_binary
from tools.wand_asset import prepare_wand, WandError
from tools.spell_catalog import REQUIRED_COMPILED_SYMBOLS, REQUIRED_TEXTURES
from tools.file_transaction import FileTransaction
from tools.casting_assets import (CASTING_MDS, SUPERSEDED_CASTING_MDS, canonical_casting_mds,
                                  validate_optional_animation_assets)
from tools.install import PatchError
from tools.harry_trousers import TrousersError, dress_harry


DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")


class PackagingError(RuntimeError):
    pass


def locate_vdfs(game: Path) -> Path:
    known = [
        game / "_work" / "tools" / "VDFS" / "GothicVDFS.exe",
        game / "_work" / "tools" / "GothicVDFS" / "GothicVDFS.exe",
        game / "System" / "GothicVDFS.exe",
        game / "GothicVDFS.exe",
    ]
    for path in known:
        if path.is_file():
            return path
    candidates = list(game.glob("**/GothicVDFS.exe"))
    if not candidates:
        raise PackagingError(
            "GothicVDFS.exe nie zostal znaleziony w ModKicie. "
            "Najpierw zainstaluj pelny Gothic 1 ModKit."
        )
    return candidates[0]


def validate_compiled(game: Path) -> tuple[Path, Path]:
    scripts = game / "_work" / "Data" / "Scripts"
    compiled = scripts / "_compiled" / "Gothic.dat"
    subtitle = scripts / "Content" / "Cutscene" / "Ou.bin"
    source = scripts / "Content" / "Magic" / "ParryHotter.d"

    if not compiled.is_file():
        raise PackagingError("Nie ma skompilowanego Gothic.dat. Wlacz Reparse Scripts.")
    if not source.is_file():
        raise PackagingError("Brak zainstalowanego ParryHotter.d")
    if not subtitle.is_file():
        raise PackagingError("Brak polskiego OU.bin")
    if compiled.stat().st_mtime + 2 < source.stat().st_mtime:
        raise PackagingError(
            "Gothic.dat jest starszy niz skrypty ParryHotter. "
            "Uruchom GothicStarter_mod z Reparse Scripts."
        )

    # Compiled Gothic DAT stores strings and instance names. Check for known
    # unique markers so we never package a stock Gothic.dat by accident.
    raw = compiled.read_bytes()
    # ZenGin and independent Daedalus compilers normalize symbol case.
    # Search case-insensitively to avoid rejecting correctly compiled DATs.
    if (b"flipendo" not in raw.lower() or
            any(symbol not in raw.lower() for symbol in REQUIRED_COMPILED_SYMBOLS)):
        raise PackagingError(
             "Gothic.dat nie zawiera aktualnych szesciu zaklec i dziesieciu slotow HUD Parry Hotter. "
            "Ponownie skompiluj skrypty przez GothicStarter_mod."
        )
    return compiled, subtitle


def release_profile(game: Path) -> tuple[Path, bytes]:
    """Validate the DEV profile before any generated assets or release changes."""
    dev = game / "System" / "ParryHotter.ini"
    if not dev.is_file():
        raise PackagingError("Brak System/ParryHotter.ini")
    raw = dev.read_bytes()
    text = raw.decode("latin-1").replace("\r\n", "\n").replace("\r", "\n")
    vdf = r"(?im)^[ \t]*vdf[ \t]*=.*$"
    if len(re.findall(vdf, text)) != 1:
        raise PackagingError("ParryHotter.ini must define exactly one VDF=")
    text = re.sub(vdf, "vdf=ParryHotter.mod", text)
    # PLAY reads the packaged .mod (no -zreparse) and, like DEV, always
    # starts windowed (-zwindow, the GothicStarter "Window Mode" switch).
    text = re.sub(r"(?im)^[ \t]*force_parameters[ \t]*=.*$", "force_parameters=-zwindow", text)
    text = re.sub(r"(?im)^[ \t]*title[ \t]*=.*$", "title=Parry Hotter (PLAY)", text)
    if b"\r\n" in raw:
        text = text.replace("\n", "\r\n")
    return game / "System" / "ParryHotter-PLAY.ini", text.encode("latin-1")


def refresh_casting_overlay(game: Path) -> bool:
    """Upgrade a superseded PH_HARRY_CAST.MDS and retire its compiled cache.

    The original is kept as a numbered backup. Compiled PH_HARRY_CAST files
    are moved (never deleted) so Gothic recompiles the overlay from source.
    """
    anims = game / "_work" / "Data" / "Anims"
    source = anims / CASTING_MDS
    if not source.is_file() or source.is_symlink():
        return False
    current = source.read_bytes()
    changed = False
    if current in SUPERSEDED_CASTING_MDS:
        for index in range(1000):
            backup = source.with_name(f"{source.name}.parryhotter-backup-{index}")
            if not backup.exists():
                backup.write_bytes(current)
                break
        else:
            raise PackagingError(f"Too many backups of {source}")
        source.write_bytes(canonical_casting_mds())
        changed = True
    compiled_dir = anims / "_compiled"
    stem = Path(CASTING_MDS).stem.upper()
    stale = [path for path in compiled_dir.glob("*") if path.is_file()
             and path.name.upper().startswith(stem)
             and (changed or path.stat().st_mtime + 2 < source.stat().st_mtime)]
    if stale:
        retired = compiled_dir / "parryhotter-stale"
        for index in range(1000):
            target = retired / str(index)
            if not target.exists():
                break
        target.mkdir(parents=True)
        for path in stale:
            shutil.move(str(path), str(target / path.name))
        changed = True
    return changed


def refresh_harry_trousers(game: Path) -> list[Path]:
    """Dress Harry's bare legs; retire the stale compiled textures."""
    asc = game / "_work" / "Data" / "Anims" / "PH_HARRY.ASC"
    textures = game / "_work" / "Data" / "Textures"
    if not asc.is_file() or asc.is_symlink():
        return []
    try:
        changed = dress_harry(asc, textures)
    except (TrousersError, ValueError) as exc:
        # Cosmetic only: an unexpected model layout keeps the converted texture.
        print(f"Uwaga: pominieto ciemne spodnie Harry'ego ({exc}).")
        return []
    except OSError as exc:
        raise PackagingError(f"Nie udalo sie ubrac nog Harry'ego: {exc}") from exc
    compiled_dir = textures / "_compiled"
    stale = []
    for texture in textures.glob("PH_HARRY_TEX*.TGA"):
        stem = texture.stem.upper()
        stale += [path for path in compiled_dir.glob("*") if path.is_file()
                  and path.name.upper() in (f"{stem}-C.TEX", f"{stem}.TEX")
                  and (texture in changed or path.stat().st_mtime + 2 < texture.stat().st_mtime)]
    if stale:
        retired = compiled_dir / "parryhotter-stale"
        for index in range(1000):
            target = retired / str(index)
            if not target.exists():
                break
        target.mkdir(parents=True)
        for path in stale:
            shutil.move(str(path), str(target / path.name))
    return changed


def private_harry_assets(game: Path, dat: Path) -> list[tuple[Path, Path]]:
    """Preflight the optional appearance/animation sources before generating files."""
    content = game / "_work" / "Data" / "Scripts" / "Content"
    armor = content / "Items" / "ParryHarryArmor.d"
    if not armor.is_file():
        return []
    compiled = dat.read_bytes().lower()
    if b"ph_armor_harry" not in compiled:
        raise PackagingError(
            "Harry armor script was installed but Gothic.dat is stale. "
            "Use Reparse Scripts again."
        )
    hero = content / "Story" / "NPC" / "PC_Hero.d"
    if not hero.is_file():
        raise PackagingError(f"Harry visual installed but hero source is missing: {hero}")
    if dat.stat().st_mtime + 2 < max(armor.stat().st_mtime, hero.stat().st_mtime):
        raise PackagingError("Converted Harry visual is newer than Gothic.dat: Reparse Scripts")
    anims = game / "_work" / "Data" / "Anims"
    try:
        casting = validate_optional_animation_assets(anims)
    except PatchError as exc:
        raise PackagingError(f"Invalid Harry casting animation sources: {exc}") from exc
    if casting:
        if CASTING_MDS.lower().encode("ascii") not in compiled:
            raise PackagingError(
                "Harry casting overlay was installed but Gothic.dat is stale. "
                "Use Reparse Scripts again."
            )
    elif re.search(
            rb'Mdl_ApplyOverlayMds\s*\(\s*self\s*,\s*"PH_HARRY_CAST\.MDS"\s*\)',
            hero.read_bytes(), re.IGNORECASE):
        raise PackagingError("PC_Hero references PH_HARRY_CAST.MDS but its animation sources are missing")
    assets = [
        (anims / "PH_HARRY.ASC", Path("_work/Data/Anims/PH_HARRY.ASC")),
        (anims / "HUMANS.MDS", Path("_work/Data/Anims/HUMANS.MDS")),
    ]
    assets.extend((source, Path("_work/Data/Anims") / source.name) for source in casting)
    for index in range(4):
        name = f"PH_HARRY_TEX{index}.TGA"
        assets.append((game / "_work" / "Data" / "Textures" / name,
                       Path("_work/Data/Textures") / name))
    for source, _ in assets:
        if not source.is_file():
            raise PackagingError(f"Harry visual installed but asset is missing: {source}")
        if source.stat().st_size == 0:
            raise PackagingError(f"Empty Harry visual asset: {source}")
    return assets


def build_mod(
    game: Path, vdfs_executable: Path | None = None,
    hud_dll: Path | None = None,
) -> Path:
    game = game.expanduser().resolve()
    if not (game / "System").is_dir():
        raise PackagingError(f"Nie znaleziono Gothic/System: {game}")
    ini, profile = release_profile(game)
    outfile = game / "Data" / "ModVDF" / "ParryHotter.mod"
    with FileTransaction(game) as files:
        files.check_file(ini)
        files.check_file(outfile)
    if vdfs_executable is not None:
        compiler = vdfs_executable.expanduser().resolve()
        if not compiler.is_file():
            raise PackagingError(f"Nie ma GothicVDFS.exe: {compiler}")
    else:
        try:
            compiler = locate_vdfs(game).expanduser().resolve()
        except PackagingError:
            # Steam installs often lack ModKit tools; tools/vdf.py writes the
            # same G1 format (byte-identical on original Piranha archives).
            compiler = None
    dat, subtitles = validate_compiled(game)
    harry_assets = private_harry_assets(game, dat)
    # Only after the read-only preflight accepted the private assets.
    if harry_assets and refresh_casting_overlay(game):
        print("Zaktualizowano PH_HARRY_CAST.MDS (animacje Harry'ego pod nazwami czarow Gothica);"
              " stara kompilacja przeniesiona do Anims\\_compiled\\parryhotter-stale.")
    if harry_assets and refresh_harry_trousers(game):
        print("Harry ma teraz ciemne spodnie pod szata (nogi nie przeswituja jasno).")
    # The mesh is embedded in the user's locally built .mod. Offline
    # procedural fallback ensures the plugin never references a missing wand.
    try:
        prepare_wand(game, allow_download=False)
    except (WandError, OSError, ValueError) as exc:
        raise PackagingError(f"Failed to prepare Gothic 3DS wand: {exc}") from exc
    # Six spell icons/glyphs, active frame, center dot and shared empty slot.
    generated_sprites = generate_hud_textures(game / "_work" / "Data" / "Textures")
    if {p.name for p in generated_sprites} != set(REQUIRED_TEXTURES):
        raise PackagingError("Nie wygenerowano wszystkich 15 elementow HUD")

    with tempfile.TemporaryDirectory(prefix="ph_vdfs_") as temp:
        stage = Path(temp)
        dat_rel = Path("_work/Data/Scripts/_compiled/Gothic.dat")
        ou_rel = Path("_work/Data/Scripts/Content/Cutscene/Ou.bin")
        for src, rel in ((dat, dat_rel), (subtitles, ou_rel)):
            dst = stage / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

        # Source icons are original and reproducible, safe to package.
        manifest = [
            "_work/Data/Scripts/_compiled/Gothic.dat",
            "_work/Data/Scripts/Content/Cutscene/Ou.bin"
        ]
        # Load only inside ParryHotter.mod via Union virtual */Autorun.
        # Never install an untested native plugin globally in Gothic/System.
        dll = hud_dll
        if dll is None:
            local = Path(__file__).resolve().parents[1] / ".parryhotter" / "build" / "ParryHotterHUD-G1.dll"
            dll = local if local.is_file() else fetch_verified_hud(stage / "hud_download" / "ParryHotterHUD-G1.dll")
        if not dll.is_file() or dll.read_bytes()[:2] != b"MZ":
            raise PackagingError("Nieprawidlowy experimentalny DLL HUD Union")
        try:
            validate_hud_binary(dll.read_bytes())
        except HudDownloadError as exc:
            raise PackagingError(str(exc)) from exc
        plugin_rel = Path("System/Autorun/ParryHotterHUD-G1.dll")
        plugin_dst = stage / plugin_rel
        plugin_dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(dll, plugin_dst)
        manifest.append(plugin_rel.as_posix())

        for source, rel in (
            (game / "_work" / "Data" / "Meshes" / "PH_WAND.3DS",
             Path("_work/Data/Meshes/PH_WAND.3DS")),
            (game / "_work" / "Data" / "Textures" / "PH_WAND.TGA",
             Path("_work/Data/Textures/PH_WAND.TGA")),
        ):
            if not source.is_file() or source.stat().st_size < 128:
                raise PackagingError(f"Missing prepared wand asset: {source}")
            dst = stage / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, dst)
            manifest.append(rel.as_posix())

        for sprite in generated_sprites:
            rel = Path("_work/Data/Textures") / sprite.name
            dst = stage / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(sprite, dst)
            manifest.append(rel.as_posix())

        # Bundle the privately converted Harry mesh ONLY for the user's
        # locally generated .mod. Never upload game/model assets to GitHub.
        if harry_assets:
            for source, rel in harry_assets:
                destination = stage / rel
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
                manifest.append(rel.as_posix())
            print("Added privately converted Harry outfit, model scripts, textures and available casting sources.")

        if compiler is None:
            result = stage / "ParryHotter.mod"
            try:
                write_vdf_from_stage(stage, manifest, result, comment="ParryHotter personal test mod")
            except (VdfError, OSError) as exc:
                raise PackagingError(f"Nie udalo sie zapisac .mod: {exc}") from exc
            print(f"Zbudowano .mod wbudowanym zapisem VDFS ({len(manifest)} plikow), bez GothicVDFS.exe.")
            with FileTransaction(game) as files:
                files.copy_file(result, outfile)
                files.write_bytes(ini, profile)
            return outfile

        vdf_script = stage / "build.vm"
        vdf_script.write_text(
            "[BEGINVDF]\n"
            "Comment=ParryHotter personal test mod\n"
            "BaseDir=.\n"
            "VDFName=ParryHotter.mod\n"
            "[FILES]\n"
            + "".join(item + "\n" for item in manifest)
            + "[EXCLUDE]\n"
            "[INCLUDE]\n"
            "[ENDVDF]\n",
            encoding="ascii",
        )
        proc = subprocess.run(
            [str(compiler), "/B", str(vdf_script)],
            cwd=stage,
            capture_output=True,
            text=True,
            timeout=180,
        )
        result = stage / "ParryHotter.mod"
        if proc.returncode or not result.is_file() or result.stat().st_size < 512:
            raise PackagingError(
                "GothicVDFS nie utworzyl poprawnego .mod.\n"
                + (proc.stderr or proc.stdout)[-1400:]
            )
        # Commit the archive and matching PLAY profile together. A failed
        # replacement restores both previous artifacts and their timestamps.
        with FileTransaction(game) as files:
            files.copy_file(result, outfile)
            files.write_bytes(ini, profile)
    return outfile


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a separate ParryHotter.mod")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--vdfs", type=Path, help="Optional exact GothicVDFS.exe path")
    parser.add_argument("--hud-dll", type=Path, help="For offline tests: local compiled G1 Union DLL")
    args = parser.parse_args()
    try:
        result = build_mod(args.game, args.vdfs, args.hud_dll)
    except (PackagingError, HudDownloadError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"BLAD: {error}\n")
    print(f"Zbudowano mod: {result}")
    print("Odtad uruchamiaj 'Parry Hotter (PLAY)' w GothicStarter.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
