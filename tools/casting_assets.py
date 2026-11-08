"""Validate optional, locally authored Gothic casting animation source assets.

These checks validate ASC data and its relationship to the converted mesh.
They do not replace compilation of MDS/MAN by Gothic or a gameplay test.
"""
from __future__ import annotations

import math
import re
from pathlib import Path

from tools.install import PatchError

CASTING_MDS = "PH_HARRY_CAST.MDS"
CHARGE_ASC = "PH_HARRY_CHARGE.ASC"
RELEASE_ASC = "PH_HARRY_RELEASE.ASC"
CASTING_ASSET_NAMES = (CASTING_MDS, CHARGE_ASC, RELEASE_ASC)
CASTING_MAX_BYTES = {CASTING_MDS: 64 * 1024,
                     CHARGE_ASC: 4 * 1024 * 1024,
                     RELEASE_ASC: 4 * 1024 * 1024}
FPS = 30
TICKS_PER_FRAME = 160
_POSITION_TOLERANCE = 0.01  # ASC units are centimetres.
_ROTATION_TOLERANCE = 0.00001
_BONE_NAMES = {
    "bip01", "bip01 pelvis", "bip01 spine", "bip01 spine1", "bip01 spine2",
    "bip01 neck", "bip01 head",
    *(f"bip01 {side} {part}" for side in ("l", "r")
      for part in ("clavicle", "upperarm", "forearm", "hand", "finger0",
                   "thigh", "calf", "foot", "toe0")),
}


_CASTING_ANIS = (
    '        ani ("T_PH_PREPARE" 1 "S_PH_CHARGE" 0.1 0.0 M. "PH_HARRY_CHARGE.ASC" F 1 12 FPS:30)\n'
    '        ani ("S_PH_CHARGE" 1 "S_PH_CHARGE" 0.0 0.0 M. "PH_HARRY_CHARGE.ASC" F 13 43 FPS:30)\n'
    '        ani ("T_PH_RELEASE" 1 "S_PH_RELEASE" 0.0 0.0 M. "PH_HARRY_RELEASE.ASC" F 1 10 FPS:30)\n'
    '        ani ("S_PH_RELEASE" 1 "S_PH_RELEASE" 0.0 0.0 M. "PH_HARRY_RELEASE.ASC" F 11 11 FPS:30)\n'
    '        ani ("T_PH_RECOVER" 1 "" 0.0 0.1 M. "PH_HARRY_RELEASE.ASC" F 12 26 FPS:30)\n'
    '        ani ("T_PH_CANCEL" 1 "" 0.0 0.1 M. "PH_HARRY_CHARGE.ASC" R 1 12 FPS:30)\n'
)

# DEV 0.13 bundles shipped this header without a skeleton declaration. ZenGin
# then compiled the overlay with no mesh ("MDL: in order to calculate ani-bbox,
# model should have a mesh at this stage, but has not") and attached the last
# loaded mesh (PH_HARRY) to the shared human prototype, so every human NPC
# rendered Harry's body under its own armor. Accepted only to be replaced.
LEGACY_CASTING_MDS = (
    'Model ("PH_HARRY_CAST")\n{\n    aniEnum\n    {\n' + _CASTING_ANIS + '    }\n}\n'
).encode("ascii")

# The second header declared the skeleton but still used new T_PH_* names.
# Gothic I refuses such an overlay entirely ("ani in overlay not present in
# base"), so the clips never played. Accepted only to be replaced.
UNIQUE_NAMES_CASTING_MDS = (
    'Model ("HuS")\n'
    '{\n'
    '    meshAndTree ("Hum_Body_Naked0.ASC" DONT_USE_MESH)\n'
    '\n'
    '    aniEnum\n'
    '    {\n' + _CASTING_ANIS + '    }\n'
    '}\n'
).encode("ascii")

# Spell animation letters (Gothic I spellFXAniLetters) of the six HUD spells:
# Windfist/Stormfist WND and Thunderball/BreathOfDeath FIB are invested;
# IceCube FRZ and Berzerk SLE shoot at once. Every name exists in the stock
# HUMANS.MDS, as an overlay requires. The overlay is applied only to PC_Hero,
# so no other human changes.
INVEST_LETTERS = ("WND", "FIB")
SHOOT_LETTERS = ("FRZ", "SLE")
_CHARGE = '"PH_HARRY_CHARGE.ASC"'
_RELEASE = '"PH_HARRY_RELEASE.ASC"'


