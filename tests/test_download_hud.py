"""Reject incompatible downloaded builds even when their checksum is valid."""
import hashlib
import io
import json
import struct
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest.mock import patch

from tools.download_hud import (
    EXPECTED_NAME, HudDownloadError, fetch_verified_hud, validate_asset, validate_hud_binary,
)
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


def win32_dll(*, machine=0x014C) -> bytes:
    """Minimal PE32 fixture with a current marker and no imported libraries."""
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, machine, 1, 0, 0, 0, 0xE0, 0x2102)
    struct.pack_into("<H", data, 0x98, 0x010B)
    section = 0x98 + 0xE0
    data[section:section + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", data, section + 8, 512, 0x1000, 512, 512)
    data[512:512 + len(HUD_BUILD_MARKER)] = HUD_BUILD_MARKER
    return bytes(data)


def release_asset(name, data, *, tag="dev013-latest"):
    return {
        "name": name,
        "browser_download_url": f"https://github.com/Bonziuu/ParryHotter/releases/download/{tag}/{name}",
        "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
        "size": len(data),
    }


def dev_release(data, checksum=None):
    if checksum is None:
        checksum = f"{hashlib.sha256(data).hexdigest()}  {EXPECTED_NAME}".encode()
    return {
        "tag_name": "dev013-latest",
        "assets": [release_asset(EXPECTED_NAME, data),
                   release_asset(EXPECTED_NAME + ".sha256", checksum)],
    }, checksum


class Dev013DownloadTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.target = self.directory / EXPECTED_NAME
        self.previous = b"existing reviewed DLL"
        self.target.write_bytes(self.previous)

    def assert_preserved(self):
        self.assertEqual(self.target.read_bytes(), self.previous)
        self.assertEqual(list(self.directory.glob("*.download*")), [])

    def test_valid_dev_release_checks_both_digests_and_downloads_current_pe32(self):
        data = win32_dll()
        metadata, checksum = dev_release(data)
        with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
            io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(checksum), io.BytesIO(data),
        ]) as network:
            self.assertEqual(fetch_verified_hud(self.target, tag="dev013-latest"), self.target)
        self.assertEqual(self.target.read_bytes(), data)
        self.assertEqual(network.call_args_list[0].args[0].full_url,
                         "https://api.github.com/repos/Bonziuu/ParryHotter/releases/tags/dev013-latest")
        self.assertEqual(network.call_args_list[1].args[0].full_url,
                         metadata["assets"][1]["browser_download_url"])
        self.assertEqual(network.call_args_list[2].args[0].full_url,
                         metadata["assets"][0]["browser_download_url"])
        self.assertEqual(list(self.directory.glob("*.download*")), [])

    def test_legacy_default_remains_compatible_without_a_checksum_sidecar(self):
        data = b"MZ" + HUD_BUILD_MARKER + b"x" * 1500
        metadata = {"tag_name": "hud-dev", "assets": [release_asset(EXPECTED_NAME, data, tag="hud-dev")]}
        with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
            io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(data),
        ]):
            fetch_verified_hud(self.target)
        self.assertEqual(self.target.read_bytes(), data)

    def test_missing_or_duplicate_sidecar_never_replaces_existing_dll(self):
        for duplicate in (False, True):
            with self.subTest(duplicate=duplicate):
                metadata, _ = dev_release(win32_dll())
                if duplicate:
                    metadata["assets"].append(metadata["assets"][1].copy())
                else:
                    metadata["assets"].pop()
                with patch("tools.download_hud.urllib.request.urlopen", return_value=
                           io.BytesIO(json.dumps(metadata).encode())) as network:
                    with self.assertRaisesRegex(HudDownloadError, "exactly one"):
                        fetch_verified_hud(self.target, "dev013-latest")
                self.assertEqual(network.call_count, 1)
                self.assert_preserved()

    def test_checksum_contents_must_be_one_entry_for_the_exact_dll(self):
        data = win32_dll()
        digest = hashlib.sha256(data).hexdigest()
        malformed = [
            f"{'0' * 64}  {EXPECTED_NAME}",
            f"{digest}  foreign.dll",
            f"{digest}  {EXPECTED_NAME}\n{digest}  {EXPECTED_NAME}",
            f"{digest}  ../{EXPECTED_NAME}",
            "<html>download error</html>",
            "niepoprawna suma",
        ]
        for contents in malformed:
            with self.subTest(contents=contents):
                metadata, checksum = dev_release(data, contents.encode())
                with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
                    io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(checksum),
                ]) as network:
                    with self.assertRaisesRegex(HudDownloadError, "SHA256"):
                        fetch_verified_hud(self.target, "dev013-latest")
                self.assertEqual(network.call_count, 2)
                self.assert_preserved()

    def test_corrupt_sidecar_or_dll_is_rejected_even_with_matching_size(self):
        data = win32_dll()
        for corrupt_sidecar in (False, True):
            with self.subTest(sidecar=corrupt_sidecar):
                metadata, checksum = dev_release(data)
                responses = [io.BytesIO(json.dumps(metadata).encode()),
                             io.BytesIO(b"x" * len(checksum) if corrupt_sidecar else checksum)]
                if not corrupt_sidecar:
                    responses.append(io.BytesIO(data[:-1] + b"x"))
                with patch("tools.download_hud.urllib.request.urlopen", side_effect=responses):
                    with self.assertRaisesRegex(HudDownloadError, "SHA256"):
                        fetch_verified_hud(self.target, "dev013-latest")
                self.assert_preserved()

    def test_incompatible_binary_rejected_after_valid_hashes(self):
        for data in (win32_dll(machine=0x8664), b"MZ" + HUD_BUILD_MARKER + b"\0" * 1500):
            with self.subTest(binary=data[:4]):
                metadata, checksum = dev_release(data)
                with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
                    io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(checksum), io.BytesIO(data),
                ]):
                    with self.assertRaisesRegex(HudDownloadError, "PE32"):
                        fetch_verified_hud(self.target, "dev013-latest")
                self.assert_preserved()

    def test_untrusted_urls_are_rejected_before_any_asset_download(self):
        for asset_index in (0, 1):
            for url in (
                "http://github.com/Bonziuu/ParryHotter/releases/download/dev013-latest/" + EXPECTED_NAME,
                "https://github.com.evil.example/Bonziuu/ParryHotter/releases/download/dev013-latest/" + EXPECTED_NAME,
                "https://github.com/Bonziuu/ParryHotter/releases/download/hud-dev/" + EXPECTED_NAME,
                "https://github.com/Bonziuu/Other/releases/download/dev013-latest/" + EXPECTED_NAME,
                "https://github.com/Bonziuu/ParryHotter/releases/download/dev013-latest/" + EXPECTED_NAME + "?redirect=1",
            ):
                with self.subTest(asset=asset_index, url=url):
                    metadata, _ = dev_release(win32_dll())
                    metadata["assets"][asset_index]["browser_download_url"] = url
                    with patch("tools.download_hud.urllib.request.urlopen", return_value=
                               io.BytesIO(json.dumps(metadata).encode())) as network:
                        with self.assertRaisesRegex(HudDownloadError, "repository"):
                            fetch_verified_hud(self.target, "dev013-latest")
                    self.assertEqual(network.call_count, 1)
                    self.assert_preserved()

    def test_missing_or_malformed_digests_and_sizes_are_rejected(self):
        for field, value in (("digest", None), ("digest", "sha256:" + "g" * 64),
                             ("digest", ""), ("size", "1024"), ("size", True),
                             ("size", 0), ("size", 4 * 1024 * 1024)):
            with self.subTest(field=field, value=value):
                asset = release_asset(EXPECTED_NAME, win32_dll())
                asset[field] = value
                with self.assertRaises(HudDownloadError):
                    validate_asset(asset, "dev013-latest")

    def test_sidecar_digest_is_required_independently_of_dll_digest(self):
        metadata, _ = dev_release(win32_dll())
        metadata["assets"][1].pop("digest")
        with patch("tools.download_hud.urllib.request.urlopen", return_value=
                   io.BytesIO(json.dumps(metadata).encode())) as network:
            with self.assertRaisesRegex(HudDownloadError, "SHA256"):
                fetch_verified_hud(self.target, "dev013-latest")
        self.assertEqual(network.call_count, 1)
        self.assert_preserved()

    def test_interrupted_download_removes_partial_file(self):
        class InterruptedDownload(io.BytesIO):
            def read(self, count=-1):
                if self.tell():
                    raise urllib.error.URLError("connection interrupted")
                return super().read(256)

        data = win32_dll()
        metadata, checksum = dev_release(data)
        with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
            io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(checksum), InterruptedDownload(data),
        ]):
            with self.assertRaisesRegex(HudDownloadError, "interrupted"):
                fetch_verified_hud(self.target, "dev013-latest")
        self.assert_preserved()

    def test_asset_size_larger_than_metadata_fails_before_replacement(self):
        data = win32_dll()
        metadata, checksum = dev_release(data)
        with patch("tools.download_hud.urllib.request.urlopen", side_effect=[
            io.BytesIO(json.dumps(metadata).encode()), io.BytesIO(checksum), io.BytesIO(data + b"extra"),
        ]):
            with self.assertRaisesRegex(HudDownloadError, "size limit"):
                fetch_verified_hud(self.target, "dev013-latest")
        self.assert_preserved()

    def test_release_tag_mismatch_and_unknown_tags_fail_closed(self):
        metadata, _ = dev_release(win32_dll())
        metadata["tag_name"] = "hud-dev"
        with patch("tools.download_hud.urllib.request.urlopen", return_value=
                   io.BytesIO(json.dumps(metadata).encode())):
            with self.assertRaisesRegex(HudDownloadError, "tag"):
                fetch_verified_hud(self.target, "dev013-latest")
        with patch("tools.download_hud.urllib.request.urlopen") as network:
            with self.assertRaisesRegex(HudDownloadError, "Unsupported"):
                fetch_verified_hud(self.target, "../main")
            network.assert_not_called()
        self.assert_preserved()
