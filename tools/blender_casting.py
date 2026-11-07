"""Author and bake original wand-casting clips on a converted Gothic rig.

Run with Blender, not the normal Python interpreter:
blender --background --python-exit-code 1 --python tools/blender_casting.py -- INPUT.blend OUTPUT_DIR

Requires an installed Kerrax exporter. These are newly authored poses, not HP2
animation data. Exporting ASC does not compile or validate a Gothic animation.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import sys
import tempfile


def smooth(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def rotate_world(rig, name, axis, degrees):
    from mathutils import Matrix
    import bpy
    bone = rig.pose.bones[name]
    pivot = bone.matrix.translation.copy()
    bone.matrix = (Matrix.Translation(pivot)
                   @ Matrix.Rotation(math.radians(degrees), 4, axis)
                   @ Matrix.Translation(-pivot) @ bone.matrix)
    bpy.context.view_layer.update()


def aim_bone(bone, target):
    from mathutils import Matrix
    source = bone.tail - bone.head
    direction = target - bone.head
    if min(source.length, direction.length) < 1e-6:
        raise RuntimeError(f"Degenerate casting arm: {bone.name}")
    turn = source.rotation_difference(direction)
    bone.matrix = Matrix.LocRotScale(
        bone.head, turn @ bone.matrix.to_quaternion(), (1.0, 1.0, 1.0))


def solve_arm(rig, side, wrist, hand_direction, amount):
    """Two-bone anatomical IK, baked without runtime constraints or helpers."""
    import bpy
    from mathutils import Vector
    upper = rig.pose.bones[f"Bip01 {side} UpperArm"]
    forearm = rig.pose.bones[f"Bip01 {side} Forearm"]
    shoulder = upper.head.copy()
    first = (forearm.head - shoulder).length
    second = (rig.pose.bones[f"Bip01 {side} Hand"].head - forearm.head).length
    direction = wrist - shoulder
    distance = direction.length
    if not abs(first - second) + 1e-5 < distance < first + second - 1e-5:
        raise RuntimeError(f"Unreachable authored {side} wrist pose: {distance}")
    direction.normalize()
    along = (first * first - second * second + distance * distance) / (2 * distance)
    outward = -1.0 if side == "R" else 1.0
    pole = Vector((outward, 0.25, -0.35))
    pole = (pole - direction * pole.dot(direction)).normalized()
    rest_pole = forearm.head - shoulder
    rest_pole = (rest_pole - direction * rest_pole.dot(direction)).normalized()
    pole = rest_pole.lerp(pole, amount).normalized()
    elbow = shoulder + direction * along + pole * math.sqrt(max(0.0, first * first - along * along))
    aim_bone(upper, elbow)
    bpy.context.view_layer.update()
    aim_bone(forearm, wrist)
    bpy.context.view_layer.update()
    hand = rig.pose.bones[f"Bip01 {side} Hand"]
    aim_bone(hand, hand.head + hand_direction)
    bpy.context.view_layer.update()
    error = (hand.head - wrist).length
    if error > 0.0001:
        raise RuntimeError(f"Authored wrist IK error: {error}")


def set_pose(rig, amount, shot, breath=0.0):
    import bpy
    from mathutils import Vector
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    bpy.context.view_layer.update()
    rest = {side: rig.pose.bones[f"Bip01 {side} Hand"].head.copy() for side in ("R", "L")}
    rotate_world(rig, "Bip01 Spine1", "X", amount * (3.0 + 4.0 * shot))
    rotate_world(rig, "Bip01 Spine2", "Z", amount * (-4.0 + 9.0 * shot))
    rotate_world(rig, "Bip01 Neck", "Z", amount * (4.0 - 9.0 * shot))
    # The right wand hand rises above shoulder level and then points forward.
    charge = {"R": Vector((-0.29, -0.25, 1.79 + breath)),
              "L": Vector((0.25, -0.23, 1.63 + breath * 0.6))}
    release = {"R": Vector((-0.19, -0.52, 1.48)),
               "L": Vector((0.25, -0.25, 1.56))}
    for side in ("R", "L"):
        target = rest[side].lerp(charge[side].lerp(release[side], shot), amount)
        original = rig.pose.bones[f"Bip01 {side} Hand"].tail - rig.pose.bones[f"Bip01 {side} Hand"].head
        raised = Vector((0.02, -0.10, 0.20)) if side == "R" else Vector((0.02, -0.12, 0.07))
        directed = Vector((0.01, -0.23, 0.02)) if side == "R" else Vector((0.02, -0.15, 0.03))
        direction = original.lerp(raised.lerp(directed, shot), amount)
        # At the exact rest pose, retain the original unmodified skeleton.
        if amount > 1e-8:
            solve_arm(rig, side, target, direction, amount)
    bpy.context.view_layer.update()


def bake_action(rig, label, end):
    import bpy
    rig.animation_data_create()
    action = bpy.data.actions.new(label)
    action.use_fake_user = True
    rig.animation_data.action = action
    reference = None
    previous = {}
    samples = {}
    for frame in range(end + 1):
        bpy.context.scene.frame_set(frame)
        if frame == 0:
            amount, shot, breath = 0.0, 0.0, 0.0
        elif label == "PH_Wand_Charge":
            amount, shot = smooth((frame - 1) / 11), 0.0
            breath = 0.012 * math.sin(2 * math.pi * (frame - 13) / 30) if frame >= 13 else 0.0
        else:
            amount = 1.0 - smooth((frame - 12) / 14) if frame >= 12 else 1.0
            shot, breath = smooth((frame - 1) / 9) if frame <= 11 else 1.0, 0.0
        set_pose(rig, amount, shot, breath)
        positions = {bone.name: list(bone.head) for bone in rig.pose.bones}
        if reference is None:
            reference = positions
        for name, point in positions.items():
            if name == "Bip01" or any(word in name for word in ("Thigh", "Calf", "Foot", "Toe0")):
                if max(abs(a - b) for a, b in zip(point, reference[name])) > 1e-6:
                    raise RuntimeError(f"Casting clip moves a root/leg: {name}")
        samples[frame] = {bone.name: [list(row) for row in bone.matrix] for bone in rig.pose.bones}
        for bone in rig.pose.bones:
            bone.rotation_mode = "QUATERNION"
            quat = bone.rotation_quaternion.copy()
            if bone.name in previous and quat.dot(previous[bone.name]) < 0:
                quat.negate()
                bone.rotation_quaternion = quat
            previous[bone.name] = quat
            bone.keyframe_insert("location", frame=frame, group=bone.name)
            bone.keyframe_insert("rotation_quaternion", frame=frame, group=bone.name)
            bone.keyframe_insert("scale", frame=frame, group=bone.name)
    for curve in action.fcurves:
        for key in curve.keyframe_points:
            key.interpolation = "LINEAR"
    return action, samples


def export_animation(rig, path, action, end):
    """Ask the installed Kerrax saver for skeletal tracks in quiet mode."""
    import bpy
    from KrxImpExp import KrxAscExp as exporter
    from tools.blender_krx import stable_krx_rotations
    original = exporter.NewASCExporterDlgOutput

    def animation_output(inputs):
        output = original(inputs)
        output.SetExportAnimation(True)
        output.SetSelectedObjects([bone.name for bone in rig.data.bones])
        return output

    rig.animation_data.action = action
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = end
    # Frame zero contains the unchanged bind pose for NODE_TM; the selected
    # animation range starts at one and is sampled independently by Kerrax.
    bpy.context.scene.frame_set(0)
    temporary = path.with_name(path.stem + ".pending.ASC")
    exporter.NewASCExporterDlgOutput = animation_output
    try:
        with stable_krx_rotations():
            result = bpy.ops.export_scene.krxascexp(filepath=str(temporary), quiet=True)
        if "FINISHED" not in result or not temporary.is_file():
            raise RuntimeError("Kerrax did not export the skeletal animation")
        if temporary.read_bytes().count(b"*TM_ANIMATION") != 25:
            raise RuntimeError("Kerrax export did not produce all 25 bone tracks")
        temporary.replace(path)
    finally:
        exporter.NewASCExporterDlgOutput = original
        temporary.unlink(missing_ok=True)


def main():
    import bpy
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from tools.blender_visual_first import enable_krx_if_installed, export_gothic_asc
    from tools.casting_assets import (canonical_casting_mds, CASTING_ASSET_NAMES,
                                      validate_optional_animation_assets)
    from tools.visual_install import OUTFIT, TEXTURES, validate_converted_assets
    from tools.file_transaction import FileTransaction
    argv = sys.argv[sys.argv.index("--") + 1:]
    if len(argv) != 2:
        raise RuntimeError("Expected converted Harry .blend and private output directory")
    source, output = map(lambda arg: Path(arg).expanduser().resolve(), argv)
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(source))
    rigs = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    if len(rigs) != 1 or len(rigs[0].data.bones) != 25:
        raise RuntimeError("Expected exactly one real converted 25-bone Gothic armature")
    rig = rigs[0]
    if rig.get("ph_reference_kind") == "original_hp2_source_not_gothic":
        raise RuntimeError("Original HP2 source rig is not a Gothic skeleton")
    bpy.context.scene.render.fps = 30
    bpy.context.scene.render.fps_base = 1.0
    enable_krx_if_installed()
    with tempfile.TemporaryDirectory(prefix="parry-casting-") as directory:
        staged = Path(directory)
        rig.animation_data_clear()
        for bone in rig.pose.bones:
            bone.matrix_basis.identity()
        bpy.context.view_layer.update()
        bpy.ops.object.select_all(action="SELECT")
        # Re-export the matching bind matrices with the same numeric bridge as
        # the clips. Keep the mesh and animation rig one validated asset set.
        export_gothic_asc(bpy, staged / OUTFIT)
        for name in TEXTURES:
            images = [image for image in bpy.data.images
                      if Path(bpy.path.abspath(image.filepath)).name.upper() == name]
            if len(images) != 1:
                raise RuntimeError(f"Expected one converted packed texture: {name}")
            image = images[0]
            data = (bytes(image.packed_file.data) if image.packed_file else
                    Path(bpy.path.abspath(image.filepath)).read_bytes())
            (staged / name).write_bytes(data)
        charge, charge_samples = bake_action(rig, "PH_Wand_Charge", 43)
        release, release_samples = bake_action(rig, "PH_Wand_Release", 26)
        for name in charge_samples[13]:
            for a, b in zip(charge_samples[13][name], charge_samples[43][name]):
                if max(abs(x - y) for x, y in zip(a, b)) > 2e-6:
                    raise RuntimeError("Charge loop does not close")
            for a, b in zip(charge_samples[43][name], release_samples[1][name]):
                if max(abs(x - y) for x, y in zip(a, b)) > 2e-6:
                    raise RuntimeError("Release does not begin at charge pose")
        export_animation(rig, staged / "PH_HARRY_CHARGE.ASC", charge, 43)
        export_animation(rig, staged / "PH_HARRY_RELEASE.ASC", release, 26)
        (staged / "PH_HARRY_CAST.MDS").write_bytes(canonical_casting_mds())
        validate_converted_assets(staged)
        validate_optional_animation_assets(staged)
        rig.animation_data.action = charge
        bpy.context.scene.frame_start = 1
        bpy.context.scene.frame_end = 43
        bpy.context.scene.frame_set(13)
        for image in bpy.data.images:
            if image.source == "FILE" and not image.packed_file:
                image.pack()
        bpy.ops.wm.save_as_mainfile(filepath=str(staged / "ParryHotter_CASTING.blend"))
        report = {"authored_original_animation": True, "source_blend": str(source),
                  "gothic_bones": 25, "fps": 30, "charge_frames": 43,
                  "release_frames": 26, "loop_closed": True,
                  "release_starts_at_charge": True, "root_and_legs_fixed": True,
                  "gothic_engine_executed": False,
                  "samples": {"charge": charge_samples, "release": release_samples}}
        (staged / "casting-authoring.json").write_text(json.dumps(report, indent=2))
        names = (OUTFIT,) + TEXTURES + CASTING_ASSET_NAMES + (
            "ParryHotter_CASTING.blend", "casting-authoring.json")
        # A failed export or write preserves the previous reviewed asset set.
        with FileTransaction(output) as files:
            for name in names:
                files.copy_file(staged / name, output / name)
    print("PASS: authored and exported two real Gothic skeletal casting clips")


if __name__ == "__main__":
    main()
