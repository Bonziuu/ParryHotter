"""Offline tests of the pinned reference cache using synthetic bytes only."""
from __future__ import annotations

import hashlib
import io
import json
import ssl
import tempfile
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from unittest.mock import Mock, patch

from tools import gothic_reference as reference


class GothicReferenceTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.game = Path(temporary.name) / "Gothic"
        self.target = self.game / "ParryHotter-private" / "Gothic-reference" / "HUM_BODY_NAKED0.ASC"
        self.data = b"synthetic reference fixture; no game assets\n"
        self.digest = hashlib.sha256(self.data).hexdigest()
        for name, value in (("REFERENCE_SHA256", self.digest), ("REFERENCE_SIZE", len(self.data))):
            stub = patch.object(reference, name, value)
            stub.start()
            self.addCleanup(stub.stop)

    def opener(self, data=None, error=None):
        opener = Mock()
        if error:
            opener.open.side_effect = error
        else:
            opener.open.return_value = io.BytesIO(self.data if data is None else data)
        return opener

    def assert_no_downloads(self):
        self.assertEqual(list(self.target.parent.glob("*.download")), [])

    def test_download_verifies_data_and_records_source_and_terms(self):
        opener = self.opener()
        with patch.object(reference.urllib.request, "build_opener", return_value=opener) as build:
            self.assertEqual(reference.ensure_gothic_reference(self.game), self.target)
        self.assertEqual(self.target.read_bytes(), self.data)
        request = opener.open.call_args.args[0]
        self.assertEqual(request.full_url, reference.REFERENCE_URL)
        self.assertIn(reference.COMMIT, request.full_url)
        https_handler = next(h for h in build.call_args.args if isinstance(h, urllib.request.HTTPSHandler))
        self.assertEqual(https_handler._context.verify_mode, ssl.CERT_REQUIRED)
        self.assertTrue(https_handler._context.check_hostname)
        metadata = json.loads((self.target.parent / "PROVENANCE.json").read_text())
        self.assertEqual(metadata["sha256"], self.digest)
        self.assertEqual(metadata["commit"], reference.COMMIT)
        self.assertEqual(metadata["terms_url"], reference.TERMS_URL)
        terms = (self.target.parent / "TERMS.txt").read_text()
        self.assertIn("non-commercial", terms)
        self.assertIn("THQ Nordic", terms)
        self.assertIn("2001 Piranha Bytes", terms)
        self.assert_no_downloads()

    def test_valid_cache_is_reused_offline_and_missing_metadata_restored(self):
        self.target.parent.mkdir(parents=True)
        self.target.write_bytes(self.data)
        with patch.object(reference.urllib.request, "build_opener", side_effect=AssertionError("network")):
            self.assertEqual(reference.ensure_gothic_reference(self.game), self.target)
        self.assertTrue((self.target.parent / "PROVENANCE.json").is_file())

    def test_bad_download_never_replaces_existing_corrupted_cache(self):
        self.target.parent.mkdir(parents=True)
        old = b"unrelated or corrupted local file"
        self.target.write_bytes(old)
        for data in (b"truncated", b"x" * len(self.data)):
            with self.subTest(data=data), patch.object(
                reference.urllib.request, "build_opener", return_value=self.opener(data)
            ):
                with self.assertRaisesRegex(reference.GothicReferenceError, "SHA256"):
                    reference.ensure_gothic_reference(self.game)
                self.assertEqual(self.target.read_bytes(), old)
                self.assert_no_downloads()

    def test_corrupted_cache_is_replaced_only_after_verified_download(self):
        self.target.parent.mkdir(parents=True)
        self.target.write_bytes(b"wrong" * 9)
        with patch.object(reference.urllib.request, "build_opener", return_value=self.opener()):
            reference.ensure_gothic_reference(self.game)
        self.assertEqual(self.target.read_bytes(), self.data)

    def test_failed_first_download_leaves_no_reference_or_temporary_file(self):
        with patch.object(reference.urllib.request, "build_opener", return_value=self.opener(b"HTML error")):
            with self.assertRaises(reference.GothicReferenceError):
                reference.ensure_gothic_reference(self.game)
        self.assertFalse(self.target.exists())
        self.assert_no_downloads()

    def test_oversized_response_stops_at_strict_cap(self):
        with patch.object(reference, "MAX_BYTES", len(self.data) + 2), patch.object(
            reference.urllib.request, "build_opener", return_value=self.opener(self.data * 3)
        ):
            with self.assertRaisesRegex(reference.GothicReferenceError, "size limit"):
                reference.ensure_gothic_reference(self.game)
        self.assertFalse(self.target.exists())
        self.assert_no_downloads()

    def test_network_failure_is_actionable_and_preserves_bad_cache(self):
        self.target.parent.mkdir(parents=True)
        self.target.write_bytes(b"existing incomplete file")
        with patch.object(reference.urllib.request, "build_opener", return_value=self.opener(
            error=urllib.error.URLError("Tunnel connection failed: 403 Forbidden")
        )):
            with self.assertRaises(reference.GothicReferenceError) as error:
                reference.ensure_gothic_reference(self.game)
        self.assertIn("raw.githubusercontent.com", str(error.exception))
        self.assertIn(str(self.target), str(error.exception))
        self.assertIn(self.digest, str(error.exception))
        self.assertEqual(self.target.read_bytes(), b"existing incomplete file")
        self.assert_no_downloads()

    def test_interrupted_response_is_cleaned_up(self):
        response = Mock()
        response.__enter__ = Mock(return_value=response)
        response.__exit__ = Mock(return_value=False)
        response.read.side_effect = [b"partial", TimeoutError("timed out")]
        opener = Mock()
        opener.open.return_value = response
        with patch.object(reference.urllib.request, "build_opener", return_value=opener):
            with self.assertRaises(reference.GothicReferenceError):
                reference.ensure_gothic_reference(self.game)
        self.assertFalse(self.target.exists())
        self.assert_no_downloads()

    def test_redirects_cannot_leave_https_source(self):
        handler = reference._HTTPSOnlyRedirect()
        request = urllib.request.Request(reference.REFERENCE_URL)
        for url in ("http://raw.githubusercontent.com/file", "https://example.com/file"):
            with self.subTest(url=url), self.assertRaises(reference.GothicReferenceError):
                handler.redirect_request(request, None, 302, "Found", {}, url)


if __name__ == "__main__":
    unittest.main()
