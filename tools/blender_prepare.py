"""Create a PRIVATE Blender project from original HP2 Harry FBX.

Run with Blender, not system Python:
    blender --background --python tools/blender_prepare.py -- INPUT_DIR OUTPUT.blend

This is a staging scene, NOT a fully retargeted Gothic 1 character.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.blender_character_import import import_character, relink_images


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) != 2:
        raise SystemExit("Usage: blender --background --python blender_prepare.py -- INPUT_DIR OUTPUT.blend")
    source = Path(args[0]).resolve()
    destination = Path(args[1]).resolve()
    file = source / "skharrymesh.fbx"
    if not file.is_file():
        raise RuntimeError(f"Missing HP2 Harry FBX: {file}")

    import bpy

    bpy.ops.wm.read_factory_settings(use_empty=True)
    imported = import_character(bpy, file)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if imported["format"] == "OBJ":
        from tools.blender_legacy_skin import recover_legacy_skin
        imported["source_skin"] = recover_legacy_skin(bpy, file, meshes)
        imported["note"] = "Original HP2 bind rig and weights recovered; Gothic reference is still required."
    rigs = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if not meshes:
        raise RuntimeError("Imported FBX contains no Blender mesh")

    # Keep the original FBX rig intact until it can be retargeted to Gothic I.
    # Automatically replacing its armature destroys existing vertex weights.
    relink_images(bpy, source)
    # Convert the FOUR original HP2 PNG textures into Gothic-readable TGAs.
    # Use names that cannot accidentally replace base-game textures. All
    # results stay under the local private folder, never in our GitHub repo.
    converted_textures = []
    for index in range(4):
        png = source / f"skharrytex{index}.png"
        if not png.is_file():
            raise RuntimeError(f"Missing Harry texture: {png}")
        image = bpy.data.images.load(str(png), check_existing=True)
        target = source / f"PH_HARRY_TEX{index}.TGA"
        image.filepath_raw = str(target)
        image.file_format = "TARGA"
        image.save()
        if not target.is_file() or target.stat().st_size < 18:
            raise RuntimeError(f"Texture conversion failed: {target}")
        converted_textures.append(target.name)

    relink_images(bpy, source, converted=True)

    destination.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.file.pack_all() # The private .blend remains inspectable off this machine.
    bpy.ops.wm.save_as_mainfile(filepath=str(destination))
    info = {
        "source": "Harry Potter and the Chamber of Secrets (PC, 2002)",
        "blender_result": imported["result"],
        "import": imported,
        "meshes": [{"name": m.name, "vertices": len(m.data.vertices)} for m in meshes],
        "armatures": [{"name": a.name, "bones": len(a.data.bones)} for a in rigs],
        "destination": str(destination),
        "gothic_tga_textures": converted_textures,
        "status": "imported for manual skeleton retargeting; NOT installed in Gothic",
    }
    destination.with_suffix(".json").write_text(
        json.dumps(info, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(info, ensure_ascii=False))


if __name__ == "__main__":
    main()
