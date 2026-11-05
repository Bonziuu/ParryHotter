"""Unit tests for local patching, no Gothic installation required."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.install import PatchError, install


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.content = Path(self.tmp.name) / "Content"
        (self.content / "Story" / "NPC").mkdir(parents=True)
        (self.content / "Magic").mkdir()
        (self.content / "Gothic.src").write_text(
            "Magic\\Spells_Params.d\nMagic\\B_AssessMagic.d\nStory\\NPC\\PC_Hero.d\n",
            encoding="latin-1",
        )
        (self.content / "Story" / "NPC" / "PC_Hero.d").write_text(
            'INSTANCE PC_Hero(NPC_DEFAULT)\n{\n'
            '    name = "Me";\n'
            '    attribute[ATR_MANA_MAX] = 5;\n'
            '    attribute[ATR_MANA] = 5;\n'
            '    CreateInvItem(self, ItWr_Fire_Letter_01);\n'
            '};\n'
            'INSTANCE PC_Hero_L2(NPC_DEFAULT)\n{\n    name = "Me";\n};\n',
            encoding="latin-1",
        )
        (self.content / "Magic" / "B_AssessMagic.d").write_text(
            'func void B_AssessMagic_ThunderBall()\n{\n    foo();\n};\n'
            'func void B_AssessMagic_Breathofdeath()\n{\n    bar();\n};\n',
            encoding="latin-1",
        )
        (self.content / "Magic" / "Spells_Params.d").write_text(
            'const int SPL_DAMAGE_BREATHOFDEATH = 100; // normal\n',
            encoding="latin-1",
        )

    def test_install_creates_scripts_and_preserves_originals(self):
        changed = install(self.content)
        self.assertEqual(len(changed), 5)
        self.assertTrue((self.content / "Magic" / "ParryHotter.d").is_file())
        hero = (self.content / "Story" / "NPC" / "PC_Hero.d").read_text(encoding="latin-1")
        self.assertIn('name = "Parry Hotter"', hero)
        self.assertEqual(hero.count("CreateInvItem(self, PH_Rune_"), 6)
        self.assertIn('INSTANCE PC_Hero_L2', hero)
        hooks = (self.content / "Magic" / "B_AssessMagic.d").read_text(encoding="latin-1")
        self.assertIn("PH_OnAvadaHit();", hooks)
        self.assertIn("PH_OnExpelliarmusHit();", hooks)
        self.assertIn("Magic\\ParryHotter.d", (self.content / "Gothic.src").read_text(encoding="latin-1"))
        self.assertEqual(
            (self.content / "Story" / "NPC" / "PC_Hero.d.parryhotter.bak").read_text(encoding="latin-1").count("Parry Hotter"),
            0,
        )

    def test_second_install_is_noop(self):
        install(self.content)
        self.assertEqual(install(self.content), [])

    def test_dry_run_does_not_change_disk(self):
        self.assertEqual(len(install(self.content, dry_run=True)), 5)
        self.assertFalse((self.content / "Magic" / "ParryHotter.d").exists())
        self.assertFalse((self.content / "Gothic.src.parryhotter.bak").exists())

    def test_missing_anchor_does_not_partially_edit(self):
        original = (self.content / "Magic" / "B_AssessMagic.d").read_bytes()
        (self.content / "Magic" / "B_AssessMagic.d").write_text("INVALID", encoding="latin-1")
        with self.assertRaises(PatchError):
            install(self.content)
        self.assertEqual((self.content / "Story" / "NPC" / "PC_Hero.d").read_text(encoding="latin-1").count("Parry Hotter"), 0)
        self.assertFalse((self.content / "Magic" / "ParryHotter.d").exists())


if __name__ == "__main__":
    unittest.main()
