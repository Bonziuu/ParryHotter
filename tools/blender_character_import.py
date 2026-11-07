"""Import Harry's supported FBX or his supplied legacy-FBX OBJ alternative.

OBJ is a source mesh for fresh Gothic skinning; it does not preserve HP2 bones.
This module can be imported without Blender for source/texture validation.
"""
from __future__ import annotations

import re
import shutil
import struct
import tempfile
from pathlib import Path


def fbx_version(path: Path) -> int | None:
    with path.open("rb") as stream:
        header = stream.read(65536)
    if header.startswith(b"Kaydara FBX Binary  \x00\x1a\x00") and len(header) >= 27:
        return struct.unpack_from("<I", header, 23)[0]
    match = re.search(rb"\bFBXVersion\s*:\s*(\d+)", header)
    return int(match.group(1)) if match else None


def local_texture(source: Path, reference: str) -> Path | None:
    # FBX and MTL often retain Windows paths, even when Blender runs on Linux.
    name = reference.replace("\\", "/").rsplit("/", 1)[-1].casefold()
    if not name:
        return None
    matches = [p for p in source.iterdir() if p.is_file() and p.name.casefold() == name]
    if len(matches) > 1:
        raise RuntimeError(f"Ambiguous texture filename: {name}")
    return matches[0] if matches else None


def relink_images(bpy, source: Path, *, converted: bool = False) -> None:
    for image in list(bpy.data.images):
        if image.source != "FILE" or not image.filepath:
            continue
        basename = image.filepath.replace("\\", "/").rsplit("/", 1)[-1]
        stem = Path(basename).stem.casefold()
        character_stems = {f"{prefix}{index}" for prefix in ("skharrytex", "ph_harry_tex")
                           for index in range(4)}
        if stem not in character_stems:
            continue # The Gothic reference may retain unrelated body images.
        target = local_texture(source, basename)
        if converted:
            for index in range(4):
                if stem in (f"skharrytex{index}", f"ph_harry_tex{index}"):
                    target = source / f"PH_HARRY_TEX{index}.TGA"
                    break
        if target is None or not target.is_file():
            raise RuntimeError(f"Missing local character texture: {basename}")
        image.filepath = str(target.resolve())
        image.reload()
        # Blender loads image buffers lazily after reload(). Access pixels
        # before checking has_data, otherwise valid files look unloaded.
        pixel_count = len(image.pixels)
        if not image.has_data or min(image.size) <= 0 or pixel_count == 0:
            raise RuntimeError(f"Character texture could not be loaded: {target.name}")


def configure_material_alpha(bpy) -> None:
    # The supplied MTL has map_Kd but no map_d. Its glasses texture carries
    # an alpha channel; ignoring it renders an opaque black strip over the eyes.
    for material in bpy.data.materials:
        if not material.use_nodes:
            continue
        shader = next((node for node in material.node_tree.nodes
                       if node.type == "BSDF_PRINCIPLED"), None)
        if shader is None:
            continue
        for node in material.node_tree.nodes:
            if node.type != "TEX_IMAGE" or node.image is None:
                continue
            name = node.image.filepath.replace("\\", "/").rsplit("/", 1)[-1]
            if not re.fullmatch(r"(?:skharrytex|ph_harry_tex)[0-3]", Path(name).stem, re.IGNORECASE):
                continue
            material.node_tree.links.new(node.outputs["Alpha"], shader.inputs["Alpha"])
            if hasattr(material, "surface_render_method"):
                material.surface_render_method = "DITHERED"
            elif hasattr(material, "blend_method"):
                material.blend_method = "HASHED"


def import_character(bpy, fbx: Path) -> dict:
    version = fbx_version(fbx)
    if version is None or version >= 7100:
        result = bpy.ops.import_scene.fbx(filepath=str(fbx), use_image_search=True)
        relink_images(bpy, fbx.parent)
        configure_material_alpha(bpy)
        return {"format": "FBX", "fbx_version": version, "result": sorted(result)}

    obj = fbx.with_suffix(".obj")
    mtl = fbx.with_suffix(".mtl")
    if not obj.is_file() or not mtl.is_file():
        raise RuntimeError(
            f"FBX {version} is unsupported by Blender. The source archive must "
            "also include skharrymesh.obj and skharrymesh.mtl for mesh import."
        )
    # Correct the supplied material paths in a temporary copy, retaining the
    # original OBJ/MTL and mapping images to the private source directory.
    lines = []
    for line in mtl.read_text("utf-8-sig").splitlines():
        if line.strip().lower().startswith("map_kd "):
            texture = local_texture(fbx.parent, line.strip().split(maxsplit=1)[1])
            if texture is None:
                raise RuntimeError(f"Missing OBJ material texture: {line}")
            line = "map_Kd " + texture.name
        lines.append(line)
    with tempfile.TemporaryDirectory(prefix="parry-obj-import-") as temporary:
        stage = Path(temporary)
        shutil.copy2(obj, stage / obj.name)
        (stage / mtl.name).write_text("\n".join(lines) + "\n", encoding="utf-8")
        for texture in fbx.parent.iterdir():
            if texture.is_file() and re.fullmatch(r"skharrytex[0-3]\.png", texture.name, re.IGNORECASE):
                shutil.copy2(texture, stage / texture.name)
        # Actual Harry OBJ is Y-up. Blender's X-forward setting maps his face
        # to -Y; its default -Z setting rotates the arm span into depth.
        result = bpy.ops.wm.obj_import(
            filepath=str(stage / obj.name), forward_axis="X", up_axis="Y",
            use_split_objects=True, use_split_groups=True, validate_meshes=False,
        )
    relink_images(bpy, fbx.parent)
    configure_material_alpha(bpy)
    return {
        "format": "OBJ", "fbx_version": version, "result": sorted(result),
        "source": str(obj), "forward_axis": "X", "up_axis": "Y",
        "note": "Original UV/material mesh; HP2 armature is not imported. Gothic reference rig is required.",
    }
