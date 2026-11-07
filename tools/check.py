#!/usr/bin/env python3
"""Run the portable development checks without a Gothic installation.

python -m tools.check
All binaries, generated textures and explicit bytecode outputs are temporary
and outside the checkout. This does not build the native Windows Union DLL
or validate gameplay. Run tools.verify_daedalus separately for the network
integration check.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import py_compile
import shlex
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.generate_hud_textures import generate
from tools.spell_catalog import REQUIRED_TEXTURES


def check_icons(directory: Path) -> int:
    generated = generate(directory)
    names = {path.name for path in generated}
    if len(generated) != len(REQUIRED_TEXTURES) or names != set(REQUIRED_TEXTURES):
        raise RuntimeError("HUD generation did not produce the complete ten-slot texture set")
    for path in generated:
        data = path.read_bytes()
        if len(data) < 18:
            raise RuntimeError(f"Truncated TGA header: {path.name}")
        width, height = struct.unpack_from("<HH", data, 12)
        if data[2] != 2 or (width, height) != (64, 64) or data[16:18] != bytes((32, 0x28)):
            raise RuntimeError(f"Invalid 64x64 BGRA TGA format: {path.name}")
        if len(data) != 18 + width * height * 4:
            raise RuntimeError(f"Invalid TGA payload length: {path.name}")
    print(f"PASS: {len(generated)} original HUD textures (header and complete pixel payload)", flush=True)
    return len(generated)


def check_python(directory: Path) -> bool:
    suite = unittest.defaultTestLoader.discover(str(ROOT / "tests"))
    if suite.countTestCases() == 0:
        raise RuntimeError("No Python tests discovered")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    # Blender is optional for the portable workflow. Compile its scripts
    # without importing bpy, and retain no explicit .pyc inside the checkout.
    for source in sorted((ROOT / "tools").glob("blender_*.py")):
        py_compile.compile(str(source), cfile=str(directory / (source.stem + ".pyc")), doraise=True)
    print("PASS: Blender script syntax (Blender itself was not executed)", flush=True)
    return result.wasSuccessful() and result.testsRun > 0


def check_cpp(directory: Path, compiler: str) -> int:
    sources = sorted((ROOT / "union" / "tests").glob("test_*.cpp"))
    if not sources:
        raise RuntimeError("No portable C++ tests discovered")
    command = shlex.split(compiler)
    if not command:
        raise RuntimeError("C++ compiler command is empty")
    for source in sources:
        output = directory / (source.stem + (".exe" if os.name == "nt" else ""))
        print(f"BUILD: {source.relative_to(ROOT)}", flush=True)
        # Assertions stay enabled; these binaries use assert as their runner.
        subprocess.run(command + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-UNDEBUG", str(source), "-o", str(output)], cwd=ROOT, check=True)
        subprocess.run([str(output)], cwd=directory, check=True)
    print(f"PASS: {len(sources)} portable C++ test binaries", flush=True)
    return len(sources)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--python-only", action="store_true", help="Run Python tests, Blender syntax and HUD generation; skip C++")
    selection.add_argument("--cpp-only", action="store_true", help="Run C++ tests and HUD generation; skip Python suite")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"), help="C++17 compiler command (default CXX or g++)")
    args = parser.parse_args()
    failures = []
    with tempfile.TemporaryDirectory(prefix="parryhotter-check-") as temporary:
        directory = Path(temporary)
        groups = []
        if not args.cpp_only:
            groups.append(("Python", lambda: check_python(directory)))
        else:
            print("SKIP: Python suite (--cpp-only)", flush=True)
        if not args.python_only:
            groups.append(("C++", lambda: check_cpp(directory, args.cxx)))
        else:
            print("SKIP: C++ binaries (--python-only)", flush=True)
        groups.append(("HUD textures", lambda: check_icons(directory / "icons")))
        for name, check in groups:
            try:
                if not check():
                    failures.append(name)
            except (RuntimeError, OSError, subprocess.CalledProcessError, py_compile.PyCompileError) as exc:
                print(f"FAIL: {name}: {exc}", file=sys.stderr, flush=True)
                failures.append(name)
    if failures:
        print("Failed checks: " + ", ".join(failures), file=sys.stderr)
        return 1
    print("All selected portable checks passed. Native DLL/gameplay require separate validation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
