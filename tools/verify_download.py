"""CI smoke test against the actual pinned Polish source archive.

No commercial game files are distributed and Gothic is NOT launched here.
"""
from __future__ import annotations

import tempfile
from pathlib import Path

from tools.setup import download_polish_scripts, install_bundle, POLISH_SOURCE_COMMIT


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="parry_ci_") as temp_name:
        temp = Path(temp_name)
        game = temp / "Gothic"
        content = game / "_work" / "DATA" / "scripts" / "Content"
        content.mkdir(parents=True)
        (content / "Gothic.src").write_text("GERMAN PLACEHOLDER")
        system = game / "System"
        system.mkdir()
        (system / "GothicStarter_mod.exe").write_bytes(b"CI stub - not executed")
        zip_path = temp / "Unified-PL.zip"
        print(f"Downloading Gothic Unified-PL commit {POLISH_SOURCE_COMMIT}...")
        download_polish_scripts(zip_path)
        starter = install_bundle(game, zip_path=zip_path)
        hero = (content / "Story" / "NPC" / "PC_Hero.d").read_bytes()
        assert b"Parry Hotter" in hero
        assert b"Humans_Mage.mds" in hero
        assert (content / "Magic" / "ParryHotter.d").exists()
        assert (content / "Cutscene" / "Ou.bin").exists()
        assert starter.is_file()
        print("OK: Polish scripts + Parry Hotter work together structurally.")
        print("NOTE: Gothic engine / Daedalus compilation was NOT performed.")


if __name__ == "__main__":
    main()
