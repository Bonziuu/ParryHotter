"""The experimental Union HUD must not force global Gothic2_Control."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from tools.configure_mouse import configure_mouse, read_option


class MouseConfigTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Gothic"
        self.system = self.game / "System"
        self.system.mkdir(parents=True)

    def test_enables_mouse_but_preserves_original_systempack_controls(self):
        systempack = self.system / "SystemPack.ini"
        gothic = self.system / "Gothic.ini"
        systempack.write_bytes(b"[DEBUG]\r\nGothic2_Control=0\r\nOther=2\r\n")
        gothic.write_bytes(b"[GAME]\r\nenableMouse=0\r\nmouseSensitivity=0.5\r\n")
        changed = configure_mouse(self.game)
        self.assertEqual([p.name for p in changed], ["Gothic.ini"])
        self.assertEqual(read_option(systempack, "Gothic2_Control"), "0")
        self.assertIn(b"enableMouse=1\r\nmouseSensitivity", gothic.read_bytes())
        self.assertEqual(configure_mouse(self.game), [])
        self.assertFalse((self.system / "SystemPack.ini.parryhotter-original").exists())
        self.assertIn(b"enableMouse=0", (self.system / "Gothic.ini.parryhotter-original").read_bytes())

    def test_restores_only_legacy_change_from_old_installer(self):
        systempack = self.system / "SystemPack.ini"
        original = b"[DEBUG]\r\nGothic2_Control=0\r\nOther=11\r\n"
        (self.system / "SystemPack.ini.parryhotter-original").write_bytes(original)
        systempack.write_bytes(b"[DEBUG]\r\nGothic2_Control=1\r\nOther=99\r\n")
        self.assertEqual([p.name for p in configure_mouse(self.game)], ["SystemPack.ini"])
        self.assertIn(b"Gothic2_Control=0\r\nOther=99", systempack.read_bytes())
        self.assertEqual(configure_mouse(self.game), [])
        self.assertEqual((self.system / "SystemPack.ini.parryhotter-original").read_bytes(), original)

    def test_do_not_change_user_activated_legacy_control_without_backup(self):
        systempack = self.system / "SystemPack.ini"
        systempack.write_bytes(b"Gothic2_Control=1\n")
        self.assertEqual(configure_mouse(self.game), [])
        self.assertEqual(read_option(systempack, "Gothic2_Control"), "1")

    def test_commented_option_is_not_activated(self):
        gothic = self.system / "Gothic.ini"
        gothic.write_text("; enableMouse=0\nOther=5\n")
        self.assertEqual(configure_mouse(self.game), [])
        self.assertEqual(gothic.read_text(), "; enableMouse=0\nOther=5\n")


if __name__ == "__main__":
    unittest.main()
