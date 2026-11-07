"""Offline tests: original Harry visual only activates after validated ASC+TGA."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.visual_install import install_visual, PatchError, validate_converted_assets, validate_tga
from tools.generate_hud_textures import generate as generate_test_icons
from tools.casting_assets import CASTING_ASSET_NAMES, CASTING_MDS, RELEASE_ASC
from tests.test_casting_assets import write_casting_assets


class VisualInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name) / "Gothic"
        self.script = self.game / "_work" / "DATA" / "scripts" / "Content"
        (self.script / "Story" / "NPC").mkdir(parents=True)
        (self.script / "Items").mkdir()
        (self.script / "Gothic.src").write_text(
            "ITEMS\\ARMOR.D\nSTORY\\NPC\\*.D\n", encoding="latin-1"
        )
        (self.script / "Story" / "NPC" / "PC_Hero.d").write_text(
            'INSTANCE PC_Hero(NPC_DEFAULT)\n{\n'
            'Mdl_SetVisualBody(self,"hum_body_Naked0",4,1,"Hum_Head_Pony",9,0,KDF_ARMOR_L);\n'
            '};\nINSTANCE PC_Hero_L2(NPC_DEFAULT)\n{\n name = "test";\n'
            'Mdl_SetVisualBody(self,"hum_body_Naked0",4,1,"Hum_Head_Pony",9,0,-1);\n};\n',
            encoding="latin-1",
        )
        self.anims = self.game / "_work" / "DATA" / "Anims"
        self.anims.mkdir(parents=True)
        (self.anims / "HUMANS.MDS").write_text(
            'Model ("HuS")\n{\n    registerMesh ("Hum_Body_Naked0.ASC")\n'
            '    aniEnum\n    {\n    }\n}\n', encoding="latin-1"
        )
        self.output = self.game / "ParryHotter-private" / "HP2-Harry-converted"
        self.output.mkdir(parents=True)

    def ready_files(self):
        fixture = Path(__file__).parent / "fixtures" / "harry_triangle.asc.txt"
        (self.output / "PH_HARRY.ASC").write_bytes(fixture.read_bytes())
        icons = Path(self.temp.name) / "icons"
        generate_test_icons(icons)
        source_texture = icons / "PH_FLIPENDO.TGA"
        for i in range(4):
            (self.output / f"PH_HARRY_TEX{i}.TGA").write_bytes(source_texture.read_bytes())

    def test_no_model_keeps_stock_robe_and_does_not_touch_scripts(self):
        src = (self.script / "Gothic.src").read_bytes()
        self.assertFalse(install_visual(self.game))
        self.assertEqual((self.script / "Gothic.src").read_bytes(), src)
        self.assertFalse((self.script / "Items" / "ParryHarryArmor.d").exists())

    def test_missing_texture_rejected_before_file_changes(self):
        self.ready_files()
        (self.output / "PH_HARRY_TEX2.TGA").unlink()
        old_mds = (self.anims / "HUMANS.MDS").read_bytes()
        with self.assertRaises(PatchError):
            install_visual(self.game)
        self.assertEqual((self.anims / "HUMANS.MDS").read_bytes(), old_mds)
        self.assertFalse((self.script / "Items" / "ParryHarryArmor.d").exists())

    def test_valid_model_prepares_item_mds_hero_and_source(self):
        self.ready_files()
        self.assertTrue(install_visual(self.game))
        hero = (self.script / "Story" / "NPC" / "PC_Hero.d").read_text("latin-1")
        self.assertIn("PH_ARMOR_HARRY);", hero)
        player, alternate = hero.split("INSTANCE PC_Hero_L2", 1)
        self.assertIn('"hum_body_Naked0",4,1,"",9,0,PH_ARMOR_HARRY', player)
        self.assertNotIn("Hum_Head_Pony", player)
        self.assertIn('"Hum_Head_Pony",9,0,-1', alternate)
        self.assertIn('INSTANCE PC_Hero_L2', hero)
        src = (self.script / "Gothic.src").read_text("latin-1")
        self.assertIn("ITEMS\\PARRYHARRYARMOR.D", src)
        self.assertTrue((self.script / "Items" / "ParryHarryArmor.d").exists())
        mds = (self.anims / "HUMANS.MDS").read_text("latin-1")
        self.assertIn('registerMesh ("PH_HARRY.ASC")', mds)
        self.assertTrue((self.anims / "PH_HARRY.ASC").exists())
        self.assertTrue((self.game / "_work" / "DATA" / "Textures" / "PH_HARRY_TEX0.TGA").exists())
        self.assertTrue(install_visual(self.game))
        self.assertEqual(hero, (self.script / "Story" / "NPC" / "PC_Hero.d").read_text("latin-1"))
        self.assertEqual(mds, (self.anims / "HUMANS.MDS").read_text("latin-1"))
        self.assertEqual(src, (self.script / "Gothic.src").read_text("latin-1"))
        self.assertTrue((self.anims / "HUMANS.MDS.parryhotter-original").exists())

    def test_existing_unrelated_mesh_is_not_overwritten(self):
        self.ready_files()
        (self.anims / "PH_HARRY.ASC").write_text("Some other mod's mesh")
        with self.assertRaises(PatchError):
            install_visual(self.game)
        self.assertIn("other mod", (self.anims / "PH_HARRY.ASC").read_text())
        self.assertNotIn(
            "PARRYHARRYARMOR", (self.script / "Gothic.src").read_text()
        )

    def test_casting_overlay_and_clips_are_installed_only_for_pc_hero(self):
        self.ready_files()
        write_casting_assets(self.output)
        self.assertTrue(install_visual(self.game))
        player, alternate = (self.script / "Story" / "NPC" / "PC_Hero.d").read_text().split("INSTANCE PC_Hero_L2", 1)
        self.assertEqual(player.count(CASTING_MDS), 1)
        self.assertNotIn(CASTING_MDS, alternate)
        for name in CASTING_ASSET_NAMES:
            self.assertEqual((self.anims / name).read_bytes(), (self.output / name).read_bytes())
            self.assertFalse((self.game / "_work" / "DATA" / "Textures" / name).exists())

    def test_incomplete_casting_assets_fail_before_visual_registration(self):
        self.ready_files()
        write_casting_assets(self.output)
        (self.output / RELEASE_ASC).unlink()
        before = {p: p.read_bytes() for p in self.game.rglob('*') if p.is_file()}
        with self.assertRaisesRegex(PatchError, "Incomplete"):
            install_visual(self.game)
        self.assertEqual(before, {p: p.read_bytes() for p in self.game.rglob('*') if p.is_file()})

    def test_direct_installer_rolls_back_sources_backups_and_assets_after_late_failure(self):
        self.ready_files()
        before = {str(p.relative_to(self.game)): p.read_bytes() for p in self.game.rglob("*") if p.is_file()}
        directories = {p for p in self.game.rglob("*") if p.is_dir()}
        failure = PermissionError("injected last texture replacement failure")
        replace = Path.replace
        injected = False

        def fail_once(path, target):
            nonlocal injected
            if Path(target).name == "PH_HARRY_TEX3.TGA" and not injected:
                injected = True
                self.assertTrue((self.anims / "PH_HARRY.ASC").exists())
                self.assertIn("PH_ARMOR_HARRY", (self.script / "Story" / "NPC" / "PC_Hero.d").read_text())
                raise failure
            return replace(path, target)

        with mock.patch.object(Path, "replace", fail_once):
            with self.assertRaises(PermissionError) as raised:
                install_visual(self.game)
        self.assertIs(raised.exception, failure)
        self.assertTrue(injected)
        self.assertEqual(before, {str(p.relative_to(self.game)): p.read_bytes() for p in self.game.rglob("*") if p.is_file()})
        self.assertEqual(directories, {p for p in self.game.rglob("*") if p.is_dir()})

    def test_marker_only_fake_model_is_rejected(self):
        self.ready_files()
        (self.output / "PH_HARRY.ASC").write_text(
            '*3DSMAX_ASCIIEXPORT 200\n*GEOMOBJECT { *NODE_NAME "Harry" }\n'
            '*MESH_SOFTSKINVERTS { Bip01 }\n' + '// not geometry\n' * 30
        )
        with self.assertRaises(PatchError):
            validate_converted_assets(self.output)

    def test_invalid_mesh_and_skin_are_rejected_before_install(self):
        for original, broken in (
            (b"C: 2", b"C: 9"),
            (b"*MESH_NUMVERTEX 3", b"*MESH_NUMVERTEX 4"),
            (b'1 "Bip01 Spine2" 1.0', b'0'),
            (b'1 "Bip01 Spine2" 1.0', b'1 "Bip01 Missing" 1.0'),
            (b'1 "Bip01 Spine2" 1.0', b'1 "Bip01 Spine2" 0.1'),
            (b"1.0 0.0 0.0", b"nan 0.0 0.0"),
        ):
            with self.subTest(broken=broken):
                self.ready_files()
                asc = self.output / "PH_HARRY.ASC"
                asc.write_bytes(asc.read_bytes().replace(original, broken, 1))
                before = (self.script / "Gothic.src").read_bytes()
                with self.assertRaises(PatchError):
                    install_visual(self.game)
                self.assertEqual((self.script / "Gothic.src").read_bytes(), before)

    def test_texture_header_with_truncated_pixels_is_rejected(self):
        self.ready_files()
        path = self.output / "PH_HARRY_TEX0.TGA"
        path.write_bytes(path.read_bytes()[:150])
        with self.assertRaisesRegex(PatchError, "Truncated"):
            validate_tga(path)

    def test_truncated_and_overflowing_rle_are_rejected(self):
        self.ready_files()
        path = self.output / "PH_HARRY_TEX0.TGA"
        header = bytearray(path.read_bytes()[:18])
        header[2] = 10
        header[12:16] = b"\x04\x00\x04\x00"
        path.write_bytes(header + b"\x8f\x00")
        with self.assertRaisesRegex(PatchError, "Invalid TGA RLE"):
            validate_tga(path)
        path.write_bytes(header + b"\xff\x00\x00\x00\xff")
        with self.assertRaisesRegex(PatchError, "Invalid TGA RLE"):
            validate_tga(path)

    def test_valid_rle_pixels_are_accepted(self):
        self.ready_files()
        path = self.output / "PH_HARRY_TEX0.TGA"
        header = bytearray(path.read_bytes()[:18])
        header[2] = 10
        header[12:16] = b"\x04\x00\x04\x00"
        path.write_bytes(header + b"\x8f\x00\x00\x00\xff")
        validate_tga(path)


if __name__ == "__main__":
    unittest.main()
