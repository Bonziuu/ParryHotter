#include "../hud/FreeCamera.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace ParryHotter;
static bool close(float a, float b) { return std::fabs(a - b) < 0.02f; }

int main() {
    FreeCamera::Settings settings;
    settings.smoothing = 0;
    FreeCamera camera(settings);
    auto out = camera.Step({true, 1.0f / 60, 100, -50, 170, 0});
    assert(out.active && close(out.yawDegrees, -176));
    assert(close(out.pitchDegrees, 7));
    out = camera.Step({true, 1.0f / 60, 0, -100000, 0, 0});
    assert(close(out.pitchDegrees, settings.maxPitch));
    out = camera.Step({true, 1.0f / 60, 0, 100000, 0, 0});
    assert(close(out.pitchDegrees, settings.minPitch));
    // Relative mouse deltas yield the same orbit at different frame rates.
    FreeCamera slow(settings), fast(settings);
    for (int i = 0; i < 30; ++i) out = slow.Step({true, 1.0f / 30, 4, 0, 0, 0});
    const auto slowYaw = out.yawDegrees;
    for (int i = 0; i < 120; ++i) out = fast.Step({true, 1.0f / 120, 1, 0, 0, 0});
    assert(close(out.yawDegrees, slowYaw));
    // Exponential smoothing follows the same impulse over the same duration.
    FreeCamera smoothSlow, smoothFast;
    smoothSlow.Step({true, 0, 100, 0, 175, 0});
    smoothFast.Step({true, 0, 100, 0, 175, 0});
    for (int i = 0; i < 15; ++i) out = smoothSlow.Step({true, 1.0f / 30});
    const auto smoothedYaw = out.yawDegrees;
    for (int i = 0; i < 60; ++i) out = smoothFast.Step({true, 1.0f / 120});
    assert(close(out.yawDegrees, smoothedYaw));
    // Menu/cutscene relinquishes control and the new engine pose seeds resume.
    assert(!camera.Step({}).active);
    out = camera.Step({true, 1.0f / 60, 0, 0, 35, 12});
    assert(close(out.yawDegrees, 35) && close(out.pitchDegrees, 12));
    out = camera.Step({true, std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(), 0, 0, 0});
    assert(std::isfinite(out.yawDegrees) && std::isfinite(out.pitchDegrees));
    std::cout << "Parry Hotter free mouse orbit and pitch limits: OK\n";
}
