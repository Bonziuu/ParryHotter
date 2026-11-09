"""Offline updater tests: one payload changes, backups and rollback remain local."""
import hashlib
from contextlib import redirect_stderr
import io
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from tests.test_verify_dll import synthetic_pe
from tools import update_hud as updater
from tools.download_hud import HudDownloadError
from tools.file_transaction import FileTransaction
from tools.vdf import build_vdf, read_vdf


class UpdateHudTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name).resolve()
        self.project = self.directory / "project"
        self.project.mkdir()
        self.game = self.directory / "Gothic"
        system = self.game / "System"
        system.mkdir(parents=True)
        (system / "ParryHotter.ini").write_bytes(b"DEV profile: keep bytes\r\n")
        (system / "ParryHotter-PLAY.ini").write_bytes(b"PLAY profile: keep bytes\r\n")
        self.archive = self.game / "Data" / "ModVDF" / "ParryHotter.mod"
        self.archive.parent.mkdir(parents=True)
        self.cached = self.project / ".parryhotter" / "build" / updater.EXPECTED_NAME
        self.cached.parent.mkdir(parents=True)
        self.old_dll = synthetic_pe() + b"old DLL"
        self.new_dll = synthetic_pe(imports=("KERNEL32.dll", "Shw32.dll")) + b"new DLL"
        self.files = {
            updater.DLL_ENTRY: self.old_dll,
            updater.DAT_ENTRY: b"Flipendo PH_UNION_LOADED PH_SELECTED_SPELL compiled locally",
            "_WORK/DATA/ANIMS/PH_HARRY.ASC": b"private model: never regenerate or upload\x00",
            "_WORK/DATA/TEXTURES/PH_HARRY_TEX0.TGA": bytes(range(256)),
            "_WORK/DATA/SCRIPTS/CONTENT/CUTSCENE/OU.BIN": b"local Polish dialogue\x00\xfe",
            "EXTRA/PRIVATE-FILE.BIN": b"unrelated custom content\x00\x01\xff",
        }
        self.archive.write_bytes(build_vdf(self.files, comment="local private mod"))
        self.cached.write_bytes(self.old_dll)
        self.side_files = {
            system / "ParryHotter.ini": (system / "ParryHotter.ini").read_bytes(),
            system / "ParryHotter-PLAY.ini": (system / "ParryHotter-PLAY.ini").read_bytes(),
            self.game / "_work" / "Data" / "Anims" / "HUMANS.MDS.parryhotter-original": b"first original MDS",
            self.game / "_work" / "Data" / "Textures" / "PH_HARRY_TEX0.TGA.parryhotter-original-legs": b"first trousers backup",
            self.game / "_work" / "Data" / "Anims" / "PH_HARRY.ASC": b"live local model unchanged",
            self.archive.with_name(self.archive.name + ".parryhotter-original"): b"first archive backup",
            self.archive.with_name(self.archive.name + ".parryhotter-backup-0"): b"previous numbered archive backup",
            self.cached.with_name(self.cached.name + ".parryhotter-original"): b"first DLL backup",
            self.cached.with_name(self.cached.name + ".parryhotter-backup-0"): b"previous numbered DLL backup",
        }
        for path, content in self.side_files.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        fetch_patch = mock.patch.object(updater, "fetch_verified_hud", side_effect=self.download)
        self.fetch = fetch_patch.start()
        self.addCleanup(fetch_patch.stop)
        closed_patch = mock.patch.object(updater, "ensure_game_closed")
        self.closed = closed_patch.start()
        self.addCleanup(closed_patch.stop)

    def download(self, destination, *, tag):
        self.assertEqual(tag, "dev013-latest")
        destination.write_bytes(self.new_dll)
        return destination

    def snapshot(self):
        return {path.relative_to(self.directory): path.read_bytes()
                for path in self.directory.rglob("*") if path.is_file()}

    def assert_side_files_preserved(self):
        for path, data in self.side_files.items():
            self.assertEqual(path.read_bytes(), data, str(path))

    def assert_persistent_backups(self, old_archive, old_cached):
        created = [path for path in self.directory.rglob("*.parryhotter-backup-*")
                   if path not in self.side_files]
        self.assertEqual(len(created), 2)
        self.assertEqual({path.read_bytes() for path in created}, {old_archive, old_cached})
        self.assertTrue(all(not path.name.lower().endswith(".mod") for path in created))
        self.assert_side_files_preserved()

    def test_update_changes_only_dll_payload_and_cache_with_persistent_backups(self):
        old_archive = self.archive.read_bytes()
        result = updater.update_hud(self.game, self.project)
        expected = dict(self.files)
        expected[updater.DLL_ENTRY] = self.new_dll
        self.assertEqual(read_vdf(self.archive.read_bytes()), expected)
        self.assertEqual(self.cached.read_bytes(), self.new_dll)
        self.assertEqual(result.archive, self.archive)
        self.assertEqual(result.dll, self.cached)
        self.assertEqual(result.sha256, hashlib.sha256(self.new_dll).hexdigest())
        self.assertTrue(result.changed)
        self.assertEqual(len(result.backups), 2)
        self.assert_persistent_backups(old_archive, self.old_dll)
        self.assertEqual(self.closed.call_count, 2)
        self.fetch.assert_called_once()

    def test_download_failure_leaves_all_files_and_existing_backups_unchanged(self):
        before = self.snapshot()
        error = HudDownloadError("checksum mismatch")
        self.fetch.side_effect = error
        with self.assertRaises(HudDownloadError) as raised:
            updater.update_hud(self.game, self.project)
        self.assertIs(raised.exception, error)
        self.assertEqual(self.snapshot(), before)

    def test_current_dll_is_idempotent_without_new_backups_or_mtime_changes(self):
        current = dict(self.files)
        current[updater.DLL_ENTRY] = self.new_dll
        self.archive.write_bytes(build_vdf(current, comment="already current"))
        self.cached.write_bytes(self.new_dll)
        for path in (self.archive, self.cached):
            os.utime(path, ns=(1_600_000_000_111_000_000, 1_600_000_000_222_000_000))
        before = self.snapshot()
        mtimes = {path: path.stat().st_mtime_ns for path in (self.archive, self.cached)}
        result = updater.update_hud(self.game, self.project)
        self.assertFalse(result.changed)
        self.assertEqual(result.backups, ())
        self.assertEqual(self.snapshot(), before)
        self.assertEqual({path: path.stat().st_mtime_ns for path in mtimes}, mtimes)

    def test_already_current_archive_updates_only_stale_cache(self):
        current = dict(self.files)
        current[updater.DLL_ENTRY] = self.new_dll
        self.archive.write_bytes(build_vdf(current))
        old_archive = self.archive.read_bytes()
        old_mtime = self.archive.stat().st_mtime_ns
        result = updater.update_hud(self.game, self.project)
        self.assertTrue(result.changed)
        self.assertEqual(len(result.backups), 1)
        self.assertEqual(result.backups[0].read_bytes(), self.old_dll)
        self.assertEqual(self.archive.read_bytes(), old_archive)
        self.assertEqual(self.archive.stat().st_mtime_ns, old_mtime)
        self.assertEqual(self.cached.read_bytes(), self.new_dll)
        self.assert_side_files_preserved()

    def test_missing_cache_is_created_without_a_backup_of_nonexistent_dll(self):
        self.cached.unlink()
        old_archive = self.archive.read_bytes()
        result = updater.update_hud(self.game, self.project)
        self.assertTrue(result.changed)
        self.assertEqual(self.cached.read_bytes(), self.new_dll)
        self.assertEqual(len(result.backups), 1)
        self.assertEqual(result.backups[0].read_bytes(), old_archive)
        self.assert_side_files_preserved()

    def test_archive_write_failure_restores_cache_and_keeps_both_backups(self):
        old_archive = self.archive.read_bytes()
        original_write = FileTransaction.write_bytes
        error = OSError("injected archive replacement failure")

        def fail_archive(transaction, path, data):
            if path == self.archive:
                self.assertEqual(self.cached.read_bytes(), self.new_dll)
                raise error
            return original_write(transaction, path, data)

        with mock.patch.object(FileTransaction, "write_bytes", fail_archive):
            with self.assertRaises(OSError) as raised:
                updater.update_hud(self.game, self.project)
        self.assertIs(raised.exception, error)
        self.assertEqual(self.archive.read_bytes(), old_archive)
        self.assertEqual(self.cached.read_bytes(), self.old_dll)
        self.assert_persistent_backups(old_archive, self.old_dll)

    def test_verification_failure_after_both_writes_rolls_back_both_roots(self):
        old_archive = self.archive.read_bytes()
        original_write = FileTransaction.write_bytes
        original_read = updater.read_vdf
        archive_written = False

        def record_write(transaction, path, data):
            nonlocal archive_written
            result = original_write(transaction, path, data)
            if path == self.archive:
                archive_written = True
            return result

        def corrupt_committed_check(data):
            parsed = original_read(data)
            if archive_written:
                self.assertEqual(self.cached.read_bytes(), self.new_dll)
                self.assertEqual(parsed[updater.DLL_ENTRY], self.new_dll)
                parsed[updater.DLL_ENTRY] = b"simulated failed disk verification"
            return parsed

        with mock.patch.object(FileTransaction, "write_bytes", record_write), \
                mock.patch.object(updater, "read_vdf", side_effect=corrupt_committed_check):
            with self.assertRaises(updater.UpdateError):
                updater.update_hud(self.game, self.project)
        self.assertTrue(archive_written)
        self.assertEqual(self.archive.read_bytes(), old_archive)
        self.assertEqual(self.cached.read_bytes(), self.old_dll)
        self.assert_persistent_backups(old_archive, self.old_dll)

    def test_backup_failure_aborts_before_either_active_file_is_replaced(self):
        old_archive = self.archive.read_bytes()
        original_backup = updater.keep_backup

        def fail_cache_backup(path):
            if path == self.cached:
                raise OSError("injected backup failure")
            return original_backup(path)

        with mock.patch.object(updater, "keep_backup", side_effect=fail_cache_backup):
            with self.assertRaises(OSError):
                updater.update_hud(self.game, self.project)
        self.assertEqual(self.archive.read_bytes(), old_archive)
        self.assertEqual(self.cached.read_bytes(), self.old_dll)
        backups = [path for path in self.archive.parent.glob("*.parryhotter-backup-*")
                   if path not in self.side_files]
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), old_archive)
        self.assert_side_files_preserved()

    def test_invalid_archive_or_mod_markers_are_refused_before_download_or_backups(self):
        without_dll = dict(self.files)
        without_dll.pop(updater.DLL_ENTRY)
        without_union = dict(self.files)
        without_union[updater.DAT_ENTRY] = b"stock Gothic.dat"
        old_build = dict(self.files)
        old_build[updater.DLL_ENTRY] = synthetic_pe(marker=b"foreign old build")
        for archive in (b"not a VDFS archive", build_vdf(without_dll),
                        build_vdf(without_union), build_vdf(old_build)):
            with self.subTest(archive_prefix=archive[:20]):
                self.archive.write_bytes(archive)
                before = self.snapshot()
                self.fetch.reset_mock()
                with self.assertRaises(updater.UpdateError):
                    updater.update_hud(self.game, self.project)
                self.fetch.assert_not_called()
                self.assertEqual(self.snapshot(), before)

    def test_game_starting_during_download_aborts_before_backups_or_writes(self):
        before = self.snapshot()
        self.closed.side_effect = [None, updater.UpdateError("Gothic started")]
        with self.assertRaises(updater.UpdateError):
            updater.update_hud(self.game, self.project)
        self.fetch.assert_called_once()
        self.assertEqual(self.closed.call_count, 2)
        self.assertEqual(self.snapshot(), before)

    def test_concurrent_archive_change_is_preserved_and_aborts_update(self):
        concurrent = dict(self.files)
        concurrent["EXTRA/PRIVATE-FILE.BIN"] = b"new user data while download runs"
        changed_archive = build_vdf(concurrent)
        before = self.snapshot()

        def download_and_change(destination, *, tag):
            result = self.download(destination, tag=tag)
            self.archive.write_bytes(changed_archive)
            return result

        self.fetch.side_effect = download_and_change
        with self.assertRaisesRegex(updater.UpdateError, "zmienily"):
            updater.update_hud(self.game, self.project)
        before[self.archive.relative_to(self.directory)] = changed_archive
        self.assertEqual(self.snapshot(), before)

    def test_concurrent_cache_change_is_preserved_and_aborts_update(self):
        before = self.snapshot()
        concurrent_cache = b"user changed local cache during download"

        def download_and_change(destination, *, tag):
            result = self.download(destination, tag=tag)
            self.cached.write_bytes(concurrent_cache)
            return result

        self.fetch.side_effect = download_and_change
        with self.assertRaisesRegex(updater.UpdateError, "zmienily"):
            updater.update_hud(self.game, self.project)
        before[self.cached.relative_to(self.directory)] = concurrent_cache
        self.assertEqual(self.snapshot(), before)

    def test_main_run_launches_once_only_after_successful_update(self):
        def launch_after_update(game):
            self.assertEqual(game, self.game)
            self.assertEqual(self.cached.read_bytes(), self.new_dll)
            self.assertEqual(read_vdf(self.archive.read_bytes())[updater.DLL_ENTRY], self.new_dll)

        with mock.patch.object(updater, "launch_starter", side_effect=launch_after_update) as launch:
            result = updater.main(["--game", str(self.game), "--project", str(self.project), "--run"])
        self.assertEqual(result, 0)
        launch.assert_called_once_with(self.game)

    def test_main_run_never_launches_after_download_or_filesystem_failure(self):
        for error in (HudDownloadError("checksum failure"), OSError("disk failure")):
            with self.subTest(error=error):
                with mock.patch.object(updater, "update_hud", side_effect=error), \
                        mock.patch.object(updater, "launch_starter") as launch, \
                        redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as raised:
                        updater.main(["--game", str(self.game), "--project", str(self.project), "--run"])
                self.assertEqual(raised.exception.code, 1)
                launch.assert_not_called()


