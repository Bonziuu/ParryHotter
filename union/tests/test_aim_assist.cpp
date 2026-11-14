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
    // Outside the 12 deg margin, behind the camera, dead or too far: ignored.
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
    // A nearby actor's head and feet can be well outside the old centre-point
    // cone while the dot is visibly on its body. Both must still acquire it.
    const AimCandidate body{{0, 170, 250}, true, {40, 90, 25}};
    assert(ScoreAimCandidate({0, 260, 0}, forward, body).inBody);
    assert(ScoreAimCandidate({0, 80, 0}, forward, body).inBody);
    assert(PickAimAssist({0, 260, 0}, forward, {body}) == 0);
    assert(PickAimAssist({0, 80, 0}, forward, {body}) == 0);
    // The margin starts at the body's projected edge, not at its centre.
    const AimCandidate beside{{200, 170, 400}, true, {150, 90, 20}};
    const auto nearEdge = ScoreAimCandidate(eye, forward, beside);
    assert(nearEdge.valid && !nearEdge.inBody && nearEdge.angleDegrees < 12);
    assert(PickAimAssist(eye, forward, {beside}) == 0);
    assert(PickAimAssist(eye, forward, {{{300,170,400},true,{150,90,20}}}) == -1);
    // Camera distance includes its offset; the adapter separately enforces
    // the unchanged 1900 cm distance from the player.
    assert(PickAimAssist(eye, forward, {{{0,170,2200},true}}) == 0);
    assert(PickAimAssist(eye, forward, {{{0,170,2400.1f},true}}) == -1);
    const auto atAngle = [](float degrees) {
        return AimCandidate{{std::tan(degrees*0.01745329252f)*1000,170,1000},true};
    };
    assert(PickAimAssist(eye, forward, {atAngle(12)}) == 0);
    assert(PickAimAssist(eye, forward, {atAngle(12.1f)}) == -1);
    // Retention tolerates jitter, but a substantially closer body wins.
    assert(PickAimAssist(eye, forward, {atAngle(13)}) == -1);
    assert(PickAimAssist(eye, forward, {atAngle(13)}, {}, 0) == 0);
    assert(PickAimAssist(eye, forward, {atAngle(16)}, {}, 0) == 0);
    assert(PickAimAssist(eye, forward, {atAngle(16.1f)}, {}, 0) == -1);
    assert(PickAimAssist(eye, forward, {atAngle(13), atAngle(12)}, {}, 0) == 0);
    assert(PickAimAssist(eye, forward, {atAngle(13), atAngle(9)}, {}, 0) == 1);
    const AimCandidate directBody{{100,170,1000},true,{150,90,30}};
    assert(PickAimAssist(eye, forward, {atAngle(13), directBody}, {}, 0) == 1);
    // The retention preference is capped at two degrees even for bad config.
    AimAssistSettings tooSticky;
    tooSticky.stickyBonusDegrees = 100;
    assert(PickAimAssist(eye, forward, {atAngle(13), atAngle(9)}, tooSticky, 0) == 1);
    // A caller can reject occluded candidates and continue to the fifth one.
    std::vector<AimCandidate> group;
    for (int i = 1; i <= 5; ++i) group.push_back(atAngle(float(i)));
    for (int i = 0; i < 5; ++i) {
        assert(PickAimAssist(eye, forward, group) == i);
        group[i].alive = false;
    }
    assert(PickAimAssist(eye, forward, group) == -1);
    // Looking vertically, non-normalized directions and a near-plane body
    // still have finite scores. Large boxes behind the camera never qualify.
    assert(PickAimAssist(eye, {0,50,0}, {{{0,500,0},true,{40,90,25}}}) == 0);
    assert(PickAimAssist(eye, forward, {{{0,170,2},true,{40,90,25}}}) == 0);
    assert(PickAimAssist(eye, forward, {{{0,170,-2},true,{40,90,25}}}) == -1);
    assert(PickAimAssist(eye, forward, {{{0,170,400},true,{NAN,90,25}}}) == -1);
    assert(PickAimAssist(eye, forward, {{{0,170,400},true,{-1,90,25}}}) == -1);
    assert(PickAimAssist({NAN,170,0}, forward, {body}) == -1);
    AimAssistSettings invalid;
    invalid.maxAngleDegrees = NAN;
    assert(PickAimAssist(eye, forward, {body}, invalid) == -1);
    invalid = {};
    invalid.retentionAngleDegrees = 90;
    assert(PickAimAssist(eye, forward, {body}, invalid) == -1);
    std::cout << "Parry Hotter HP2 body aim assist, vicinity and target switching: OK\n";
}
