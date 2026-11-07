"""Converted bundle validation and source installation without game assets."""
from __future__ import annotations

import stat
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

from tools.import_harry_bundle import ASSET_NAMES, MAX_ASSET_BYTES, import_harry_bundle, validate_harry_bundle
from tools.visual_install import PatchError
from tools.casting_assets import CASTING_ASSET_NAMES, CASTING_MDS, CHARGE_ASC, RELEASE_ASC
from tests.test_casting_assets import write_casting_assets


def texture(red=255):
    header = bytearray(18)
    header[2] = 2
    header[12:16] = b"\x04\x00\x04\x00"
    header[16:18] = b"\x20\x28"
    return bytes(header) + bytes((0, 0, red, 255)) * 16


class ConvertedBundleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.game = self.root / "Gothic"
        (self.game / "System").mkdir(parents=True)
        self.content = self.game / "_work" / "DATA" / "scripts" / "Content"
        (self.content / "Story" / "NPC").mkdir(parents=True)
        (self.content / "Items").mkdir()
        (self.content / "Gothic.src").write_text("ITEMS\\ARMOR.D\n", encoding="latin-1")
        self.hero = self.content / "Story" / "NPC" / "PC_Hero.d"
        self.hero.write_text(
            'INSTANCE PC_Hero(NPC_DEFAULT)\n{\n'
            'Mdl_SetVisualBody(self,"hum_body_Naked0",4,1,"Hum_Head_Pony",9,0,KDF_ARMOR_L);\n};\n',
            encoding="latin-1",
        )
        self.anims = self.game / "_work" / "DATA" / "Anims"
        self.anims.mkdir(parents=True)
        self.mds = self.anims / "HUMANS.MDS"
        self.mds.write_text('Model("HUMANS")\n{\nregisterMesh("Hum_Body_Naked0.ASC")\naniEnum\n{\n}\n}\n')
        self.assets = self.root / "assets"
        self.assets.mkdir()
        fixture = Path(__file__).parent / "fixtures" / "harry_triangle.asc.txt"
        (self.assets / "PH_HARRY.ASC").write_bytes(fixture.read_bytes())
        for index in range(4):
            (self.assets / f"PH_HARRY_TEX{index}.TGA").write_bytes(texture())
        self.converted = self.game / "ParryHotter-private" / "HP2-Harry-converted"

    def snapshot(self):
        return {str(p.relative_to(self.game)): p.read_bytes()
                for p in self.game.rglob("*") if p.is_file()}

    def directories(self):
        return {str(p.relative_to(self.game)) for p in self.game.rglob("*") if p.is_dir()}

    def assert_failed_replace_restores_installation(self, target):
        before, directories = self.snapshot(), self.directories()
        failure = OSError("injected model replacement failure")
        replace = Path.replace
        injected = False

        def fail_once(path, destination):
            nonlocal injected
            if Path(destination) == target and not injected:
                injected = True
                raise failure
            return replace(path, destination)

        with mock.patch.object(Path, "replace", fail_once):
            with self.assertRaises(OSError) as raised:
                import_harry_bundle(self.game, self.assets)
        self.assertIs(raised.exception, failure)
        self.assertTrue(injected)
        self.assertEqual(before, self.snapshot())
        self.assertEqual(directories, self.directories())

    def bundle_zip(self):
        archive = self.root / "converted.zip"
        with zipfile.ZipFile(archive, "w") as output:
            for name in ASSET_NAMES + CASTING_ASSET_NAMES:
                if (self.assets / name).is_file():
                    output.write(self.assets / name, "Model/" + name)
            output.writestr("README.txt", "Not executed; source assets only")
        return archive

    def assert_rejected_without_changes(self, bundle, pattern=None):
        before = self.snapshot()
        if pattern:
            with self.assertRaisesRegex(PatchError, pattern):
                import_harry_bundle(self.game, bundle)
        else:
            with self.assertRaises(PatchError):
                import_harry_bundle(self.game, bundle)
        self.assertEqual(before, self.snapshot())
        self.assertFalse(self.converted.exists())

    def test_zip_installs_validated_model_and_preserves_other_files(self):
        other = self.game / "System" / "user.ini"
        other.write_bytes(b"user configuration")
        result = import_harry_bundle(self.game, self.bundle_zip())
        self.assertEqual(result, self.converted)
        self.assertEqual(other.read_bytes(), b"user configuration")
        self.assertIn("PH_ARMOR_HARRY", self.hero.read_text())
        self.assertNotIn("Hum_Head_Pony", self.hero.read_text())
        self.assertIn('registerMesh ("PH_HARRY.ASC")', self.mds.read_text())
        self.assertTrue((self.content / "Items" / "ParryHarryArmor.d").is_file())
        for name in ASSET_NAMES:
            self.assertEqual((self.converted / name).read_bytes(), (self.assets / name).read_bytes())
        self.assertFalse((self.game / "README.txt").exists())

    def test_repeat_directory_installation_is_unchanged(self):
        import_harry_bundle(self.game, self.assets)
        before = self.snapshot()
        mtimes = {p: p.stat().st_mtime_ns for p in self.game.rglob("*") if p.is_file()}
        import_harry_bundle(self.game, self.assets)
        self.assertEqual(before, self.snapshot())
        self.assertEqual(mtimes, {p: p.stat().st_mtime_ns for p in self.game.rglob("*") if p.is_file()})

    def test_optional_animation_zip_installs_only_for_pc_hero_and_is_repeatable(self):
        write_casting_assets(self.assets)
        self.hero.write_text(self.hero.read_text().replace('};\n',
            'Mdl_ApplyOverlayMds(self,"Humans_Mage.mds");\n};\n')
            + 'INSTANCE PC_Hero_L2(NPC_DEFAULT)\n{\nname="untouched";\n};\n')
        import_harry_bundle(self.game, self.bundle_zip())
        first, mtimes = self.snapshot(), {p: p.stat().st_mtime_ns for p in self.game.rglob("*") if p.is_file()}
        for name in CASTING_ASSET_NAMES:
            self.assertEqual((self.anims / name).read_bytes(), (self.assets / name).read_bytes())
            self.assertEqual((self.converted / name).read_bytes(), (self.assets / name).read_bytes())
            self.assertFalse((self.game / "_work" / "DATA" / "Textures" / name).exists())
        player, other = self.hero.read_text().split('INSTANCE PC_Hero_L2', 1)
        self.assertEqual(player.count(CASTING_MDS), 1)
        self.assertGreater(player.index(CASTING_MDS), player.index('Humans_Mage.mds'))
        self.assertNotIn(CASTING_MDS, other)
        import_harry_bundle(self.game, self.assets)
        self.assertEqual(self.snapshot(), first)
        self.assertEqual(mtimes, {p: p.stat().st_mtime_ns for p in self.game.rglob("*") if p.is_file()})

    def test_partial_animation_bundle_is_rejected_before_any_game_changes(self):
        write_casting_assets(self.assets)
        (self.assets / RELEASE_ASC).unlink()
        bundle = self.bundle_zip()
        self.assert_rejected_without_changes(bundle, "Incomplete")
        with self.assertRaisesRegex(PatchError, "Incomplete"):
            validate_harry_bundle(bundle)

    def test_bad_animation_root_is_rejected_before_cache_or_game_changes(self):
        write_casting_assets(self.assets)
        clip = self.assets / CHARGE_ASC
        clip.write_bytes(clip.read_bytes().replace(b'*CONTROL_POS_SAMPLE 320 0 0 0', b'*CONTROL_POS_SAMPLE 320 5 0 0', 1))
        self.assert_rejected_without_changes(self.assets, "root must stay fixed")

    def test_late_animation_write_failure_restores_model_scripts_cache_and_backups(self):
        write_casting_assets(self.assets)
        self.assert_failed_replace_restores_installation(self.anims / RELEASE_ASC)
        self.assertFalse(self.converted.exists())

    def test_legacy_model_bundle_preserves_existing_matching_animation_cache(self):
        write_casting_assets(self.assets)
        import_harry_bundle(self.game, self.assets)
        before = {name: ((self.anims / name).read_bytes(), (self.anims / name).stat().st_mtime_ns)
                  for name in CASTING_ASSET_NAMES}
        for name in CASTING_ASSET_NAMES:
            (self.assets / name).unlink()
        model = self.assets / "PH_HARRY.ASC"
        model.write_bytes(model.read_bytes() + b'\n// updated mesh metadata\n')
        import_harry_bundle(self.game, self.assets)
        for name, (data, timestamp) in before.items():
            self.assertEqual((self.anims / name).read_bytes(), data)
            self.assertEqual((self.converted / name).read_bytes(), data)
            self.assertEqual((self.anims / name).stat().st_mtime_ns, timestamp)
        self.assertEqual(self.hero.read_text().count(CASTING_MDS), 1)

    def test_legacy_model_update_cannot_reuse_clips_for_a_different_bind_rig(self):
        write_casting_assets(self.assets)
        import_harry_bundle(self.game, self.assets)
        before = self.snapshot()
        for name in CASTING_ASSET_NAMES:
            (self.assets / name).unlink()
        model = self.assets / "PH_HARRY.ASC"
        model.write_bytes(model.read_bytes().replace(b'*TM_ROW3 0 0 2\n', b'*TM_ROW3 0 0 3\n', 1))
        with self.assertRaisesRegex(PatchError, "different bind rig"):
            import_harry_bundle(self.game, self.assets)
        self.assertEqual(before, self.snapshot())

    def test_failed_animation_update_restores_previous_active_and_private_clips(self):
        write_casting_assets(self.assets)
        import_harry_bundle(self.game, self.assets)
        clip = self.assets / RELEASE_ASC
        text = clip.read_text()
        start = text.index('*TM_ANIMATION {\n*NODE_NAME "Bip01 R Hand"')
        end = text.index('*HELPEROBJECT', start)
        section = text[start:end]
        row = next(row for row in section.splitlines() if row.startswith('*CONTROL_ROT_SAMPLE 800 '))
        clip.write_text(text[:start] + section.replace(row, '*CONTROL_ROT_SAMPLE 800 0 0 1 0.4') + text[end:])
        self.assert_failed_replace_restores_installation(self.anims / RELEASE_ASC)
        self.assertFalse((self.converted / (RELEASE_ASC + '.parryhotter-original')).exists())

    def test_bad_skin_fails_before_cache_or_game_writes(self):
        (self.assets / "PH_HARRY.ASC").write_bytes(b"fake ASC with Bip01 marker")
        self.assert_rejected_without_changes(self.assets)

    def test_truncated_texture_fails_before_cache_or_game_writes(self):
        (self.assets / "PH_HARRY_TEX2.TGA").write_bytes(texture()[:20])
        self.assert_rejected_without_changes(self.bundle_zip(), "Truncated")

    def test_unsafe_zip_entries_are_rejected_even_when_not_assets(self):
        for name in ("../outside.txt", "/absolute.txt", "C:/drive.txt", "folder/../escape.txt", r"..\escape.txt"):
            with self.subTest(name=name):
                archive = self.bundle_zip()
                with zipfile.ZipFile(archive, "a") as output:
                    output.writestr(name, "unsafe")
                self.assert_rejected_without_changes(archive, "Unsafe")

    def test_zip_symlink_is_rejected(self):
        archive = self.bundle_zip()
        info = zipfile.ZipInfo("linked.txt")
        info.create_system = 3
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        with zipfile.ZipFile(archive, "a") as output:
            output.writestr(info, "/outside")
        self.assert_rejected_without_changes(archive, "Unsafe")

    def test_duplicate_case_insensitive_asset_is_rejected(self):
        archive = self.bundle_zip()
        with zipfile.ZipFile(archive, "a") as output:
            output.writestr("Other/ph_harry.asc", (self.assets / "PH_HARRY.ASC").read_bytes())
        self.assert_rejected_without_changes(archive, "Duplicate")

    def test_missing_asset_is_rejected(self):
        (self.assets / "PH_HARRY_TEX3.TGA").unlink()
        self.assert_rejected_without_changes(self.assets, "missing")

    def test_zip_size_cap_is_enforced_before_validation_and_writes(self):
        archive = self.bundle_zip()
        with mock.patch.dict(MAX_ASSET_BYTES, {"PH_HARRY.ASC": 20}):
            self.assert_rejected_without_changes(archive, "size limit")

    def test_missing_modkit_sources_fail_before_private_cache_creation(self):
        self.mds.unlink()
        self.assert_rejected_without_changes(self.assets, "ModKit")

    def test_missing_system_is_not_an_installation(self):
        (self.game / "System").rmdir()
        self.assert_rejected_without_changes(self.assets, "System")

    def test_bad_hero_anchor_fails_before_private_cache_creation(self):
        self.hero.write_text("unrelated character script")
        self.assert_rejected_without_changes(self.assets, "PC_Hero")

    def test_unrelated_active_model_is_preserved_before_cache_writes(self):
        active = self.anims / "PH_HARRY.ASC"
        active.write_bytes(b"other mod")
        self.assert_rejected_without_changes(self.assets, "unrelated")
        self.assertEqual(active.read_bytes(), b"other mod")

    def test_existing_cache_and_first_backup_are_preserved(self):
        self.converted.mkdir(parents=True)
        old = (self.assets / "PH_HARRY.ASC").read_bytes() + b"\n// user's draft\n"
        (self.converted / "PH_HARRY.ASC").write_bytes(old)
        first = self.converted / "PH_HARRY.ASC.parryhotter-original"
        first.write_bytes(b"first original conversion")
        note = self.converted / "notes.txt"
        note.write_bytes(b"keep user notes")
        import_harry_bundle(self.game, self.assets)
        self.assertEqual(first.read_bytes(), b"first original conversion")
        self.assertEqual((self.converted / "PH_HARRY.ASC.parryhotter-backup-1").read_bytes(), old)
        self.assertEqual(note.read_bytes(), b"keep user notes")

    def test_public_bundle_validation_never_changes_game(self):
        before = self.snapshot()
        self.assertIsNone(validate_harry_bundle(self.bundle_zip()))
        self.assertEqual(before, self.snapshot())
        self.assertFalse(self.converted.exists())
        (self.assets / "PH_HARRY_TEX3.TGA").write_bytes(b"bad texture")
        with self.assertRaises(PatchError):
            validate_harry_bundle(self.assets)
        self.assertEqual(before, self.snapshot())

    def test_cache_write_failure_removes_only_created_files_and_directories(self):
        self.assert_failed_replace_restores_installation(self.converted / "PH_HARRY_TEX1.TGA")
        self.assertFalse(self.converted.parent.exists())

    def test_armor_write_failure_restores_scripts_and_removes_cache_and_backups(self):
        self.assert_failed_replace_restores_installation(self.content / "Items" / "ParryHarryArmor.d")
        self.assertNotIn("PH_ARMOR_HARRY", self.hero.read_text())

    def test_final_texture_failure_restores_all_active_assets_and_sources(self):
        target = self.game / "_work" / "DATA" / "Textures" / "PH_HARRY_TEX3.TGA"
        self.assert_failed_replace_restores_installation(target)
        self.assertFalse(target.parent.exists())

    def test_own_active_assets_can_be_updated_from_a_new_valid_bundle(self):
        import_harry_bundle(self.game, self.assets)
        previous = self.snapshot()
        (self.assets / "PH_HARRY.ASC").write_bytes((self.assets / "PH_HARRY.ASC").read_bytes() + b"\n// revision 2\n")
        for index in range(4):
            (self.assets / f"PH_HARRY_TEX{index}.TGA").write_bytes(texture(red=128))
        import_harry_bundle(self.game, self.assets)
        for name in ASSET_NAMES:
            active = (self.anims if name.endswith(".ASC") else self.game / "_work" / "DATA" / "Textures") / name
            self.assertEqual(active.read_bytes(), (self.assets / name).read_bytes())
            backup = active.with_name(active.name + ".parryhotter-original")
            self.assertEqual(backup.read_bytes(), previous[str(active.relative_to(self.game))])

    def test_failed_update_restores_cache_active_assets_and_versioned_backups(self):
        import_harry_bundle(self.game, self.assets)
        first = self.converted / "PH_HARRY.ASC.parryhotter-original"
        first.write_bytes(b"keep first conversion backup")
        note = self.converted / "notes.txt"
        note.write_bytes(b"keep private notes")
        (self.assets / "PH_HARRY.ASC").write_bytes((self.assets / "PH_HARRY.ASC").read_bytes() + b"\n// revision 2\n")
        for index in range(4):
            (self.assets / f"PH_HARRY_TEX{index}.TGA").write_bytes(texture(red=128))
        self.assert_failed_replace_restores_installation(self.game / "_work" / "DATA" / "Textures" / "PH_HARRY_TEX3.TGA")
        self.assertEqual(first.read_bytes(), b"keep first conversion backup")
        self.assertEqual(note.read_bytes(), b"keep private notes")
        self.assertFalse((self.converted / "PH_HARRY.ASC.parryhotter-backup-1").exists())

    def test_asset_symlinks_are_rejected_without_reading_their_target(self):
        source = self.assets / "PH_HARRY.ASC"
        actual = self.root / "actual.asc"
        source.rename(actual)
        try:
            source.symlink_to(actual)
        except OSError:
            self.skipTest("Creating file symlinks is unavailable on this platform")
        self.assert_rejected_without_changes(self.assets, "regular file")

    def test_game_source_directory_cannot_escape_the_installation(self):
        outside = self.root / "outside"
        original = self.game / "_work"
        original.rename(outside)
        try:
            original.symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest("Creating directory symlinks is unavailable on this platform")
        self.assert_rejected_without_changes(self.assets, "escapes")


if __name__ == "__main__":
    unittest.main()