def _ani(name: str, following: str, blend_in: str, blend_out: str, asc: str,
         first: int, last: int) -> str:
    return (f'        ani ("{name}" 1 "{following}" {blend_in} {blend_out} M. '
            f'{asc} F {first} {last} FPS:30)\n')


def stock_casting_anis(recover_next: str = "") -> str:
    # Like the stock *_2_STAND clips, recovery ends with no follow-up ("")
    # so the native magic AI sees the transition finish.
    lines = []
    for letter in INVEST_LETTERS:
        cast, shoot = f"S_{letter}CAST", f"S_{letter}SHOOT"
        lines += [
            _ani(f"T_MAGRUN_2_{letter}CAST", cast, "0.1", "0.0", _CHARGE, 1, 12),
            _ani(cast, cast, "0.0", "0.0", _CHARGE, 13, 43),
            _ani(f"T_{letter}CAST_2_{letter}SHOOT", shoot, "0.0", "0.0", _RELEASE, 1, 10),
            _ani(shoot, shoot, "0.0", "0.0", _RELEASE, 11, 11),
            _ani(f"T_{letter}SHOOT_2_STAND", recover_next, "0.0", "0.1", _RELEASE, 12, 26),
        ]
    for letter in SHOOT_LETTERS:
        shoot = f"S_{letter}SHOOT"
        lines += [
            _ani(f"T_MAGRUN_2_{letter}SHOOT", shoot, "0.1", "0.0", _RELEASE, 1, 10),
            _ani(shoot, shoot, "0.0", "0.0", _RELEASE, 11, 11),
            _ani(f"T_{letter}SHOOT_2_STAND", recover_next, "0.0", "0.1", _RELEASE, 12, 26),
        ]
    return "".join(lines)


def _stock_names_mds(recover_next: str) -> bytes:
    return (
        'Model ("HuS")\n'
        '{\n'
        '    meshAndTree ("Hum_Body_Naked0.ASC" DONT_USE_MESH)\n'
        '\n'
        '    aniEnum\n'
        '    {\n' + stock_casting_anis(recover_next) + '    }\n'
        '}\n'
    ).encode("ascii")


def canonical_casting_mds() -> bytes:
    """Harry's clips under the stock spell animation names.

    Like every stock human overlay (HUMANS_MAGE.MDS), it names the human
    model and declares the skeleton without a mesh, so the clips bind to the
    existing hierarchy and no mesh is added to any model.
    """
    return _stock_names_mds("")


# First stock-name build: recovery chained into S_MAGRUN, unlike the stock
# clips. Accepted only to be replaced.
MAGRUN_RECOVERY_CASTING_MDS = _stock_names_mds("S_MAGRUN")

SUPERSEDED_CASTING_MDS = (LEGACY_CASTING_MDS, UNIQUE_NAMES_CASTING_MDS,
                          MAGRUN_RECOVERY_CASTING_MDS)


def upgrade_casting_mds(directory: Path) -> bool:
    """Replace a superseded overlay in place; True when a file changed."""
    path = Path(directory) / "PH_HARRY_CAST.MDS"
    if path.is_file() and not path.is_symlink() and path.read_bytes() in SUPERSEDED_CASTING_MDS:
        path.write_bytes(canonical_casting_mds())
        return True
    return False


def _blocks(text: str, tag: str) -> list[str]:
    # Lazy import keeps visual_install free to use this validator too.
    from tools.visual_install import _asc_blocks
    return _asc_blocks(text, tag)


def _floats(values: list[str], context: str) -> tuple[float, ...]:
    try:
        result = tuple(float(value) for value in values)
    except ValueError as error:
        raise PatchError(f"Invalid animation number: {context}") from error
    if not all(math.isfinite(value) for value in result):
        raise PatchError(f"Non-finite animation number: {context}")
    return result


def _bone_nodes(data: bytes) -> dict[str, tuple[str, str]]:
    text = data.decode("latin-1")
    nodes = {}
    for body in _blocks(text, "HELPEROBJECT") + _blocks(text, "GEOMOBJECT"):
        match = re.search(r'\*NODE_NAME\s+"([^"\r\n]+)"', body, re.IGNORECASE)
        if not match or not match.group(1).casefold().startswith("bip01"):
            continue
        name = match.group(1).casefold()
        if name in nodes:
            raise PatchError(f"Duplicate animation skeleton node: {name}")
        parent = re.search(r'\*NODE_PARENT\s+"([^"\r\n]+)"', body, re.IGNORECASE)
        nodes[name] = (parent.group(1).casefold() if parent else "", body)
    if nodes.keys() != _BONE_NAMES or nodes["bip01"][0]:
        raise PatchError("Casting animations require the complete 25-bone Gothic skeleton")
    for name, (parent, _) in nodes.items():
        if name != "bip01" and parent not in nodes:
            raise PatchError(f"Animation skeleton parent is missing: {name}")
    for name, (parent, _) in nodes.items():
        seen = {name}
        while parent:
            if parent in seen:
                raise PatchError("Animation skeleton contains a parent cycle")
            seen.add(parent)
            parent = nodes[parent][0]
    return nodes


