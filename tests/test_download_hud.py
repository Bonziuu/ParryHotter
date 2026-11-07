"""Reject incompatible downloaded builds even when their checksum is valid."""
import hashlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.download_hud import HudDownloadError, fetch_verified_hud, validate_hud_binary
from tools.spell_catalog import HUD_BUILD_MARKER


class HudBuildTests(unittest.TestCase):
    def test_old_and_non_dll_builds_are_rejected(self):
        for data in (b"MZold build", HUD_BUILD_MARKER, b"<html>error</html>"):
            with self.assertRaises(HudDownloadError):
                validate_hud_binary(data)
        validate_hud_binary(b"MZ" + HUD_BUILD_MARKER)

    def test_verified_old_release_does_not_replace_existing_dll(self):
        data = b"MZold six-slot build" + b"x" * 1500
        metadata = {
            "tag_name": "hud-dev", "assets": [{
                "name": "ParryHotterHUD-G1.dll",
                "browser_download_url": "https://github.com/Bonziuu/ParryHotter/releases/download/hud-dev/ParryHotterHUD-G1.dll",
                "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
                "size": len(data),
            }],
        }
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "hud.dll"
            target.write_bytes(b"existing reviewed DLL")
            with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
                io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(data)
            ]):
                with self.assertRaisesRegex(HudDownloadError, "starszej"):
                    fetch_verified_hud(target)
            self.assertEqual(target.read_bytes(), b"existing reviewed DLL")
            self.assertFalse(target.with_suffix(".dll.download").exists())
