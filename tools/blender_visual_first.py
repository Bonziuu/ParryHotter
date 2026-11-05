"""Visual-first Harry body preparation in Blender (private HP2 archive only).

PURPOSE
-------
Use the ORIGINAL Harry HP2 mesh and textures (including Hogwarts robe/cape)
with a stock Gothic 1 human skeleton, instead of recreating HP2 animations.
Builds a Blender scene for a subsequent KrxImpExp ASC export and Gothic test.

REQUIRES
--------
1. A local Harry FBX (from tools/model_asset.py).
2. A .blend containing a Gothic human armature and its original skinned body.
   Create it once by importing HUM_BODY_NAKED0.ASC using KrxImpExp.
   (The Gothic ModKit data / Gothic Sourcer can provide the ASC source.)

INVOKE
------
blender --background --python tools/blender_visual_first.py -- HARRY.fbx GOTHIC_RIG.blend OUT.blend

This deliberately does NOT install an unconverted mesh in Gothic 1.
Neither collision nor HP2's own animation skeleton is copied into the game.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REMOVE_NAMES = ("broom", "quidditch", "miotla", "miota")
CAPE_NAMES = ("cape", "cloak", "peleryn", "plaszcz")
TARGET_HEIGHT = 1.0  # relative to the Gothic reference body height


def is_cape(obj) -> bool:
    texts = [obj.name]
    texts += [slot.material.name for slot in obj.material_slots if slot.material]
    text = " ".join(texts).lower()
    return any(word in text for word in CAPE_NAMES)


def is_broom(obj) -> bool:
    return any(word in obj.name.lower() for word in REMOVE_NAMES)


def copy_rig_from_blend(bpy, path: Path):
    """Append an existing Gothic rig and one mesh with skinning weights."""
    before_objects = set(bpy.data.objects)
    with bpy.data.libraries.load(str(path), link=False) as (src, dst):
        dst.objects = list(src.objects)
    appended = [obj for obj in bpy.data.objects if obj not in before_objects]
    rigs = [o for o in appended if o and o.type == "ARMATURE"]
    if not rigs:
        raise RuntimeError("Gothic .blend file has no armature")
    rig = max(rigs, key=lambda x: len(x.data.bones))
    gothic_meshes = [
        o for o in appended
        if o and o.type == "MESH" and
        (any(m.type == "ARMATURE" and m.object == rig for m in o.modifiers)
         or o.parent == rig)
    ]
    if not gothic_meshes:
        raise RuntimeError("Gothic reference must contain a skin-weighted body mesh")
    for obj in appended:
        if obj and obj.name not in bpy.context.scene.collection.all_objects:
            try:
                bpy.context.scene.collection.objects.link(obj)
            except RuntimeError:
                pass
    return rig, gothic_meshes


def bbox_world(obj):
    from mathutils import Vector
    coords = [obj.matrix_world @ Vector(v) for v in obj.bound_box]
    return (
        min(v.x for v in coords), max(v.x for v in coords),
        min(v.y for v in coords), max(v.y for v in coords),
        min(v.z for v in coords), max(v.z for v in coords),
    )


def bounds(meshes):
    ext = [bbox_world(m) for m in meshes]
    return [
        min(x[0] for x in ext), max(x[1] for x in ext),
        min(x[2] for x in ext), max(x[3] for x in ext),
        min(x[4] for x in ext), max(x[5] for x in ext),
    ]


def align_harry(meshes, gothic_body):
    """Fit the costume silhouette to Gothic body height/ground/center.

    No attempt at preserving exact hitboxes; proportions remain those of Harry.
    """
    from mathutils import Vector
    source = bounds(meshes)
    reference = bounds(gothic_body)
    src_h = source[5] - source[4]
    ref_h = reference[5] - reference[4]
    if src_h <= 0 or ref_h <= 0:
        raise RuntimeError("Cannot calculate height of imported meshes")
    ratio = (ref_h / src_h) * TARGET_HEIGHT
    src_cx = (source[0] + source[1]) / 2
    src_cy = (source[2] + source[3]) / 2
    ref_cx = (reference[0] + reference[1]) / 2
    ref_cy = (reference[2] + reference[3]) / 2
    for mesh in meshes:
        # Mesh transform is applied to vertices in world space, then restored
        # to local space; keeps the original submeshes/textures in one scene.
        world_matrix = mesh.matrix_world.copy()
        inverse = world_matrix.inverted()
        for v in mesh.data.vertices:
            point = world_matrix @ v.co
            transformed = Vector((
                (point.x - src_cx) * ratio + ref_cx,
                (point.y - src_cy) * ratio + ref_cy,
                (point.z - source[4]) * ratio + reference[4],
            ))
            v.co = inverse @ transformed


def closest_body(mesh, gothic_mesh):
    """Transfer stock Gothic bone weights to new mesh by nearest surface."""
    import bpy

    # Remove every HP2-specific bone weight: its Unreal skeleton cannot
    # animate on a ZenGin human rig. Retain only the Gothic reference groups.
    for group in list(mesh.vertex_groups):
        mesh.vertex_groups.remove(group)
    # Target must share vertex group names for DataTransfer of vertex weights.
    for vg in gothic_mesh.vertex_groups:
        if mesh.vertex_groups.get(vg.name) is None:
            mesh.vertex_groups.new(name=vg.name)
    modifier = mesh.modifiers.new("GOTHIC_WEIGHT_TRANSFER", "DATA_TRANSFER")
    modifier.object = gothic_mesh
    modifier.use_vert_data = True
    modifier.data_types_verts = {"VGROUP_WEIGHTS"}
    modifier.vert_mapping = "POLYINTERP_NEAREST"
    modifier.layers_vgroup_select_src = "ALL"
    modifier.layers_vgroup_select_dst = "NAME"
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = mesh
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    # Never save a character that would stand in T-pose. A costume needs
    # almost all vertices weighted to Gothic bones after the transfer.
    vertices = list(mesh.data.vertices)
    assigned = sum(1 for v in vertices if any(g.weight > 0.0001 for g in v.groups))
    if not vertices or assigned < int(0.85 * len(vertices)):
        raise RuntimeError(
            f"Gothic skin transfer incomplete for {mesh.name}: "
            f"{assigned}/{len(vertices)} vertices weighted"
        )


def anchor_cape(mesh, rig):
    """Keep cape visually attached to upper back, avoiding floppy limbs.

    This is a crude temporary cloak, not a physically simulated fabric.
    """
    candidates = [b for b in rig.data.bones if "SPINE" in b.name.upper()]
    if not candidates:
        candidates = [b for b in rig.data.bones if "CHEST" in b.name.upper()]
    if not candidates:
        raise RuntimeError("Gothic rig has no spine/chest bone for robe")
    spine = candidates[-1].name
    for group in list(mesh.vertex_groups):
        mesh.vertex_groups.remove(group)
    group = mesh.vertex_groups.new(name=spine)
    if mesh.data.vertices:
        group.add(list(range(len(mesh.data.vertices))), 1.0, "REPLACE")


def skin_to_gothic(mesh, rig):
    import bpy
    mesh.parent = rig
    mesh.matrix_parent_inverse = rig.matrix_world.inverted()
    modifier = mesh.modifiers.new("GOTHIC_HUMAN_SKIN", "ARMATURE")
    modifier.object = rig
    modifier.use_deform_preserve_volume = True


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) != 3:
        raise SystemExit("Usage: blender --background --python blender_visual_first.py -- HARRY.fbx GOTHIC_RIG.blend OUTPUT.blend")
    harry_fbx, gothic_reference, out = [Path(x).resolve() for x in args]
    if not harry_fbx.is_file() or not gothic_reference.is_file():
        raise RuntimeError("Harry FBX and Gothic human reference .blend must both exist")

    import bpy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    # Load Gothic rig, keep body hidden in both viewport and render.
    # Its vertex weights remain available to the DataTransfer modifier.
    # Load Gothic rig, keep body invisible as a weight transfer reference.
    rig, sources = copy_rig_from_blend(bpy, gothic_reference)
    gothic_body = max(sources, key=lambda x: len(x.data.vertices))
    for obj in sources:
        obj.hide_render = True
        obj.hide_set(True)

    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(harry_fbx), use_image_search=True)
    harry_objs = [
        o for o in bpy.data.objects
        if o not in before and o.type == "MESH" and not is_broom(o)
    ]
    if not harry_objs:
        raise RuntimeError("Harry FBX has no character meshes")

    # Bake all original HP2 object transformations into Harry's mesh
    # BEFORE parenting to Gothic skeleton, without touching Gothic body.
    for obj in harry_objs:
        original_world = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = original_world
        for modifier in list(obj.modifiers):
            if modifier.type == "ARMATURE":
                obj.modifiers.remove(modifier)
        obj.data = obj.data.copy()

    align_harry(harry_objs, [gothic_body])
    for obj in harry_objs:
        cape = is_cape(obj)
        if cape:
            anchor_cape(obj, rig)
        else:
            closest_body(obj, gothic_body)
        skin_to_gothic(obj, rig)
        obj.name = "PH_HARRY_CAPE" if cape else "PH_HARRY_" + obj.name
    rig.name = "PH_GOTHIC_HUMAN_RIG"

    # Repoint imported FBX materials to the four TGA textures prepared
    # locally by blender_prepare.py. Gothic reads TGA, not the original PNG.
    for image in bpy.data.images:
        name = Path(bpy.path.abspath(image.filepath)).stem.lower()
        for i in range(4):
            if name in (f"skharrytex{i}", f"ph_harry_tex{i}"):
                tex = harry_fbx.parent / f"PH_HARRY_TEX{i}.TGA"
                if tex.is_file():
                    image.filepath = str(tex)
                break

    out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(out))
    report = {
        "stage": "visual-first Blender skinning preview, not Gothic-ready ASC",
        "model": "ordinary Harry Potter PC Chamber of Secrets",
        "meshes": [o.name for o in harry_objs],
        "cape_meshes": [o.name for o in harry_objs if "PH_HARRY_CAPE" in o.name],
        "gothic_rig": rig.name,
        "note": "Review alignment, robe weights and texture paths; export ASC with KrxImpExp separately.",
    }
    out.with_suffix(".json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
