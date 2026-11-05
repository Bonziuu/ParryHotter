"""Offline tests: original Harry visual only activates after validated ASC+TGA."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.visual_install import install_visual, PatchError
from tools.generate_hud_textures import generate as generate_test_icons


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
            '};\nINSTANCE PC_Hero_L2(NPC_DEFAULT)\n{\n name = "test";\n};\n',
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
        source = (
            '*3DSMAX_ASCIIEXPORT 200\n*GEOMOBJECT\n{\n'
            '*MESH {\n*BITMAP "PH_HARRY_TEX0.TGA"\n}\n}\n'
            '*MESH_SOFTSKINVERTS {\n1\n"Bip01 Spine2" 1.0\n}\n'
            + ("// geometry + weights\n" * 24)
        )
        (self.output / "PH_HARRY.ASC").write_text(source, encoding="latin-1")
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
        self.assertIn('INSTANCE PC_Hero_L2', hero)
        src = (self.script / "Gothic.src").read_text("latin-1")
        self.assertIn("ITEMS\\PARRYHARRYARMOR.D", src)
        self.assertTrue((self.script / "Items" / "ParryHarryArmor.d").exists())
        mds = (self.anims / "HUMANS.MDS").read_text("latin-1")
        self.assertIn('registerMesh ("PH_HARRY.ASC")', mds)
        self.assertTrue((self.anims / "PH_HARRY.ASC").exists())
        self.assertTrue((self.game / "_work" / "DATA" / "Textures" / "PH_HARRY_TEX0.TGA").exists())
        self.assertTrue(install_visual(self.game))
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


if __name__ == "__main__":
    unittest.main()
