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
from tools.generate_hud_textures import generate as generate_hud_textures
from tools.download_hud import HudDownloadError, fetch_verified_hud
from tools.wand_asset import prepare_wand, WandError


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
    if b"flipendo" not in raw.lower() or b"ph_rune_flipendo" not in raw.lower():
        raise PackagingError(
            "Gothic.dat nie zawiera zaklec Parry Hotter. "
            "Ponownie skompiluj skrypty przez GothicStarter_mod."
        )
    return compiled, subtitle


def build_mod(
    game: Path, vdfs_executable: Path | None = None,
    hud_dll: Path | None = None,
) -> Path:
    game = game.expanduser().resolve()
    if not (game / "System").is_dir():
        raise PackagingError(f"Nie znaleziono Gothic/System: {game}")
    compiler = (vdfs_executable or locate_vdfs(game)).expanduser().resolve()
    if not compiler.is_file():
        raise PackagingError(f"Nie ma GothicVDFS.exe: {compiler}")
    dat, subtitles = validate_compiled(game)
    # The mesh is embedded in the user's locally built .mod. Offline
    # procedural fallback ensures the plugin never references a missing wand.
    try:
        prepare_wand(game, allow_download=False)
    except (WandError, OSError, ValueError) as exc:
        raise PackagingError(f"Failed to prepare Gothic 3DS wand: {exc}") from exc
    # 6 slot icons, 6 target glyphs, active frame and center dot.
    generated_sprites = generate_hud_textures(game / "_work" / "Data" / "Textures")
    if len(generated_sprites) != 14:
        raise PackagingError("Nie wygenerowano wszystkich 14 elementow HUD")

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
            dll = fetch_verified_hud(stage / "hud_download" / "ParryHotterHUD-G1.dll")
        if not dll.is_file() or dll.read_bytes()[:2] != b"MZ":
            raise PackagingError("Nieprawidlowy experimentalny DLL HUD Union")
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
        armor_src = game / "_work" / "Data" / "Scripts" / "Content" / "Items" / "ParryHarryArmor.d"
        if armor_src.is_file():
            if b"ph_armor_harry" not in dat.read_bytes().lower():
                raise PackagingError(
                    "Harry armor script was installed but Gothic.dat is stale. "
                    "Use Reparse Scripts again."
                )
            hero_src = game / "_work" / "Data" / "Scripts" / "Content" / "Story" / "NPC" / "PC_Hero.d"
            if dat.stat().st_mtime + 2 < max(armor_src.stat().st_mtime, hero_src.stat().st_mtime):
                raise PackagingError("Converted Harry visual is newer than Gothic.dat: Reparse Scripts")
            private_assets = [
                (game / "_work" / "Data" / "Anims" / "PH_HARRY.ASC",
                 Path("_work/Data/Anims/PH_HARRY.ASC")),
                (game / "_work" / "Data" / "Anims" / "HUMANS.MDS",
                 Path("_work/Data/Anims/HUMANS.MDS")),
            ]
            for i in range(4):
                name = f"PH_HARRY_TEX{i}.TGA"
                private_assets.append((
                    game / "_work" / "Data" / "Textures" / name,
                    Path("_work/Data/Textures") / name
                ))
            for source, rel in private_assets:
                if not source.is_file():
                    raise PackagingError(f"Harry visual installed but asset is missing: {source}")
                if source.stat().st_size == 0:
                    raise PackagingError(f"Empty Harry visual asset: {source}")
                destination = stage / rel
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
                manifest.append(rel.as_posix())
            print("Added privately converted Harry outfit, HUMANS.MDS and four textures.")

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
        destination = game / "Data" / "ModVDF"
        destination.mkdir(parents=True, exist_ok=True)
        outfile = destination / "ParryHotter.mod"
        shutil.copy2(result, outfile)

    # Create a separate *release* profile. Keep the development one unchanged
    # for new experiments and source reparsing.
    ini = game / "System" / "ParryHotter-PLAY.ini"
    dev = game / "System" / "ParryHotter.ini"
    if not dev.is_file():
        raise PackagingError("Brak System/ParryHotter.ini")
    text = dev.read_text(encoding="latin-1")
    if re.search(r"(?im)^vdf=.*$", text):
        text = re.sub(r"(?im)^vdf=.*$", "vdf=ParryHotter.mod", text)
    else:
        raise PackagingError("ParryHotter.ini does not define VDF=")
    text = re.sub(r"(?im)^force_parameters=.*$", "force_parameters=", text)
    text = re.sub(r"(?im)^title=.*$", "title=Parry Hotter (PLAY)", text)
    ini.write_text(text, encoding="latin-1")
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
