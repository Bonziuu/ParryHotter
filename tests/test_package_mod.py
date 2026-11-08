"""Tests of the separate ParryHotter.mod build, without proprietary assets."""

import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.package_mod import PackagingError, build_mod, validate_compiled, release_profile
from tools.spell_catalog import HUD_BUILD_MARKER, REQUIRED_COMPILED_SYMBOLS, REQUIRED_TEXTURES
from tools.casting_assets import CASTING_MDS, CASTING_ASSET_NAMES, canonical_casting_mds

COMPILED_TEN_SLOTS = b"Flipendo " + b" ".join(REQUIRED_COMPILED_SYMBOLS)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name).resolve() / "Gothic"
        self.system = self.game / "System"
        self.system.mkdir(parents=True)
        self.src = self.game / "_work" / "Data" / "Scripts" / "Content" / "Magic" / "ParryHotter.d"
        self.src.parent.mkdir(parents=True)
        self.src.write_text("test source", encoding="ascii")
        self.dat = self.game / "_work" / "Data" / "Scripts" / "_compiled" / "Gothic.dat"
        self.dat.parent.mkdir(parents=True)
        self.dat.write_bytes(
            COMPILED_TEN_SLOTS
            + b"\x00" * 1024
        )
        self.subtitle = self.game / "_work" / "Data" / "Scripts" / "Content" / "Cutscene" / "Ou.bin"
        self.subtitle.parent.mkdir(parents=True)
        self.subtitle.write_bytes(b"Polski OU test")
        self.vdfs = self.game / "GothicVDFS.exe"
        self.vdfs.write_bytes(b"not actually run")
        self.hud_dll = self.game / "ParryHotterHUD-G1.dll"
        self.hud_dll.write_bytes(b"MZ" + HUD_BUILD_MARKER + b"mock native DLL" * 100)

        (self.system / "ParryHotter.ini").write_text(
            "[INFO]\ntitle=Parry Hotter (DEV)\n"
            "[FILES]\nvdf=\n[OPTIONS]\nforce_parameters=-zreparse\n",
            encoding="ascii",
        )
        source_mtime = self.src.stat().st_mtime
        os.utime(self.dat, (source_mtime + 5, source_mtime + 5))

    def ready_harry_visual(self, *, casting=False, overlay_in_dat=True):
        content = self.game / "_work" / "Data" / "Scripts" / "Content"
        items = content / "Items"
        items.mkdir(parents=True)
        armor = items / "ParryHarryArmor.d"
        armor.write_text("INSTANCE PH_ARMOR_HARRY(C_Item) {}", encoding="ascii")
        hero = content / "Story" / "NPC" / "PC_Hero.d"
        hero.parent.mkdir(parents=True)
        hero.write_text(
            'Mdl_ApplyOverlayMds(self,"PH_HARRY_CAST.MDS");' if casting
            else "PC hero visual override", encoding="ascii"
        )
        anims = self.game / "_work" / "Data" / "Anims"
        anims.mkdir(parents=True)
        if casting:
            from tests.test_casting_assets import write_casting_assets
            write_casting_assets(anims)
        else:
            (anims / "PH_HARRY.ASC").write_bytes(b"*GEOMOBJECT *MESH")
        (anims / "HUMANS.MDS").write_bytes(b'registerMesh ("PH_HARRY.ASC")')
        textures = self.game / "_work" / "Data" / "Textures"
        textures.mkdir(parents=True)
        for index in range(4):
            (textures / f"PH_HARRY_TEX{index}.TGA").write_bytes(b"private test TGA")
        self.dat.write_bytes(COMPILED_TEN_SLOTS + b" PH_ARMOR_HARRY"
                             + (b" PH_HARRY_CAST.MDS" if casting and overlay_in_dat else b""))
        stamp = max(self.src.stat().st_mtime, armor.stat().st_mtime, hero.stat().st_mtime) + 5
        os.utime(self.dat, (stamp, stamp))
        return anims, hero

    def assert_harry_preflight_rejected_without_writes(self, pattern):
        before = {str(path.relative_to(self.game)): path.read_bytes()
                  for path in self.game.rglob("*") if path.is_file()}
        with mock.patch("tools.package_mod.prepare_wand") as wand, \
                mock.patch("tools.package_mod.generate_hud_textures") as textures, \
                mock.patch("tools.package_mod.subprocess.run") as builder:
            with self.assertRaisesRegex(PackagingError, pattern):
                build_mod(self.game, self.vdfs, self.hud_dll)
        wand.assert_not_called()
        textures.assert_not_called()
        builder.assert_not_called()
        self.assertEqual(before, {str(path.relative_to(self.game)): path.read_bytes()
                                  for path in self.game.rglob("*") if path.is_file()})

    def test_old_hud_binary_never_reaches_archive_builder(self):
        self.hud_dll.write_bytes(b"MZ" + b"old six-slot DLL" * 100)
        with mock.patch("tools.package_mod.subprocess.run") as builder:
            with self.assertRaisesRegex(PackagingError, "starszej"):
                build_mod(self.game, self.vdfs, self.hud_dll)
        builder.assert_not_called()

    def test_validate_compiled_requires_mod_marker(self):
        validate_compiled(self.game)
        self.dat.write_bytes(b"Gothic stock scripts only")
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)

    def test_reject_stale_dat_missing_union_presence_symbol(self):
        self.dat.write_bytes(b"Flipendo PH_Rune_Flipendo PH_SELECTED_SPELL")
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)

    def test_reject_stale_dat_missing_union_spell_bridge(self):
        self.dat.write_bytes(b"Flipendo PH_Rune_Flipendo without newer bridge")
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)

    def test_uppercase_dat_symbols_are_accepted(self):
        # ZenGin can emit normalized uppercase instance names.
        self.dat.write_bytes(
            COMPILED_TEN_SLOTS.upper()
        )
        validate_compiled(self.game)

    def test_windows_profile_preserves_crlf_and_normalizes_spaced_case_insensitive_keys(self):
        dev = self.system / "ParryHotter.ini"
        raw = b"[INFO]\r\n TITLE =DEV\r\n[FILES]\r\n VDF =old.mod\r\n[OPTIONS]\r\n FORCE_PARAMETERS =-zreparse\r\n"
        dev.write_bytes(raw)
        path, profile = release_profile(self.game)
        self.assertEqual(path, self.system / "ParryHotter-PLAY.ini")
        self.assertIn(b"title=Parry Hotter (PLAY)\r\n", profile)
        self.assertIn(b"vdf=ParryHotter.mod\r\n", profile)
        self.assertIn(b"force_parameters=\r\n", profile)
        self.assertNotIn(b"-zreparse", profile)
        self.assertNotIn(b"\n", profile.replace(b"\r\n", b""))
        self.assertEqual(dev.read_bytes(), raw)

    def test_build_separate_release_and_unchanged_dev_ini(self):
        def fake_builder(cmd, *, cwd, **kwargs):
            self.assertEqual(cmd[1], "/B")
            stage = Path(cwd)
            vm = (stage / "build.vm").read_text()
            self.assertIn("_work/Data/Scripts/_compiled/Gothic.dat", vm)
            self.assertIn("_work/Data/Scripts/Content/Cutscene/Ou.bin", vm)
            self.assertIn("System/Autorun/ParryHotterHUD-G1.dll", vm)
            self.assertIn("_work/Data/Textures/PH_TARGET_RICTUSEMPRA.TGA", vm)
            self.assertIn("_work/Data/Meshes/PH_WAND.3DS", vm)
            self.assertIn("_work/Data/Textures/PH_WAND.TGA", vm)
            self.assertTrue((stage / "System" / "Autorun" / "ParryHotterHUD-G1.dll").is_file())
            textures = list((stage / "_work" / "Data" / "Textures").glob("*.TGA"))
            self.assertEqual(len(textures), 16)
            for name in REQUIRED_TEXTURES:
                self.assertIn(f"_work/Data/Textures/{name}", vm)

            (stage / "ParryHotter.mod").write_bytes(b"mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="ok", stderr="")

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder):
            result = build_mod(self.game, self.vdfs, self.hud_dll)
        self.assertTrue(result.is_file())
        play = (self.system / "ParryHotter-PLAY.ini").read_text(encoding="latin-1")
        dev = (self.system / "ParryHotter.ini").read_text(encoding="latin-1")
        self.assertIn("vdf=ParryHotter.mod", play)
        self.assertIn("force_parameters=", play)
        self.assertNotIn("force_parameters=-zreparse", play)
        self.assertIn("title=Parry Hotter (PLAY)", play)
        self.assertIn("title=Parry Hotter (DEV)", dev)
        self.assertIn("force_parameters=-zreparse", dev)

    def test_builtin_vdf_writer_used_when_modkit_tool_is_missing(self):
        from tools.vdf import read_vdf
        self.vdfs.unlink()
        with mock.patch("tools.package_mod.subprocess.run") as run:
            result = build_mod(self.game, None, self.hud_dll)
        run.assert_not_called()
        archive = read_vdf(result.read_bytes())
        self.assertIn("SYSTEM/AUTORUN/PARRYHOTTERHUD-G1.DLL", archive)
        self.assertIn("_WORK/DATA/SCRIPTS/_COMPILED/GOTHIC.DAT", archive)
        self.assertIn("_WORK/DATA/MESHES/PH_WAND.3DS", archive)
        self.assertEqual(archive["SYSTEM/AUTORUN/PARRYHOTTERHUD-G1.DLL"], self.hud_dll.read_bytes())
        play = (self.system / "ParryHotter-PLAY.ini").read_text(encoding="latin-1")
        self.assertIn("vdf=ParryHotter.mod", play)

    def test_missing_dev_profile_is_rejected_before_asset_or_output_changes(self):
        (self.system / "ParryHotter.ini").unlink()
        before = {str(path.relative_to(self.game)): path.read_bytes()
                  for path in self.game.rglob("*") if path.is_file()}
        with mock.patch("tools.package_mod.prepare_wand") as wand, \
                mock.patch("tools.package_mod.generate_hud_textures") as textures, \
                mock.patch("tools.package_mod.subprocess.run") as builder:
            with self.assertRaisesRegex(PackagingError, "ParryHotter.ini"):
                build_mod(self.game, self.vdfs, self.hud_dll)
        wand.assert_not_called()
        textures.assert_not_called()
        builder.assert_not_called()
        self.assertEqual(before, {str(path.relative_to(self.game)): path.read_bytes()
                                  for path in self.game.rglob("*") if path.is_file()})
        self.assertFalse((self.game / "Data").exists())

    def test_dev_profile_without_vdf_is_rejected_before_asset_or_output_changes(self):
        (self.system / "ParryHotter.ini").write_text(
            "[INFO]\ntitle=DEV\n[FILES]\n[OPTIONS]\nforce_parameters=-zreparse\n",
            encoding="ascii",
        )
        before = {str(path.relative_to(self.game)): path.read_bytes()
                  for path in self.game.rglob("*") if path.is_file()}
        with mock.patch("tools.package_mod.prepare_wand") as wand, \
                mock.patch("tools.package_mod.generate_hud_textures") as textures, \
                mock.patch("tools.package_mod.subprocess.run") as builder:
            with self.assertRaisesRegex(PackagingError, "VDF"):
                build_mod(self.game, self.vdfs, self.hud_dll)
        wand.assert_not_called()
        textures.assert_not_called()
        builder.assert_not_called()
        self.assertEqual(before, {str(path.relative_to(self.game)): path.read_bytes()
                                  for path in self.game.rglob("*") if path.is_file()})
        self.assertFalse((self.game / "Data").exists())

    def test_play_commit_failure_restores_previous_mod_and_profile(self):
        archive = self.game / "Data" / "ModVDF" / "ParryHotter.mod"
        archive.parent.mkdir(parents=True)
        archive.write_bytes(b"previous known-working archive\x00")
        play = self.system / "ParryHotter-PLAY.ini"
        play.write_bytes(b"previous known-working profile\r\n")
        note = archive.parent / "user-notes.txt"
        note.write_bytes(b"keep unrelated files")
        before = {path: path.read_bytes() for path in (archive, play, note)}
        original_dirs = {path: set(path.iterdir())
                         for path in (archive.parent, self.system)}
        new_archive = b"new mocked vdf content" * 80
        failure = OSError("injected PLAY profile replacement failure")
        original_replace = Path.replace
        failure_triggered = False

        def fake_builder(cmd, *, cwd, **kwargs):
            (Path(cwd) / "ParryHotter.mod").write_bytes(new_archive)
            return mock.Mock(returncode=0, stdout="", stderr="")

        def fail_play_once(source, target):
            nonlocal failure_triggered
            if Path(target) == play and not failure_triggered:
                failure_triggered = True
                self.assertEqual(archive.read_bytes(), new_archive)
                raise failure
            return original_replace(source, target)

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder), \
                mock.patch.object(Path, "replace", fail_play_once):
            with self.assertRaises(OSError) as raised:
                build_mod(self.game, self.vdfs, self.hud_dll)
        self.assertIs(raised.exception, failure)
        self.assertTrue(failure_triggered)
        self.assertEqual(before, {path: path.read_bytes() for path in before})
        for directory, original_entries in original_dirs.items():
            self.assertEqual(set(directory.iterdir()), original_entries)

    def test_play_commit_failure_removes_new_archive_and_created_directories(self):
        archive = self.game / "Data" / "ModVDF" / "ParryHotter.mod"
        play = self.system / "ParryHotter-PLAY.ini"
        self.assertFalse(archive.parent.parent.exists())
        original_system_entries = set(self.system.iterdir())
        failure = OSError("injected new PLAY replacement failure")
        original_replace = Path.replace
        failure_triggered = False

        def fake_builder(cmd, *, cwd, **kwargs):
            (Path(cwd) / "ParryHotter.mod").write_bytes(b"new mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="", stderr="")

        def fail_play_once(source, target):
            nonlocal failure_triggered
            if Path(target) == play and not failure_triggered:
                failure_triggered = True
                self.assertTrue(archive.is_file())
                raise failure
            return original_replace(source, target)

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder), \
                mock.patch.object(Path, "replace", fail_play_once):
            with self.assertRaises(OSError) as raised:
                build_mod(self.game, self.vdfs, self.hud_dll)
        self.assertIs(raised.exception, failure)
        self.assertTrue(failure_triggered)
        self.assertFalse((self.game / "Data").exists())
        self.assertFalse(play.exists())
        self.assertEqual(set(self.system.iterdir()), original_system_entries)

    def test_failed_commit_preserves_unrelated_file_in_new_output_directory(self):
        archive = self.game / "Data" / "ModVDF" / "ParryHotter.mod"
        play = self.system / "ParryHotter-PLAY.ini"
        note = archive.parent / "user-notes.txt"
        failure = OSError("injected PLAY replacement failure after unrelated file arrived")
        original_replace = Path.replace
        failure_triggered = False

        def fake_builder(cmd, *, cwd, **kwargs):
            (Path(cwd) / "ParryHotter.mod").write_bytes(b"new mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="", stderr="")

        def fail_play_once(source, target):
            nonlocal failure_triggered
            if Path(target) == play and not failure_triggered:
                failure_triggered = True
                self.assertTrue(archive.is_file())
                note.write_bytes(b"unrelated file written while packaging")
                raise failure
            return original_replace(source, target)

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder), \
                mock.patch.object(Path, "replace", fail_play_once):
            with self.assertRaises(OSError) as raised:
                build_mod(self.game, self.vdfs, self.hud_dll)
        self.assertIs(raised.exception, failure)
        self.assertTrue(failure_triggered)
        self.assertFalse(archive.exists())
        self.assertFalse(play.exists())
        self.assertEqual(note.read_bytes(), b"unrelated file written while packaging")
        self.assertEqual(set(archive.parent.iterdir()), {note})

    def test_packages_private_harry_costume_only_after_compilation(self):
        items = self.game / "_work" / "Data" / "Scripts" / "Content" / "Items"
        items.mkdir(parents=True)
        armor = items / "ParryHarryArmor.d"
        armor.write_text("INSTANCE PH_ARMOR_HARRY(C_Item) {}", encoding="ascii")
        hero = self.game / "_work" / "Data" / "Scripts" / "Content" / "Story" / "NPC" / "PC_Hero.d"
        hero.parent.mkdir(parents=True)
        hero.write_text("PC hero visual override", encoding="ascii")
        anims = self.game / "_work" / "Data" / "Anims"
        anims.mkdir(parents=True)
        (anims / "PH_HARRY.ASC").write_bytes(b"*GEOMOBJECT *MESH")
        (anims / "HUMANS.MDS").write_bytes(b'registerMesh ("PH_HARRY.ASC")')
        textures = self.game / "_work" / "Data" / "Textures"
        textures.mkdir(parents=True)
        for i in range(4):
            (textures / f"PH_HARRY_TEX{i}.TGA").write_bytes(b"private test TGA")
        self.dat.write_bytes(COMPILED_TEN_SLOTS + b" PH_ARMOR_HARRY")
        stamp = max(self.src.stat().st_mtime, armor.stat().st_mtime,
                    hero.stat().st_mtime) + 5
        os.utime(self.dat, (stamp, stamp))

        def fake_builder(cmd, *, cwd, **kwargs):
            stage = Path(cwd)
            vm = (stage / "build.vm").read_text()
            self.assertIn("_work/Data/Anims/PH_HARRY.ASC", vm)
            self.assertIn("_work/Data/Anims/HUMANS.MDS", vm)
            self.assertIn("_work/Data/Textures/PH_HARRY_TEX3.TGA", vm)
            self.assertEqual((stage / "_work/Data/Anims/PH_HARRY.ASC").read_bytes(),
                             b"*GEOMOBJECT *MESH")
            self.assertEqual(len(list((stage / "_work/Data/Textures").glob("*.TGA"))), 20)
            (stage / "ParryHotter.mod").write_bytes(b"mocked vdf content" * 80)
            return mock.Mock(returncode=0, stdout="", stderr="")

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder):
            self.assertTrue(build_mod(self.game, self.vdfs, self.hud_dll).exists())

    def test_missing_private_harry_costume_blocks_incomplete_mod(self):
        items = self.game / "_work" / "Data" / "Scripts" / "Content" / "Items"
        items.mkdir(parents=True)
        (items / "ParryHarryArmor.d").write_text("visual_change=PH_HARRY.ASC")
        # No compiled armor symbol, so do not make a broken .mod.
        with self.assertRaises(PackagingError):
            build_mod(self.game, self.vdfs, self.hud_dll)

    def test_packages_complete_validated_casting_sources_and_overlay(self):
        anims, _ = self.ready_harry_visual(casting=True)
        expected = {name: (anims / name).read_bytes() for name in CASTING_ASSET_NAMES}

        def fake_builder(cmd, *, cwd, **kwargs):
            stage = Path(cwd)
            vm = (stage / "build.vm").read_text()
            for name, data in expected.items():
                relative = Path("_work/Data/Anims") / name
                self.assertIn(relative.as_posix(), vm)
                self.assertEqual((stage / relative).read_bytes(), data)
                self.assertFalse((stage / "_work/Data/Textures" / name).exists())
            (stage / "ParryHotter.mod").write_bytes(b"mocked VDF with casting sources" * 80)
            return mock.Mock(returncode=0, stdout="", stderr="")

        with mock.patch("tools.package_mod.subprocess.run", side_effect=fake_builder):
            self.assertTrue(build_mod(self.game, self.vdfs, self.hud_dll).is_file())

    def test_partial_casting_set_rejected_before_any_generation_or_output(self):
        anims, _ = self.ready_harry_visual()
        (anims / CASTING_MDS).write_bytes(canonical_casting_mds())
        self.assert_harry_preflight_rejected_without_writes("casting animation sources")

    def test_casting_sources_require_overlay_in_compiled_dat(self):
        self.ready_harry_visual(casting=True, overlay_in_dat=False)
        self.assert_harry_preflight_rejected_without_writes("casting overlay.*stale")

    def test_hero_cannot_reference_missing_casting_sources(self):
        _, hero = self.ready_harry_visual()
        hero.write_text('Mdl_ApplyOverlayMds ( self, "PH_HARRY_CAST.MDS" );', encoding="ascii")
        self.assert_harry_preflight_rejected_without_writes("animation sources are missing")

    def test_missing_polish_output_blocks_build(self):
        self.subtitle.unlink()
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)

    def test_older_dat_is_rejected_even_with_existing_union_bridge(self):
        self.dat.write_bytes(b"Flipendo PH_Rune_Flipendo PH_SELECTED_SPELL PH_UNION_LOADED")
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)

    def test_missing_slot_count_rejects_partial_upgrade(self):
        self.dat.write_bytes(COMPILED_TEN_SLOTS.replace(b"ph_slot_count", b""))
        with self.assertRaises(PackagingError):
            validate_compiled(self.game)


if __name__ == "__main__":
    unittest.main()