def animation_rig_signature(data: bytes) -> tuple:
    """Exact bone hierarchy/bind matrices, independent of mesh and comments."""
    result = []
    for name, (parent, body) in sorted(_bone_nodes(data).items()):
        transforms = _blocks(body, "NODE_TM")
        if len(transforms) != 1:
            raise PatchError(f"Animation skeleton needs one bind transform: {name}")
        rows = []
        for index in range(4):
            matches = re.findall(r"\*TM_ROW" + str(index) + r"\s+(\S+)\s+(\S+)\s+(\S+)",
                                 transforms[0], re.IGNORECASE)
            if len(matches) != 1:
                raise PatchError(f"Animation skeleton has an incomplete bind matrix: {name}")
            rows.extend(_floats(list(matches[0]), name))
        result.append((name, parent, tuple(rows)))
    return tuple(result)


def _quaternion(values: tuple[float, ...], context: str) -> tuple[float, ...]:
    x, y, z, angle = values
    length = math.hypot(x, y, z)
    if (abs(angle) > 100 * math.pi or length > 1.001 or
            (abs(angle) > 1e-8 and abs(length - 1.0) > 0.001)):
        raise PatchError(f"Invalid animation rotation axis/angle: {context}")
    if abs(angle) <= 1e-8:
        return (1.0, 0.0, 0.0, 0.0)
    factor = math.sin(angle * 0.5) / length
    return (math.cos(angle * 0.5), x * factor, y * factor, z * factor)


def _same_pose(first: dict, second: dict, context: str) -> None:
    for name in first:
        a_pos, a_rot = first[name]
        b_pos, b_rot = second[name]
        if max(abs(a - b) for a, b in zip(a_pos, b_pos)) > _POSITION_TOLERANCE:
            raise PatchError(f"Animation poses do not join: {context}: {name}")
        dot = abs(sum(a * b for a, b in zip(a_rot, b_rot)))
        if abs(1.0 - dot) > _ROTATION_TOLERANCE:
            raise PatchError(f"Animation rotations do not join: {context}: {name}")


