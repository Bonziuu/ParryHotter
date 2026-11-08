"""Every script a BAT file starts must import when run as a plain file.

ZBUDUJ_MOD_PO_REPARSE.bat runs `py -3 tools\\package_mod.py`; Python then puts
tools/ (not the project root) on sys.path, so `from tools...` failed with
ModuleNotFoundError on Windows. Found in the first real Windows run.
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BatEntrypointTests(unittest.TestCase):
    def test_bat_scripts_import_from_any_working_directory(self):
        commands = []
        for bat in sorted(ROOT.glob("*.bat")):
            text = bat.read_text(encoding="utf-8", errors="replace")
            for script in re.findall(r'"%~dp0(tools\\[\w]+\.py)"', text):
                commands.append([sys.executable, str(ROOT / script.replace("\\", "/")), "--help"])
            for module in re.findall(r"py -3 -m ([\w.]+)", text):
                commands.append([sys.executable, "-m", module, "--help"])
        self.assertGreaterEqual(len(commands), 4)
        with tempfile.TemporaryDirectory() as elsewhere:
            for command in commands:
                cwd = ROOT if "-m" in command else elsewhere
                result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0, f"{command}: {result.stderr[-600:]}")


if __name__ == "__main__":
    unittest.main()
