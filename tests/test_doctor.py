"""Read-only diagnosis of an installed Parry Hotter folder."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from tools.doctor import report, REQUIRED_TEXTURES


class DoctorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Gothic"
        self.system = self.game / "System"
        self.system.mkdir(parents=True)

    def test_missing_installation_reports_not_present_without_changes(self):
        original = sorted(p.name for p in self.system.iterdir())
        result = report(self.game)
        self.assertIn("[BRAK] GothicStarter_mod.exe", result)
        self.assertIn("[BRAK] ParryHotter.mod", result)
        self.assertIn("Ikony HUD: 0/14", result)
        self.assertEqual(sorted(p.name for p in self.system.iterdir()), original)

    def test_complete_local_layout_checks_script_and_icons(self):
        (self.system / "GothicStarter_mod.exe").write_bytes(b"x")
        (self.system / "ParryHotter-PLAY.ini").write_text("play")
        (self.system / "Gothic.ini").write_text("; enableMouse=0\nenableMouse=1\n")
        (self.system / "SystemPack.ini").write_text("Gothic2_Control=0\n")
        src = self.game / "_work" / "DATA" / "scripts"
        (src / "Content" / "Magic").mkdir(parents=True)
        (src / "Content" / "Magic" / "ParryHotter.d").write_text("source")
        (src / "Content" / "Cutscene").mkdir()
        (src / "Content" / "Cutscene" / "OU.bin").write_bytes(b"PL")
        (src / "_compiled").mkdir()
        (src / "_compiled" / "Gothic.dat").write_bytes(b"PH_RUNE_FLIPENDO PH_SELECTED_SPELL PH_UNION_LOADED\x00")
        textures = self.game / "_work" / "DATA" / "Textures"
        textures.mkdir()
        for name in REQUIRED_TEXTURES:
            (textures / name).write_bytes(b"TGA")
        result = report(self.game)
        self.assertIn("[OK] Runa Flipendo", result)
        self.assertIn("[OK] Mostek HUD/Daedalus PH_SELECTED_SPELL", result)
        self.assertIn("[OK] Ochrona zwyklych czarow Gothic PH_UNION_LOADED", result)
        self.assertIn("[OK] Ikony HUD: 14/14", result)
        self.assertIn("enableMouse = 1", result)
        self.assertIn("Gothic2_Control = 0", result)


if __name__ == "__main__":
    unittest.main()
