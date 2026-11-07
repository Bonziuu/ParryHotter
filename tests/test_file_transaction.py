"""Filesystem failure recovery preserves the original error and recovery data."""
from __future__ import annotations

import contextlib
import io
import os
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.file_transaction import FileTransaction


class FileTransactionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source.txt"
        self.source.write_bytes(b"previous source")
        self.target = self.root / "target.txt"
        self.target.write_bytes(b"previous target")

    def test_failed_rollback_reports_and_retains_originals_without_replacing_first_error(self):
        transaction = FileTransaction(self.root)
        self.addCleanup(shutil.rmtree, transaction._storage, ignore_errors=True)
        replace = Path.replace
        failure = PermissionError("original commit failure")
        rollback_failure = PermissionError("rollback file is locked")

        def fail_commit_and_source_restore(path, target):
            if Path(target) == self.target and path.read_bytes() == b"new target":
                raise failure
            if Path(target) == self.source and path.read_bytes() == b"previous source":
                raise rollback_failure
            return replace(path, target)

        stderr = io.StringIO()
        with mock.patch.object(Path, "replace", fail_commit_and_source_restore), contextlib.redirect_stderr(stderr):
            with self.assertRaises(PermissionError) as raised:
                with transaction:
                    transaction.write_bytes(self.source, b"new source")
                    transaction.write_bytes(self.target, b"new target")
        self.assertIs(raised.exception, failure)
        self.assertEqual(self.target.read_bytes(), b"previous target")
        self.assertEqual(self.source.read_bytes(), b"new source")
        self.assertIn("Rollback incomplete", stderr.getvalue())
        self.assertIn(str(self.source), stderr.getvalue())
        self.assertIn("rollback file is locked", stderr.getvalue())
        self.assertTrue(any("Rollback incomplete" in note for note in raised.exception.__notes__))
        self.assertIn(str(self.source), (transaction._storage / "RESTORE.txt").read_text())
        self.assertTrue(any(path.read_bytes() == b"previous source" for path in transaction._storage.iterdir()))

    def test_restored_original_has_previous_bytes_mode_and_mtime(self):
        self.source.chmod(0o640)
        os.utime(self.source, ns=(1_700_000_000_000_000_000, 1_700_000_001_123_456_789))
        previous = self.source.stat()
        failure = OSError("later failure")
        with self.assertRaises(OSError) as raised:
            with FileTransaction(self.root) as transaction:
                transaction.write_bytes(self.source, b"new source")
                raise failure
        self.assertIs(raised.exception, failure)
        self.assertEqual(self.source.read_bytes(), b"previous source")
        self.assertEqual(self.source.stat().st_mode, previous.st_mode)
        self.assertEqual(self.source.stat().st_mtime_ns, previous.st_mtime_ns)


if __name__ == "__main__":
    unittest.main()
