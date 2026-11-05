"""Make Gothic 1 spell casting possible with one left mouse click.

Uses SystemPack/Union's established Gothic2_Control=1 option; no untested
C++ keyboard hook, global Windows keyboard events, or DLL injection.
Preserves INI formatting/comments and keeps a backup of the original file.
"""
from __future__ import annotations

import re
import shutil
from pathlib import Path


def set_existing_option(path: Path, key: str, value: str) -> bool:
    if not path.is_file():
        return False
    data = path.read_bytes()
    text = data.decode("latin-1")
    newline = "\r\n" if "\r\n" in text else "\n"
    expression = re.compile(
        r"(?im)^(?![ \t]*[;#])[ \t]*" + re.escape(key)
        + r"[ \t]*=[ \t]*[^\r\n]*"
    )
    matches = list(expression.finditer(text))
    if len(matches) != 1:
        return False
    old_line = matches[0].group(0)
    if old_line.strip().lower() == f"{key}={value}".lower():
        return False
    updated = text[:matches[0].start()] + f"{key}={value}" + text[matches[0].end():]
    backup = path.with_name(path.name + ".parryhotter-original")
    if not backup.exists():
        shutil.copy2(path, backup)
    path.write_bytes(updated.encode("latin-1"))
    return True



def read_option(path: Path, key: str) -> str | None:
    if not path.is_file():
        return None
    content = path.read_bytes().decode("latin-1")
    expression = re.compile(
        r"(?im)^(?![ \\t]*[;#])[ \\t]*" + re.escape(key)
        + r"[ \\t]*=[ \\t]*([^\\r\\n]*)"
    )
    matches = expression.findall(content)
    return matches[0].strip() if len(matches) == 1 else None


def restore_legacy_gothic2_control(path: Path) -> bool:
    backup = path.with_name(path.name + ".parryhotter-original")
    old_value = read_option(backup, "Gothic2_Control")
    current_value = read_option(path, "Gothic2_Control")
    # Previous versions of ParryHotter changed 0 => 1. Only revert that
    # precise case; preserve user changes and every other SystemPack option.
    if old_value == "0" and current_value == "1":
        return set_existing_option(path, "Gothic2_Control", "0")
    return False


def configure_mouse(game: Path) -> list[Path]:
    game = game.expanduser().resolve()
    system = game / "System"
    modified = []
    config = (("Gothic.ini", "enableMouse", "1"),)
    for filename, key, value in config:
        # Case-insensitive file matching, but don't create new foreign options
        # if the file doesn't already expose this setting.
        source = next(
            (p for p in system.iterdir() if p.name.lower() == filename.lower()),
            None,
        ) if system.is_dir() else None
        if source and set_existing_option(source, key, value):
            modified.append(source)
    # Revert only *our own* legacy global G2 controls change, if backed up.
    systempack = next(
        (p for p in system.iterdir() if p.name.lower() == "systempack.ini"),
        None
    ) if system.is_dir() else None
    if systempack and restore_legacy_gothic2_control(systempack):
        modified.append(systempack)
    return modified


def main():
    from argparse import ArgumentParser

    parser = ArgumentParser()
    parser.add_argument(
        "--game", type=Path,
        default=Path(r"C:\Programy\Steam\steamapps\common\Gothic"),
    )
    args = parser.parse_args()
    changed = configure_mouse(args.game)
    for path in changed:
        print(f"Zaktualizowano ustawienia myszy: {path}")
    if not changed:
        print("Brak zmian: mysz juz wlaczona lub klucz enableMouse nie istnieje.")
    print("Sterowanie PPM + LPM wymaga eksperymentalnej wtyczki Union w profilu PLAY.")
    print("Rolka myszy nie wybiera jeszcze naszych 6 zaklec.")


if __name__ == "__main__":
    main()
