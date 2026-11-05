#!/usr/bin/env python3
"""Compile Gothic 1 Daedalus from pinned Polish source and Parry Hotter.

Only temporary script files / generated DAT. No original game binaries.
"""
from __future__ import annotations

import hashlib
import platform
import subprocess
import tempfile
import urllib.request
from pathlib import Path

from tools.setup import download_polish_scripts, unpack_polish_content
from tools.install import install, PatchError

COMPILER_VERSION = "v0.9.5"
COMPILER_URL = (
    "https://github.com/dzieje-khorinis/DaedalusCompiler/releases/download/"
    f"{COMPILER_VERSION}/gdc-windows-x64.exe"
)
COMPILER_SHA256 = "a85fd199099fb39e1eb07aeec8bed8a82e8cdc13c8b7df86bbfd36c8e3252345"


def get_compiler(path: Path) -> None:
    request = urllib.request.Request(
        COMPILER_URL, headers={"User-Agent": "ParryHotter-daedalus-ci/1"}
    )
    total, digest = 0, hashlib.sha256()
    with urllib.request.urlopen(request, timeout=90) as response, path.open("wb") as out:
        while chunk := response.read(1024 * 1024):
            total += len(chunk)
            if total > 28 * 1024 * 1024:
                raise RuntimeError("Compiler exceeds maximum allowed size")
            digest.update(chunk)
            out.write(chunk)
    if digest.hexdigest() != COMPILER_SHA256:
        raise RuntimeError("Compiler SHA256 does not match pinned GitHub release")


def main() -> int:
    if platform.system() != "Windows":
        raise RuntimeError("This test uses the Windows 64-bit Daedalus compiler")
    with tempfile.TemporaryDirectory(prefix="parry_compile_") as tempname:
        temp = Path(tempname)
        archive = temp / "Polish.zip"
        content = temp / "Content"
        content.mkdir()
        compiler = temp / "gdc-windows-x64.exe"
        output = temp / "Gothic.dat"

        download_polish_scripts(archive)
        unpack_polish_content(archive, content)
        # GDC's lexer rejects an original Gothic 1 source identifier with Ö,
        # although ZenGin's own parser accepts it. Fix only a temporary CI
        # copy, NOT the user's Polish game files or public game sources.
        misc = content / "Items" / "Misc.d"
        raw = misc.read_bytes()
        for original in (b"Value_\xd6l", b"Value_\xc3\x96l"):
            raw = raw.replace(original, b"Value_Oel")
        misc.write_bytes(raw)
        install(content)
        get_compiler(compiler)

        process = subprocess.run(
            [
                str(compiler), str(content / "Gothic.src"),
                "--output-dat", str(output),
            ],
            cwd=temp, capture_output=True, text=False, timeout=240,
        )
        print(("Daedalus stdout:\n" + (process.stdout or b"").decode("utf-8", "replace")[-12000:]).encode("ascii", "backslashreplace").decode("ascii"))
        print(("Daedalus stderr:\n" + (process.stderr or b"").decode("utf-8", "replace")[-12000:]).encode("ascii", "backslashreplace").decode("ascii"))
        if process.returncode or not output.is_file():
            raise PatchError(f"Daedalus compiler failed (exit={process.returncode})")
        compiled = output.read_bytes()
        for marker in (b"Flipendo", b"PH_Rune_Flipendo", b"Avada Kedavra"):
            if marker not in compiled:
                raise PatchError(f"Compiled Gothic.dat missing marker {marker!r}")
        print(f"OK: Daedalus compiled Gothic.dat ({len(compiled)} bytes)")
        print("Engine, animations, mouse and gameplay still require a real game test")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
