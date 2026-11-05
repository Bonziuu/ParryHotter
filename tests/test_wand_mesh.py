"""Offline tests for the original/private HP2 wand and Gothic 3DS fallback."""
from __future__ import annotations

import os
import struct
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path
from unittest import mock

from tools.wand_mesh import (WandError, original_wand, read_obj,
                             generate_wand, png_to_tga)
from tools.wand_asset import prepare_wand, download_wand_zip


def make_png(path: Path) -> None:
    def pngchunk(tag: bytes, body: bytes) -> bytes:
        payload = tag + body
        return (len(body).to_bytes(4, "big") + payload
                + (zlib.crc32(payload) & 0xFFFFFFFF).to_bytes(4, "big"))
    ihdr = struct.pack(">IIBBBBB", 2, 1, 8, 2, 0, 0, 0)
    scanline = b"\x00" + bytes((190, 120, 56, 25, 14, 10))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + pngchunk(b"IHDR", ihdr)
        + pngchunk(b"IDAT", zlib.compress(scanline))
        + pngchunk(b"IEND", b"")
    )


def make_obj(path: Path) -> None:
    path.write_text(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 0 0 14\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "f 1/1 2/2 3/3\nf 1/1 2/2 4/4\n"
        "f 2/2 3/3 4/4\nf 3/3 1/1 4/4\n",
        encoding="ascii",
    )


class WandTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.game = self.root / "Gothic"
        (self.game / "System").mkdir(parents=True)
        self.mesh = self.root / "PH_WAND.3DS"
        self.texture = self.root / "PH_WAND.TGA"
        self.obj = self.root / "skwandmesh.obj"
        self.png = self.root / "skwandtex0.png"

    def assert_valid_3ds(self, path):
        data = path.read_bytes()
        self.assertGreater(len(data), 200)
        self.assertEqual(data[:2], b"\x4d\x4d")
        self.assertEqual(int.from_bytes(data[2:6], "little"), len(data))
        self.assertIn(b"PH_WAND_WOOD\x00", data)
        self.assertIn(b"PH_WAND.TGA\x00", data)

    def test_original_procedural_wand_is_reproducible_without_dependencies(self):
        original = original_wand()
        self.assertEqual(len(original[0]), len(original[1]))
        self.assertGreater(len(original[2]), 50)
        self.assertFalse(generate_wand(self.mesh, self.texture))
        data = self.mesh.read_bytes()
        self.assert_valid_3ds(self.mesh)
        tex = self.texture.read_bytes()
        self.assertEqual(tex[2], 2)
        self.assertEqual(tex[16], 24)
        self.assertEqual(len(tex), 18 + 64 * 64 * 3)
        self.assertFalse(generate_wand(self.mesh, self.texture))
        self.assertEqual(self.mesh.read_bytes(), data)

    def test_original_hp2_obj_png_conversion_keeps_geometry_and_pixels(self):
        make_obj(self.obj)
        make_png(self.png)
        self.assertTrue(generate_wand(self.mesh, self.texture, self.obj, self.png))
        self.assert_valid_3ds(self.mesh)
        tex = self.texture.read_bytes()
        self.assertEqual(tex[12:16], struct.pack("<HH", 2, 1))
        self.assertEqual(tex[18:24], bytes((56, 120, 190, 10, 14, 25)))
        original_obj = self.obj.read_bytes()
        self.assertEqual(self.obj.read_bytes(), original_obj)

    def test_bad_obj_does_not_publish_partial_assets(self):
        self.obj.write_text("v 0 0 0\nf 99 100 101\n")
        with self.assertRaises(WandError):
            generate_wand(self.mesh, self.texture, self.obj)
        self.assertFalse(self.mesh.exists())
        self.assertFalse(self.texture.exists())

    def test_valid_original_wand_zip_from_downloads_is_used_offline(self):
        make_obj(self.obj)
        make_png(self.png)
        folder = self.root / "Downloads"
        folder.mkdir()
        path = folder / "291995.zip"
        with zipfile.ZipFile(path, "w") as archive:
            archive.write(self.obj, arcname="skWandmesh.obj")
            archive.write(self.png, arcname="skWandTex0.png")
            archive.writestr("../another_location.txt", b"ignore traversal")
        with mock.patch("tools.wand_asset.Path.home", return_value=self.root):
            self.assertTrue(prepare_wand(self.game, allow_download=False))
        wand_mesh = self.game / "_work" / "Data" / "Meshes" / "PH_WAND.3DS"
        self.assert_valid_3ds(wand_mesh)
        self.assertFalse((self.game / "another_location.txt").exists())

    def test_missing_original_wand_falls_back_offline(self):
        with mock.patch("tools.wand_asset.Path.home", return_value=self.root):
            self.assertFalse(prepare_wand(self.game, allow_download=False))
        self.assertTrue((self.game / "_work" / "Data" / "Meshes" / "PH_WAND.3DS").exists())
        self.assertTrue((self.game / "_work" / "Data" / "Textures" / "PH_WAND.TGA").exists())

    def test_invalid_download_does_not_leave_zip_or_partial(self):
        path = self.root / "291995.zip"
        with mock.patch("tools.wand_asset.urllib.request.urlopen",
                        return_value=mock.MagicMock(
                            __enter__=lambda instance: instance,
                            __exit__=lambda instance, *args: None,
                            read=mock.Mock(side_effect=[b"<html>oops</html>", b""])
                        )):
            with self.assertRaises(WandError):
                download_wand_zip(path)
        self.assertFalse(path.exists())
        self.assertFalse(path.with_suffix(".zip.download").exists())


if __name__ == "__main__":
    unittest.main()