class WindowsProcessChecks(unittest.TestCase):
    NORMAL = '"System Idle Process","0","Services","0","8 K"\r\n'

    def check(self, output, *, returncode=0):
        result = SimpleNamespace(returncode=returncode, stdout=output, stderr="")
        with mock.patch.object(updater.sys, "platform", "win32"), \
                mock.patch.object(updater.subprocess, "run", return_value=result) as run:
            updater.ensure_game_closed()
        command = run.call_args.args[0]
        self.assertTrue(command[0].endswith("tasklist.exe"))
        self.assertEqual(command[1:], ["/FO", "CSV", "/NH"])
        self.assertEqual(run.call_args.kwargs["timeout"], 20)

    def test_valid_tasklist_with_closed_game_and_starter_is_allowed(self):
        self.check(self.NORMAL + '"GothicStarter_mod.exe","123","Console","1","20 K"\r\n')

    def test_running_game_is_refused_case_insensitively(self):
        for executable in ("Gothic.exe", "GOTHICMOD.EXE", "gothicmod.exe"):
            with self.subTest(executable=executable):
                with self.assertRaisesRegex(updater.UpdateError, "Zamknij Gothic"):
                    self.check(self.NORMAL + f'"{executable}","123","Console","1","200 K"\r\n')

    def test_failed_empty_and_malformed_tasklist_are_refused(self):
        for output, returncode in ((self.NORMAL, 1), ("", 0), ("access denied", 0),
                                   ('"unfinished quote', 0),
                                   (self.NORMAL + '"GothicMod.exe","not-a-pid"\r\n', 0)):
            with self.subTest(output=output, returncode=returncode):
                with self.assertRaises(updater.UpdateError):
                    self.check(output, returncode=returncode)

    def test_tasklist_launch_error_and_timeout_are_refused(self):
        for error in (OSError("tasklist unavailable"), subprocess.TimeoutExpired("tasklist", 20)):
            with self.subTest(error=error):
                with mock.patch.object(updater.sys, "platform", "win32"), \
                        mock.patch.object(updater.subprocess, "run", side_effect=error):
                    with self.assertRaises(updater.UpdateError):
                        updater.ensure_game_closed()

    def test_non_windows_test_environment_does_not_launch_tasklist(self):
        with mock.patch.object(updater.sys, "platform", "linux"), \
                mock.patch.object(updater.subprocess, "run") as run:
            updater.ensure_game_closed()
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
