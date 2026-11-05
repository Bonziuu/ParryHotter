// Camera assistance for Parry Hotter - pure C++17, no Union dependencies.
// The game retains control of actual mouse rotation, collision and dialogue cameras.
#pragma once
#include <algorithm>
#include <cmath>

namespace ParryHotter {

class AimCamera {
public:
    // Gothic 1 coordinates use centimeters. Never change the near/far
    // collision clipping planes or overwrite scripted/cutscene camera modes.
    template <typename Camera>
    void Update(Camera* camera, bool aiming) {
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

        const float proposed = savedRange_ * 0.82f;
        const float safeRange = proposed < 175.0f ? 175.0f
                              : proposed > 290.0f ? 290.0f : proposed;
        const float goalRange = aiming ? safeRange : savedRange_;
        const float goalShoulder = aiming
            ? savedShoulder_ + 38.0f : savedShoulder_;

        camera->bestRange += (goalRange - camera->bestRange) * 0.24f;
        camera->focusOffsetX += (goalShoulder - camera->focusOffsetX) * 0.24f;

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
