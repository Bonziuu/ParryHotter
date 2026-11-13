// Gothic I 1.08k: immediate native magic preparation without weapon messages.
// Include after the SDK. The caller owns input/context and release requests.
#pragma once
#include "SpellTargeting.hpp"
#include <cstdint>

namespace ParryWand {
using namespace Gothic_I_Classic;

namespace Detail {
inline oCNpc* owner = nullptr; // Compared only; Forget never dereferences it.
inline oCMag_Book* ownedBook = nullptr;
inline int previousMode = 0;
inline int preparedId = -1;
inline const char* reason = "idle";
inline std::uint64_t epoch = 0;

inline void ClearOwnership() {
    owner = nullptr; ownedBook = nullptr; previousMode = 0; preparedId = -1;
}
inline void Abandon(std::uint64_t token, const char* failure) {
    if (epoch != token) return; // Preserve a replacement created by a callback.
    ClearOwnership();
    reason = failure;
}

struct ActorReference {
    oCNpc* actor;
    explicit ActorReference(oCNpc* value) : actor(value) { actor->AddRef(); }
    ~ActorReference() { actor->Release(); }
};
struct ItemReference {
    oCItem* item;
    explicit ItemReference(oCItem* value) : item(value) { item->AddRef(); }
    ~ItemReference() { item->Release(); }
};

inline bool Current(oCNpc* actor, oCMag_Book* book, zCWorld* world, std::uint64_t token) {
    return token == epoch && actor == player && actor->GetHomeWorld() == world &&
        actor->GetSpellBook() == book && actor->human_ai;
}

} // namespace Detail

inline void Forget() {
    ++Detail::epoch;
    Detail::ClearOwnership();
    Detail::reason = "forgotten";
}

inline const char* Reason() { return Detail::reason; }
inline bool Owns(oCNpc* actor) { return actor && actor == Detail::owner; }

inline bool Prepare(oCNpc* actor, int spellId, zCVob* target) {
    const auto token = ++Detail::epoch;
    Detail::reason = "context";
    if (!actor || actor != player || !actor->human_ai || spellId < 0) return false;
    auto* book = actor->GetSpellBook();
    auto* world = actor->GetHomeWorld();
    if (!book || !world) { Detail::reason = "no_spellbook"; return false; }
    Detail::ActorReference actorReference(actor);
    // Same inventory registration used by ReadySpell at 0x74AF2A..0x74AFAD,
    // without its final oCMsgWeapon(DRAWWEAPON,7,0). Do not invent a rune.
    const int count = actor->inventory2.GetNumItemsInCategory(3);
    oCItem* rune = nullptr;
    for (int i = 0; i < count; ++i) {
        auto* item = actor->inventory2.GetItem(3, i);
        if (item && item->spell == spellId) { rune = item; break; }
    }
    if (!rune) { Detail::reason = "missing_rune"; return false; }
    Detail::ItemReference runeReference(rune);
    if (!rune->HasFlag(ITM_FLAG_ACTIVE)) {
        actor->EquipItem(rune);
        if (!Detail::Current(actor, book, world, token)) {
            Detail::Abandon(token, "changed_during_equip"); return false;
        }
    }
    int index = -1;
    auto* registered = book->GetSpell(rune, index);
    if (!registered) {
        // An interrupted Equip can leave ACTIVE set without a book entry.
        book->Register(rune, 1);
        if (!Detail::Current(actor, book, world, token)) {
            Detail::Abandon(token, "changed_during_register"); return false;
        }
        registered = book->GetSpell(rune, index);
    }
    if (!registered || index < 0 || index >= book->GetNoOfSpells() ||
        registered->GetSpellID() != spellId) {
        Detail::reason = "rune_not_registered"; return false;
    }
    auto* found = book->GetSpell(index);
    if (!found || !found->GetEnabled()) { Detail::reason = "disabled"; return false; }

    if (Detail::owner != actor || Detail::ownedBook != book) {
        Detail::ClearOwnership();
        Detail::owner = actor;
        Detail::ownedBook = book;
        Detail::previousMode = actor->fmode >= 0 && actor->fmode <= 7 ? actor->fmode : 0;
    }
    // A repeated prepare of the same slot must get a fresh native spell.
    // Spell_Stop replaces the selected object, so reacquire it afterwards.
    if (book->GetSelectedSpell() == found && Detail::preparedId >= 0) {
        book->StopSelectedSpell();
        if (!Detail::Current(actor, book, world, token)) {
            Detail::Abandon(token, "changed_during_stop"); return false;
        }
    }
    Detail::preparedId = spellId;
    // SetWeaponMode2 only supports 0 -> mode and mode -> 0. The native method
    // updates overlays/fight/walk animations without DRAW/REMOVE messages.
    if (actor->fmode != 0 && actor->fmode != 7) actor->SetWeaponMode2(0);
    if (!Detail::Current(actor, book, world, token)) {
        Detail::Abandon(token, "changed_during_mode"); return false;
    }
    book->SetFrontSpell(index);
    if (!Detail::Current(actor, book, world, token)) {
        Detail::Abandon(token, "changed_during_select"); return false;
    }
    if (actor->fmode != 7) actor->SetWeaponMode2(7);
    if (!Detail::Current(actor, book, world, token) || actor->fmode != 7) {
        Detail::Abandon(token, "magic_mode_failed"); return false;
    }
    auto* prepared = book->GetSelectedSpell();
    if (!prepared || prepared->GetSpellID() != spellId) {
        Detail::reason = "selected_spell_changed"; return false;
    }
    // SetFrontSpell before fmode=7 only selects an index. Setup must still
    // initialize caster/target/effect. Geometry/type checks stay native.
    {
        ParryTargeting::Scope targeting(prepared);
        book->Spell_Setup(actor, target);
    }
    if (!Detail::Current(actor, book, world, token) || book->GetSelectedSpell() != prepared) {
        Detail::Abandon(token, "changed_during_setup"); return false;
    }
    Detail::preparedId = spellId;
    if (prepared->spellCasterNpc != actor || !prepared->GetEnabled()) {
        Detail::reason = "setup_incomplete";
        return false;
    }
    Detail::reason = "ready";
    return true;
}

inline void Cancel(oCNpc* actor) {
    const auto token = ++Detail::epoch;
    if (!actor || actor != player || actor != Detail::owner) { Forget(); return; }
    auto* book = actor->GetSpellBook();
    auto* world = actor->GetHomeWorld();
    const int restore = Detail::previousMode;
    const int id = Detail::preparedId;
    const bool sameBook = book && book == Detail::ownedBook;
    Detail::ClearOwnership(); // Native Stop may run a replacement Prepare.
    if (!sameBook || !actor->human_ai) return;
    Detail::ActorReference actorReference(actor);
    auto* spell = book->GetSelectedSpell();
    if (spell && spell->GetSpellID() != id) return; // A foreign selection owns this mode now.
    if (spell && spell->GetSpellID() == id) {
        book->StopSelectedSpell();
        if (!Detail::Current(actor, book, world, token)) return;
    }
    // Do not overwrite a weapon mode chosen by an unrelated engine callback.
    if (actor->fmode == 7) {
        actor->SetWeaponMode2(0);
        if (!Detail::Current(actor, book, world, token)) return;
        if (restore != 0 && restore != 7 && actor->fmode == 0)
            actor->SetWeaponMode2(restore);
    }
    if (token != Detail::epoch) return;
    Detail::reason = "cancelled";
}
} // namespace ParryWand
