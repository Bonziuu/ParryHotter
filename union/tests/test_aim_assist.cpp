#include "../hud/AimAssist.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace ParryHotter;

int main() {
    const AimVec eye{0, 170, 0};
    const AimVec forward{0, 0, 1};
    // Diego standing beside the screen centre (5 deg off at 4 m) is picked.
    const float off = std::tan(5.0f * 0.01745329252f) * 400.0f;
    std::vector<AimCandidate> c{{{off, 170, 400}, true}};
    assert(PickAimAssist(eye, forward, c) == 0);
    // Outside the 8 deg cone, behind the camera, dead or too far: ignored.
    assert(PickAimAssist(eye, forward, {{{300, 170, 400}, true}}) == -1);
    assert(PickAimAssist(eye, forward, {{{0, 170, -400}, true}}) == -1);
    assert(PickAimAssist(eye, forward, {{{0, 170, 400}, false}}) == -1);
    assert(PickAimAssist(eye, forward, {{{0, 170, 3000}, true}}) == -1);
    // The candidate closest to the centre wins; equal angles prefer nearer.
    std::vector<AimCandidate> two{{{off, 170, 400}, true}, {{1, 170, 800}, true}};
    assert(PickAimAssist(eye, forward, two) == 1);
    std::vector<AimCandidate> line{{{0, 170, 900}, true}, {{0, 170, 300}, true}};
    assert(PickAimAssist(eye, forward, line) == 1);
    // Unnormalised direction and degenerate input.
    assert(PickAimAssist(eye, {0, 0, 50}, c) == 0);
    assert(PickAimAssist(eye, {0, 0, 0}, c) == -1);
    assert(PickAimAssist(eye, {NAN, 0, 1}, c) == -1);
    std::cout << "Parry Hotter HP2 aim assist cone: OK\n";
}
