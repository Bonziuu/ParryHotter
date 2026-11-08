// Mouse orbit intent only: the engine adapter must keep native collision,
// dialogue and scripted cameras in control of the actual camera placement.
#pragma once
#include <algorithm>
#include <cmath>

namespace ParryHotter {

inline float WrapDegrees(float angle) {
    angle = std::fmod(angle + 180.0f, 360.0f);
    if (angle < 0.0f) angle += 360.0f;
    return angle - 180.0f;
}

struct FreeCameraInput {
    bool enabled = false;
    float deltaSeconds = 1.0f / 60.0f;
    // Relative mouse displacement, NOT mouse velocity. Do not multiply by dt.
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
    float initialYawDegrees = 0.0f;
    float initialPitchDegrees = 0.0f;
    // HP2: while Harry walks and is not aiming, the camera eases back to a
    // slightly downward view and vertical mouse movement is ignored.
    bool autoCenter = false;
};

struct FreeCameraOutput {
    bool active = false;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
};

class FreeCamera final {
public:
    struct Settings {
        float sensitivity = 0.14f; // degrees per relative mouse unit
        // HP2 (PC) BaseCam: pitch limited to +-14000 rotator units (~77 deg),
        // rotation tightness 8 per second, auto-center pitch -1500 (~-8.2 deg).
        float minPitch = -77.0f;
        float maxPitch = 77.0f;
        float smoothing = 8.0f; // inverse seconds; zero disables smoothing
        float autoCenterPitch = -8.24f;
        bool invertY = false;
    };

    FreeCamera() = default;
    explicit FreeCamera(Settings settings) : settings_(settings) {}

    FreeCameraOutput Step(const FreeCameraInput& input) {
        if (!input.enabled) {
            Reset();
            return {};
        }
        if (!initialized_) {
            yaw_ = targetYaw_ = WrapDegrees(Finite(input.initialYawDegrees));
            pitch_ = targetPitch_ = std::clamp(Finite(input.initialPitchDegrees),
                                              settings_.minPitch, settings_.maxPitch);
            initialized_ = true;
        }
        targetYaw_ = WrapDegrees(targetYaw_ +
                                Finite(input.mouseDeltaX) * settings_.sensitivity);
        const float ySign = settings_.invertY ? 1.0f : -1.0f;
        if (input.autoCenter) {
            targetPitch_ = std::clamp(settings_.autoCenterPitch,
                                     settings_.minPitch, settings_.maxPitch);
        } else {
            targetPitch_ = std::clamp(targetPitch_ +
                                     Finite(input.mouseDeltaY) * settings_.sensitivity * ySign,
                                     settings_.minPitch, settings_.maxPitch);
        }
        const float dt = std::clamp(Finite(input.deltaSeconds), 0.0f, 0.1f);
        const float blend = settings_.smoothing <= 0.0f ? 1.0f
            : 1.0f - std::exp(-settings_.smoothing * dt);
        yaw_ = WrapDegrees(yaw_ + WrapDegrees(targetYaw_ - yaw_) * blend);
        pitch_ += (targetPitch_ - pitch_) * blend;
        return {true, yaw_, pitch_};
    }

    void Reset() { initialized_ = false; }

private:
    static float Finite(float value) { return std::isfinite(value) ? value : 0.0f; }
    Settings settings_{};
    bool initialized_ = false;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float targetYaw_ = 0.0f;
    float targetPitch_ = 0.0f;
};
} // namespace ParryHotter
