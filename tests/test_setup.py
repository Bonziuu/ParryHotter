"""Offline tests for the Polish one-click installer and safe backup workflow."""

from __future__ import annotations

import tempfile
import unittest
import zipfile
from unittest import mock
from contextlib import ExitStack
from pathlib import Path

from tools.setup import install_bundle, PatchError, main


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name)
        self.game = base / "Gothic"
        self.scripts = self.game / "_work" / "DATA" / "scripts"
        self.content = self.scripts / "Content"
        self.content.mkdir(parents=True)
        self.system = self.game / "System"
        self.system.mkdir()
        (self.system / "GothicStarter_mod.exe").write_bytes(b"test stub")
        (self.content / "Gothic.src").write_text("GERMAN ORIGINAL", encoding="latin-1")

        self.zip_path = base / "PolishScripts.zip"
        self.fake_src = {
            "Gothic.src": "MAGIC\\SPELLS_PARAMS.D\nMAGIC\\B_ASSESSMAGIC.D\nSTORY\\NPC\\*.D\n",
            "Story/NPC/PC_Hero.d": (
                'INSTANCE PC_Hero(NPC_DEFAULT)\n{\n'
                '    name = "Ja";\n'
                '    attribute[ATR_MANA_MAX] = 5;\n'
                '    attribute[ATR_MANA] = 5;\n'
                '    CreateInvItem(self, ItWr_Fire_Letter_01);\n'
                '};\nINSTANCE PC_Hero_L2(NPC_DEFAULT)\n{\nname="Ja";\n};\n'
            ),
            "Magic/B_AssessMagic.d": (
                'func void B_AssessMagic_IceCube()\n{\n    freeze();\n};\n'
            'func void B_AssessMagic_ThunderBall()\n{\n    foo();\n};\n'
                'func void B_AssessMagic_Breathofdeath()\n{\n    bar();\n};\n'
            'func void B_AssessMagic_Berzerk()\n{\n    baz();\n};\n'
            'func void B_AssessMagic_Stormfist()\n{\n    gust();\n};\n'
            ),
            "Magic/Spells_Params.d": 'const int SPL_DAMAGE_BREATHOFDEATH = 100;\n',
            "Cutscene/Ou.bin": "POLISH DIALOGUE FIXTURE\n",
        }
        for index in range(120):
            self.fake_src[f"Story/Other/placeholder{index}.d"] = f"// {index}\n"
        self.create_archive()

    def create_archive(self):
        with zipfile.ZipFile(self.zip_path, "w") as archive:
            for path, data in self.fake_src.items():
                archive.writestr(
                    "gothic-1-classic-scripts-test/_work/Data/Scripts/Content/" + path,
                    data.encode("latin-1"),
                )

    def test_installs_polish_content_and_preserves_backup(self):
        starter = install_bundle(self.game, zip_path=self.zip_path)
        self.assertEqual(starter.name, "GothicStarter_mod.exe")
        hero = (self.content / "Story" / "NPC" / "PC_Hero.d").read_text("latin-1")
        self.assertIn('name = "Parry Hotter"', hero)
        self.assertIn('Mdl_ApplyOverlayMds(self, "Humans_Mage.mds")', hero)
        self.assertIn("PH_Rune_Avada", hero)
        self.assertNotIn("PH_Rune_Depulso", hero)
        self.assertEqual(hero.count("CreateInvItem(self, PH_Rune_"), 6)
        self.assertIn("PH_OnAvadaHit();",
            (self.content / "Magic" / "B_AssessMagic.d").read_text("latin-1"))
        self.assertEqual((self.content / "Cutscene" / "Ou.bin").read_text(), "POLISH DIALOGUE FIXTURE\n")
        backups = list(self.scripts.glob("Content_ParryHotter_Backup_*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / "Gothic.src").read_text(), "GERMAN ORIGINAL")
        self.assertTrue((self.system / "ParryHotter.ini").is_file())

    def test_bad_archive_does_not_modify_game(self):
        self.fake_src.pop("Magic/Spells_Params.d")
        self.create_archive()
        with self.assertRaises(PatchError):
            install_bundle(self.game, zip_path=self.zip_path)
        self.assertEqual((self.content / "Gothic.src").read_text(), "GERMAN ORIGINAL")
        self.assertFalse((self.system / "ParryHotter.ini").exists())
        self.assertFalse(list(self.scripts.glob("Content_ParryHotter_Backup_*")))

    def test_converted_bundle_route_skips_source_download_and_blender(self):
        bundle = Path(self.tmp.name) / "model.zip"
        with ExitStack() as stack:
            stack.enter_context(mock.patch("sys.argv", ["setup", "--game", str(self.game), "--model-bundle", str(bundle)]))
            stack.enter_context(mock.patch("tools.setup.install_bundle", return_value=self.system / "GothicStarter_mod.exe"))
            stack.enter_context(mock.patch("tools.setup.generate_hud_textures", return_value=[]))
            stack.enter_context(mock.patch("tools.setup.prepare_wand", return_value=False))
            stack.enter_context(mock.patch("tools.setup.configure_mouse", return_value=[]))
            validator = stack.enter_context(mock.patch("tools.import_harry_bundle.validate_harry_bundle"))
            importer = stack.enter_context(mock.patch("tools.import_harry_bundle.import_harry_bundle"))
            source = stack.enter_context(mock.patch("tools.setup.prepare_model"))
            blender = stack.enter_context(mock.patch("tools.setup.prepare_blender_project"))
            stage = stack.enter_context(mock.patch("tools.setup.stage_export"))
            self.assertEqual(main(), 0)
            validator.assert_called_once_with(bundle)
            importer.assert_called_once_with(self.game, bundle)
            source.assert_not_called()
            blender.assert_not_called()
            stage.assert_not_called()

    def test_invalid_explicit_bundle_is_a_failed_installation(self):
        # Use the real preflight and real install_bundle. A missing model used
        # to return failure only after replacing Content and the DEV profile.
        profile = self.system / "ParryHotter.ini"
        profile.write_bytes(b"existing profile\r\n")
        before = {p.relative_to(self.game): p.read_bytes()
                  for p in self.game.rglob("*") if p.is_file()}
        with ExitStack() as stack:
            stack.enter_context(mock.patch("sys.argv", ["setup", "--game", str(self.game),
                "--zip", str(self.zip_path), "--model-bundle", str(Path(self.tmp.name) / "missing.zip")]))
            self.assertEqual(main(), 1)
        after = {p.relative_to(self.game): p.read_bytes()
                 for p in self.game.rglob("*") if p.is_file()}
        self.assertEqual(after, before)
        self.assertFalse(list(self.scripts.glob("Content_ParryHotter_Backup_*")))


if __name__ == "__main__":
    unittest.main()
