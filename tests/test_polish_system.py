import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.install import PatchError
from tools.polish_system import REQUIRED_SYSTEM_FILES, install_polish_system, unpack_polish_system

PREFIX = "gothic-1-classic-scripts-abc/_work/Data/Scripts/System/"


def polish_archive(path: Path, *, extra=(), drop=()) -> Path:
    names = [name for name in REQUIRED_SYSTEM_FILES if name not in drop]
    names += [f"menu/extra_{index}.d" for index in range(30)]
    with zipfile.ZipFile(path, "w") as archive:
        for name in names:
            archive.writestr(PREFIX + name, f"// PL {name}\n")
        archive.writestr("gothic-1-classic-scripts-abc/_work/Data/Scripts/Content/Gothic.src", "x")
        for name in extra:
            archive.writestr(name, "x")
    return path


def game_tree(root: Path) -> Path:
    game = root / "Gothic"
    system = game / "_work" / "Data" / "Scripts" / "System"
    (game / "_work" / "Data" / "Scripts" / "Content").mkdir(parents=True)
    (system / "Menu").mkdir(parents=True)
    (system / "Menu" / "Menu_Main.d").write_text("// DE Hauptmenue\n")
    (system / "Menu.src").write_text("// DE\n")
    return game


class PolishSystemTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name).resolve()

    def tearDown(self):
        self.temporary.cleanup()

    def test_unpack_takes_only_system_scripts(self):
        stage = self.root / "stage"
        stage.mkdir()
        count = unpack_polish_system(polish_archive(self.root / "pl.zip"), stage)
        self.assertEqual(count, len(REQUIRED_SYSTEM_FILES) + 30)
        self.assertTrue((stage / "menu" / "menu_main.d").is_file())
        self.assertFalse((stage / "Gothic.src").exists())

    def test_unpack_rejects_incomplete_or_unsafe_archive(self):
        stage = self.root / "stage"
        stage.mkdir()
        with self.assertRaisesRegex(PatchError, "missing"):
            unpack_polish_system(polish_archive(self.root / "a.zip", drop=("menu.src",)), stage)
        with self.assertRaisesRegex(PatchError, "Unsafe"):
            unpack_polish_system(polish_archive(self.root / "b.zip", extra=(PREFIX + "../evil.d",)), stage)

    def test_install_backs_up_german_scripts_and_is_idempotent(self):
        game = game_tree(self.root)
        archive = polish_archive(self.root / "pl.zip")
        backup = install_polish_system(game, zip_path=archive)
        system = game / "_work" / "Data" / "Scripts" / "System"
        self.assertEqual((system / "menu" / "menu_main.d").read_text(), "// PL menu/menu_main.d\n")
        self.assertEqual((backup / "Menu" / "Menu_Main.d").read_text(), "// DE Hauptmenue\n")
        self.assertIsNone(install_polish_system(game, zip_path=archive))
        self.assertEqual(len(list(system.parent.glob("System_ParryHotter_Backup_*"))), 1)

    def test_failed_archive_leaves_game_untouched(self):
        game = game_tree(self.root)
        bad = polish_archive(self.root / "bad.zip", drop=("menu/menu_main.d",))
        with self.assertRaises(PatchError):
            install_polish_system(game, zip_path=bad)
        system = game / "_work" / "Data" / "Scripts" / "System"
        self.assertEqual((system / "Menu" / "Menu_Main.d").read_text(), "// DE Hauptmenue\n")
        self.assertFalse(list(system.parent.glob("System_ParryHotter_Backup_*")))


if __name__ == "__main__":
    unittest.main()
