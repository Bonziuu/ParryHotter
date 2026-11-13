// Small original lower-body gait for casting. No animation files or root motion.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace ParryHotter {

struct CastingGaitPose {
    bool active = false;
    float phase = 0.0f;
    float weight = 0.0f;
    float speed = 0.0f;
    // Hip swing axis in model coordinates, perpendicular to actual travel.
    std::array<float, 3> hipAxis{{1.0f, 0.0f, 0.0f}};
    // L/R thigh, L/R calf, L/R foot, in degrees.
    std::array<float, 6> angles{};
};

class CastingGait final {
public:
    CastingGaitPose Step(bool enabled, float velocityX, float velocityZ,
                         float actorYawDegrees, float deltaSeconds) {
        if (!enabled) { Reset(); return {}; }
        const float dt = std::clamp(Finite(deltaSeconds), 0.0f, 0.1f);
        const float x = Finite(velocityX), z = Finite(velocityZ);
        const float speed = std::min(std::hypot(x, z), 800.0f);
        const float target = std::clamp((speed - 8.0f) / 420.0f, 0.0f, 1.0f);
        weight_ += (target - weight_) * (1.0f - std::exp(-12.0f * dt));
        if (speed > 8.0f) {
            // One complete stride per 160 cm of actual travel. A blocked
            // character does not keep running its legs against the wall.
            phase_ = std::fmod(phase_ + speed * dt * 6.283185307f / 160.0f,
                               6.283185307f);
            const float yaw = Finite(actorYawDegrees) * 0.01745329252f;
            const float length = std::hypot(x, z);
            axis_ = {{(x * std::sin(yaw) + z * std::cos(yaw)) / length,
                      0.0f,
                      -(x * std::cos(yaw) - z * std::sin(yaw)) / length}};
        }
        CastingGaitPose result;
        result.active = weight_ > 0.001f;
        result.phase = phase_;
        result.weight = weight_;
        result.speed = speed;
        result.hipAxis = axis_;
        const float stride = std::sin(phase_);
        result.angles[0] = 16.0f * stride * weight_;
        result.angles[1] = -result.angles[0];
        result.angles[2] = 18.0f * std::max(0.0f, -stride) * weight_;
        result.angles[3] = 18.0f * std::max(0.0f, stride) * weight_;
        result.angles[4] = std::clamp(-result.angles[0] * 0.3f - result.angles[2] * 0.3f,
                                      -8.0f, 8.0f);
        result.angles[5] = std::clamp(-result.angles[1] * 0.3f - result.angles[3] * 0.3f,
                                      -8.0f, 8.0f);
        return result;
    }

    void Reset() { phase_ = weight_ = 0.0f; axis_ = {{1.0f, 0.0f, 0.0f}}; }

private:
    static float Finite(float x) { return std::isfinite(x) ? x : 0.0f; }
    float phase_ = 0.0f;
    float weight_ = 0.0f;
    std::array<float, 3> axis_{{1.0f, 0.0f, 0.0f}};
};
} // namespace ParryHotter
