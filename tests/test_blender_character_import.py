"""Legacy FBX mesh fallback and Windows texture path regressions."""
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from tools.blender_character_import import fbx_version, import_character, local_texture, relink_images


class CharacterImportTests(unittest.TestCase):
    def test_detects_old_binary_and_ascii_fbx_without_blender(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "character.fbx"
            path.write_bytes(b"Kaydara FBX Binary  \x00\x1a\x00" + struct.pack("<I", 6000))
            self.assertEqual(fbx_version(path), 6000)
            path.write_text("FBXHeaderExtension: {\n FBXVersion: 7400\n}")
            self.assertEqual(fbx_version(path), 7400)
            path.write_bytes(b"not an FBX header")
            self.assertIsNone(fbx_version(path))

    def test_windows_texture_paths_match_local_filename_case(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            texture = source / "skharrytex0.png"
            texture.write_bytes(b"texture")
            self.assertEqual(local_texture(source, r"..\Desktop\skharryTex0.png"), texture)
            self.assertIsNone(local_texture(source, r"C:\missing.png"))
            (source / "skharryTex0.png").write_bytes(b"ambiguous")
            if len(list(source.iterdir())) == 1:
                return  # Case-insensitive filesystem (NTFS): no second file can exist.
            with self.assertRaisesRegex(RuntimeError, "Ambiguous"):
                local_texture(source, "skharrytex0.png")

    def test_obj_fallback_keeps_groups_and_materials_with_original_files_unchanged(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            fbx = source / "skharrymesh.fbx"
            fbx.write_text("FBXVersion: 6000")
            obj = fbx.with_suffix(".obj")
            obj.write_text("mtllib skharrymesh.mtl\ng material_0\n")
            mtl = fbx.with_suffix(".mtl")
            material = "newmtl material_0\nmap_Kd skharryTex0.png\n"
            mtl.write_text(material)
            (source / "skharrytex0.png").write_bytes(b"texture")
            calls = []

            def importer(**kwargs):
                calls.append(kwargs)
                staged = Path(kwargs["filepath"]).parent
                self.assertEqual((staged / "skharrytex0.png").read_bytes(), b"texture")
                self.assertIn("map_Kd skharrytex0.png", (staged / mtl.name).read_text())
                return {"FINISHED"}

            bpy = SimpleNamespace(ops=SimpleNamespace(wm=SimpleNamespace(obj_import=importer)),
                                  data=SimpleNamespace(images=[], materials=[]))
            report = import_character(bpy, fbx)
            self.assertEqual(report["format"], "OBJ")
            self.assertEqual(calls[0]["forward_axis"], "X")
            self.assertEqual(calls[0]["up_axis"], "Y")
            self.assertTrue(calls[0]["use_split_groups"])
            self.assertFalse(calls[0]["validate_meshes"])
            self.assertEqual(mtl.read_text(), material)
            self.assertFalse(Path(calls[0]["filepath"]).exists())

    def test_old_fbx_without_material_file_is_an_explicit_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "skharrymesh.fbx"
            source.write_text("FBXVersion: 6000")
            source.with_suffix(".obj").write_text("mesh")
            with self.assertRaisesRegex(RuntimeError, "skharrymesh.mtl"):
                import_character(SimpleNamespace(), source)

    def test_reload_checks_lazy_pixels_and_ignores_reference_body_texture(self):
        class LazyImage:
            source = "FILE"
            filepath = r"C:\Desktop\skharryTex0.png"
            has_data = False
            size = (4, 4)

            def reload(self):
                self.has_data = False

            @property
            def pixels(self):
                self.has_data = True
                return [1.0] * 64

        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary).resolve()
            target = source / "PH_HARRY_TEX0.TGA"
            target.write_bytes(b"test texture")
            image = LazyImage()
            reference = SimpleNamespace(source="FILE", filepath="HUM_BODY.TGA")
            bpy = SimpleNamespace(data=SimpleNamespace(images=[image, reference]))
            relink_images(bpy, source, converted=True)
            self.assertEqual(image.filepath, str(target))
            self.assertTrue(image.has_data)
            self.assertEqual(reference.filepath, "HUM_BODY.TGA")
