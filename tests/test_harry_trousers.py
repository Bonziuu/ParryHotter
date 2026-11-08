import tempfile
import unittest
from pathlib import Path

from tools.harry_trousers import (BACKUP_SUFFIX, dress_harry, leg_triangles, read_tga,
                                  write_tga)

ASC = """*3DSMAX_ASCIIEXPORT 110
*MATERIAL_LIST {
  *MATERIAL 0 {
    *MATERIAL_NAME "m"
    *MAP_DIFFUSE {
      *BITMAP "C:\\\\x\\\\SKIN.TGA"
    }
  }
}
*GEOMOBJECT {
  *NODE_NAME "BODY"
  *MESH {
    *MESH_VERTEX_LIST {
      *MESH_VERTEX 0 0 0 0
      *MESH_VERTEX 1 1 0 0
      *MESH_VERTEX 2 0 1 0
      *MESH_VERTEX 3 1 1 0
      *MESH_VERTEX 4 2 2 0
    }
    *MESH_FACE_LIST {
      *MESH_FACE 0: A: 0 B: 1 C: 2 AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING 0 *MESH_MTLID 0
      *MESH_FACE 1: A: 1 B: 3 C: 4 AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING 0 *MESH_MTLID 0
    }
    *MESH_TVERTLIST {
      *MESH_TVERT 0 0.0 0.0 0
      *MESH_TVERT 1 0.5 0.0 0
      *MESH_TVERT 2 0.0 0.5 0
      *MESH_TVERT 3 1.0 1.0 0
      *MESH_TVERT 4 0.5 1.0 0
      *MESH_TVERT 5 1.0 0.5 0
    }
    *MESH_TFACELIST {
      *MESH_TFACE 0 0 1 2
      *MESH_TFACE 1 3 4 5
    }
  }
}
*MESH_SOFTSKINVERTS {
BODY
5
1 "Bip01 L Thigh" 1.0
2 "Bip01 Pelvis" 0.6 "Bip01 L Calf" 0.4
1 "Bip01 R Calf" 1.0
1 "Bip01 R Hand" 1.0
1 "Bip01 R Hand" 1.0
}
"""


def skin_tga(size=16):
    rows = [[200, 180, 230] * size for _ in range(size)]  # BGR skin tone
    return write_tga(size, size, 3, rows)


class HarryTrousersTests(unittest.TestCase):
    def test_only_faces_skinned_to_legs_are_selected(self):
        triangles = leg_triangles(ASC)
        self.assertEqual(list(triangles), ["SKIN.TGA"])
        self.assertEqual(len(triangles["SKIN.TGA"]), 1)

    def test_rle_and_bottom_origin_tga_are_read(self):
        header = bytearray(18)
        header[2], header[12], header[14], header[16] = 10, 2, 2, 24
        # Bottom-left origin: the first stored row is the bottom one.
        data = bytes(header) + bytes([0x81, 1, 2, 3, 0x81, 4, 5, 6])
        _, _, _, top, rows = read_tga(data)
        self.assertFalse(top)
        self.assertEqual(rows, [[4, 5, 6, 4, 5, 6], [1, 2, 3, 1, 2, 3]])

    def test_leg_texels_turn_dark_and_the_rest_is_kept(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "PH_HARRY.ASC").write_text(ASC, encoding="latin-1")
            texture = root / "SKIN.TGA"
            texture.write_bytes(skin_tga())
            self.assertEqual(dress_harry(root / "PH_HARRY.ASC", root), [texture])
            _, _, _, _, rows = read_tga(texture.read_bytes())
            # UV (0..0.5, 0..0.5) is the bottom-left quarter of the image.
            self.assertLess(max(rows[14][3:6]), 90)
            self.assertEqual(rows[1][3 * 14:3 * 15], [200, 180, 230])
            self.assertEqual((root / ("SKIN.TGA" + BACKUP_SUFFIX)).read_bytes(), skin_tga())
            # Idempotent, and a newly converted texture becomes the new original.
            self.assertEqual(dress_harry(root / "PH_HARRY.ASC", root), [])
            replacement = skin_tga(8)
            texture.write_bytes(replacement)
            self.assertEqual(dress_harry(root / "PH_HARRY.ASC", root), [texture])
            self.assertEqual((root / ("SKIN.TGA" + BACKUP_SUFFIX)).read_bytes(), replacement)


if __name__ == "__main__":
    unittest.main()
