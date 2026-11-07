"""Legacy skin reader tests use synthetic records, never commercial models."""
from types import SimpleNamespace
import unittest

from tools.blender_legacy_skin import parse_legacy_skin, source_head_anatomy


def element(identifier, props=(), children=()):
    return SimpleNamespace(id=identifier, props=list(props), elems=list(children))


def fixture(*, typed_arrays=False):
    root = b"bip01\x00\x01Model"
    cloth = b"Cloak01 Tail\x00\x01Model"
    mesh = b"material_robe\x00\x01Model"
    skin = b"Skin robe\x00\x01Deformer"
    cluster_root = b"Cluster root\x00\x01SubDeformer"
    cluster_cloth = b"Cluster cloth\x00\x01SubDeformer"
    identity = [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.]
    def array(identifier, values):
        return element(identifier, [values] if typed_arrays else values)
    records = [
        element(b"Model", [root, b"Root"]),
        element(b"Model", [cloth, b"LimbNode"], [
            element(b"Properties60", children=[
                element(b"Property", [b"Lcl Translation", b"Vector3D", b"A+", 0., 1., 0.])
            ])
        ]),
        element(b"Model", [mesh, b"Mesh"], [array(b"Vertices", [0., 0., 0., 0., 1., 0.])]),
        element(b"Deformer", [skin, b"Skin"]),
        element(b"Deformer", [cluster_root, b"Cluster"], [
            array(b"Indexes", [0]), array(b"Weights", [0.2]), array(b"TransformLink", identity)
        ]),
        element(b"Deformer", [cluster_cloth, b"Cluster"], [
            array(b"Indexes", [1]), array(b"Weights", [1.0]), array(b"TransformLink", identity)
        ]),
    ]
    links = [(cloth, root), (root, cluster_root), (cloth, cluster_cloth),
             (cluster_root, skin), (cluster_cloth, skin), (skin, mesh)]
    tree = element(b"", children=[element(b"Objects", children=records),
        element(b"Connections", children=[element(b"C", [b"OO", *link]) for link in links])])
    return tree, records


class LegacySkinReaderTests(unittest.TestCase):
    def test_face_and_glasses_descendants_belong_to_real_head_not_cloak_clone(self):
        bones = {
            "bip01": {"parent": None},
            "bip01 Neck": {"parent": "bip01"},
            "bip01 Head": {"parent": "bip01 Neck"},
            "Glasses": {"parent": "bip01 Head"},
            "LeftEyebrow": {"parent": "Glasses"},
            "Cloak01 Head": {"parent": "bip01"},
        }
        head, neck, members = source_head_anatomy(bones)
        self.assertEqual((head, neck), ("bip01 Head", "bip01 Neck"))
        self.assertEqual(members, {"bip01 Head", "Glasses", "LeftEyebrow"})

    def test_scalar_fbx6_records_preserve_source_bind_and_cloak_mask(self):
        tree, _ = fixture()
        data = parse_legacy_skin(tree, 6000)
        self.assertEqual(data["cluster_count"], 2)
        self.assertEqual(data["bones"]["Cloak01 Tail"]["parent"], "bip01")
        self.assertEqual(data["bones"]["Cloak01 Tail"]["properties"]["Lcl Translation"], [0., 1., 0.])
        self.assertEqual(len(data["bones"]["bip01"]["bind_matrix"]), 16)
        source = data["meshes"]["material_robe"]
        self.assertEqual(source["cloth"], [False, True])
        self.assertEqual(source["weights"], [{"bip01": 1.0}, {"Cloak01 Tail": 1.0}])

    def test_typed_array_records_are_also_supported(self):
        tree, _ = fixture(typed_arrays=True)
        source = parse_legacy_skin(tree, 6100)["meshes"]["material_robe"]
        self.assertEqual(source["vertices"], [(0., 0., 0.), (0., 1., 0.)])

    def test_bad_vertex_index_is_rejected(self):
        tree, records = fixture()
        records[-1].elems[0].props = [8]
        with self.assertRaisesRegex(ValueError, "outside"):
            parse_legacy_skin(tree, 6000)

    def test_nonfinite_skin_weights_are_rejected(self):
        tree, records = fixture()
        records[-1].elems[1].props = [float("nan")]
        with self.assertRaisesRegex(ValueError, "weight"):
            parse_legacy_skin(tree, 6000)

    def test_modern_fbx_and_unweighted_source_are_rejected(self):
        tree, records = fixture()
        with self.assertRaisesRegex(ValueError, "pre-7.1"):
            parse_legacy_skin(tree, 7400)
        records[-1].elems[1].props = [0.0]
        with self.assertRaisesRegex(ValueError, "no skin weights"):
            parse_legacy_skin(tree, 6000)
