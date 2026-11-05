#include "../hud/AimTargetState.hpp"
#include <cassert>
#include <iostream>
#include <cstring>

using namespace ParryHotter;

int main() {
    assert(TargetCanCast(AimTargetState::ValidNpc));
    const AimTargetState blocked[] = {
        AimTargetState::NoPlayer, AimTargetState::NoCamera,
        AimTargetState::NoWorld, AimTargetState::NoRayHit,
        AimTargetState::WallOrObject, AimTargetState::DeadNpc,
        AimTargetState::WrongWorld, AimTargetState::OutOfRange,
        AimTargetState::NoSpellBook
    };
    for (const auto state : blocked) {
        assert(!TargetCanCast(state));
        assert(std::strlen(AimTargetDescription(state)) > 3);
    }
    assert(std::strstr(AimTargetDescription(AimTargetState::WallOrObject),
                       "NPC") != nullptr);
    assert(std::strstr(AimTargetDescription(AimTargetState::ValidNpc),
                       "celowniku") != nullptr);
    std::cout << "Parry Hotter screen-center target diagnostics: OK\n";
}
