"""Integrity and host selection checks for the native compiler downloader."""
import hashlib
import io
import stat
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools import verify_daedalus


class NativeCompilerDownloadTests(unittest.TestCase):
    def test_bad_download_preserves_existing_compiler_and_removes_partial(self):
        with tempfile.TemporaryDirectory() as directory:
            compiler = Path(directory) / "gdc"
            compiler.write_bytes(b"previous verified compiler")
            with mock.patch.object(verify_daedalus, "compiler_asset", return_value=("gdc-linux-x64", "0" * 64)), \
                    mock.patch.object(verify_daedalus.urllib.request, "urlopen", return_value=io.BytesIO(b"invalid replacement")):
                with self.assertRaisesRegex(RuntimeError, "SHA256"):
                    verify_daedalus.get_compiler(compiler)
            self.assertEqual(compiler.read_bytes(), b"previous verified compiler")
            self.assertFalse(compiler.with_suffix(".download").exists())

    @unittest.skipIf(verify_daedalus.platform.system() == "Windows", "POSIX executable permission")
    def test_linux_compiler_becomes_executable_only_after_digest_matches(self):
        fixture = b"trusted test compiler payload"
        digest = hashlib.sha256(fixture).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            compiler = Path(directory) / "gdc"
            with mock.patch.object(verify_daedalus, "compiler_asset", return_value=("gdc-linux-x64", digest)), \
                    mock.patch.object(verify_daedalus.urllib.request, "urlopen", return_value=io.BytesIO(fixture)):
                verify_daedalus.get_compiler(compiler)
            self.assertEqual(compiler.read_bytes(), fixture)
            self.assertTrue(compiler.stat().st_mode & stat.S_IXUSR)
            self.assertFalse(compiler.with_suffix(".download").exists())

    def test_unconfigured_architecture_fails_before_network_download(self):
        with mock.patch.object(verify_daedalus.platform, "machine", return_value="arm64"), \
                mock.patch.object(verify_daedalus.urllib.request, "urlopen") as download:
            with self.assertRaisesRegex(RuntimeError, "x86-64"):
                verify_daedalus.get_compiler(Path("must-not-be-created"))
            download.assert_not_called()


if __name__ == "__main__":
    unittest.main()
