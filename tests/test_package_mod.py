"""Tests of the separate ParryHotter.mod build, without proprietary assets."""

import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.package_mod import PackagingError, build_mod, validate_compiled


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Gothic"
        self.system = self.game / "System"
        self.system.mkdir(parents=True)
        self.src = self.game / "_work" / "Data" / "Scripts" / "Content" / "Magic" / "ParryHotter.d"
        self.src.parent.mkdir(parents=True)
        self.src.write_text("test source", encoding="ascii")
        self.dat = self.game / "_work" / "Data" / "Scripts" / "_compiled" / "Gothic.dat"
        self.dat.parent.mkdir(parents=True)
        self.dat.write_bytes(
            b"Fake test data only; PH_Rune_Flipendo; Flipendo;"
            + b"\x00" * 1024
        )
        self.subtitle = self.game / "_work" / "Data" / "Scripts" / "Content" / "Cutscene" / "Ou.bin"
        self.subtitle.parent.mkdir(parents=True)
        self.subtitle.write_bytes(b"Polski OU test")
        self.vdfs = self.game / "GothicVDFS.exe"
        self.vdfs.write_bytes(b"not actually run")
        self.hud_dll = self.game / "ParryHotterHUD-G1.dll"
        self.hud_dll.write_bytes(b"MZ" + b"mock native DLL" * 100)

        (self.system / "ParryHotter.ini").write_text(
            "[INFO]\ntitle=Parry Hotter (DEV)\n"
            "[FILES]\nvdf=\n[OPTIONS]\nforce_parameters=-zreparse\n",
            encoding="ascii",
        )
        source_mtime = self.src.stat().st_mtime
        os.utime(self.dat, (source_mtime + 5, source_mtime + 5))

    def test_validate_compiled_requires_mod_marker(self):
        validate_compiled(self.game)
        self.dat.write_bytes(b"Gothic stock scripts only")
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)

    def test_uppercase_dat_symbols_are_accepted(self):
        # ZenGin can emit normalized uppercase instance names.
        self.dat.write_bytes(
            b"Compiled bytecode PH_RUNE_FLIPENDO, FLIPENDO, AVADA KEDAVRA"
        )
        validate_compiled(self.game)

    def test_build_separate_release_and_unchanged_dev_ini(self):
        def fake_builder(cmd, *, cwd, **kwargs):
            self.assertEqual(cmd[1], "/B")
            stage = Path(cwd)
            vm = (stage / "build.vm").read_text()
            self.assertIn("_work/Data/Scripts/_compiled/Gothic.dat", vm)
            self.assertIn("_work/Data/Scripts/Content/Cutscene/Ou.bin", vm)
            self.assertIn("System/Autorun/ParryHotterHUD-G1.dll", vm)
            self.assertIn("_work/Data/Textures/PH_TARGET_RICTUSEMPRA.TGA", vm)
            self.assertTrue((stage / "System" / "Autorun" / "ParryHotterHUD-G1.dll").is_file())
            textures = list((stage / "_work" / "Data" / "Textures").glob("*.TGA"))
            self.assertEqual(len(textures), 14)

            (stage / "ParryHotter.mod").write_bytes(b"mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="ok", stderr="")

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder):
            result = build_mod(self.game, self.vdfs, self.hud_dll)
        self.assertTrue(result.is_file())
        play = (self.system / "ParryHotter-PLAY.ini").read_text(encoding="latin-1")
        dev = (self.system / "ParryHotter.ini").read_text(encoding="latin-1")
        self.assertIn("vdf=ParryHotter.mod", play)
        self.assertIn("force_parameters=", play)
        self.assertNotIn("force_parameters=-zreparse", play)
        self.assertIn("title=Parry Hotter (PLAY)", play)
        self.assertIn("title=Parry Hotter (DEV)", dev)
        self.assertIn("force_parameters=-zreparse", dev)

    def test_packages_private_harry_costume_only_after_compilation(self):
        items = self.game / "_work" / "Data" / "Scripts" / "Content" / "Items"
        items.mkdir(parents=True)
        armor = items / "ParryHarryArmor.d"
        armor.write_text("INSTANCE PH_ARMOR_HARRY(C_Item) {}", encoding="ascii")
        hero = self.game / "_work" / "Data" / "Scripts" / "Content" / "Story" / "NPC" / "PC_Hero.d"
        hero.parent.mkdir(parents=True)
        hero.write_text("PC hero visual override", encoding="ascii")
        anims = self.game / "_work" / "Data" / "Anims"
        anims.mkdir(parents=True)
        (anims / "PH_HARRY.ASC").write_bytes(b"*GEOMOBJECT *MESH")
        (anims / "HUMANS.MDS").write_bytes(b'registerMesh ("PH_HARRY.ASC")')
        textures = self.game / "_work" / "Data" / "Textures"
        textures.mkdir(parents=True)
        for i in range(4):
            (textures / f"PH_HARRY_TEX{i}.TGA").write_bytes(b"private test TGA")
        self.dat.write_bytes(b"Flipendo PH_Rune_Flipendo PH_ARMOR_HARRY")
        stamp = max(self.src.stat().st_mtime, armor.stat().st_mtime,
                    hero.stat().st_mtime) + 5
        os.utime(self.dat, (stamp, stamp))

        def fake_builder(cmd, *, cwd, **kwargs):
            stage = Path(cwd)
            vm = (stage / "build.vm").read_text()
            self.assertIn("_work/Data/Anims/PH_HARRY.ASC", vm)
            self.assertIn("_work/Data/Anims/HUMANS.MDS", vm)
            self.assertIn("_work/Data/Textures/PH_HARRY_TEX3.TGA", vm)
            self.assertEqual((stage / "_work/Data/Anims/PH_HARRY.ASC").read_bytes(),
                             b"*GEOMOBJECT *MESH")
            self.assertEqual(len(list((stage / "_work/Data/Textures").glob("*.TGA"))), 18)
            (stage / "ParryHotter.mod").write_bytes(b"mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="", stderr="")

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder):
            self.assertTrue(build_mod(self.game, self.vdfs, self.hud_dll).exists())

    def test_missing_private_harry_costume_blocks_incomplete_mod(self):
        items = self.game / "_work" / "Data" / "Scripts" / "Content" / "Items"
        items.mkdir(parents=True)
        (items / "ParryHarryArmor.d").write_text("visual_change=PH_HARRY.ASC")
        # No compiled armor symbol, so do not make a broken .mod.
        with self.assertRaises(PackagingError):
            build_mod(self.game, self.vdfs, self.hud_dll)

    def test_missing_polish_output_blocks_build(self):
        self.subtitle.unlink()
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)


if __name__ == "__main__":
    unittest.main()