def _read_animation(path: Path, model_nodes: dict, model_rig: tuple,
                    last_frame: int) -> dict[int, dict]:
    text = path.read_bytes().decode("latin-1")
    if not re.match(r"\s*\*3DSMAX_ASCIIEXPORT\s+\d+", text, re.IGNORECASE):
        raise PatchError(f"Animation ASC has no export header: {path.name}")
    scenes = _blocks(text, "SCENE")
    if len(scenes) != 1:
        raise PatchError(f"Animation ASC needs one scene: {path.name}")
    expected = {"FIRSTFRAME": 1, "LASTFRAME": last_frame,
                "FRAMESPEED": FPS, "TICKSPERFRAME": TICKS_PER_FRAME}
    for field, value in expected.items():
        values = re.findall(r"\*SCENE_" + field + r"\s+(\S+)", scenes[0], re.IGNORECASE)
        if len(values) != 1 or _floats(values, field) != (float(value),):
            raise PatchError(f"Unexpected animation scene {field}: {path.name}")
    nodes = _bone_nodes(text.encode("latin-1"))
    if ({name: node[0] for name, node in nodes.items()} !=
            {name: node[0] for name, node in model_nodes.items()}):
        raise PatchError(f"Animation skeleton differs from PH_HARRY.ASC: {path.name}")
    animation_rig = animation_rig_signature(text.encode("latin-1"))
    for expected, actual in zip(model_rig, animation_rig):
        if (expected[:2] != actual[:2] or
                max(abs(a - b) for a, b in zip(expected[2], actual[2])) > 0.0001):
            raise PatchError(f"Animation bind transform differs from PH_HARRY.ASC: {path.name}")
    if _blocks(text, "MESH") or _blocks(text, "MESH_SOFTSKIN"):
        raise PatchError(f"Casting animation must not contain another mesh: {path.name}")
    poses = {frame: {} for frame in range(1, last_frame + 1)}
    names = set()
    for track in _blocks(text, "TM_ANIMATION"):
        named = re.findall(r'\*NODE_NAME\s+"([^"\r\n]+)"', track, re.IGNORECASE)
        if len(named) != 1:
            raise PatchError(f"Animation track needs one node name: {path.name}")
        name = named[0].casefold()
        if name not in nodes or name in names:
            raise PatchError(f"Unknown/duplicate animation track: {name}")
        names.add(name)
        channels = []
        for tag, width in (("POS", 3), ("ROT", 4)):
            blocks = _blocks(track, "CONTROL_" + tag + "_TRACK")
            if len(blocks) != 1:
                raise PatchError(f"Animation needs one {tag} channel: {name}")
            samples = re.findall(r"\*CONTROL_" + tag + r"_SAMPLE\s+([^\r\n]+)", blocks[0], re.IGNORECASE)
            if len(samples) != last_frame:
                raise PatchError(f"Animation has incomplete {tag} samples: {name}")
            channel = []
            for frame, sample in enumerate(samples, 1):
                values = sample.split()
                if len(values) != width + 1:
                    raise PatchError(f"Animation has malformed {tag} sample: {name}")
                numeric = _floats(values, name)
                if numeric[0] != frame * TICKS_PER_FRAME:
                    raise PatchError(f"Animation sample times are not complete and ordered: {name}")
                if tag == "POS" and max(abs(value) for value in numeric[1:]) > 10000:
                    raise PatchError(f"Animation bone position is outside model space: {name}")
                channel.append(numeric[1:] if tag == "POS" else _quaternion(numeric[1:], name))
            channels.append(channel)
        scale = re.findall(r"\*CONTROL_SCALE_SAMPLE\s+([^\r\n]+)", track, re.IGNORECASE)
        for sample in scale:
            numeric = _floats(sample.split(), name)
            if len(numeric) != 8 or numeric[1:4] != (1.0, 1.0, 1.0) or numeric[7] != 0.0:
                raise PatchError(f"Casting animation contains scale changes: {name}")
        for frame in poses:
            poses[frame][name] = (channels[0][frame - 1], channels[1][frame - 1])
    if names != nodes.keys():
        raise PatchError(f"Casting animation must sample all 25 bones: {path.name}")
    initial_root = {"bip01": poses[1]["bip01"]}
    for frame in poses:
        _same_pose(initial_root, {"bip01": poses[frame]["bip01"]}, "root must stay fixed")
    root_tm = _blocks(model_nodes["bip01"][1], "NODE_TM")[0]
    axis = re.findall(r"\*TM_ROTAXIS\s+(\S+)\s+(\S+)\s+(\S+)", root_tm, re.IGNORECASE)
    angle = re.findall(r"\*TM_ROTANGLE\s+(\S+)", root_tm, re.IGNORECASE)
    if len(axis) != 1 or len(angle) != 1:
        raise PatchError("Animation root bind rotation is missing from PH_HARRY.ASC")
    root_position = next(rows[9:12] for name, _, rows in model_rig if name == "bip01")
    root_rotation = _quaternion(_floats(list(axis[0]) + angle, "root bind rotation"), "root bind rotation")
    _same_pose({"bip01": (root_position, root_rotation)}, initial_root,
               "animation root differs from mesh bind")
    return poses


def validate_optional_animation_assets(directory: Path) -> list[Path]:
    """Return the complete validated optional set, or [] when none is present."""
    files = [Path(directory) / name for name in CASTING_ASSET_NAMES]
    if not any(path.exists() or path.is_symlink() for path in files):
        return []
    for path in files:
        if path.is_symlink() or not path.is_file():
            raise PatchError(f"Incomplete casting animation set: {path.name}")
        if not 1 <= path.stat().st_size <= CASTING_MAX_BYTES[path.name]:
            raise PatchError(f"Unexpected casting animation size: {path.name}")
    if files[0].read_bytes() not in (canonical_casting_mds(), *SUPERSEDED_CASTING_MDS):
        raise PatchError("Casting overlay MDS does not match the supported animation states")
    model = Path(directory) / "PH_HARRY.ASC"
    if not model.is_file() or model.is_symlink():
        raise PatchError("Casting animations require a regular PH_HARRY.ASC")
    data = model.read_bytes()
    nodes = _bone_nodes(data)
    rig = animation_rig_signature(data)
    charge = _read_animation(files[1], nodes, rig, 43)
    release = _read_animation(files[2], nodes, rig, 26)
    _same_pose(charge[13], charge[43], "charge loop")
    _same_pose(charge[43], release[1], "charge to release")
    return files
