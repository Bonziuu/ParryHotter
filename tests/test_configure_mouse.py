"""Test safe, original-line-preserving mouse control configuration."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.configure_mouse import configure_mouse


class MouseConfigTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Gothic"
        self.system = self.game / "System"
        self.system.mkdir(parents=True)

    def test_enable_mouse_and_one_click_preserves_other_lines(self):
        systempack = self.system / "SystemPack.ini"
        gothic = self.system / "Gothic.ini"
        systempack.write_bytes(b"[DEBUG]\r\n; keep it\r\nGothic2_Control=0\r\nOther=2\r\n")
        gothic.write_bytes(b"[GAME]\r\nenableMouse=0\r\nmouseSensitivity=0.5\r\n")
        changed = configure_mouse(self.game)
        self.assertEqual(set(p.name for p in changed), {"SystemPack.ini", "Gothic.ini"})
        self.assertIn(b"Gothic2_Control=1\r\nOther=2", systempack.read_bytes())
        self.assertIn(b"enableMouse=1", gothic.read_bytes())
        self.assertIn(b"; keep it", systempack.read_bytes())
        self.assertEqual(configure_mouse(self.game), [])
        self.assertEqual(
            (self.system / "SystemPack.ini.parryhotter-original").read_bytes(),
            b"[DEBUG]\r\n; keep it\r\nGothic2_Control=0\r\nOther=2\r\n",
        )

    def test_no_matching_option_does_not_invent_settings(self):
        path = self.system / "SystemPack.ini"
        path.write_text("[DEBUG]\nOther=1\n; Gothic2_Control=0\n")
        self.assertEqual(configure_mouse(self.game), [])
        self.assertEqual(path.read_text(), "[DEBUG]\nOther=1\n; Gothic2_Control=0\n")

    def test_existing_user_setting_not_overwritten(self):
        path = self.system / "SystemPack.ini"
        path.write_text("Gothic2_Control=1\n")
        self.assertEqual(configure_mouse(self.game), [])
        self.assertFalse((self.system / "SystemPack.ini.parryhotter-original").exists())


if __name__ == "__main__":
    unittest.main()
