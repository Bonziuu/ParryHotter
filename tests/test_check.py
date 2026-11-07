"""A successful check must reject corrupt generated HUD artifacts."""
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools import check
from tools.generate_hud_textures import generate


class DevelopmentCheckTests(unittest.TestCase):
    def test_truncated_pixel_payload_is_a_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            icons = Path(directory)
            files = generate(icons)
            files[0].write_bytes(files[0].read_bytes()[:-1])
            with mock.patch.object(check, "generate", return_value=files):
                with self.assertRaisesRegex(RuntimeError, "payload length"):
                    check.check_icons(icons)

    def test_incomplete_texture_set_is_a_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            icons = Path(directory)
            files = generate(icons)
            with mock.patch.object(check, "generate", return_value=files[:-1]):
                with self.assertRaisesRegex(RuntimeError, "complete ten-slot"):
                    check.check_icons(icons)

    def test_failed_cpp_process_propagates(self):
        with tempfile.TemporaryDirectory() as directory:
            import subprocess
            with mock.patch.object(check.subprocess, "run", side_effect=subprocess.CalledProcessError(7, "compiler")):
                with self.assertRaises(subprocess.CalledProcessError):
                    check.check_cpp(Path(directory), "g++")


if __name__ == "__main__":
    unittest.main()
