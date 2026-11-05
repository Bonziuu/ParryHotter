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
            'func void B_AssessMagic_IceCube()\n{\n    freeze();\n};\n'
            'func void B_AssessMagic_ThunderBall()\n{\n    foo();\n};\n'
            'func void B_AssessMagic_Breathofdeath()\n{\n    bar();\n};\n'
            'func void B_AssessMagic_Berzerk()\n{\n    baz();\n};\n'
            'func void B_AssessMagic_Stormfist()\n{\n    gust();\n};\n',
            encoding="latin-1",
        )
        (self.content / "Magic" / "Spells_Params.d").write_text(
            'const int SPL_DAMAGE_BREATHOFDEATH = 100; // normal\n',
            encoding="latin-1",
        )

    def test_install_creates_scripts_and_preserves_originals(self):
        changed = install(self.content)
        self.assertEqual(len(changed), 4)
        self.assertTrue((self.content / "Magic" / "ParryHotter.d").is_file())
        hero = (self.content / "Story" / "NPC" / "PC_Hero.d").read_text(encoding="latin-1")
        self.assertIn('name = "Parry Hotter"', hero)
        self.assertEqual(hero.count("CreateInvItem(self, PH_Rune_"), 6)
        self.assertIn('INSTANCE PC_Hero_L2', hero)
        hooks = (self.content / "Magic" / "B_AssessMagic.d").read_text(encoding="latin-1")
        self.assertIn("PH_OnAvadaHit();", hooks)
        self.assertIn("PH_OnLevicorpusHit(); return;", hooks)
        self.assertEqual(hooks.count("PH_OnLevicorpusHit();"), 1)
        self.assertIn("PH_OnExpelliarmusHit();", hooks)
        self.assertIn("PH_IsSelectedSpell(2)", hooks)
        self.assertIn("PH_IsSelectedSpell(3)", hooks)
        self.assertIn("PH_IsSelectedSpell(4)", hooks)
        self.assertIn("PH_IsSelectedSpell(5)", hooks)
        self.assertIn("PH_IsSelectedSpell(6)", hooks)
        self.assertIn("PH_OnBumbardoHit();", hooks)
        params = (self.content / "Magic" / "Spells_Params.d").read_text("latin-1")
        self.assertIn("SPL_DAMAGE_BREATHOFDEATH = 100;", params)
        self.assertFalse((self.content / "Magic" / "Spells_Params.d.parryhotter.bak").exists())
        self.assertIn("Magic\\ParryHotter.d", (self.content / "Gothic.src").read_text(encoding="latin-1"))
        self.assertEqual(
            (self.content / "Story" / "NPC" / "PC_Hero.d.parryhotter.bak").read_text(encoding="latin-1").count("Parry Hotter"),
            0,
        )

    def test_starting_gothic_mage_robe_on_realistic_player_visual(self):
        hero_path = self.content / "Story" / "NPC" / "PC_Hero.d"
        original = hero_path.read_text(encoding="latin-1")
        original = original.replace(
            "    CreateInvItem(self, ItWr_Fire_Letter_01);",
            '    Mdl_SetVisual (self,"HUMANS.MDS");\n'
            '    Mdl_SetVisualBody (self,"hum_body_Naked0", 4, 1,'
            ' "Hum_Head_Pony", 9, 0, -1);\n'
            '    CreateInvItem(self, ItWr_Fire_Letter_01);',
        )
        hero_path.write_text(original, encoding="latin-1")
        install(self.content)
        changed = hero_path.read_text(encoding="latin-1")
        self.assertIn('"Hum_Head_Pony", 9, 0, KDF_ARMOR_L);', changed)
        self.assertEqual(changed.count("KDF_ARMOR_L"), 1)
        self.assertEqual(install(self.content), [])
        # Preserve the unchanged test NPC's visual and item definitions.
        self.assertIn('INSTANCE PC_Hero_L2', changed)

    def test_existing_v01_hero_gets_mage_overlay_once(self):
        install(self.content)
        hero_path = self.content / "Story" / "NPC" / "PC_Hero.d"
        source = hero_path.read_text(encoding="latin-1")
        source = source.replace('    Mdl_ApplyOverlayMds(self, "Humans_Mage.mds");\n', "")
        hero_path.write_text(source, encoding="latin-1")
        self.assertEqual([p.name for p in install(self.content)], ["PC_Hero.d"])
        fixed = hero_path.read_text(encoding="latin-1")
        self.assertEqual(fixed.count('Mdl_ApplyOverlayMds(self, "Humans_Mage.mds");'), 1)
        self.assertEqual(install(self.content), [])

    def test_upgrades_legacy_hooks_without_double_hit(self):
        path = self.content / "Magic" / "B_AssessMagic.d"
        original = path.read_text(encoding="latin-1")
        legacy = {
            "ThunderBall": "PH_OnExpelliarmusHit();",
            "Breathofdeath": "PH_OnAvadaHit();",
            "IceCube": "if (Npc_IsPlayer(other) && C_NpcIsHuman(self)) { PH_OnLevicorpusHit(); return; };",
            "Berzerk": "if (Npc_IsPlayer(other) && C_NpcIsHuman(self)) { PH_OnRictusempraHit(); return; };",
        }
        for function, callback in legacy.items():
            start = "func void B_AssessMagic_" + function + "()\n{"
            self.assertIn(start, original)
            original = original.replace(start, start + "\n    " + callback)
        path.write_text(original, encoding="latin-1")
        install(self.content)
        patched = path.read_text(encoding="latin-1")
        for callback in (
            "PH_OnExpelliarmusHit();", "PH_OnAvadaHit();",
            "PH_OnLevicorpusHit();", "PH_OnRictusempraHit();",
            "PH_OnBumbardoHit();",
        ):
            self.assertEqual(patched.count(callback), 1, callback)
        self.assertNotIn(
            "if (Npc_IsPlayer(other) && C_NpcIsHuman(self)) { PH_OnRictusempraHit();",
            patched,
        )
        self.assertEqual(install(self.content), [])

    def test_restores_old_breath_damage_only_with_proven_backup(self):
        path = self.content / "Magic" / "Spells_Params.d"
        backup = path.with_name(path.name + ".parryhotter.bak")
        backup.write_text("const int SPL_DAMAGE_BREATHOFDEATH = 100; // original\\n",
                          encoding="latin-1")
        path.write_text("const int SPL_DAMAGE_BREATHOFDEATH = 1; // legacy Parry\\n",
                        encoding="latin-1")
        install(self.content)
        self.assertIn("SPL_DAMAGE_BREATHOFDEATH = 100;",
                      path.read_text("latin-1"))
        self.assertEqual(install(self.content), [])

    def test_preserves_custom_breath_damage_without_backup(self):
        path = self.content / "Magic" / "Spells_Params.d"
        path.write_text("const int SPL_DAMAGE_BREATHOFDEATH = 70; // custom\\n",
                        encoding="latin-1")
        install(self.content)
        self.assertIn("SPL_DAMAGE_BREATHOFDEATH = 70;",
                      path.read_text("latin-1"))

    def test_second_install_is_noop(self):
        install(self.content)
        self.assertEqual(install(self.content), [])

    def test_dry_run_does_not_change_disk(self):
        self.assertEqual(len(install(self.content, dry_run=True)), 4)
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
