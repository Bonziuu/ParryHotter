// Targeting adapter simulation, not an SDK ABI or ZenGin execution test.
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <type_traits>

namespace Gothic_I_Classic {
struct Target {
    float distance = 0.0f;
    float azimuth = 0.0f;
    float elevation = 0.0f;
    bool lineOfSight = true;
    bool dead = false;
    int type = 4; // NPC; an item has type 2.
};

struct oCSpell {
    int targetCollectRange = 800;
    int targetCollectAzi = 45;
    int targetCollectElev = 30;
    int targetCollectType = 4;
    int targetCollectAlgo = 2;
    int references = 1;
    int retained = 0;
    int released = 0;
    std::function<void()> onRelease;

    void AddRef() { assert(references > 0); ++references; ++retained; }
    void Release() {
        assert(references > 0);
        if (onRelease) onRelease();
        --references;
        ++released;
    }

    bool IsValidTarget(const Target& target) const {
        return target.lineOfSight && !target.dead &&
               (target.type & targetCollectType) != 0 &&
               std::fabs(target.azimuth) < targetCollectAzi &&
               std::fabs(target.elevation) < targetCollectElev &&
               target.distance <= targetCollectRange;
    }
};
} // namespace Gothic_I_Classic

#include "../Plugin/SpellTargeting.hpp"

using Gothic_I_Classic::oCSpell;
using Gothic_I_Classic::Target;
using ParryTargeting::Scope;

static_assert(!std::is_copy_constructible<Scope>::value);
static_assert(!std::is_copy_assignable<Scope>::value);
static_assert(!std::is_move_constructible<Scope>::value);
static_assert(!std::is_move_assignable<Scope>::value);

int main() {
    { Scope empty(nullptr); }

    oCSpell spell;
    const Target behind{900.0f, 180.0f, 45.0f};
    assert(!spell.IsValidTarget(behind));
    bool restoredBeforeRelease = false;
    spell.onRelease = [&] {
        restoredBeforeRelease = spell.targetCollectRange == 800 &&
            spell.targetCollectAzi == 45 && spell.targetCollectElev == 30;
    };
    {
        Scope targetFromCamera(&spell);
        assert(spell.references == 2);
        assert(spell.IsValidTarget(behind));
        auto target = behind;
        target.distance = 1900.0f;
        assert(spell.IsValidTarget(target));
        target.distance = 1900.1f;
        assert(!spell.IsValidTarget(target));
        target = behind;
        target.lineOfSight = false;
        assert(!spell.IsValidTarget(target));
        target = behind;
        target.dead = true;
        assert(!spell.IsValidTarget(target));
        target = behind;
        target.type = 2;
        assert(!spell.IsValidTarget(target));
        assert(spell.targetCollectType == 4 && spell.targetCollectAlgo == 2);
    }
    assert(restoredBeforeRelease && spell.references == 1);
    assert(spell.retained == 1 && spell.released == 1);
    assert(!spell.IsValidTarget(behind));

    // An inner native call cannot prematurely restore the outer call's
    // camera limits, including when the spell's normal range was smaller.
    spell.onRelease = {};
    {
        Scope outer(&spell);
        {
            Scope inner(&spell);
            assert(spell.references == 3 && spell.IsValidTarget(behind));
        }
        assert(spell.references == 2 && spell.IsValidTarget(behind));
        assert(spell.targetCollectRange == ParryTargeting::kRangeCm);
    }
    assert(spell.references == 1 && spell.targetCollectRange == 800);
    assert(spell.targetCollectAzi == 45 && spell.targetCollectElev == 30);

    // Restore each field independently; preserve foreign changes even when
    // callbacks make them during a nested call.
    {
        Scope outer(&spell);
        {
            Scope inner(&spell);
            spell.targetCollectRange = 1200;
            spell.targetCollectElev = 70;
            spell.targetCollectType = 2;
        }
        assert(spell.targetCollectRange == 1200);
        assert(spell.targetCollectAzi == 360 && spell.targetCollectElev == 70);
    }
    assert(spell.targetCollectRange == 1200);
    assert(spell.targetCollectAzi == 45 && spell.targetCollectElev == 70);
    assert(spell.targetCollectType == 2 && spell.targetCollectAlgo == 2);
    assert(spell.references == 1 && spell.retained == spell.released);

    // Release runs after restoration even when the caller dropped its last
    // reference while the native call was in progress.
    oCSpell lastOwner;
    {
        Scope retained(&lastOwner);
        lastOwner.Release();
        lastOwner.onRelease = [&] {
            assert(lastOwner.references == 1);
            assert(lastOwner.targetCollectRange == 800);
            assert(lastOwner.targetCollectAzi == 45);
            assert(lastOwner.targetCollectElev == 30);
        };
    }
    assert(lastOwner.references == 0);

    std::cout << "Scoped camera targeting: range, native filters, nesting and restoration OK\n";
}
