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


def configure_mouse(game: Path) -> list[Path]:
    game = game.expanduser().resolve()
    system = game / "System"
    modified = []
    config = (
        ("SystemPack.ini", "Gothic2_Control", "1"),
        ("Gothic.ini", "enableMouse", "1"),
    )
    for filename, key, value in config:
        # Case-insensitive file matching, but don't create new foreign options
        # if the file doesn't already expose this setting.
        source = next(
            (p for p in system.iterdir() if p.name.lower() == filename.lower()),
            None,
        ) if system.is_dir() else None
        if source and set_existing_option(source, key, value):
            modified.append(source)
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
        print(f"Ustawiono prosty klik mysza: {path}")
    if not changed:
        print("Brak zmian. Upewnij sie, ze opcje enableMouse i Gothic2_Control istnieja w plikach INI.")
    print("Wybierz rune w grze i nacisnij LPM: tryb Gothic2_Control=1.")
    print("Rolka myszy nie wybiera jeszcze naszych 6 zaklec.")


if __name__ == "__main__":
    main()
