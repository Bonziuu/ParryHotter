"""Malformed skeletal animation assets must fail before game installation."""
from __future__ import annotations

import math
import tempfile
import unittest
from pathlib import Path

from tools.casting_assets import (
    CASTING_ASSET_NAMES, CASTING_MDS, CHARGE_ASC, RELEASE_ASC,
    animation_rig_signature, canonical_casting_mds,
    validate_optional_animation_assets,
)
from tools.install import PatchError


# Synthetic data exercises the file contract; it is never delivered as Harry.
_BONES = [("Bip01", ""), ("Bip01 Pelvis", "Bip01"),
          ("Bip01 Spine", "Bip01 Pelvis"), ("Bip01 Spine1", "Bip01 Spine"),
          ("Bip01 Spine2", "Bip01 Spine1"), ("Bip01 Neck", "Bip01 Spine2"),
          ("Bip01 Head", "Bip01 Neck")]
for side in ("L", "R"):
    previous = "Bip01 Spine2"
    for part in ("Clavicle", "UpperArm", "Forearm", "Hand", "Finger0"):
        name = f"Bip01 {side} {part}"
        _BONES.append((name, previous))
        previous = name
for side in ("L", "R"):
    previous = "Bip01 Pelvis"
    for part in ("Thigh", "Calf", "Foot", "Toe0"):
        name = f"Bip01 {side} {part}"
        _BONES.append((name, previous))
        previous = name


def _node(name: str, parent: str, index: int) -> str:
    return (f'*HELPEROBJECT {{\n*NODE_NAME "{name}"\n'
            + (f'*NODE_PARENT "{parent}"\n' if parent else "")
            + f'*NODE_TM {{\n*NODE_NAME "{name}"\n'
            '*TM_ROW0 1 0 0\n*TM_ROW1 0 1 0\n*TM_ROW2 0 0 1\n'
            f'*TM_ROW3 0 0 {index * 2}\n*TM_POS 0 0 {index * 2}\n'
            '*TM_ROTAXIS 0 0 1\n*TM_ROTANGLE 0\n}\n')


def _animation(last_frame: int) -> bytes:
    result = ('*3DSMAX_ASCIIEXPORT 110\n*SCENE {\n*SCENE_FIRSTFRAME 1\n'
              f'*SCENE_LASTFRAME {last_frame}\n*SCENE_FRAMESPEED 30\n'
              '*SCENE_TICKSPERFRAME 160\n}\n')
    for index, (name, parent) in enumerate(_BONES):
        result += _node(name, parent, index)
        result += f'*TM_ANIMATION {{\n*NODE_NAME "{name}"\n*CONTROL_POS_TRACK {{\n'
        result += ''.join(f'*CONTROL_POS_SAMPLE {frame * 160} 0 0 {index * 2}\n'
                          for frame in range(1, last_frame + 1))
        result += '}\n*CONTROL_ROT_TRACK {\n'
        for frame in range(1, last_frame + 1):
            if last_frame == 43:
                angle = math.sin((frame - 13) * math.tau / 30) * 0.03 if frame >= 13 else 0.0
            else:
                angle = math.sin((frame - 1) * math.pi / 25) * 0.1
            if name != "Bip01 R Hand":
                angle = 0.0
            result += f'*CONTROL_ROT_SAMPLE {frame * 160} 0 0 1 {angle}\n'
        result += '}\n*CONTROL_SCALE_TRACK {\n*CONTROL_SCALE_SAMPLE 0 1 1 1 0 0 0 0\n}\n}\n}\n'
    return result.encode("ascii")


def write_casting_assets(directory: Path) -> None:
    """Write synthetic complete mesh+animations for installer/package tests."""
    directory.mkdir(parents=True, exist_ok=True)
    triangle = (Path(__file__).parent / "fixtures" / "harry_triangle.asc.txt").read_text()
    header = triangle[:triangle.index("*HELPEROBJECT")]
    mesh = triangle[triangle.index("*GEOMOBJECT"):]
    nodes = ''.join(_node(name, parent, index) + '}\n'
                    for index, (name, parent) in enumerate(_BONES))
    (directory / "PH_HARRY.ASC").write_text(header + nodes + mesh, encoding="ascii")
    (directory / CASTING_MDS).write_bytes(canonical_casting_mds())
    (directory / CHARGE_ASC).write_bytes(_animation(43))
    (directory / RELEASE_ASC).write_bytes(_animation(26))


class CastingAssetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        write_casting_assets(self.root)

    def replace(self, name: str, before: bytes, after: bytes, count: int = 1):
        path = self.root / name
        original = path.read_bytes()
        self.assertIn(before, original)
        path.write_bytes(original.replace(before, after, count))

    def test_complete_animation_set_joins_and_keeps_root_fixed(self):
        self.assertEqual([path.name for path in validate_optional_animation_assets(self.root)],
                         list(CASTING_ASSET_NAMES))

    def test_no_optional_files_keeps_original_five_asset_bundle_valid(self):
        for name in CASTING_ASSET_NAMES:
            (self.root / name).unlink()
        self.assertEqual(validate_optional_animation_assets(self.root), [])

    def test_partial_optional_set_is_rejected(self):
        (self.root / RELEASE_ASC).unlink()
        with self.assertRaisesRegex(PatchError, "Incomplete"):
            validate_optional_animation_assets(self.root)

    def test_overlay_cannot_reference_arbitrary_files_or_replace_stock_animations(self):
        self.replace(CASTING_MDS, b'T_PH_PREPARE', b'T_MAGRUN_2_WNDCAST')
        with self.assertRaisesRegex(PatchError, "overlay MDS"):
            validate_optional_animation_assets(self.root)

    def test_different_animation_hierarchy_is_rejected(self):
        self.replace(CHARGE_ASC, b'*NODE_PARENT "Bip01 Pelvis"', b'*NODE_PARENT "Bip01"')
        with self.assertRaisesRegex(PatchError, "skeleton differs"):
            validate_optional_animation_assets(self.root)

    def test_missing_grandparent_is_a_reported_validation_failure(self):
        # Root's descendants may occur before an invalid parent in the file.
        self.replace(CHARGE_ASC, b'*NODE_PARENT "Bip01 Spine1"', b'*NODE_PARENT "missing"')
        with self.assertRaisesRegex(PatchError, "parent is missing"):
            validate_optional_animation_assets(self.root)

    def test_arbitrary_renamed_25_bone_rig_is_not_a_gothic_human_rig(self):
        for name in ("PH_HARRY.ASC", CHARGE_ASC, RELEASE_ASC):
            self.replace(name, b'Bip01 L Finger0', b'Bip01 FakeFinger', count=10)
        with self.assertRaisesRegex(PatchError, "25-bone Gothic skeleton"):
            validate_optional_animation_assets(self.root)

    def test_missing_sample_is_rejected(self):
        self.replace(CHARGE_ASC, b'*CONTROL_POS_SAMPLE 160 0 0 0\n', b'')
        with self.assertRaisesRegex(PatchError, "incomplete POS"):
            validate_optional_animation_assets(self.root)

    def test_animation_bind_transform_must_match_the_mesh(self):
        self.replace(CHARGE_ASC, b'*TM_ROW3 0 0 2\n', b'*TM_ROW3 0 0 3\n')
        with self.assertRaisesRegex(PatchError, "bind transform differs"):
            validate_optional_animation_assets(self.root)

    def test_static_root_offset_must_not_move_the_mesh_off_its_bind_root(self):
        for name in (CHARGE_ASC, RELEASE_ASC):
            path = self.root / name
            text = path.read_text()
            start = text.index('*TM_ANIMATION {\n*NODE_NAME "Bip01"')
            end = text.index('*HELPEROBJECT', start)
            section = text[start:end]
            import re
            section = re.sub(r'(\*CONTROL_POS_SAMPLE \d+) 0 0 0', r'\1 0 3 0', section)
            path.write_text(text[:start] + section + text[end:])
        with self.assertRaisesRegex(PatchError, "root differs from mesh bind"):
            validate_optional_animation_assets(self.root)

    def test_missing_bone_track_is_rejected(self):
        self.replace(CHARGE_ASC, b'*TM_ANIMATION {', b'*UNUSED_ANIMATION {')
        with self.assertRaisesRegex(PatchError, "all 25 bones"):
            validate_optional_animation_assets(self.root)

    def test_non_finite_positions_are_rejected(self):
        self.replace(CHARGE_ASC, b'*CONTROL_POS_SAMPLE 160 0 0 0', b'*CONTROL_POS_SAMPLE 160 nan 0 0')
        with self.assertRaisesRegex(PatchError, "Non-finite"):
            validate_optional_animation_assets(self.root)

    def test_root_motion_is_rejected(self):
        self.replace(CHARGE_ASC, b'*CONTROL_POS_SAMPLE 320 0 0 0', b'*CONTROL_POS_SAMPLE 320 5 0 0')
        with self.assertRaisesRegex(PatchError, "root must stay fixed"):
            validate_optional_animation_assets(self.root)

    def test_wrong_fps_is_rejected(self):
        self.replace(RELEASE_ASC, b'*SCENE_FRAMESPEED 30', b'*SCENE_FRAMESPEED 25')
        with self.assertRaisesRegex(PatchError, "FRAMESPEED"):
            validate_optional_animation_assets(self.root)

    def test_non_unit_rotation_axis_is_rejected(self):
        self.replace(RELEASE_ASC, b'*CONTROL_ROT_SAMPLE 160 0 0 1 0.0', b'*CONTROL_ROT_SAMPLE 160 0 0 4 0.3')
        with self.assertRaisesRegex(PatchError, "axis/angle"):
            validate_optional_animation_assets(self.root)

    def test_charge_loop_must_close(self):
        path = self.root / CHARGE_ASC
        text = path.read_text()
        start = text.index('*TM_ANIMATION {\n*NODE_NAME "Bip01 R Hand"')
        end = text.index('*HELPEROBJECT', start)
        section = text[start:end]
        rows = section.splitlines()
        row = next(row for row in rows if row.startswith('*CONTROL_ROT_SAMPLE 6880 '))
        text = text[:start] + section.replace(row, '*CONTROL_ROT_SAMPLE 6880 0 0 1 0.4') + text[end:]
        path.write_text(text)
        with self.assertRaisesRegex(PatchError, "charge loop"):
            validate_optional_animation_assets(self.root)

    def test_release_must_begin_at_the_prepared_pose(self):
        self.replace(RELEASE_ASC, b'*CONTROL_POS_SAMPLE 160 0 0 2', b'*CONTROL_POS_SAMPLE 160 0 8 2')
        with self.assertRaisesRegex(PatchError, "charge to release"):
            validate_optional_animation_assets(self.root)

    def test_bind_signature_ignores_mesh_but_detects_changed_bone_transform(self):
        raw = (self.root / "PH_HARRY.ASC").read_bytes()
        original = animation_rig_signature(raw)
        self.assertEqual(animation_rig_signature(raw.replace(b'*MESH_VERTEX 1 1.0', b'*MESH_VERTEX 1 2.0')), original)
        self.assertNotEqual(animation_rig_signature(raw.replace(b'*TM_ROW3 0 0 2\n', b'*TM_ROW3 0 0 3\n', 1)), original)


if __name__ == "__main__":
    unittest.main()
