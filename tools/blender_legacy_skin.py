"""Recover source skin from legacy binary FBX when Blender imports its OBJ.

FBX 6 stores geometry and skin arrays as scalar properties. The native Blender
FBX reader can parse those records even though its importer requires FBX 7.1.
Only original source bind transforms/weights are recovered here. This creates
an HP2 preparation rig, never a replacement for a real Gothic skeleton.
"""
from __future__ import annotations

import math
from pathlib import Path


def _name(value):
    if isinstance(value, bytes):
        value = value.split(b"\x00\x01", 1)[0].decode("utf-8", "replace")
    return str(value)


def _child(element, identifier):
    return next((item for item in element.elems if item.id == identifier), None)


def _values(element, identifier):
    item = _child(element, identifier)
    if item is None:
        return []
    props = item.props
    if len(props) == 1 and not isinstance(props[0], (str, bytes, int, float)):
        return list(props[0])
    return list(props)


def _properties(element):
    block = _child(element, b"Properties60") or _child(element, b"Properties70")
    if block is None:
        return {}
    result = {}
    for item in block.elems:
        if len(item.props) < 3:
            continue
        start = 3 if item.id == b"Property" else 4
        result[_name(item.props[0])] = list(item.props[start:])
    return result


def parse_legacy_skin(root, version):
    """Return plain metadata without depending on bpy/mathutils (unit-testable)."""
    if version >= 7100:
        raise ValueError("Legacy skin recovery is restricted to pre-7.1 FBX")
    objects = _child(root, b"Objects")
    connections = _child(root, b"Connections")
    if objects is None or connections is None:
        raise ValueError("FBX is missing Objects or Connections")
    table = {item.props[0]: item for item in objects.elems if item.props}
    parents = {}
    children = {}
    for link in connections.elems:
        if len(link.props) != 3 or link.props[0] != b"OO":
            continue
        child, parent = link.props[1:]
        parents.setdefault(child, []).append(parent)
        children.setdefault(parent, []).append(child)

    bones = {}
    meshes = {}
    for identifier, item in table.items():
        if item.id != b"Model" or len(item.props) < 2:
            continue
        kind = item.props[1]
        if kind in (b"LimbNode", b"Root", b"Null"):
            parent = next((value for value in parents.get(identifier, ())
                           if value in table and table[value].id == b"Model"
                           and len(table[value].props) > 1
                           and table[value].props[1] in (b"LimbNode", b"Root", b"Null")), None)
            bones[_name(identifier)] = {
                "parent": _name(parent) if parent is not None else None,
                "properties": _properties(item), "bind_matrix": None,
            }
        elif kind == b"Mesh":
            coordinates = _values(item, b"Vertices")
            if len(coordinates) % 3 or not all(math.isfinite(x) for x in coordinates):
                raise ValueError("Invalid legacy FBX vertex coordinates")
            vertices = [tuple(coordinates[i:i + 3]) for i in range(0, len(coordinates), 3)]
            meshes[_name(identifier)] = {
                "vertices": vertices, "weights": [{} for _ in vertices],
                "cloth": [False for _ in vertices], "properties": _properties(item),
            }

    cluster_count = 0
    for identifier, item in table.items():
        if item.id != b"Deformer" or len(item.props) < 2 or item.props[1] != b"Cluster":
            continue
        linked_bones = [value for value in children.get(identifier, ())
                        if _name(value) in bones]
        skins = parents.get(identifier, ())
        linked_meshes = [_name(mesh) for skin in skins for mesh in parents.get(skin, ())
                         if _name(mesh) in meshes]
        if len(linked_bones) != 1 or len(linked_meshes) != 1:
            raise ValueError("A legacy skin cluster has ambiguous bone/mesh connections")
        bone_name = _name(linked_bones[0])
        mesh = meshes[linked_meshes[0]]
        indices, weights = _values(item, b"Indexes"), _values(item, b"Weights")
        if len(indices) != len(weights):
            raise ValueError("Legacy skin cluster index/weight lengths differ")
        for index, weight in zip(indices, weights):
            if not isinstance(index, int) or not 0 <= index < len(mesh["vertices"]):
                raise ValueError("Legacy skin references a vertex outside its mesh")
            if not math.isfinite(weight) or weight < 0:
                raise ValueError("Invalid legacy skin weight")
            if weight > 0:
                influences = mesh["weights"][index]
                influences[bone_name] = influences.get(bone_name, 0) + weight
                mesh["cloth"][index] |= "cloak" in bone_name.casefold()
        matrix = _values(item, b"TransformLink")
        if len(matrix) != 16 or not all(math.isfinite(value) for value in matrix):
            raise ValueError("A legacy skin cluster lacks a finite TransformLink")
        previous = bones[bone_name]["bind_matrix"]
        if previous is not None and any(abs(a - b) > 0.001 for a, b in zip(previous, matrix)):
            raise ValueError(f"Conflicting bind transforms for source bone {bone_name}")
        bones[bone_name]["bind_matrix"] = matrix
        cluster_count += 1
    if not bones or not meshes or not cluster_count:
        raise ValueError("No original armature/skin was found in legacy FBX")
    for mesh in meshes.values():
        for influences in mesh["weights"]:
            total = sum(influences.values())
            if total <= 0:
                raise ValueError("A source FBX vertex has no skin weights")
            for bone in influences:
                influences[bone] /= total
    return {"version": version, "bones": bones, "meshes": meshes,
            "cluster_count": cluster_count}


