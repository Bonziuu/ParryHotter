"""Offline checks for local-only Harry model ZIP preparation."""

import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.model_asset import EXPECTED, ModelAssetError, prepare_model


class ModelAssetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name) / "Gothic"
        (self.game / "System").mkdir(parents=True)
        self.zip_path = Path(self.temp.name) / "Harry.zip"
        self.write_zip()

    def write_zip(self, *, missing=None, with_traversal=False):
        with zipfile.ZipFile(self.zip_path, "w") as archive:
            for name in sorted(EXPECTED - ({missing} if missing else set())):
                archive.writestr(f"skharrymesh/{name}", b"sample source")
            if with_traversal:
                archive.writestr("../../../important.txt", b"should not extract")

    def test_prepare_stages_exact_model_without_deleting_other_files(self):
        other = self.game / "System" / "Gothic.ini"
        other.write_text("keep me")
        self.write_zip(with_traversal=True)
        stage = prepare_model(self.game, self.zip_path)
        self.assertTrue((stage / "skharrymesh.fbx").is_file())
        self.assertTrue((stage / "skharrytex3.png").is_file())
        self.assertEqual(other.read_text(), "keep me")
        self.assertFalse((self.game / "important.txt").exists())
        self.assertFalse((self.game / "_work").exists())

    def test_missing_fbx_fails_without_in_game_changes(self):
        self.write_zip(missing="skharrymesh.fbx")
        with self.assertRaises(ModelAssetError):
            prepare_model(self.game, self.zip_path)
        self.assertFalse((self.game / "_work").exists())

    def test_unknown_game_folder_fails(self):
        with self.assertRaises(ModelAssetError):
            prepare_model(Path(self.temp.name) / "Missing", self.zip_path)


if __name__ == "__main__":
    unittest.main()
