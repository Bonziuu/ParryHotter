#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace Gothic_I_Classic {
struct zCVob { virtual ~zCVob() = default; };
struct zCWorld {};
struct oCNpc;
inline oCNpc* player = nullptr;
inline std::vector<std::string> calls;
constexpr int ITM_FLAG_ACTIVE = 1 << 30;
struct oCItem {
    int spell = 0, flags = 0, refs = 1;
    void AddRef() { ++refs; }
    void Release() { --refs; }
    bool HasFlag(int flag) const { return (flags & flag) != 0; }
};
struct oCSpell {
    int id, refs = 1, manaInvested = 0, spellStatus = 1;
    int targetCollectRange = 500, targetCollectAzi = 30, targetCollectElev = 20;
    oCNpc* spellCasterNpc = nullptr;
    zCVob* target = nullptr;
    bool enabled = true;
    explicit oCSpell(int value) : id(value) {}
    int GetSpellID() const { return id; }
    bool GetEnabled() const { return enabled; }
    void AddRef() { ++refs; }
    void Release() { --refs; }
};
struct oCAIHuman {};
struct oCMag_Book {
    std::vector<std::unique_ptr<oCSpell>> spells;
    std::vector<std::unique_ptr<oCSpell>> retired;
    int selected = 0, stops = 0, setups = 0, registers = 0;
    bool interruptSetup = false;
    void (*setupCallback)() = nullptr;
    void (*stopCallback)() = nullptr;
    int GetNoOfSpells() const { return static_cast<int>(spells.size()); }
    oCSpell* GetSpell(int index) {
        assert(index >= 0 && index < GetNoOfSpells());
        return spells[index].get();
    }
    oCSpell* GetSelectedSpell() {
        return selected >= 0 && selected < GetNoOfSpells() ? spells[selected].get() : nullptr;
    }
    int Register(oCItem* item, int active) {
        calls.push_back("register"); ++registers;
        assert(active == 1 && item->HasFlag(ITM_FLAG_ACTIVE));
        for (const auto& spell : spells) if (spell->id == item->spell) return 1;
        spells.push_back(std::make_unique<oCSpell>(item->spell));
        return 1;
    }
    oCSpell* GetSpell(oCItem* item, int& index) {
        for (int i = 0; i < GetNoOfSpells(); ++i)
            if (spells[i]->id == item->spell) { index = i; return spells[i].get(); }
        return nullptr;
    }
    void StopSelectedSpell() {
        calls.push_back("stop"); ++stops;
        auto* old = GetSelectedSpell();
        if (!old) return;
        const int id = old->id;
        old->manaInvested = -1;
        retired.push_back(std::move(spells[selected]));
        spells[selected] = std::make_unique<oCSpell>(id);
        if (stopCallback) { auto callback = stopCallback; stopCallback = nullptr; callback(); }
    }
    void SetFrontSpell(int index);
    void Spell_Setup(oCNpc* actor, zCVob* target) {
        calls.push_back("setup"); ++setups;
        auto* spell = GetSelectedSpell();
        assert(spell && spell->targetCollectRange == 1900 && spell->targetCollectAzi == 360);
        spell->spellCasterNpc = actor;
        spell->target = target;
        spell->manaInvested = 0;
        spell->spellStatus = 1;
        if (interruptSetup) { player = nullptr; interruptSetup = false; }
        if (setupCallback) { auto callback = setupCallback; setupCallback = nullptr; callback(); }
    }
};
struct oCNpcInventory {
    std::vector<oCItem*> items;
    int GetNumItemsInCategory(int category) { assert(category == 3); return static_cast<int>(items.size()); }
    oCItem* GetItem(int category, int index) { assert(category == 3); return items.at(index); }
};
struct oCNpc : zCVob {
    oCAIHuman* human_ai = nullptr;
    oCMag_Book book;
    oCNpcInventory inventory2;
    zCWorld world;
    int fmode = 0, mana = 200, refs = 1;
    bool interruptEquip = false;
    void (*equipCallback)() = nullptr;
    oCMag_Book* GetSpellBook() { return &book; }
    zCWorld* GetHomeWorld() { return &world; }
    void AddRef() { ++refs; }
    void Release() { --refs; }
    void EquipItem(oCItem* item) {
        calls.push_back("equip"); item->flags |= ITM_FLAG_ACTIVE;
        if (interruptEquip) { player = nullptr; interruptEquip = false; }
        if (equipCallback) { auto callback = equipCallback; equipCallback = nullptr; callback(); }
    }
    void SetWeaponMode2(int mode) {
        calls.push_back("mode" + std::to_string(mode));
        // Native supports only same mode, 0->mode and mode->0.
        assert(fmode == 0 || mode == 0 || fmode == mode);
        fmode = mode;
    }
};
inline void oCMag_Book::SetFrontSpell(int index) {
    calls.push_back("select" + std::to_string(index));
    assert(index >= 0 && index < GetNoOfSpells());
    if (index != selected && player && player->fmode == 7) StopSelectedSpell();
    selected = index;
}
} // namespace Gothic_I_Classic

#include "../Plugin/NativeWand.hpp"
using namespace Gothic_I_Classic;

