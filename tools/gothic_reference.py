#!/usr/bin/env python3
"""Fetch the pinned Gothic I MDK reference outside the mod's distributable files.

The source mesh is provided by PhoenixTales' update of the official development
kit for non-commercial Gothic modifications. This module contains metadata only;
the original mesh is downloaded into the user's private conversion directory.
"""
from __future__ import annotations

import hashlib
import json
import os
import ssl
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

REPOSITORY = "https://github.com/PhoenixTales/gothic-devkit"
COMMIT = "0b81c1932d11fd212c877cb8fd4e4a85ab2584de"
SOURCE_PATH = "gothic/_work/data/Anims/ASC_BODIES/HUMAN/HUM_BODY_NAKED0.ASC"
REFERENCE_URL = f"https://raw.githubusercontent.com/PhoenixTales/gothic-devkit/{COMMIT}/{SOURCE_PATH}"
REFERENCE_SHA256 = "7ae64e6fe8ae41034763fb9e9d2d4391bb3441b094efe349c29ec53031e053c4"
REFERENCE_SIZE = 135_693
MAX_BYTES = 256 * 1024
TERMS_URL = f"{REPOSITORY}/blob/{COMMIT}/license"
README_URL = f"{REPOSITORY}/blob/{COMMIT}/README.md"
USER_AGENT = "ParryHotter-Gothic-Reference/1.0"


class GothicReferenceError(RuntimeError):
    """The pinned source could not be obtained or safely cached."""


class _HTTPSOnlyRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, new_url):
        parsed = urllib.parse.urlsplit(new_url)
        if (parsed.scheme != "https" or parsed.netloc != "raw.githubusercontent.com"):
            raise GothicReferenceError("Gothic reference redirect left its HTTPS source host")
        return super().redirect_request(request, response, code, message, headers, new_url)


def _matches_reference(path: Path) -> bool:
    """Read at most the pinned length plus one byte, including for a bad cache."""
    if not path.is_file() or path.stat().st_size != REFERENCE_SIZE:
        return False
    with path.open("rb") as source:
        data = source.read(min(REFERENCE_SIZE, MAX_BYTES) + 1)
    return len(data) == REFERENCE_SIZE and hashlib.sha256(data).hexdigest() == REFERENCE_SHA256


def _atomic_text(path: Path, text: str) -> None:
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", dir=path.parent,
            prefix=f".{path.name}.", suffix=".download", delete=False,
        ) as output:
            temporary = Path(output.name)
            output.write(text)
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _write_provenance(directory: Path) -> None:
    metadata = {
        "repository": REPOSITORY,
        "commit": COMMIT,
        "source_path": SOURCE_PATH,
        "url": REFERENCE_URL,
        "bytes": REFERENCE_SIZE,
        "sha256": REFERENCE_SHA256,
        "terms_url": TERMS_URL,
        "readme_url": README_URL,
        "copyright": "All rights belong to THQ Nordic. GOTHIC (c) 2001 Piranha Bytes.",
        "purpose": "Private reference for non-commercial Gothic I model conversion; do not package the untouched source.",
    }
    _atomic_text(directory / "PROVENANCE.json", json.dumps(metadata, indent=2) + "\n")
    _atomic_text(directory / "TERMS.txt", (
        "Gothic Development Kit reference - terms and origin\n\n"
        "The use of the MOD PLAYER-KIT and the use of the MOD DEVELOPMENT-KIT "
        "is exclusively allowed for non-commercial purposes.\n"
        "All tools and assets included in the PLAYER-KIT and the DEVELOPMENT-KIT, "
        "as well as all assets created or modified by it, must not be used commercially.\n"
        "All rights belong to THQ Nordic.\n"
        "GOTHIC (c) 2001 Piranha Bytes.\n\n"
        "Keep this untouched reference in the private conversion directory. "
        "Do not include it in a mod package.\n"
        f"Source: {REFERENCE_URL}\nTerms: {TERMS_URL}\nDocumentation: {README_URL}\n"
    ))


def _download_reference(destination: Path) -> None:
    temporary = None
    # The default proxy handler preserves the environment's configured proxy.
    # A default SSL context verifies server certificates and hostnames.
    opener = urllib.request.build_opener(
        urllib.request.HTTPSHandler(context=ssl.create_default_context()), _HTTPSOnlyRedirect()
    )
    request = urllib.request.Request(REFERENCE_URL, headers={"User-Agent": USER_AGENT})
    try:
        with opener.open(request, timeout=45) as response, tempfile.NamedTemporaryFile(
            mode="wb", dir=destination.parent, prefix=f".{destination.name}.",
            suffix=".download", delete=False,
        ) as output:
            temporary = Path(output.name)
            digest = hashlib.sha256()
            total = 0
            while block := response.read(min(64 * 1024, MAX_BYTES - total + 1)):
                total += len(block)
                if total > MAX_BYTES:
                    raise GothicReferenceError("Gothic reference exceeds the download size limit")
                digest.update(block)
                output.write(block)
            if total != REFERENCE_SIZE or digest.hexdigest() != REFERENCE_SHA256:
                raise GothicReferenceError("Gothic reference failed pinned length/SHA256 verification")
            output.flush()
            os.fsync(output.fileno())
        temporary.replace(destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def ensure_gothic_reference(game_dir: Path) -> Path:
    """Return the verified ASC, reusing a valid private cache without networking.

    Only an explicit caller invokes this download. An invalid existing file is
    preserved until its replacement has passed the pinned size and SHA256 checks.
    """
    destination = Path(game_dir) / "ParryHotter-private" / "Gothic-reference" / "HUM_BODY_NAKED0.ASC"
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not _matches_reference(destination):
            _download_reference(destination)
        _write_provenance(destination.parent)
    except (OSError, TimeoutError, urllib.error.URLError, GothicReferenceError) as exc:
        raise GothicReferenceError(
            f"Nie mozna przygotowac referencji Gothic: {exc}. "
            f"Zrodlo: {REFERENCE_URL}. Zezwol na HTTPS do raw.githubusercontent.com "
            f"albo umiesc plik w {destination} "
            f"({REFERENCE_SIZE} bajtow, SHA256 {REFERENCE_SHA256}). "
            "Nieprawidlowa referencja nie zostanie uzyta do konwersji."
        ) from exc
    return destination
