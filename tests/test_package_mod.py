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
            (stage / "ParryHotter.mod").write_bytes(b"mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="ok", stderr="")

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder):
            result = build_mod(self.game, self.vdfs)
        self.assertTrue(result.is_file())
        play = (self.system / "ParryHotter-PLAY.ini").read_text(encoding="latin-1")
        dev = (self.system / "ParryHotter.ini").read_text(encoding="latin-1")
        self.assertIn("vdf=ParryHotter.mod", play)
        self.assertIn("force_parameters=", play)
        self.assertNotIn("force_parameters=-zreparse", play)
        self.assertIn("title=Parry Hotter (PLAY)", play)
        self.assertIn("title=Parry Hotter (DEV)", dev)
        self.assertIn("force_parameters=-zreparse", dev)

    def test_missing_polish_output_blocks_build(self):
        self.subtitle.unlink()
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)


if __name__ == "__main__":
    unittest.main()
