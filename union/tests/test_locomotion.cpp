#include "../hud/Locomotion.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace ParryHotter;
static bool close(float a, float b) { return std::fabs(a - b) < 0.02f; }
static LocomotionOutput simulate(float dt, int frames, float forward, float right) {
    Locomotion controller;
    LocomotionInput input;
    input.enabled = input.grounded = true;
    input.deltaSeconds = dt;
    input.forward = forward;
    input.right = right;
    LocomotionOutput out;
    for (int i = 0; i < frames; ++i) {
        out = controller.Step(input);
        input.velocityX = out.velocityX;
        input.velocityZ = out.velocityZ;
    }
    return out;
}

int main() {
    auto cardinal = simulate(1.0f / 60, 60, 1, 0);
    auto diagonal = simulate(1.0f / 60, 60, 1, 1);
    assert(close(std::hypot(cardinal.velocityX, cardinal.velocityZ), 540));
    assert(close(std::hypot(diagonal.velocityX, diagonal.velocityZ), 540));
    assert(close(diagonal.facingYawDegrees, 0)); // HP2: faces the camera, W+D walks diagonally
    auto lowFps = simulate(1.0f / 30, 3, 1, 0);
    auto highFps = simulate(1.0f / 120, 12, 1, 0);
    assert(close(lowFps.velocityZ, highFps.velocityZ));

    Locomotion ctl;
    LocomotionInput input;
    input.enabled = input.grounded = true;
    input.deltaSeconds = 0.1f;
    input.forward = 1;
    input.cameraYawDegrees = 90;
    auto out = ctl.Step(input);
    assert(out.velocityX > 250 && std::fabs(out.velocityZ) < 0.01f);
    assert(close(out.facingYawDegrees, 90));
    input.aiming = true;
    input.right = 1;
    input.forward = 0;
    out = ctl.Step(input);
    assert(close(out.facingYawDegrees, 90)); // strafing preserves aim direction

    // Midair reversal is controlled by planar acceleration; vertical motion
    // is entirely owned by the native jump/gravity/collision implementation.
    input.aiming = input.grounded = false;
    input.cameraYawDegrees = 0;
    input.right = -1;
    input.velocityX = 200;
    out = ctl.Step(input);
    assert(out.velocityX < 200 && out.velocityX > -430);
    input.right = 0;
    input.velocityX = 200;
    out = ctl.Step(input);
    assert(out.velocityX > 180); // releasing keys does not stop a jump midair

    // Coyote jump is allowed shortly after leaving the ledge, exactly once.
    ctl.Reset();
    input = {};
    input.enabled = input.grounded = true;
    ctl.Step(input);
    input.grounded = false;
    input.deltaSeconds = 0.06f;
    input.jumpHeld = true;
    assert(ctl.Step(input).jump);
    input.jumpHeld = false;
    ctl.Step(input);
    input.jumpHeld = true;
    assert(!ctl.Step(input).jump);
    // A new grounded controller cannot use a coyote window after it expires.
    ctl.Reset();
    input.grounded = true;
    input.jumpHeld = false;
    ctl.Step(input);
    input.grounded = false;
    input.deltaSeconds = 0.1f;
    ctl.Step(input);
    input.jumpHeld = true;
    assert(!ctl.Step(input).jump);

    // Press just before landing: retain the request and issue it on contact.
    ctl.Reset();
    input = {};
    input.enabled = true;
    input.jumpHeld = true;
    assert(!ctl.Step(input).jump);
    input.grounded = true;
    assert(ctl.Step(input).jump);
    assert(!ctl.Step(input).jump); // held jump cannot auto-repeat on contact
    input.grounded = false;
    ctl.Step(input);
    input.grounded = true;
    assert(!ctl.Step(input).jump);
    input.jumpHeld = false;
    ctl.Step(input);
    input.jumpHeld = true;
    assert(ctl.Step(input).jump);

    // Pauses, dialogue, swimming etc. relinquish movement and discard buffers.
    assert(!ctl.Step({}).active);
    assert(!ctl.Step(input).jump); // held space on resume cannot launch
    input.jumpHeld = false;
    ctl.Step(input);
    input.jumpHeld = true;
    assert(ctl.Step(input).jump);
    std::cout << "Parry Hotter camera-relative movement, air steering and jump buffer: OK\n";
}