def read_legacy_skin(path):
    from io_scene_fbx import parse_fbx
    root, version = parse_fbx.parse(str(Path(path)))
    return parse_legacy_skin(root, version)


def source_head_anatomy(bones):
    """Locate the actual biped head branch, excluding separate cloak rigs."""
    head = next((name for name in bones if name.casefold() == "bip01 head"), None)
    neck = next((name for name in bones if name.casefold() == "bip01 neck"), None)
    members = set()
    if head:
        for name in bones:
            current, seen = name, set()
            while current is not None and current not in seen:
                if current == head:
                    members.add(name)
                    break
                seen.add(current)
                current = bones.get(current, {}).get("parent")
    return head, neck, members


def _local_matrix(properties):
    from mathutils import Euler, Matrix, Vector
    def vector(name, default):
        value = properties.get(name, default)
        if len(value) != 3 or not all(math.isfinite(x) for x in value):
            raise ValueError(f"Invalid source transform property {name}")
        return Vector(value)
    def translation(name):
        return Matrix.Translation(vector(name, (0, 0, 0)))
    order_value = properties.get("RotationOrder", [0])[0]
    orders = ("XYZ", "XZY", "YZX", "YXZ", "ZXY", "ZYX")
    if order_value not in range(len(orders)):
        raise ValueError("Unsupported legacy FBX rotation order")
    def rotation(name, order="XYZ"):
        values = vector(name, (0, 0, 0))
        return Euler(tuple(math.radians(x) for x in values), order).to_matrix().to_4x4()
    scale = Matrix.Diagonal((*vector("Lcl Scaling", (1, 1, 1)), 1))
    return (translation("Lcl Translation") @ translation("RotationOffset") @
            translation("RotationPivot") @ rotation("PreRotation") @
            rotation("Lcl Rotation", orders[order_value]) @
            rotation("PostRotation").inverted() @ translation("RotationPivot").inverted() @
            translation("ScalingOffset") @ translation("ScalingPivot") @ scale @
            translation("ScalingPivot").inverted())


def _source_world_matrices(data):
    from mathutils import Matrix
    matrices = {}
    visiting = set()
    def resolve(name):
        if name in matrices:
            return matrices[name]
        if name in visiting:
            raise ValueError("Cycle in the source bone hierarchy")
        visiting.add(name)
        bone = data["bones"][name]
        values = bone["bind_matrix"]
        if values is not None:
            # FBX stores matrices with the translation in the last row.
            matrix = Matrix([values[i:i + 4] for i in range(0, 16, 4)]).transposed()
        else:
            local = _local_matrix(bone["properties"])
            matrix = resolve(bone["parent"]) @ local if bone["parent"] else local
        visiting.remove(name)
        matrices[name] = matrix
        return matrix
    for name in data["bones"]:
        resolve(name)
    return matrices


