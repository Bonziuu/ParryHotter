#!/usr/bin/env python3
"""Download ONLY our GitHub-published experimental Gothic I Union HUD DLL.

We verify SHA256 supplied by GitHub's release API before installing this
native binary. If unavailable, the caller must NOT silently substitute code.
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.spell_catalog import HUD_BUILD_MARKER
from tools.verify_dll import inspect_dll

RELEASE_API = "https://api.github.com/repos/Bonziuu/ParryHotter/releases/tags/hud-dev"
ALLOWED_TAGS = ("hud-dev", "dev013-latest")
EXPECTED_NAME = "ParryHotterHUD-G1.dll"
MAX_BYTES = 3 * 1024 * 1024
MAX_CHECKSUM_BYTES = 4096
MAX_METADATA_BYTES = 2 * 1024 * 1024
UA = "ParryHotter-HUD-Installer/0.13"


class HudDownloadError(RuntimeError):
    pass


def validate_hud_binary(data: bytes) -> None:
    if data[:2] != b"MZ":
        raise HudDownloadError("Downloaded file is not a Windows DLL")
    if HUD_BUILD_MARKER not in data:
        raise HudDownloadError(
            "DLL HUD pochodzi ze starszej lub niezgodnej wersji. "
            "Zbuduj aktualna wtyczke G1 Release/Win32; stara DLL hud-dev "
            "nie zawiera nowego ruchu, znakow celu i dziesieciu pol."
        )


def _check_tag(tag: str) -> None:
    if tag not in ALLOWED_TAGS:
        raise HudDownloadError("Unsupported HUD release tag")


def _validate_asset(asset: dict, tag: str, name: str,
                    minimum: int, maximum: int) -> tuple[str, str, int]:
    _check_tag(tag)
    if not isinstance(asset, dict) or asset.get("name") != name:
        raise HudDownloadError("Wrong release asset name")
    url = asset.get("browser_download_url", "")
    allowed_url = f"https://github.com/Bonziuu/ParryHotter/releases/download/{tag}/{name}"
    if url != allowed_url:
        raise HudDownloadError("Release URL is not our repository")
    raw_hash = asset.get("digest", "")
    if not isinstance(raw_hash, str) or not raw_hash.startswith("sha256:") or len(raw_hash) != 71:
        raise HudDownloadError("Release metadata lacks verified SHA256 digest")
    digest = raw_hash.split(":", 1)[1].lower()
    if any(c not in "0123456789abcdef" for c in digest):
        raise HudDownloadError("Malformed SHA256 digest")
    size = asset.get("size", 0)
    if not isinstance(size, int) or isinstance(size, bool) or not (minimum <= size <= maximum):
        raise HudDownloadError("Unexpected release asset size")
    return url, digest, size


def validate_asset(asset: dict, tag: str = "hud-dev") -> tuple[str, str, int]:
    """Return verified DLL download URL, expected hash and size."""
    return _validate_asset(asset, tag, EXPECTED_NAME, 1024, MAX_BYTES)


def _find_asset(release: dict, name: str) -> dict:
    assets = release.get("assets", [])
    if not isinstance(assets, list):
        raise HudDownloadError("Malformed release assets")
    entries = [a for a in assets if isinstance(a, dict) and a.get("name") == name]
    if len(entries) != 1:
        raise HudDownloadError(f"Expected exactly one release asset: {name}")
    return entries[0]


def _download_to(out, url: str, digest: str, size: int, limit: int) -> None:
    h = hashlib.sha256()
    total = 0
    try:
        with urllib.request.urlopen(
            urllib.request.Request(url, headers={"User-Agent": UA}), timeout=60
        ) as response:
            while block := response.read(256 * 1024):
                total += len(block)
                if total > limit or total > size:
                    raise HudDownloadError("HUD release asset exceeds the size limit")
                h.update(block)
                out.write(block)
    except (TimeoutError, urllib.error.URLError) as exc:
        raise HudDownloadError(f"Could not download HUD release asset: {exc}") from exc
    if total != size or h.hexdigest() != digest:
        raise HudDownloadError("HUD release asset failed length/SHA256 verification")


def _verify_checksum_file(release: dict, tag: str, dll_digest: str) -> None:
    import io
    name = EXPECTED_NAME + ".sha256"
    url, digest, size = _validate_asset(
        _find_asset(release, name), tag, name, 1, MAX_CHECKSUM_BYTES
    )
    out = io.BytesIO()
    _download_to(out, url, digest, size, MAX_CHECKSUM_BYTES)
    try:
        checksum = out.getvalue().decode("ascii").strip()
    except UnicodeDecodeError as exc:
        raise HudDownloadError("Malformed SHA256 file") from exc
    match = re.fullmatch(r"([0-9a-fA-F]{64})[ \t]+\*?" + re.escape(EXPECTED_NAME), checksum)
    if not match or match.group(1).lower() != dll_digest:
        raise HudDownloadError("SHA256 file does not match the DLL release metadata")


def fetch_verified_hud(destination: Path, tag: str = "hud-dev") -> Path:
    """Download atomically; DEV013 additionally requires its SHA256 sidecar and PE32."""
    _check_tag(tag)
    api = RELEASE_API.rsplit("/", 1)[0] + "/" + tag
    request = urllib.request.Request(
        api, headers={"User-Agent": UA, "Accept": "application/vnd.github+json"}
    )
    try:
        with urllib.request.urlopen(request, timeout=35) as response:
            raw = response.read(MAX_METADATA_BYTES + 1)
            if len(raw) > MAX_METADATA_BYTES:
                raise HudDownloadError("HUD release metadata exceeds the size limit")
            release = json.loads(raw)
    except (ValueError, TimeoutError, urllib.error.URLError) as exc:
        raise HudDownloadError(f"Could not read release metadata: {exc}") from exc
    if not isinstance(release, dict) or release.get("tag_name") != tag:
        raise HudDownloadError("Release tag has changed unexpectedly")
    url, digest, size = validate_asset(_find_asset(release, EXPECTED_NAME), tag)
    if tag == "dev013-latest":
        _verify_checksum_file(release, tag, digest)

    destination.parent.mkdir(parents=True, exist_ok=True)
    tmp = None
    try:
        # Each invocation owns its temporary file; two simultaneous downloads
        # cannot truncate or delete the other one's incomplete DLL.
        with tempfile.NamedTemporaryFile(prefix=destination.name + ".download-",
                                         dir=destination.parent, delete=False) as out:
            tmp = Path(out.name)
            _download_to(out, url, digest, size, MAX_BYTES)
        data = tmp.read_bytes()
        validate_hud_binary(data)
        if tag == "dev013-latest":
            try:
                inspect_dll(data)
            except (ValueError, struct.error) as exc:
                raise HudDownloadError(f"Invalid PE32 i386 HUD DLL: {exc}") from exc
        tmp.replace(destination)
    finally:
        if tmp is not None and tmp.is_file():
            tmp.unlink()
    return destination


def main() -> int:
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--tag", choices=ALLOWED_TAGS, default="hud-dev")
    args = parser.parse_args()
    try:
        path = fetch_verified_hud(args.out, tag=args.tag)
        print(f"Verified native Gothic I test plugin: {path}")
    except (OSError, HudDownloadError) as exc:
        parser.exit(1, f"BLAD: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
