#include "../hud/ImpactImpulse.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace ParryHotter;

int main() {
    const ImpactVec caster{0.0f, 0.0f, 0.0f};
    const ImpactVec target{100.0f, 0.0f, 0.0f};
    const ImpactVec stationary{0.0f, 0.0f, 0.0f};

    auto lev = PlanImpact(ImpactKind::Levicorpus, stationary,
                          caster, target, true, true);
    assert(lev.apply && lev.upward && !lev.outward);
    assert(lev.velocity.y == 165.0f);
    assert(lev.velocity.x == 0.0f && lev.velocity.z == 0.0f);

    auto blocked = PlanImpact(ImpactKind::Levicorpus, stationary,
                             caster, target, false, true);
    assert(!blocked.apply);   // no launch through a low ceiling

    auto bomb = PlanImpact(ImpactKind::Bumbardo, stationary,
                          caster, target, true, true);
    assert(bomb.apply && bomb.upward && bomb.outward);
    assert(bomb.velocity.y == 285.0f);
    assert(bomb.velocity.x == 155.0f && bomb.velocity.z == 0.0f);

    auto bUnderCeiling = PlanImpact(ImpactKind::Bumbardo, stationary,
                                   caster, target, false, true);
    assert(bUnderCeiling.apply && !bUnderCeiling.upward &&
           bUnderCeiling.outward && bUnderCeiling.velocity.y == 0.0f);
    auto walled = PlanImpact(ImpactKind::Bumbardo, stationary,
                            caster, target, true, false);
    assert(walled.apply && walled.upward && !walled.outward);
    auto trapped = PlanImpact(ImpactKind::Bumbardo, stationary,
                             caster, target, false, false);
    assert(!trapped.apply);

    auto closeHit = PlanImpact(ImpactKind::Bumbardo, stationary,
                              caster, caster, true, true);
    assert(closeHit.apply && closeHit.upward && !closeHit.outward);
    assert(closeHit.velocity.x == 0.0f && closeHit.velocity.z == 0.0f);

    // Existing fast Gothic knockback is preserved — never slow the native
    // simulation or overwrite a stronger Stormfist upward launch.
    auto fast = PlanImpact(ImpactKind::Bumbardo, {300, 900, 200},
                           caster, target, true, true);
    assert(!fast.apply);
    assert(fast.velocity.x == 300.0f && fast.velocity.y == 900.0f);
    assert(fast.velocity.z == 200.0f);
    auto rising = PlanImpact(ImpactKind::Bumbardo, {0, 900, 0},
                             caster, target, true, true);
    assert(rising.apply && !rising.upward && rising.outward);
    assert(rising.velocity.y == 900.0f);
    assert(rising.velocity.x == 155.0f);

    auto bad = PlanImpact(
        ImpactKind::Levicorpus, {std::numeric_limits<float>::quiet_NaN(), 0, 0},
        caster, target, true, true
    );
    assert(!bad.apply);
    assert(!PlanImpact(
        ImpactKind::Bumbardo, stationary,
        caster, {std::numeric_limits<float>::infinity(), 0, 0}, true, true
    ).apply);

    std::cout << "Parry Hotter bounded spell impulses: OK\n";
}