def recover_legacy_skin(bpy, fbx, imported_meshes):
    """Attach the actual HP2 preparation rig and record per-vertex cloth masks.

    Imported OBJ objects must share the same source-to-Blender axis transform.
    Coordinates are matched in world space, including OBJ seam duplicates.
    Ambiguous different weights at the same point are rejected, never guessed.
    """
    from mathutils import Vector
    from mathutils.kdtree import KDTree
    data = read_legacy_skin(fbx)
    if not imported_meshes:
        raise ValueError("No imported OBJ meshes to recover")
    # Blender's native OBJ importer retains source coordinates in the mesh
    # and stores its import-axis conversion in matrix_world. Reuse that exact
    # matrix instead of assuming its internal forward-axis sign convention.
    axis = imported_meshes[0].matrix_world.copy()
    if any(abs(obj.matrix_world[row][column] - axis[row][column]) > 0.00001
           for obj in imported_meshes for row in range(4) for column in range(4)):
        raise ValueError("Split OBJ meshes have inconsistent import transforms")
    source_world = _source_world_matrices(data)
    source_head, source_neck, head_members = source_head_anatomy(data["bones"])
    prepared = {}
    for name, source in data["meshes"].items():
        transform = axis @ _local_matrix(source["properties"])
        coordinates = [transform @ Vector(point) for point in source["vertices"]]
        tree = KDTree(len(coordinates))
        for index, point in enumerate(coordinates):
            tree.insert(point, index)
        tree.balance()
        prepared[name] = (source, tree)

    assignments = []
    tolerance = 0.0002
    cloth_count = vertex_count = 0
    for obj in imported_meshes:
        names = {obj.name.split(".")[0]}
        names.update(slot.material.name.split(".")[0] for slot in obj.material_slots if slot.material)
        candidates = [name for name in prepared if name in names]
        if len(candidates) != 1:
            raise ValueError(f"Cannot identify source material group for OBJ mesh {obj.name}")
        source, tree = prepared[candidates[0]]
        mapped = []
        cloth_vertices = []
        for vertex in obj.data.vertices:
            point = obj.matrix_world @ vertex.co
            close = tree.find_range(point, tolerance)
            if not close:
                raise ValueError(f"OBJ vertex {obj.name}:{vertex.index} does not match source FBX")
            indices = [item[1] for item in close]
            weights = source["weights"][indices[0]]
            for index in indices[1:]:
                other = source["weights"][index]
                keys = set(weights) | set(other)
                if any(abs(weights.get(key, 0) - other.get(key, 0)) > 0.0001 for key in keys):
                    raise ValueError(f"Ambiguous skin at OBJ seam {obj.name}:{vertex.index}")
            mapped.append(weights)
            if any(source["cloth"][index] for index in indices):
                cloth_vertices.append(vertex.index)
        obj["ph_cloak_vertices"] = cloth_vertices
        # Keep anatomical source data before Gothic skin transfer clears the
        # HP2 groups. Facial auxiliaries and glasses descend from the real
        # biped head, even when their own group names do not contain "Head".
        if source_head and source_neck:
            obj["ph_source_head_pivot"] = list((axis @ source_world[source_head]).translation)
            obj["ph_source_neck_pivot"] = list((axis @ source_world[source_neck]).translation)
            obj["ph_head_weights"] = [sum(weight for name, weight in weights.items()
                                          if name in head_members) for weights in mapped]
            obj["ph_neck_weights"] = [weights.get(source_neck, 0.0) for weights in mapped]
        cloth_count += len(cloth_vertices)
        vertex_count += len(mapped)
        assignments.append((obj, mapped))

    # Complete matching happens before touching any original mesh skin.
    bpy.ops.object.select_all(action="DESELECT")
    armature = bpy.data.armatures.new("HP2_ORIGINAL_SOURCE_RIG")
    rig = bpy.data.objects.new("HP2_ORIGINAL_SOURCE_RIG", armature)
    bpy.context.scene.collection.objects.link(rig)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    try:
        for name, matrix in source_world.items():
            bone = armature.edit_bones.new(name)
            bone.matrix = axis @ matrix
            bone.length = 1.0
        for name, info in data["bones"].items():
            bone = armature.edit_bones[name]
            if info["parent"]:
                bone.parent = armature.edit_bones[info["parent"]]
            child_distances = [(source_world[child].translation - source_world[name].translation).length
                               for child, child_info in data["bones"].items()
                               if child_info["parent"] == name]
            positive = [distance for distance in child_distances if distance > 0.001]
            bone.length = min(positive) if positive else 1.0
    finally:
        bpy.ops.object.mode_set(mode="OBJECT")
    for name, info in data["bones"].items():
        if info["bind_matrix"] is not None:
            armature.bones[name]["ph_source_bind_matrix"] = info["bind_matrix"]
    for obj, mapped in assignments:
        world = obj.matrix_world.copy()
        obj.parent = rig
        obj.matrix_world = world
        for group in list(obj.vertex_groups):
            obj.vertex_groups.remove(group)
        groups = {name: obj.vertex_groups.new(name=name) for name in data["bones"]}
        for index, weights in enumerate(mapped):
            for bone, weight in weights.items():
                groups[bone].add([index], weight, "REPLACE")
        modifier = obj.modifiers.new("HP2_ORIGINAL_SOURCE_SKIN", "ARMATURE")
        modifier.object = rig
    rig["ph_reference_kind"] = "original_hp2_source_not_gothic"
    return {"status": "original_source_skin_recovered", "version": data["version"],
            "armature": rig.name, "bones": len(data["bones"]),
            "clusters": data["cluster_count"], "meshes": len(assignments),
            "weighted_vertices": vertex_count, "cloak_vertices": cloth_count,
            "source_head": source_head, "source_neck": source_neck,
            "animations_imported": False, "gothic_reference": False}
