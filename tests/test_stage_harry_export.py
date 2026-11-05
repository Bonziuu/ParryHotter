"""Pure-local staging of Harry ASC after manual KrxImpExp export."""
import tempfile
import unittest
from pathlib import Path

from tools.generate_hud_textures import generate
from tools.install import PatchError
from tools.stage_harry_export import stage_export


class StagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name) / "Gothic"
        self.original = self.game / "ParryHotter-private" / "HP2-Harry-original"
        self.original.mkdir(parents=True)
        icons = Path(self.temp.name) / "icons"
        generate(icons)
        for i in range(4):
            (self.original / f"PH_HARRY_TEX{i}.TGA").write_bytes(
                (icons / "PH_FLIPENDO.TGA").read_bytes()
            )

    def make_asc(self, bitmap=b"skharryTex0.png"):
        content = (
            b"*3DSMAX_ASCIIEXPORT 200\n*GEOMOBJECT {\n*MESH {\n"
            + b'*BITMAP "' + bitmap + b'"\n}\n}\n'
            + b"// test mesh triangle weights\n" * 20
        )
        (self.original / "PH_HARRY.ASC").write_bytes(content)

    def test_stages_export_and_rewrites_png_to_tga_without_touching_source(self):
        self.make_asc()
        before = (self.original / "PH_HARRY.ASC").read_bytes()
        self.assertTrue(stage_export(self.game))
        converted = self.game / "ParryHotter-private" / "HP2-Harry-converted"
        data = (converted / "PH_HARRY.ASC").read_bytes()
        self.assertIn(b'PH_HARRY_TEX0.TGA', data)
        self.assertNotIn(b"skharryTex0.png", data)
        self.assertEqual((self.original / "PH_HARRY.ASC").read_bytes(), before)
        self.assertEqual(len(list(converted.glob("*.TGA"))), 4)
        self.assertTrue(stage_export(self.game))
        self.assertFalse(list(converted.glob("*.parryhotter-original")))

    def test_no_export_does_not_change_game(self):
        self.assertFalse(stage_export(self.game))
        self.assertFalse((self.game / "_work").exists())

    def test_wrong_texture_reference_is_rejected(self):
        self.make_asc(b"C:\\secret\\OtherGame.TGA")
        with self.assertRaises(PatchError):
            stage_export(self.game)
        self.assertFalse((self.original.parent / "HP2-Harry-converted").exists())

    def test_missing_tga_does_not_stage_partially(self):
        self.make_asc()
        (self.original / "PH_HARRY_TEX3.TGA").unlink()
        with self.assertRaises(PatchError):
            stage_export(self.game)
        self.assertFalse((self.original.parent / "HP2-Harry-converted").exists())


if __name__ == "__main__":
    unittest.main()
