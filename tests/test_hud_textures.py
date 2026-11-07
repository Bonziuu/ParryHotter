"""Check six spell sprites and empty slots in the ten-position HUD."""
import tempfile
import unittest
from pathlib import Path

from tools.generate_hud_textures import generate
from tools.spell_catalog import REQUIRED_TEXTURES, SPRITE_NAMES


class HudTextureTests(unittest.TestCase):
    def test_six_spell_sprites_and_blank_slots_are_distinct_and_reproducible(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            generated = generate(folder)
            self.assertEqual({p.name for p in generated}, set(REQUIRED_TEXTURES))
            self.assertEqual(len(generated), 15)
            first = {path.name: path.read_bytes() for path in generated}
            for name, data in first.items():
                self.assertEqual(len(data), 18 + 64 * 64 * 4, name)
                self.assertEqual(data[2], 2, name)
                self.assertEqual(data[16:18], bytes((32, 0x28)), name)
            slots = [first[f"PH_{name}.TGA"] for name in SPRITE_NAMES]
            self.assertEqual(len(set(slots)), 6)
            targets = [first[f"PH_TARGET_{name}.TGA"] for name in SPRITE_NAMES]
            self.assertEqual(len(set(targets)), 6)
            self.assertTrue(all(slot != target for slot, target in zip(slots, targets)))
            empty = first["PH_EMPTY.TGA"]
            self.assertNotIn(empty, slots)
            # Empty slots retain a translucent square but never a spell glyph.
            self.assertEqual(empty[18 + (32 * 64 + 32) * 4:18 + (32 * 64 + 32) * 4 + 4], bytes((14, 7, 4, 165)))
            self.assertEqual({p.name: p.read_bytes() for p in generate(folder)}, first)
