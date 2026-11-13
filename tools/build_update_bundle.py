#!/usr/bin/env python3
"""Build the small updater ZIP without DLLs or private game/model assets."""
from __future__ import annotations

import argparse
import hashlib
import io
from pathlib import Path
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.file_transaction import FileTransaction


ROOT = Path(__file__).resolve().parents[1]
BUNDLE_FILES = (
    "AKTUALIZUJ_PARRY_HOTTER.bat",
    "docs/AKTUALIZACJA-DLL.md",
    "docs/TESTY-STEROWANIA-HP2.md",
    "tools/analyze_controls_log.py",
    "tools/__init__.py",
    "tools/update_hud.py",
    "tools/download_hud.py",
    "tools/verify_dll.py",
    "tools/spell_catalog.py",
    "tools/vdf.py",
    "tools/file_transaction.py",
)


class UpdateBundleError(RuntimeError):
    pass


def build_update_bundle(output: Path) -> Path:
    """Write an explicit source-only updater bundle and its SHA-256 file."""
    output = Path(output).expanduser().resolve()
    if output.suffix.lower() != ".zip":
        raise UpdateBundleError("Paczka aktualizatora musi miec rozszerzenie .zip")

    root = ROOT.resolve()
    sources = {}
    for relative in BUNDLE_FILES:
        source = root / relative
        if (not source.is_file() or source.is_symlink() or
                not source.resolve().is_relative_to(root)):
            raise UpdateBundleError(f"Brak zwyklego pliku aktualizatora: {relative}")
        sources[relative] = source.read_bytes()

    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for relative, content in sources.items():
            # A fixed timestamp makes the bundle independent of local checkout
            # mtimes. Preserve source bytes and extract straight into the project.
            entry = zipfile.ZipInfo(relative, date_time=(1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, content)

    data = buffer.getvalue()
    digest = hashlib.sha256(data).hexdigest()
    checksum = output.with_suffix(output.suffix + ".sha256")
    output.parent.mkdir(parents=True, exist_ok=True)
    with FileTransaction(output.parent) as files:
        files.write_bytes(output, data)
        files.write_bytes(checksum, f"{digest}  {output.name}\n".encode("utf-8"))
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description="Zbuduj mala paczke aktualizatora DLL")
    parser.add_argument("--out", type=Path, required=True, help="Wynikowy plik ZIP")
    args = parser.parse_args()
    try:
        result = build_update_bundle(args.out)
    except (UpdateBundleError, OSError) as error:
        parser.exit(1, f"BLAD: {error}\n")
    print(f"Paczka aktualizatora: {result}")
    print(f"Suma SHA-256: {result.with_suffix(result.suffix + '.sha256')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
