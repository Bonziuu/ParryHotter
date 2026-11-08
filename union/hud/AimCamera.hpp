// Shoulder framing, separate from FreeCamera's mouse orbit controller.
// Keep the native camera's collision and dialogue modes.
#pragma once
#include <algorithm>
#include <cmath>

namespace ParryHotter {

class AimCamera {
public:
    // Unit-agnostic: works on whatever unit zCAICamera::bestRange holds.
    // Never change the near/far collision clipping planes or overwrite
    // scripted/cutscene camera modes. Harry Potter 2 does not reframe the
    // camera while aiming, so the plugin currently never requests aiming.
    template <typename Camera>
    void Update(Camera* camera, bool aiming, float deltaSeconds = 1.0f / 60.0f) {
        if (!camera) {
            Forget();
            return;
        }
        if (tracked_ != static_cast<const void*>(camera)) {
            Forget();
            tracked_ = static_cast<const void*>(camera);
        }
        if (aiming && !captured_) {
            savedRange_ = camera->bestRange;
            savedShoulder_ = camera->focusOffsetX;
            captured_ = true;
        }
        if (!captured_) return;

        // Relative only: Gothic camera scripts define bestRange in METERS
        // (CAMMODNORMAL 2.5). The former absolute 175..290 clamp assumed
        // centimeters and could throw the camera ~175 m away while aiming.
        const float goalRange = aiming ? savedRange_ * 0.82f : savedRange_;
        const float goalShoulder = savedShoulder_;

        const float dt = std::isfinite(deltaSeconds)
            ? std::clamp(deltaSeconds, 0.0f, 0.1f) : 0.0f;
        const float blend = 1.0f - std::exp(-16.4662f * dt);
        camera->bestRange += (goalRange - camera->bestRange) * blend;
        camera->focusOffsetX += (goalShoulder - camera->focusOffsetX) * blend;

        if (!aiming &&
            std::fabs(camera->bestRange - savedRange_) < 0.35f &&
            std::fabs(camera->focusOffsetX - savedShoulder_) < 0.35f) {
            camera->bestRange = savedRange_;
            camera->focusOffsetX = savedShoulder_;
            captured_ = false;
        }
    }

    template <typename Camera>
    void Restore(Camera* camera) {
        if (camera && captured_ &&
            tracked_ == static_cast<const void*>(camera)) {
            camera->bestRange = savedRange_;
            camera->focusOffsetX = savedShoulder_;
        }
        Forget();
    }

    // Call this on a world transition if the old camera pointer could be invalid.
    void Forget() {
        tracked_ = nullptr;
        captured_ = false;
    }

    bool IsActive() const { return captured_; }

private:
    const void* tracked_ = nullptr;
    float savedRange_ = 0.0f;
    float savedShoulder_ = 0.0f;
    bool captured_ = false;
};
} // namespace ParryHotter
