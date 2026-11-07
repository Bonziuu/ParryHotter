"""Shared names for the ten-slot installer, HUD assets and diagnostics."""

RUNE_NAMES = (
    "Flipendo", "Levicorpus", "Rictusempra", "Expelliarmus", "Bumbardo",
    "Avada",
)
SLOT_COUNT = 10
SPELL_COUNT = len(RUNE_NAMES)
HUD_BUILD_MARKER = b"PH_BUILD_013_TEN_SLOTS_PROJECTILE_INPUT_CAST_ANIM"
SPRITE_NAMES = tuple(name.upper() for name in RUNE_NAMES)
REQUIRED_TEXTURES = tuple(
    name
    for spell in SPRITE_NAMES
    for name in (f"PH_{spell}.TGA", f"PH_TARGET_{spell}.TGA")
) + ("PH_SELECTED.TGA", "PH_RETICLE.TGA", "PH_EMPTY.TGA")
REQUIRED_COMPILED_SYMBOLS = (
    b"ph_spell_count", b"ph_slot_count", b"ph_selected_spell", b"ph_union_loaded",
) + tuple(f"ph_rune_{name}".encode("ascii").lower() for name in RUNE_NAMES)
