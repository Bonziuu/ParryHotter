"""Visual-first Harry body preparation in Blender (private HP2 archive only).

PURPOSE
-------
Use the ORIGINAL Harry HP2 mesh and textures (including Hogwarts robe/cape)
with a stock Gothic 1 human skeleton, instead of recreating HP2 animations.
Builds a Blender scene and uses an installed KrxImpExp ASC exporter when available.
An absent exporter is reported explicitly; a .blend is never treated as an ASC.

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
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.blender_character_import import import_character, relink_images

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


def enable_krx_if_installed():
    """Enable a user-installed add-on for this process, without installing code."""
    try:
        import addon_utils
        if any(module.__name__ == "KrxImpExp" for module in addon_utils.modules()):
            addon_utils.enable("KrxImpExp", default_set=False, persistent=False)
    except (ImportError, RuntimeError) as exc:
        print(f"KrxImpExp could not be enabled: {exc}")


def select_reference_rig(rigs):
    candidates = [rig for rig in rigs
                  if rig.get("ph_reference_kind") != "original_hp2_source_not_gothic"]
    if not candidates:
        raise RuntimeError("Harry's original HP2 rig cannot replace the required Gothic reference skeleton/body")
    return max(candidates, key=lambda rig: len(rig.data.bones))


def copy_rig_from_blend(bpy, path: Path):
    """Append an existing Gothic rig and one mesh with skinning weights."""
    before_objects = set(bpy.data.objects)
    if path.suffix.lower() == ".asc":
        try:
            bpy.ops.import_scene.krxascimp(filepath=str(path), quiet=True)
        except (AttributeError, RuntimeError) as exc:
            raise RuntimeError("Importing a Gothic ASC reference requires the KrxImpExp import_scene.krxascimp operator") from exc
    else:
        with bpy.data.libraries.load(str(path), link=False) as (src, dst):
            dst.objects = list(src.objects)
    appended = [obj for obj in bpy.data.objects if obj not in before_objects]
    rigs = [o for o in appended if o and o.type == "ARMATURE"]
    if not rigs:
        raise RuntimeError("Gothic .blend file has no armature")
    rig = select_reference_rig(rigs)
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


def anatomical_scale(source_feet, source_head, target_feet, target_head):
    """Match a real head pivot and floor, rather than a headless body bbox."""
    values = (source_feet, source_head, target_feet, target_head)
    if not all(math.isfinite(value) for value in values):
        raise RuntimeError("Character alignment landmarks must be finite")
    if source_head <= source_feet or target_head <= target_feet:
        raise RuntimeError("Character head pivot must be above its feet")
    return (target_head - target_feet) / (source_head - source_feet)


def align_harry(meshes, gothic_body, rig=None):
    """Preserve Harry's proportions and align his actual head pivot and feet.

    The Gothic body reference lacks the separate head silhouette. Its bbox
    therefore cannot determine the total height of a full-character mesh.
    """
    from mathutils import Vector
    source = bounds(meshes)
    reference = bounds(gothic_body)
    src_h = source[5] - source[4]
    ref_h = reference[5] - reference[4]
    if src_h <= 0 or ref_h <= 0:
        raise RuntimeError("Cannot calculate height of imported meshes")
    source_pivot = next((mesh.get("ph_source_head_pivot") for mesh in meshes
                         if mesh.get("ph_source_head_pivot") is not None), None)
    target_head = None
    if source_pivot is not None:
        if rig is None:
            raise RuntimeError("Anatomical character alignment requires the Gothic rig")
        target_bone = next((bone for bone in rig.data.bones
                            if bone.name.casefold() == "bip01 head"), None)
        if target_bone is None:
            raise RuntimeError("Gothic rig has no Bip01 Head alignment landmark")
        target_head = rig.matrix_world @ target_bone.head_local
        ratio = anatomical_scale(source[4], source_pivot[2], reference[4], target_head.z)
    else:
        ratio = (ref_h / src_h) * TARGET_HEIGHT
    src_cx = (source[0] + source[1]) / 2
    src_cy = (source[2] + source[3]) / 2
    ref_cx = (reference[0] + reference[1]) / 2
    ref_cy = (reference[2] + reference[3]) / 2
    if source_pivot is not None:
        src_cx, src_cy = source_pivot[:2]
        ref_cx, ref_cy = target_head.x, target_head.y
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
        mesh["ph_alignment_scale"] = ratio
        if source_pivot is not None:
            mesh["ph_aligned_head_pivot"] = list(target_head)
            neck = mesh.get("ph_source_neck_pivot")
            if neck is not None:
                mesh["ph_aligned_neck_pivot"] = [
                    (neck[0] - src_cx) * ratio + ref_cx,
                    (neck[1] - src_cy) * ratio + ref_cy,
                    (neck[2] - source[4]) * ratio + reference[4],
                ]


def semantic_bone_map(source_parents, target_names):
    """Map original auxiliary bones to the nearest known biped ancestor."""
    targets = {name.casefold(): name for name in target_names}
    result = {}
    for name in source_parents:
        current, seen = name, set()
        while current is not None and current not in seen:
            if current.casefold() in targets:
                mapped = targets[current.casefold()]
                if mapped.casefold() == "bip01" and "bip01 pelvis" in targets:
                    mapped = targets["bip01 pelvis"]
                # HP's Finger0 is a finger base while Gothic's single Finger0
                # is near the fingertips. Treating them as equal landmarks
                # stretches the hand; retain the source fingers on the hand.
                for side in ("l", "r"):
                    if mapped.casefold().startswith(f"bip01 {side} finger"):
                        mapped = targets.get(f"bip01 {side} hand", mapped)
                result[name] = mapped
                break
            seen.add(current)
            current = source_parents.get(current)
        else:
            result[name] = None
    return result


def retarget_source_skin(meshes, source_rig, target_rig):
    """Rest-pose the original skin onto Gothic's actual joint locations.

    Biped files use different local bone roll conventions (often 180 degrees).
    Anatomical joint axes avoid flipping the limbs when comparing bind frames.
    Source skin weights drive both the rest-pose warp and the target binding;
    face/glasses retain their detail and cloak geometry follows the pelvis.
    """
    if source_rig is None:
        return set()
    from mathutils import Vector
    parents = {bone.name: bone.parent.name if bone.parent else None
               for bone in source_rig.data.bones}
    mapping = semantic_bone_map(parents, [bone.name for bone in target_rig.data.bones])
    source_bones = {bone.name.casefold(): bone for bone in source_rig.data.bones}
    target_bones = {bone.name.casefold(): bone for bone in target_rig.data.bones}
    children = {
        "bip01 pelvis": "bip01 spine", "bip01 spine": "bip01 spine1",
        "bip01 spine1": "bip01 spine2", "bip01 spine2": "bip01 neck",
        "bip01 neck": "bip01 head",
    }
    for side in ("l", "r"):
        for first, second in (("clavicle", "upperarm"), ("upperarm", "forearm"),
                              ("forearm", "hand"),
                              ("thigh", "calf"), ("calf", "foot"), ("foot", "toe0")):
            children[f"bip01 {side} {first}"] = f"bip01 {side} {second}"
    changed = set()
    for mesh in meshes:
        source_head = mesh.get("ph_source_head_pivot")
        target_head = mesh.get("ph_aligned_head_pivot")
        if source_head is None or target_head is None:
            raise RuntimeError("Source skin retarget requires actual aligned head landmarks")
        scale = float(mesh["ph_alignment_scale"])
        source_head, target_head = Vector(source_head), Vector(target_head)

        def aligned(point):
            return (point - source_head) * scale + target_head

        transforms = {}
        for key, target_bone in target_bones.items():
            if key not in source_bones:
                continue
            source_bone = source_bones[key]
            a = aligned(source_rig.matrix_world @ source_bone.head_local)
            b = target_rig.matrix_world @ target_bone.head_local
            child = children.get(key)
            if child and child in source_bones and child in target_bones:
                source_end = aligned(source_rig.matrix_world @ source_bones[child].head_local)
                target_end = target_rig.matrix_world @ target_bones[child].head_local
                before, after = source_end - a, target_end - b
                if before.length < 0.00001 or after.length < 0.00001:
                    raise RuntimeError(f"Degenerate anatomical joint segment: {target_bone.name}")
                transforms[target_bone.name] = (a, b, before.normalized(),
                    before.rotation_difference(after), after.length / before.length)
            else:
                transforms[target_bone.name] = (a, b, None, None, 1.0)
        # Terminal hands/toes follow the preceding segment's orientation,
        # retaining their own local shape instead of a different bone roll.
        for key, target_bone in target_bones.items():
            if key == "bip01 head" or key in children or not target_bone.parent:
                continue
            own = transforms.get(target_bone.name)
            parent = transforms.get(target_bone.parent.name)
            if own and parent and parent[3] is not None:
                transforms[target_bone.name] = (own[0], own[1], parent[2], parent[3], 1.0)
        pelvis = target_bones.get("bip01 pelvis")
        if pelvis is None or pelvis.name not in transforms:
            raise RuntimeError("Both original and Gothic skeletons need a pelvis")
        cloth = set(mesh.get("ph_cloak_vertices", ()))
        positions, bindings = [], []
        for vertex in mesh.data.vertices:
            point = mesh.matrix_world @ vertex.co
            weights = {}
            if vertex.index in cloth:
                a, b, _, _, _ = transforms[pelvis.name]
                positions.append(point + b - a)
                bindings.append({pelvis.name: 1.0})
                continue
            for influence in vertex.groups:
                if influence.weight <= 0.000001:
                    continue
                name = mesh.vertex_groups[influence.group].name
                target = mapping.get(name)
                if target is None or target not in transforms:
                    raise RuntimeError(f"No Gothic biped ancestor for source skin bone: {name}")
                weights[target] = weights.get(target, 0.0) + influence.weight
            total = sum(weights.values())
            if total <= 0:
                raise RuntimeError(f"Unweighted source vertex before rest-pose transfer: {mesh.name}")
            weights = {name: weight / total for name, weight in weights.items()}
            warped = Vector((0, 0, 0))
            for name, weight in weights.items():
                a, b, direction, rotation, factor = transforms[name]
                offset = point - a
                if direction is not None:
                    axial = direction * offset.dot(direction)
                    offset = rotation @ (offset - axial + axial * factor)
                warped += (b + offset) * weight
            positions.append(warped)
            bindings.append(weights)
        inverse = mesh.matrix_world.inverted()
        for vertex, position in zip(mesh.data.vertices, positions):
            vertex.co = inverse @ position
        for group in list(mesh.vertex_groups):
            mesh.vertex_groups.remove(group)
        names = {name for weights in bindings for name in weights}
        groups = {name: mesh.vertex_groups.new(name=name) for name in sorted(names)}
        for index, weights in enumerate(bindings):
            for name, weight in weights.items():
                groups[name].add([index], weight, "REPLACE")
        mesh["ph_semantic_retarget"] = "source_biped_joint_axes"
        changed.add(mesh.name)
    return changed


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


def linear_bone_weights(height, anchors):
    """Blend between torso anchors instead of attaching a whole cape rigidly."""
    ordered = sorted(anchors, key=lambda item: item[1])
    if not ordered:
        raise RuntimeError("No torso bones for robe")
    if height <= ordered[0][1]:
        return [(ordered[0][0], 1.0)]
    for (lower, z0), (upper, z1) in zip(ordered, ordered[1:]):
        if height <= z1 and z1 > z0:
            t = (height - z0) / (z1 - z0)
            return [(lower, 1.0 - t), (upper, t)]
    return [(ordered[-1][0], 1.0)]


def anchor_cape(mesh, rig, indices=None):
    """Bind cloth to pelvis/spine/chest, retaining torso bends (not cloth physics)."""
    candidates = [b for b in rig.data.bones
                  if any(word in b.name.upper() for word in ("SPINE", "CHEST", "PELVIS"))]
    if len(candidates) < 2:
        raise RuntimeError("Gothic rig needs pelvis/spine bones for deforming robe")
    anchors = [(b.name, (rig.matrix_world @ b.head_local).z) for b in candidates]
    if indices is None:
        for group in list(mesh.vertex_groups):
            mesh.vertex_groups.remove(group)
    else:
        indices = set(indices)
    groups = {name: mesh.vertex_groups.get(name) or mesh.vertex_groups.new(name=name)
              for name, _ in anchors}
    for vertex in mesh.data.vertices:
        if indices is not None:
            if vertex.index not in indices:
                continue
            for group in list(vertex.groups):
                mesh.vertex_groups[group.group].remove([vertex.index])
        height = (mesh.matrix_world @ vertex.co).z
        for name, weight in linear_bone_weights(height, anchors):
            if weight > 0.000001:
                groups[name].add([vertex.index], weight, "REPLACE")


def normalize_skin(mesh, rig):
    """Require every vertex to have valid, normalized Gothic bone influences."""
    bones = {b.name for b in rig.data.bones}
    missing = []
    for vertex in mesh.data.vertices:
        weights = [(g.group, g.weight) for g in vertex.groups
                   if mesh.vertex_groups[g.group].name in bones
                   and math.isfinite(g.weight) and g.weight > 0.000001]
        weights = sorted(weights, key=lambda item: item[1], reverse=True)[:4]
        total = sum(weight for _, weight in weights)
        if total <= 0:
            missing.append(vertex.index)
            continue
        for group in list(vertex.groups):
            mesh.vertex_groups[group.group].remove([vertex.index])
        for index, weight in weights:
            mesh.vertex_groups[index].add([vertex.index], weight / total, "REPLACE")
    if missing:
        raise RuntimeError(f"Unweighted vertices in {mesh.name}: {len(missing)}; correct skin weights before export")


def bind_head(mesh, rig):
    """Bind the recovered facial branch, including glasses, to the Gothic head."""
    head = next((b for b in rig.data.bones if b.name.upper().endswith("HEAD")), None)
    neck = next((b for b in rig.data.bones if b.name.upper().endswith("NECK")), None)
    if not head or not neck:
        raise RuntimeError("Full Harry replacement requires Gothic head and neck bones")
    z0 = (rig.matrix_world @ neck.head_local).z
    z1 = (rig.matrix_world @ head.head_local).z
    if z1 <= z0:
        raise RuntimeError("Gothic reference head must be above neck in the rest pose")
    head_group = mesh.vertex_groups.get(head.name) or mesh.vertex_groups.new(name=head.name)
    source_head = mesh.get("ph_head_weights")
    source_neck = mesh.get("ph_neck_weights")
    if source_head is not None:
        if len(source_head) != len(mesh.data.vertices) or source_neck is None or len(source_neck) != len(source_head):
            raise RuntimeError("Recovered source anatomy does not match character topology")
        neck_group = mesh.vertex_groups.get(neck.name) or mesh.vertex_groups.new(name=neck.name)
        for vertex in mesh.data.vertices:
            weight = float(source_head[vertex.index])
            neck_weight = float(source_neck[vertex.index])
            if not math.isfinite(weight) or not math.isfinite(neck_weight):
                raise RuntimeError("Recovered character head weights must be finite")
            if weight <= 0.000001 and neck_weight <= 0.000001:
                continue
            for group in list(vertex.groups):
                mesh.vertex_groups[group.group].remove([vertex.index])
            weight = min(1.0, max(0.0, weight))
            if weight > 0.000001:
                head_group.add([vertex.index], weight, "REPLACE")
            if 1.0 - weight > 0.000001:
                neck_group.add([vertex.index], 1.0 - weight, "REPLACE")
        return
    for vertex in mesh.data.vertices:
        height = (mesh.matrix_world @ vertex.co).z
        if height < z1:
            continue
        for group in list(vertex.groups):
            mesh.vertex_groups[group.group].remove([vertex.index])
        head_group.add([vertex.index], 1.0, "REPLACE")


def export_gothic_asc(bpy, destination):
    """Use KrxImpExp's documented quiet operator; never fabricate an ASC file."""
    operator = getattr(bpy.ops.export_scene, "krxascexp", None)
    try:
        available = operator is not None and operator.get_rna_type() is not None
    except (AttributeError, KeyError, RuntimeError):
        available = False
    if not available:
        return {"status": "missing_exporter", "required_operator": "export_scene.krxascexp",
                "action": "Install a KrxImpExp version compatible with this Blender, then rerun conversion or export PH_HARRY.ASC manually."}
    temporary = destination.with_name(destination.stem + ".pending.asc")
    try:
        from tools.blender_krx import stable_krx_rotations
        with stable_krx_rotations():
            result = operator(filepath=str(temporary), quiet=True)
        if "FINISHED" not in result or not temporary.is_file():
            raise RuntimeError("KrxImpExp did not produce an ASC")
        repo = str(Path(__file__).resolve().parents[1])
        if repo not in sys.path:
            sys.path.insert(0, repo)
        from tools.visual_install import validate_asc
        from tools.install import PatchError
        try:
            validate_asc(temporary.read_bytes())
        except PatchError as exc:
            raise RuntimeError(f"KrxImpExp output is not a complete skinned Gothic ASC: {exc}") from exc
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    return {"status": "exported_pending_engine_validation", "asc": str(destination)}


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
    enable_krx_if_installed()
    # Load Gothic rig, keep body hidden in both viewport and render.
    # Its vertex weights remain available to the DataTransfer modifier.
    # Load Gothic rig, keep body invisible as a weight transfer reference.
    rig, sources = copy_rig_from_blend(bpy, gothic_reference)
    gothic_body = max(sources, key=lambda x: len(x.data.vertices))
    for obj in sources:
        obj.hide_render = True
        obj.hide_set(True)

    before = set(bpy.data.objects)
    imported = import_character(bpy, harry_fbx)
    harry_objs = [
        o for o in bpy.data.objects
        if o not in before and o.type == "MESH" and not is_broom(o)
    ]
    if not harry_objs:
        raise RuntimeError("Harry FBX has no character meshes")
    if imported["format"] == "OBJ":
        from tools.blender_legacy_skin import recover_legacy_skin
        imported["source_skin"] = recover_legacy_skin(bpy, harry_fbx, harry_objs)
        imported["note"] = "Original HP2 skin recovered, then remapped to the supplied Gothic reference."

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

    align_harry(harry_objs, [gothic_body], rig)
    source_rig = bpy.data.objects.get(imported.get("source_skin", {}).get("armature", ""))
    semantic_meshes = retarget_source_skin(harry_objs, source_rig, rig)
    for obj in harry_objs:
        cape = is_cape(obj)
        if cape:
            anchor_cape(obj, rig)
        else:
            if obj.name not in semantic_meshes:
                closest_body(obj, gothic_body)
            bind_head(obj, rig)
            cloak_vertices = list(obj.get("ph_cloak_vertices", ()))
            if cloak_vertices:
                anchor_cape(obj, rig, cloak_vertices)
        normalize_skin(obj, rig)
        skin_to_gothic(obj, rig)
        # KrxImpExp classifies mesh nodes by the Gothic ZM_ prefix. Using
        # PH_HARRY_ makes them dummy nodes and may omit skin from the ASC.
        obj.name = "ZM_HARRY_CAPE" if cape else "ZM_HARRY_" + obj.name
    # KrxImpExp's Blender adapter prefixes bone names with the armature name,
    # except for the special name Armature. Keep exported names as Bip01*.
    for obj in list(bpy.data.objects):
        if obj != rig and obj not in harry_objs:
            bpy.data.objects.remove(obj, do_unlink=True)
    rig.name = "Armature"

    # Repoint imported FBX materials to the four TGA textures prepared
    # locally by blender_prepare.py. Gothic reads TGA, not the original PNG.
    relink_images(bpy, harry_fbx.parent, converted=True)

    out.parent.mkdir(parents=True, exist_ok=True)
    for image in bpy.data.images:
        if Path(image.filepath).name.upper() in {f"PH_HARRY_TEX{i}.TGA" for i in range(4)}:
            image.pack()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.wm.save_as_mainfile(filepath=str(out))
    exported = export_gothic_asc(bpy, harry_fbx.parent / "PH_HARRY.ASC")
    report = {
        "stage": "skinned Blender scene; engine validation still required",
        "model": "ordinary Harry Potter PC Chamber of Secrets",
        "import": imported,
        "meshes": [o.name for o in harry_objs],
        "cape_meshes": [o.name for o in harry_objs if "ZM_HARRY_CAPE" in o.name or len(o.get("ph_cloak_vertices", ()))],
        "gothic_rig": rig.name,
        "gothic_reference": str(gothic_reference),
        "gothic_bones": len(rig.data.bones),
        "export": exported,
        "note": "Review alignment, robe weights and texture paths; exported ASC still needs strict staging validation and a Gothic engine test.",
    }
    out.with_suffix(".json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
