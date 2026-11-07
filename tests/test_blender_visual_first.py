"""Export failure handling and deforming cape weights without proprietary assets."""
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from tools.blender_visual_first import anatomical_scale, export_gothic_asc, linear_bone_weights, select_reference_rig, semantic_bone_map


class BlenderPreparationTests(unittest.TestCase):
    def test_auxiliary_skin_bones_map_by_biped_ancestry_without_guessing_cloak(self):
        parents = {"bip01": None, "bip01 Pelvis": "bip01",
                   "bip01 Head": "bip01", "Glasses": "bip01 Head",
                   "bip01 L Hand": "bip01", "bip01 L Finger0": "bip01 L Hand",
                   "FingerTip": "bip01 L Finger0",
                   "Cloak01": None, "Cloak01 Tail": "Cloak01"}
        mapped = semantic_bone_map(parents, ["Bip01", "Bip01 Pelvis", "Bip01 Head", "Bip01 L Hand", "Bip01 L Finger0"])
        self.assertEqual(mapped["bip01"], "Bip01 Pelvis")
        self.assertEqual(mapped["Glasses"], "Bip01 Head")
        self.assertEqual(mapped["FingerTip"], "Bip01 L Hand")
        self.assertEqual(mapped["bip01 L Finger0"], "Bip01 L Hand")
        self.assertIsNone(mapped["Cloak01 Tail"])

    def test_full_character_height_uses_head_pivot_not_reference_body_bbox(self):
        scale = anatomical_scale(0.0, 0.8, -0.02, 1.62)
        self.assertAlmostEqual(scale, 2.05)
        self.assertAlmostEqual((0.8 - 0.0) * scale - 0.02, 1.62)
        for arguments in [(0, 0, 0, 1), (1, 0, 0, 1), (0, float("nan"), 0, 1)]:
            with self.assertRaises(RuntimeError):
                anatomical_scale(*arguments)

    def test_original_hp2_rig_is_not_mistaken_for_the_gothic_reference(self):
        class Rig:
            def __init__(self, count, kind=None):
                self.data = SimpleNamespace(bones=list(range(count)))
                self.kind = kind

            def get(self, key):
                return self.kind if key == "ph_reference_kind" else None

        hp2 = Rig(143, "original_hp2_source_not_gothic")
        gothic = Rig(25)
        self.assertIs(select_reference_rig([hp2, gothic]), gothic)
        with self.assertRaisesRegex(RuntimeError, "Gothic reference"):
            select_reference_rig([hp2])

    def test_cape_follows_multiple_torso_bones_with_normalized_blends(self):
        anchors = [("Bip01 Spine2", 150), ("Bip01 Pelvis", 80), ("Bip01 Spine", 100)]
        self.assertEqual(linear_bone_weights(60, anchors), [("Bip01 Pelvis", 1.0)])
        middle = dict(linear_bone_weights(90, anchors))
        self.assertEqual(middle, {"Bip01 Pelvis": 0.5, "Bip01 Spine": 0.5})
        self.assertEqual(linear_bone_weights(200, anchors), [("Bip01 Spine2", 1.0)])

    def test_missing_exporter_reports_blocker_without_creating_fake_asc(self):
        bpy = SimpleNamespace(ops=SimpleNamespace(export_scene=SimpleNamespace()))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "PH_HARRY.ASC"
            result = export_gothic_asc(bpy, path)
            self.assertEqual(result["status"], "missing_exporter")
            self.assertFalse(path.exists())

    def test_failed_export_preserves_previous_reviewed_asc(self):
        class BrokenExporter:
            def get_rna_type(self):
                return object()

            def __call__(self, *, filepath, quiet):
                Path(filepath).write_bytes(b"not a skinned ASC")
                return {"FINISHED"}

        bpy = SimpleNamespace(ops=SimpleNamespace(export_scene=SimpleNamespace(krxascexp=BrokenExporter())))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "PH_HARRY.ASC"
            path.write_bytes(b"previous reviewed export")
            with self.assertRaises(RuntimeError):
                export_gothic_asc(bpy, path)
            self.assertEqual(path.read_bytes(), b"previous reviewed export")
            self.assertFalse(path.with_name("PH_HARRY.pending.asc").exists())


if __name__ == "__main__":
    unittest.main()
