// Input requests and collision feedback. No engine objects or replacement
// gravity: the Gothic adapter owns a single accepted native jump impulse.
#pragma once
#include <algorithm>
#include <cmath>

namespace ParryHotter {

class JumpIntent final {
public:
    // True only when a new physical edge creates an intent. The caller can
    // assign one stable diagnostic ID and keep it through native retries.
    bool Update(bool enabled, float dt, bool held, bool pressed) {
        if (!enabled) { Reset(); return false; }
        const float seconds = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
        remaining_ = std::max(0.0f, remaining_ - seconds);
        if (!held) requireRelease_ = false;
        // pressed is a captured physical edge, including a complete short
        // click between AI frames. Holding RMB never renews the buffer.
        if (pressed && !requireRelease_) {
            remaining_ = 0.15f;
            requireRelease_ = held;
            return true;
        }
        return false;
    }
    bool Pending() const { return remaining_ > 0.0f; }
    float Remaining() const { return remaining_; }
    void Accepted() { remaining_ = 0.0f; }
    void Reset() { remaining_ = 0.0f; requireRelease_ = true; }

private:
    float remaining_ = 0.0f;
    bool requireRelease_ = false;
};

struct PlanarFeedback {
    float velocityX = 0.0f;
    float velocityZ = 0.0f;
    float wantedDistance = 0.0f;
    float actualDistance = 0.0f;
    bool blocked = false;
};

// Sample position immediately before and after our MoveWorld steps, excluding
// native physics/platform movement. Feed this velocity into the next intent
// step, so pushing against a wall cannot store speed behind the collision.
inline PlanarFeedback ResolvePlanarFeedback(float wantedX, float wantedZ,
                                            float actualX, float actualZ, float dt) {
    PlanarFeedback out;
    if (!std::isfinite(dt) || dt <= 0.000001f ||
        !std::isfinite(wantedX) || !std::isfinite(wantedZ) ||
        !std::isfinite(actualX) || !std::isfinite(actualZ)) return out;
    out.wantedDistance = std::hypot(wantedX, wantedZ);
    out.actualDistance = std::hypot(actualX, actualZ);
    const float progress = out.wantedDistance > 0.00001f
        ? (wantedX * actualX + wantedZ * actualZ) / out.wantedDistance : 0.0f;
    out.blocked = progress + 0.05f < out.wantedDistance;
    if (out.wantedDistance <= 0.00001f ||
        wantedX * actualX + wantedZ * actualZ <= 0.0f) return out;
    // Collision corrections must not inject additional command speed.
    const float scale = out.actualDistance > out.wantedDistance
        ? out.wantedDistance / out.actualDistance : 1.0f;
    out.velocityX = actualX * scale / dt;
    out.velocityZ = actualZ * scale / dt;
    return out;
}

inline float LowJumpLaunchSpeed(float heightCm = 70.0f) {
    return std::sqrt(1962.0f * std::max(0.0f, heightCm));
}

} // namespace ParryHotter
