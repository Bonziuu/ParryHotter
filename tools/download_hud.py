#!/usr/bin/env python3
"""Download ONLY our GitHub-published experimental Gothic I Union HUD DLL.

We verify SHA256 supplied by GitHub's release API before installing this
native binary. If unavailable, the caller must NOT silently substitute code.
"""
from __future__ import annotations

import hashlib
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.spell_catalog import HUD_BUILD_MARKER

RELEASE_API = "https://api.github.com/repos/Bonziuu/ParryHotter/releases/tags/hud-dev"
EXPECTED_NAME = "ParryHotterHUD-G1.dll"
MAX_BYTES = 3 * 1024 * 1024
UA = "ParryHotter-HUD-Installer/0.4"


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


def validate_asset(asset: dict) -> tuple[str, str, int]:
    """Return verified download URL, expected hash and size."""
    if asset.get("name") != EXPECTED_NAME:
        raise HudDownloadError("Wrong release asset name")
    url = asset.get("browser_download_url", "")
    parsed = urllib.parse.urlparse(url)
    allowed_prefix = (
        "https://github.com/Bonziuu/ParryHotter/releases/download/hud-dev/"
    )
    if parsed.scheme != "https" or not url.startswith(allowed_prefix):
        raise HudDownloadError("Release URL is not our repository")
    raw_hash = asset.get("digest", "")
    if not raw_hash.startswith("sha256:") or len(raw_hash) != 71:
        raise HudDownloadError("Release metadata lacks verified SHA256 digest")
    digest = raw_hash.split(":", 1)[1].lower()
    if any(c not in "0123456789abcdef" for c in digest):
        raise HudDownloadError("Malformed SHA256 digest")
    size = int(asset.get("size", 0))
    if not (1024 <= size <= MAX_BYTES):
        raise HudDownloadError("Unexpected DLL asset size")
    return url, digest, size


def fetch_verified_hud(destination: Path) -> Path:
    """Download to a temp file; only rename once checksums match."""
    request = urllib.request.Request(
        RELEASE_API, headers={"User-Agent": UA, "Accept": "application/vnd.github+json"}
    )
    try:
        with urllib.request.urlopen(request, timeout=35) as response:
            release = json.load(response)
    except (ValueError, TimeoutError, urllib.error.URLError) as exc:
        raise HudDownloadError(f"Could not read release metadata: {exc}") from exc
    if release.get("tag_name") != "hud-dev":
        raise HudDownloadError("Release tag has changed unexpectedly")
    entries = [
        a for a in release.get("assets", [])
        if a.get("name") == EXPECTED_NAME
    ]
    if len(entries) != 1:
        raise HudDownloadError("Expected exactly one Gothic I HUD DLL asset")
    url, digest, size = validate_asset(entries[0])

    tmp = destination.with_suffix(destination.suffix + ".download")
    destination.parent.mkdir(parents=True, exist_ok=True)
    h = hashlib.sha256()
    total = 0
    try:
        with urllib.request.urlopen(
            urllib.request.Request(url, headers={"User-Agent": UA}), timeout=60
        ) as response, tmp.open("wb") as out:
            while block := response.read(256 * 1024):
                total += len(block)
                if total > MAX_BYTES:
                    raise HudDownloadError("HUD DLL exceeds the size limit")
                h.update(block)
                out.write(block)
        if total != size or h.hexdigest() != digest:
            raise HudDownloadError("HUD DLL failed length/SHA256 verification")
        # PE32 from MSVC. Defend against accidental download of HTML error page.
        validate_hud_binary(tmp.read_bytes())
        tmp.replace(destination)
    except Exception:
        if tmp.is_file():
            tmp.unlink()
        raise
    return destination


def main() -> int:
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    try:
        path = fetch_verified_hud(args.out)
        print(f"Verified native Gothic I test plugin: {path}")
    except (OSError, HudDownloadError) as exc:
        parser.exit(1, f"BLAD: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
