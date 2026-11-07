import struct
import tempfile
import unittest
from pathlib import Path

from tools.verify_dll import BUILD_MARKER, DllError, inspect_dll, main


def synthetic_pe(*, machine=0x014C, characteristics=0x2102, magic=0x010B,
                 imports=("KERNEL32.dll", "USER32.dll"), marker=BUILD_MARKER) -> bytes:
    """A minimal PE with one section that holds an import table and the marker."""
    pe_offset, optional_size, section_rva, raw_pointer = 0x80, 0xE0, 0x1000, 0x200
    body = bytearray(0x200)
    descriptors = 0
    names_at = 20 * (len(imports) + 1)
    cursor = names_at
    for index, name in enumerate(imports):
        struct.pack_into("<IIIII", body, descriptors + index * 20, 0, 0, 0, section_rva + cursor, 0)
        encoded = name.encode() + b"\0"
        body[cursor:cursor + len(encoded)] = encoded
        cursor += len(encoded)
    body[cursor:cursor + len(marker)] = marker
    data = bytearray(raw_pointer)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, pe_offset)
    data[pe_offset:pe_offset + 4] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, pe_offset + 4, machine, 1, 0, 0, 0, optional_size, characteristics)
    optional = pe_offset + 24
    struct.pack_into("<H", data, optional, magic)
    struct.pack_into("<I", data, optional + 92, 16)
    struct.pack_into("<II", data, optional + 96 + 8, section_rva, 20 * (len(imports) + 1))
    section = optional + optional_size
    data[section:section + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", data, section + 8, len(body), section_rva, len(body), raw_pointer)
    return bytes(data + body)


class VerifyDllTests(unittest.TestCase):
    def test_current_win32_plugin_reports_imports_and_hash(self):
        report = inspect_dll(synthetic_pe())
        self.assertEqual(report.imports, ("KERNEL32.dll", "USER32.dll"))
        self.assertEqual(len(report.sha256), 64)

    def test_rejects_x64_build(self):
        with self.assertRaisesRegex(DllError, "not i386"):
            inspect_dll(synthetic_pe(machine=0x8664))

    def test_rejects_executable(self):
        with self.assertRaisesRegex(DllError, "not a DLL"):
            inspect_dll(synthetic_pe(characteristics=0x0102))

    def test_rejects_pe32_plus(self):
        with self.assertRaisesRegex(DllError, "PE32"):
            inspect_dll(synthetic_pe(magic=0x020B))

    def test_rejects_old_dll_without_current_marker(self):
        with self.assertRaisesRegex(DllError, "old or foreign"):
            inspect_dll(synthetic_pe(marker=b"PH_BUILD_012_SOMETHING_OLDER"))

    def test_rejects_non_pe_files(self):
        for data in (b"", b"MZ" + b"\0" * 10, b"\0" * 0x100):
            with self.assertRaises(DllError):
                inspect_dll(data)

    def test_command_line_exit_codes(self):
        with tempfile.TemporaryDirectory() as temporary:
            good = Path(temporary) / "good.dll"
            good.write_bytes(synthetic_pe())
            bad = Path(temporary) / "bad.dll"
            bad.write_bytes(synthetic_pe(machine=0x8664))
            self.assertEqual(main([str(good)]), 0)
            self.assertEqual(main([str(bad)]), 1)
            self.assertEqual(main([str(Path(temporary) / "missing.dll")]), 1)


if __name__ == "__main__":
    unittest.main()
