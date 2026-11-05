#include "../hud/AimCamera.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

struct GothicCameraStub {
    float bestRange = 310.0f;
    float focusOffsetX = 0.0f;
};

static bool close(float a, float b) { return std::fabs(a - b) < 0.01f; }

int main() {
    ParryHotter::AimCamera aiming;
    GothicCameraStub cam;
    aiming.Update(&cam, false);
    assert(!aiming.IsActive());
    assert(close(cam.bestRange, 310.0f));

    for (int i = 0; i < 28; ++i) aiming.Update(&cam, true);
    assert(aiming.IsActive());
    assert(cam.bestRange < 280.0f && cam.bestRange > 240.0f);
    assert(cam.focusOffsetX > 32.0f && cam.focusOffsetX < 39.0f);

    // Camera returns to original settings when PPM is released.
    for (int i = 0; i < 35; ++i) aiming.Update(&cam, false);
    assert(close(cam.bestRange, 310.0f));
    assert(close(cam.focusOffsetX, 0.0f));
    assert(!aiming.IsActive());

    cam.bestRange = 420.0f;
    cam.focusOffsetX = -12.0f;
    for (int i = 0; i < 10; ++i) aiming.Update(&cam, true);
    aiming.Restore(&cam);
    assert(close(cam.bestRange, 420.0f));
    assert(close(cam.focusOffsetX, -12.0f));

    // World transition: old camera may have been freed. Forget, don't deref.
    aiming.Update(&cam, true);
    aiming.Forget();
    GothicCameraStub newWorld;
    newWorld.bestRange = 280.0f;
    aiming.Update(&newWorld, false);
    assert(close(newWorld.bestRange, 280.0f));
    assert(!aiming.IsActive());

    std::cout << "Parry Hotter camera restore/aim transition: OK\n";
}
