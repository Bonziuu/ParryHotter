"""Updater deliveries contain only the explicit scripts, never local assets."""
import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock
import zipfile

from tools import build_update_bundle as bundle


class UpdateBundleTests(unittest.TestCase):
    FILES = (
        "AKTUALIZUJ_PARRY_HOTTER.bat",
        "docs/AKTUALIZACJA-DLL.md",
        "tools/__init__.py",
        "tools/update_hud.py",
        "tools/download_hud.py",
        "tools/verify_dll.py",
        "tools/spell_catalog.py",
        "tools/vdf.py",
        "tools/file_transaction.py",
    )

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.root = self.directory / "project"
        self.root.mkdir()
        self.sources = {}
        for relative in self.FILES:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            self.sources[relative] = f"test source for {relative}\r\n".encode("ascii")
            path.write_bytes(self.sources[relative])
        # Real installations can contain all of these: none belong in the ZIP.
        for relative in ("PH_HARRY_TEX0.TGA", "ParryHotter_Harry_Gothic.zip",
                         "ParryHotter_cast.log", ".parryhotter/build/ParryHotterHUD-G1.dll",
                         "tools/unrelated_private_file.py"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"must remain local")
        self.output = self.directory / "delivery" / "ParryHotter-Aktualizator.zip"

    def test_bundle_contains_exact_sources_without_a_root_folder_and_matching_checksum(self):
        with mock.patch.object(bundle, "ROOT", self.root):
            result = bundle.build_update_bundle(self.output)
        self.assertEqual(result, self.output)
        with zipfile.ZipFile(result) as archive:
            self.assertEqual(archive.namelist(), list(self.FILES))
            self.assertEqual({name: archive.read(name) for name in archive.namelist()},
                             self.sources)
        expected = hashlib.sha256(result.read_bytes()).hexdigest()
        self.assertEqual(result.with_suffix(".zip.sha256").read_text(encoding="ascii"),
                         f"{expected}  {result.name}\n")

    def test_identical_source_bytes_produce_identical_bundle_bytes(self):
        other = self.output.with_name("another.zip")
        with mock.patch.object(bundle, "ROOT", self.root):
            bundle.build_update_bundle(self.output)
            bundle.build_update_bundle(other)
        self.assertEqual(self.output.read_bytes(), other.read_bytes())

    def test_missing_source_preserves_existing_delivery(self):
        self.output.parent.mkdir()
        self.output.write_bytes(b"previous delivery")
        checksum = self.output.with_suffix(".zip.sha256")
        checksum.write_bytes(b"previous checksum")
        (self.root / "tools/update_hud.py").unlink()
        with mock.patch.object(bundle, "ROOT", self.root):
            with self.assertRaisesRegex(bundle.UpdateBundleError, "tools/update_hud.py"):
                bundle.build_update_bundle(self.output)
        self.assertEqual(self.output.read_bytes(), b"previous delivery")
        self.assertEqual(checksum.read_bytes(), b"previous checksum")

    def test_cli_help_imports_when_started_outside_project(self):
        script = Path(bundle.__file__).resolve()
        result = subprocess.run([sys.executable, str(script), "--help"],
                                cwd=self.directory, capture_output=True, text=True,
                                timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--out", result.stdout)


if __name__ == "__main__":
    unittest.main()
