import struct
import unittest

from tools.vdf import (HEADER_SIZE, SIGNATURE_G1, TYPE_DIRECTORY, TYPE_LAST,
                       VdfError, build_vdf, read_vdf)


class VdfTests(unittest.TestCase):
    FILES = {
        "_work/Data/Scripts/_compiled/Gothic.dat": b"DAT" * 10,
        "_work/Data/Textures/PH_WAND.TGA": b"TGA",
        "_work/Data/Textures/PH_EMPTY.TGA": b"EMPTY",
        "System/Autorun/ParryHotterHUD-G1.dll": b"MZ" + b"\0" * 30,
    }

    def test_round_trip_upper_cases_paths(self):
        back = read_vdf(build_vdf(self.FILES, comment="test"))
        self.assertEqual(back["SYSTEM/AUTORUN/PARRYHOTTERHUD-G1.DLL"], self.FILES["System/Autorun/ParryHotterHUD-G1.dll"])
        self.assertEqual(len(back), 4)

    def test_header_and_breadth_first_catalog(self):
        data = build_vdf(self.FILES)
        self.assertEqual(data[256:272], SIGNATURE_G1)
        count, files, _, size, offset, version = struct.unpack_from("<6I", data, 272)
        self.assertEqual((files, offset, version), (4, HEADER_SIZE, 0x50))
        self.assertEqual(size, sum(map(len, self.FILES.values())))
        names = [data[HEADER_SIZE + i * 80:HEADER_SIZE + i * 80 + 64].rstrip() for i in range(count)]
        self.assertEqual(names[:2], [b"SYSTEM", b"_WORK"])
        flags = [struct.unpack_from("<I", data, HEADER_SIZE + i * 80 + 72)[0] for i in range(count)]
        self.assertEqual(flags[0], TYPE_DIRECTORY)
        self.assertEqual(flags[1], TYPE_DIRECTORY | TYPE_LAST)

    def test_rejects_unsafe_or_conflicting_paths(self):
        for files in ({"../x.dat": b""}, {"C:/x.dat": b""}, {"a/" + "x" * 65: b""},
                      {"a": b"1", "a/b": b"2"}, {}):
            with self.assertRaises(VdfError):
                build_vdf(files)

    def test_reader_rejects_corruption(self):
        data = bytearray(build_vdf(self.FILES))
        with self.assertRaises(VdfError):
            read_vdf(bytes(data[:300]))
        data[256] = 0
        with self.assertRaises(VdfError):
            read_vdf(bytes(data))


if __name__ == "__main__":
    unittest.main()
