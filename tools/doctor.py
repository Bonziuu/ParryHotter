#!/usr/bin/env python3
"""Read-only local Gothic I / Parry Hotter diagnostics; no private data upload."""
from __future__ import annotations

import argparse
import datetime as dt
from pathlib import Path

DEFAULT_GAME = Path(r"C:\Programy\Steam\steamapps\common\Gothic")
REQUIRED_TEXTURES = (
    "PH_FLIPENDO.TGA", "PH_LEVICORPUS.TGA", "PH_RICTUSEMPRA.TGA",
    "PH_EXPELLIARMUS.TGA", "PH_BUMBARDO.TGA", "PH_AVADA.TGA",
    "PH_TARGET_FLIPENDO.TGA", "PH_TARGET_LEVICORPUS.TGA",
    "PH_TARGET_RICTUSEMPRA.TGA", "PH_TARGET_EXPELLIARMUS.TGA",
    "PH_TARGET_BUMBARDO.TGA", "PH_TARGET_AVADA.TGA",
    "PH_SELECTED.TGA", "PH_RETICLE.TGA",
)


def case_path(base: Path, *parts: str) -> Path:
    """Look up existing components without assuming Windows path case."""
    found = base
    for name in parts:
        if found.is_dir():
            match = next((p for p in found.iterdir() if p.name.casefold() == name.casefold()), None)
            if match is not None:
                found = match
                continue
        found = found / name
    return found


def option_value(path: Path, key: str) -> str:
    if not path.is_file():
        return "BRAK PLIKU"
    try:
        for raw in path.read_bytes().decode("latin-1").splitlines():
            line = raw.strip()
            if not line or line.startswith((";", "#")) or "=" not in line:
                continue
            k, value = line.split("=", 1)
            if k.strip().casefold() == key.casefold():
                return value.strip()
    except OSError:
        pass
    return "NIE USTAWIONO"


def report(game: Path) -> str:
    game = game.expanduser().resolve()
    script = case_path(game, "_work", "Data", "Scripts")
    content = case_path(script, "Content")
    sys = case_path(game, "System")
    dat = case_path(script, "_compiled", "Gothic.dat")
    source = case_path(content, "Magic", "ParryHotter.d")
    subtitles = case_path(content, "Cutscene", "OU.bin")
    output = []
    def check(label: str, path: Path) -> None:
        output.append(("[OK] " if path.is_file() else "[BRAK] ") + label)

    output.append("Parry Hotter — lokalna diagnostyka instalacji")
    output.append("Data: " + dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    output.append("Katalog Gothic 1: " + str(game))
    output.append("")
    check("GothicStarter_mod.exe", case_path(sys, "GothicStarter_mod.exe"))
    check("Profil DEV", case_path(sys, "ParryHotter.ini"))
    check("Profil PLAY", case_path(sys, "ParryHotter-PLAY.ini"))
    check("ParryHotter.mod", case_path(game, "Data", "ModVDF", "ParryHotter.mod"))
    check("Zrodlo ParryHotter.d", source)
    check("Polski plik OU.bin (obecnosc; jezyk NIE zweryfikowany)", subtitles)
    check("Skompilowany Gothic.dat", dat)
    if dat.is_file():
        data = dat.read_bytes().lower()
        output.append(
            ("[OK] " if b"ph_rune_flipendo" in data else "[UWAGA] ")
            + "Runa Flipendo w skompilowanym Gothic.dat"
        )
        if source.is_file() and dat.stat().st_mtime < source.stat().st_mtime:
            output.append("[UWAGA] Gothic.dat jest starszy niz ParryHotter.d — wykonaj Reparse")
    tex = case_path(game, "_work", "Data", "Textures")
    missing = [name for name in REQUIRED_TEXTURES if not case_path(tex, name).is_file()]
    output.append(
        f"[{'OK' if not missing else 'BRAK'}] Ikony HUD: "
        f"{len(REQUIRED_TEXTURES) - len(missing)}/{len(REQUIRED_TEXTURES)}"
    )
    if missing:
        output.append("Brakuje ikon: " + ", ".join(missing))
    output.append("")
    output.append("Ustawienia sterowania:")
    output.append("Gothic.ini / enableMouse = " + option_value(case_path(sys, "Gothic.ini"), "enableMouse"))
    output.append("SystemPack.ini / Gothic2_Control = " + option_value(case_path(sys, "SystemPack.ini"), "Gothic2_Control"))
    output.append("Uwaga: tryb PPM + LPM wymaga wtyczki Union w profilu PLAY.")
    output.append("Wersja DLL wewnatrz ParryHotter.mod nie jest weryfikowana tym testem.")
    output.append("Nie sprawdzono animacji, postaci ani czarow w silniku Gothic.")
    return "\n".join(output) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Parry Hotter read-only game checks")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--out", type=Path, default=Path("ParryHotter_diagnostics.txt"))
    args = parser.parse_args()
    text = report(args.game)
    args.out.write_text(text, encoding="utf-8")
    print(text)
    print("Raport: " + str(args.out.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
