#!/usr/bin/env python3
"""Inject Parry Hotter v0.1 into a locally installed Gothic 1 ModKit.

Does not distribute Gothic game scripts. Works on a COPY of your game folder.
All edits are idempotent and original files get a .parryhotter.bak backup.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
if __package__:
    from .spell_catalog import RUNE_NAMES
else:
    from spell_catalog import RUNE_NAMES


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "mod" / "Magic" / "ParryHotter.d"
HERO_MARK = "// PARRY_HOTTER_HERO_BEGIN"
HERO_END = "// PARRY_HOTTER_HERO_END"


class PatchError(RuntimeError):
    """Unsupported or incomplete ModKit source tree."""


def exactly_one(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, lambda _m: replacement, text, count=1, flags=re.IGNORECASE | re.MULTILINE)
    if count != 1:
        raise PatchError(f"Could not find expected anchor for {label}")
    return updated


def patch_source(text: str) -> str:
    if re.search(r"(?im)^\s*Magic\\ParryHotter\.d\s*$", text):
        return text

    newline = "\r\n" if "\r\n" in text else "\n"
    match = re.search(r"(?im)^[ \t]*Magic\\B_AssessMagic\.d[ \t]*\r?$", text)
    if match is None:
        raise PatchError("Gothic.src: Magic\\B_AssessMagic.d not found")
    return text[:match.start()] + "Magic\\ParryHotter.d" + newline + text[match.start():]


def patch_hero_temporary_robe(text: str) -> str:
    """Use stock Gothic I mage robes until a validated original HP2 mesh exists.

    Changes only the player's visual armor; keeps original Gothic animations
    and hitboxes. No proprietary graphics or converted model needed.
    """
    found = re.search(r"(?is)\bINSTANCE\s+PC_Hero\s*\(\s*NPC_DEFAULT\s*\)\s*\{", text)
    if found is None:
        raise PatchError("PC_Hero.d: PC_Hero instance missing for appearance")
    next_hero = re.search(r"(?im)^\s*INSTANCE\s+PC_Hero_\w+\s*\(", text[found.end():])
    end = found.end() + next_hero.start() if next_hero else len(text)
    section = text[found.start():end]
    pattern = r"(?is)(Mdl_SetVisualBody\s*\(\s*self\s*,[^;]*?,\s*)(-1|KDF_ARMOR_L|PH_ARMOR_HARRY)(\s*\)\s*;)"
    matches = list(re.finditer(pattern, section))
    # Some test fixtures and customized ModKits omit the visual call.
    # Never block spell installation just because a visual fallback is absent.
    if not matches:
        return text
    if len(matches) != 1:
        raise PatchError("PC_Hero.d: multiple Mdl_SetVisualBody calls in player instance")
    appearance = matches[0]
    if appearance.group(2).upper() in ("KDF_ARMOR_L", "PH_ARMOR_HARRY"):
        return text
    replacement = appearance.group(1) + "KDF_ARMOR_L" + appearance.group(3)
    section = section[:appearance.start()] + replacement + section[appearance.end():]
    return text[:found.start()] + section + text[end:]


def patch_hero(text: str) -> str:
    # Existing v0.1 installations must be upgraded, too. The mage overlay
    # supplies the human spellcasting animation set used by Gothic 1 mages.
    mage_overlay = 'Mdl_ApplyOverlayMds(self, "Humans_Mage.mds");'
    if HERO_MARK in text:
        if text.count(HERO_MARK) != 1 or text.count(HERO_END) != 1:
            raise PatchError("PC_Hero.d: invalid Parry Hotter upgrade markers")
        begin, end = text.index(HERO_MARK), text.index(HERO_END)
        if end <= begin:
            raise PatchError("PC_Hero.d: invalid Parry Hotter upgrade block")
        newline = "\r\n" if "\r\n" in text else "\n"
        block = text[begin:end]
        if mage_overlay not in block:
            block = block.replace(HERO_MARK, HERO_MARK + newline + "    " + mage_overlay, 1)
        # Add only missing runes, preserving local additions inside the block.
        for rune in RUNE_NAMES:
            if not re.search(r"\bCreateInvItem\s*\(\s*self\s*,\s*PH_Rune_" + rune + r"\s*\)", block, re.IGNORECASE):
                block = block.rstrip() + newline + f"    CreateInvItem(self, PH_Rune_{rune});" + newline + "    "
        text = text[:begin] + block + text[end:]
        return patch_hero_temporary_robe(text)

    found = re.search(r"(?is)\bINSTANCE\s+PC_Hero\s*\(\s*NPC_DEFAULT\s*\)\s*\{", text)
    if found is None:
        raise PatchError("PC_Hero.d: INSTANCE PC_Hero(NPC_DEFAULT) missing")

    next_hero = re.search(r"(?im)^\s*INSTANCE\s+PC_Hero_\w+\s*\(", text[found.end():])
    end = found.end() + next_hero.start() if next_hero else len(text)
    section = text[found.start():end]

    section = exactly_one(
        section, r'(?im)^(\s*name\s*=\s*)"[^"]*"(\s*;)',
        '    name = "Parry Hotter";', "hero name"
    )
    section = exactly_one(
        section, r'(?im)^\s*attribute\s*\[\s*ATR_MANA_MAX\s*\]\s*=\s*\d+\s*;',
        '    attribute[ATR_MANA_MAX] = 200;', "hero maximum mana"
    )
    section = exactly_one(
        section, r'(?im)^\s*attribute\s*\[\s*ATR_MANA\s*\]\s*=\s*\d+\s*;',
        '    attribute[ATR_MANA] = 200;', "hero mana"
    )

    closing = section.rfind("};")
    if closing == -1:
        raise PatchError("PC_Hero.d: hero instance closing delimiter missing")

    newline = "\r\n" if "\r\n" in text else "\n"
    addon = [
        f"    {HERO_MARK}",
        "    Npc_SetTalentSkill(self, NPC_TALENT_MAGE, 6);",
        '    Mdl_ApplyOverlayMds(self, "Humans_Mage.mds");',
        *(f"    CreateInvItem(self, PH_Rune_{rune});" for rune in RUNE_NAMES),
        f"    {HERO_END}",
        "",
    ]
    section = section[:closing] + newline.join(addon) + section[closing:]
    patched = text[:found.start()] + section + text[end:]
    return patch_hero_temporary_robe(patched)


def add_hit_hook(
    text: str, function: str, invocation: str, old_invocation: str | None = None
) -> str:
    """Patch only one hit handler and upgrade legacy unconditional callbacks.

    Re-running a newer installer must REPLACE the old callback instead of
    stacking new and old hooks (which would hit an NPC twice).
    """
    anchor = re.compile(
        r"(?im)^\s*func\s+void\s+" + re.escape(function)
        + r"\s*\(\s*\)\s*\{"
    )
    match = anchor.search(text)
    if match is None:
        raise PatchError(f"B_AssessMagic.d: {function} not found")
    newline = "\r\n" if "\r\n" in text else "\n"
    after = text[match.end():]
    if re.match(r"\s*" + re.escape(invocation), after):
        return text
    if old_invocation:
        previous = re.match(
            r"(?P<space>\s*)" + re.escape(old_invocation), after
        )
        if previous:
            return (
                text[:match.end()] + previous.group("space") + invocation
                + after[previous.end():]
            )
        # Legacy code further down in the handler would cause a double hit.
        handler_end = re.search(r"(?im)^\s*func\s+void\s+", after)
        body = after[:handler_end.start()] if handler_end else after
        if old_invocation in body:
            raise PatchError(
                f"{function} contains an unexpected older callback; "
                "refusing to duplicate it"
            )
    return text[:match.end()] + newline + f"    {invocation}" + after


def patch_assess_magic(text: str) -> str:
    text = add_hit_hook(
        text, "B_AssessMagic_IceCube",
        "if (PH_IsSelectedSpell(2) && C_NpcIsHuman(self)) { PH_OnLevicorpusHit(); return; };",
        "if (Npc_IsPlayer(other) && C_NpcIsHuman(self)) { PH_OnLevicorpusHit(); return; };",
    )
    text = add_hit_hook(
        text, "B_AssessMagic_ThunderBall",
        "if (PH_IsSelectedSpell(4)) { PH_OnExpelliarmusHit(); };",
        "PH_OnExpelliarmusHit();",
    )
    text = add_hit_hook(
        text, "B_AssessMagic_Breathofdeath",
        "if (PH_IsSelectedSpell(6)) { PH_OnAvadaHit(); };",
        "PH_OnAvadaHit();",
    )
    text = add_hit_hook(
        text, "B_AssessMagic_Berzerk",
        "if (PH_IsSelectedSpell(3) && C_NpcIsHuman(self)) { PH_OnRictusempraHit(); return; };",
        "if (Npc_IsPlayer(other) && C_NpcIsHuman(self)) { PH_OnRictusempraHit(); return; };",
    )
    text = add_hit_hook(
        text, "B_AssessMagic_Stormfist",
        "if (PH_IsSelectedSpell(5)) { PH_OnBumbardoHit(); };",
    )
    return text


def patch_spell_params(text: str, original_backup: str | None = None) -> str:
    """Avoid changing global Gothic spell balance for all NPCs.

    Older Parry Hotter installers forced BreathOfDeath damage down to 1,
    affecting vanilla NPC casters too. Restore the backed-up original only
    for a proven legacy 1 -> original transition. Otherwise do nothing.
    """
    pattern = (
        r"(?im)^(\s*const\s+int\s+SPL_DAMAGE_BREATHOFDEATH\s*=\s*)"
        r"(-?\d+)(\s*;[^\r\n]*)"
    )
    m = re.search(pattern, text)
    if not m:
        raise PatchError("Spells_Params.d: SPL_DAMAGE_BREATHOFDEATH not found")
    if m.group(2) != "1" or original_backup is None:
        return text
    original = re.search(pattern, original_backup)
    if not original or original.group(2) == "1":
        return text
    return (
        text[:m.start()] + m.group(1) + original.group(2) + m.group(3)
        + text[m.end():]
    )


def install(content_dir: Path, dry_run: bool = False) -> list[Path]:
    content_dir = content_dir.expanduser().resolve()
    paths = {
        content_dir / "Gothic.src": patch_source,
        content_dir / "Story" / "NPC" / "PC_Hero.d": patch_hero,
        content_dir / "Magic" / "B_AssessMagic.d": patch_assess_magic,
        content_dir / "Magic" / "Spells_Params.d": patch_spell_params,
    }
    for path in paths:
        if not path.is_file():
            raise PatchError(f"Required Gothic 1 ModKit file missing: {path}")
    if not SCRIPT.is_file():
        raise PatchError(f"Mod script missing: {SCRIPT}")

    # Validate *all* patches before touching anything.
    updates: dict[Path, bytes] = {}
    for path, patch in paths.items():
        original = path.read_bytes()
        # Daedalus from the classic ModKit is typically ANSI, not UTF-8.
        if patch is patch_spell_params:
            backup = path.with_name(path.name + ".parryhotter.bak")
            backup_text = (
                backup.read_bytes().decode("latin-1")
                if backup.is_file() else None
            )
            updated = patch(original.decode("latin-1"), backup_text)
        else:
            updated = patch(original.decode("latin-1"))
        new_content = updated.encode("latin-1")
        if new_content != original:
            updates[path] = new_content

    target = content_dir / "Magic" / "ParryHotter.d"
    script_bytes = SCRIPT.read_bytes()
    if not target.exists() or target.read_bytes() != script_bytes:
        updates[target] = script_bytes

    if not dry_run:
        for path, new_content in updates.items():
            if path.exists():
                backup = path.with_name(path.name + ".parryhotter.bak")
                if not backup.exists():
                    backup.write_bytes(path.read_bytes())
            path.parent.mkdir(parents=True, exist_ok=True)
            temp = path.with_name(path.name + ".parryhotter.tmp")
            temp.write_bytes(new_content)
            temp.replace(path)
    return list(updates)


def main() -> int:
    parser = argparse.ArgumentParser(description="Patch the Gothic 1 ModKit with Parry Hotter")
    parser.add_argument("--content", required=True, type=Path, help="Path to Gothic/_work/Data/Scripts/Content")
    parser.add_argument("--dry-run", action="store_true", help="Validate the source tree without changing it")
    args = parser.parse_args()
    try:
        changes = install(args.content, dry_run=args.dry_run)
    except PatchError as exc:
        parser.exit(2, f"ERROR: {exc}\n")

    if not changes:
        print("Parry Hotter already installed; no changes necessary.")
    else:
        action = "Would change" if args.dry_run else "Updated"
        for path in changes:
            print(f"{action}: {path}")
        if not args.dry_run:
            print("Now launch GothicStarter_mod and reparse the scripts; start a new game.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
