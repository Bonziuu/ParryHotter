// HP2-style aim assist: when the exact centre ray misses, the spell cursor
// snaps to the living NPC closest to the screen centre inside a small cone.
// Pure geometry (portable tests); the plugin adds the line-of-sight trace and
// the 19 m player-distance rule.
#pragma once
#include <cmath>
#include <vector>

namespace ParryHotter {

struct AimVec { float x = 0, y = 0, z = 0; };

struct AimCandidate {
    AimVec position;
    bool alive = true;
};

struct AimAssistSettings {
    float maxAngleDegrees = 8.0f;
    float maxEyeDistance = 2400.0f; // camera sits ~2.5 m behind the player
};

// Returns the index of the best candidate or -1. `direction` need not be
// normalised. Candidates behind the camera, dead, too far or outside the
// cone are ignored; ties prefer the nearer one.
inline int PickAimAssist(const AimVec& eye, const AimVec& direction,
                         const std::vector<AimCandidate>& candidates,
                         const AimAssistSettings& settings = {}) {
    const float dirLength = std::sqrt(direction.x * direction.x +
                                      direction.y * direction.y +
                                      direction.z * direction.z);
    if (!(dirLength > 1e-4f) || !std::isfinite(dirLength)) return -1;
    const float cosLimit = std::cos(settings.maxAngleDegrees * 0.01745329252f);
    int best = -1;
    float bestCos = cosLimit;
    float bestDistance = 0.0f;
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const auto& c = candidates[i];
        if (!c.alive) continue;
        const float dx = c.position.x - eye.x;
        const float dy = c.position.y - eye.y;
        const float dz = c.position.z - eye.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!(distance > 1.0f) || distance > settings.maxEyeDistance) continue;
        const float cosAngle = (dx * direction.x + dy * direction.y + dz * direction.z) /
                               (distance * dirLength);
        if (!std::isfinite(cosAngle) || cosAngle < cosLimit) continue;
        const bool better = best < 0 || cosAngle > bestCos + 1e-6f ||
                            (std::fabs(cosAngle - bestCos) <= 1e-6f && distance < bestDistance);
        if (better) {
            best = i;
            bestCos = cosAngle;
            bestDistance = distance;
        }
    }
    return best;
}

} // namespace ParryHotter