int main() {
    oCAIHuman ai;
    oCNpc actor;
    actor.human_ai = &ai;
    player = &actor;
    oCItem flip{9}, ice{43}, invalid{99};
    actor.inventory2.items = {&invalid, &flip, &ice};
    zCVob target;
    assert(ParryWand::Prepare(&actor, 9, &target));
    assert(actor.fmode == 7 && actor.book.GetSelectedSpell()->spellCasterNpc == &actor);
    assert(actor.book.GetSelectedSpell()->target == &target);
    assert(actor.book.GetSelectedSpell()->targetCollectRange == 500);
    assert(actor.mana == 200 && actor.refs == 1 && flip.refs == 1);
    assert((calls == std::vector<std::string>{"equip", "register", "select0", "mode7", "setup"}));

    // Repeated same-slot preparation reacquires the object replaced by Stop.
    auto* old = actor.book.GetSelectedSpell();
    old->manaInvested = 3;
    calls.clear();
    assert(ParryWand::Prepare(&actor, 9, &target));
    assert(actor.book.GetSelectedSpell() != old && old->manaInvested == -1);
    assert(actor.book.GetSelectedSpell()->manaInvested == 0 && actor.mana == 200);
    assert((calls == std::vector<std::string>{"stop", "select0", "setup"}));

    // Selecting another spell while already ready does not cycle weapon mode.
    calls.clear();
    assert(ParryWand::Prepare(&actor, 43, &target));
    assert(actor.fmode == 7 && actor.book.GetSelectedSpell()->id == 43);
    for (const auto& call : calls) assert(call != "mode0" && call != "mode7");
    assert(actor.book.GetSelectedSpell()->spellCasterNpc == &actor);
    calls.clear();
    ParryWand::Cancel(&actor);
    assert(actor.fmode == 0 && actor.mana == 200 && !ParryWand::Owns(&actor));
    assert((calls == std::vector<std::string>{"stop", "mode0"}));
    calls.clear();
    ParryWand::Cancel(&actor);
    assert(calls.empty());

    // A genuine inventory rune is required; no fabricated spell backend.
    calls.clear();
    assert(!ParryWand::Prepare(&actor, 47, &target));
    assert(std::string(ParryWand::Reason()) == "missing_rune" && calls.empty());
    assert(actor.fmode == 0 && !ParryWand::Owns(&actor));

    // Prior native weapon mode is restored through native instant setters.
    actor.fmode = 3;
    calls.clear();
    assert(ParryWand::Prepare(&actor, 9, nullptr));
    assert(actor.fmode == 7);
    assert((calls == std::vector<std::string>{"mode0", "select0", "mode7", "setup"}));
    calls.clear();
    ParryWand::Cancel(&actor);
    assert(actor.fmode == 3);
    assert((calls == std::vector<std::string>{"stop", "mode0", "mode3"}));

    // Foreign mode changes are not overwritten on cancellation.
    actor.fmode = 0;
    assert(ParryWand::Prepare(&actor, 9, &target));
    actor.fmode = 4;
    calls.clear();
    ParryWand::Cancel(&actor);
    assert(actor.fmode == 4 && calls == std::vector<std::string>{"stop"});

    // Lifecycle callback during setup or equip stops further dereferences.
    actor.fmode = 0;
    actor.book.interruptSetup = true;
    assert(!ParryWand::Prepare(&actor, 9, &target));
    assert(!ParryWand::Owns(&actor) && actor.refs == 1);
    player = &actor;
    oCItem bomb{47};
    actor.inventory2.items.push_back(&bomb);
    actor.interruptEquip = true;
    calls.clear();
    assert(!ParryWand::Prepare(&actor, 47, &target));
    assert((calls == std::vector<std::string>{"equip"}));
    assert(actor.refs == 1 && bomb.refs == 1);
    player = &actor;
    calls.clear();
    ParryWand::Forget();
    assert(calls.empty());

    // Same-world reentrancy also invalidates the old operation. An inner
    // prepare remains final instead of being cleared by the outer failure.
    actor.book.setupCallback = +[] { ParryWand::Forget(); };
    assert(!ParryWand::Prepare(&actor, 9, &target));
    assert(!ParryWand::Owns(&actor));
    actor.book.setupCallback = +[] {
        assert(ParryWand::Prepare(player, 43, nullptr));
    };
    assert(!ParryWand::Prepare(&actor, 9, &target));
    assert(ParryWand::Owns(&actor) && actor.book.GetSelectedSpell()->id == 43);
    assert(std::string(ParryWand::Reason()) == "ready");
    actor.book.stopCallback = +[] {
        assert(ParryWand::Prepare(player, 9, nullptr));
    };
    ParryWand::Cancel(&actor);
    assert(ParryWand::Owns(&actor) && actor.book.GetSelectedSpell()->id == 9);
    assert(actor.fmode == 7);
    ParryWand::Cancel(&actor);
    actor.fmode = 0;
    oCItem thunder{42};
    actor.inventory2.items.push_back(&thunder);
    actor.equipCallback = +[] { ParryWand::Forget(); };
    calls.clear();
    assert(!ParryWand::Prepare(&actor, 42, &target));
    assert((calls == std::vector<std::string>{"equip"}));
    assert(actor.fmode == 0 && !ParryWand::Owns(&actor));
    assert(thunder.refs == 1 && actor.refs == 1);

    // All six backends can prepare/cancel repeatedly without spending mana.
    oCItem berzerk{53}, death{49};
    actor.inventory2.items.push_back(&berzerk);
    actor.inventory2.items.push_back(&death);
    for (int id : {9, 43, 53, 42, 47, 49}) {
        assert(ParryWand::Prepare(&actor, id, &target));
        assert(actor.book.GetSelectedSpell()->id == id);
        assert(actor.book.GetSelectedSpell()->spellCasterNpc == &actor);
        ParryWand::Cancel(&actor);
        assert(actor.fmode == 0 && actor.mana == 200);
    }
    std::cout << "Native wand: immediate selection/setup, repeat/slot change, cancel and lifecycle OK\n";
}
