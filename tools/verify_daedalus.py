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
from tools.visual_install import register_armor_source, select_visual_armor, ARMOR_SCRIPT

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
        runtime = temp / "gothic-runtime.d"
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
        # GDC's built-in engine profile expects Gothic 2's C_NPC size
        # (800 bytes), while genuine Gothic 1 uses 552 bytes. Extend
        # the temporary CI-only class with 62 dummy 4-byte integers so
        # that we can statically validate code. Never deploy this DAT.
        classes = content / "_intern" / "Classes.d"
        class_data = classes.read_bytes()
        start = class_data.upper().find(b"CLASS C_NPC")
        end = class_data.find(b"};", start)
        if start < 0 or end < 0:
            raise PatchError("Could not locate Gothic 1 C_NPC for CI-only class padding")
        class_data = (
            class_data[:end]
            + b"    VAR INT _CI_ONLY_GDC_COMPAT[62];\r\n"
            + class_data[end:]
        )
        classes.write_bytes(class_data)
        install(content)
        get_compiler(compiler)
        # The independent compiler is not the game itself: explicitly load the
        # ZenGin externals it needs to understand Npc_*, AI_*, PrintScreen, etc.
        runtime_url = (
            "https://raw.githubusercontent.com/dzieje-khorinis/DaedalusCompiler/"
            "7fc987118e432569e078cd738fbc7ff5a2194742/"
            "src/DaedalusCompiler/DaedalusBuiltins/gothic.d"
        )
        with urllib.request.urlopen(runtime_url, timeout=40) as source:
            runtime.write_bytes(source.read(300_000))


        process = subprocess.run(
            [
                str(compiler), str(content / "Gothic.src"),
                "--output-dat", str(output),
                "--runtime", str(runtime),
            ],
            cwd=temp, capture_output=True, text=False, timeout=240,
        )
        print(("Daedalus stdout:\n" + (process.stdout or b"").decode("utf-8", "replace")[-12000:]).encode("ascii", "backslashreplace").decode("ascii"))
        print(("Daedalus stderr:\n" + (process.stderr or b"").decode("utf-8", "replace")[-12000:]).encode("ascii", "backslashreplace").decode("ascii"))
        if process.returncode or not output.is_file():
            raise PatchError(f"Daedalus compiler failed (exit={process.returncode})")
        compiled = output.read_bytes()
        # GDC normalizes symbol identifiers to uppercase in its output.
        # User-facing spell names must remain embedded as readable strings.
        symbols_upper = compiled.upper()
        for marker in (b"FLIPENDO", b"PH_RUNE_FLIPENDO", b"AVADA KEDAVRA"):
            if marker not in symbols_upper:
                raise PatchError(f"Compiled Gothic.dat missing marker {marker!r}")
        print(f"OK: Daedalus compiled Gothic.dat ({len(compiled)} bytes)")

        # Second compile validates the *optional* converted Harry costume
        # C_Item script and hero visual switch, without including any of the
        # copyrighted 3D assets in the CI runner or published repository.
        source_file = content / "Gothic.src"
        hero_file = content / "Story" / "NPC" / "PC_Hero.d"
        source_file.write_bytes(register_armor_source(
            source_file.read_bytes().decode("latin-1")
        ).encode("latin-1"))
        hero_file.write_bytes(select_visual_armor(
            hero_file.read_bytes().decode("latin-1")
        ).encode("latin-1"))
        (content / "Items" / "ParryHarryArmor.d").write_bytes(
            ARMOR_SCRIPT.read_bytes()
        )
        outfit_dat = temp / "HarryGothic.dat"
        second = subprocess.run(
            [str(compiler), str(source_file), "--output-dat", str(outfit_dat),
             "--runtime", str(runtime)],
            cwd=temp, capture_output=True, text=False, timeout=240,
        )
        if second.returncode or not outfit_dat.is_file():
            message = (second.stderr or second.stdout or b"").decode(
                "utf-8", "replace"
            )[-6000:]
            raise PatchError("Optional Harry outfit Daedalus compile failed:\n" +
                             message.encode("ascii", "backslashreplace").decode("ascii"))
        if b"PH_ARMOR_HARRY" not in outfit_dat.read_bytes().upper():
            raise PatchError("Converted Harry armor symbol missing from compiled DAT")
        print(f"OK: optional local Harry armor script compiled ({outfit_dat.stat().st_size} bytes)")
        print("Engine, animations, mouse and gameplay still require a real game test")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
