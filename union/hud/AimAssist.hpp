// HP2-style aim assist: acquire the body under/near the centre dot rather
// than requiring one point on the upper torso to enter a narrow cone.
// Pure geometry; the adapter retains native LOS and the 19 m actor range.
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace ParryHotter {

struct AimVec { float x = 0, y = 0, z = 0; };

struct AimCandidate {
    AimVec position;
    bool alive = true;
    AimVec halfExtents{}; // World-axis-aligned body bounds; zero = old point API.
};

struct AimAssistSettings {
    float maxAngleDegrees = 12.0f; // Margin measured from the projected body.
    float maxEyeDistance = 2400.0f; // Camera offset; actor range is checked separately.
    float retentionAngleDegrees = 16.0f;
    float stickyBonusDegrees = 2.0f;
};

struct AimAssistScore {
    bool valid = false;
    bool inBody = false;
    float angleDegrees = 0.0f;
    float eyeDistance = 0.0f;
};

namespace AimDetail {
inline bool Finite(AimVec value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace AimDetail

// Screen-space rectangle enclosing all eight body corners, expressed as
// tangents to the camera axes. This avoids a dependency on render matrices,
// which may still belong to the previous native camera position this frame.
inline AimAssistScore ScoreAimCandidate(const AimVec& eye, const AimVec& direction,
                                       const AimCandidate& candidate) {
    AimAssistScore out;
    if (!candidate.alive || !AimDetail::Finite(eye) || !AimDetail::Finite(direction) ||
        !AimDetail::Finite(candidate.position) || !AimDetail::Finite(candidate.halfExtents) ||
        candidate.halfExtents.x < 0 || candidate.halfExtents.y < 0 || candidate.halfExtents.z < 0)
        return out;
    const double length = std::hypot(double(direction.x), double(direction.y), double(direction.z));
    if (!(length > 1e-4)) return out;
    const double fx = direction.x/length, fy = direction.y/length, fz = direction.z/length;
    const double horizontal = std::hypot(fx, fz);
    const double rx = horizontal > 1e-8 ? fz/horizontal : 1.0;
    const double rz = horizontal > 1e-8 ? -fx/horizontal : 0.0;
    const double ux = fy*rz, uy = fz*rx-fx*rz, uz = -fy*rx;
    const double cx = double(candidate.position.x)-eye.x;
    const double cy = double(candidate.position.y)-eye.y;
    const double cz = double(candidate.position.z)-eye.z;
    const double centreDepth = cx*fx + cy*fy + cz*fz;
    const double distance = std::hypot(cx, cy, cz);
    // A large box must not make an actor behind the camera an eligible target.
    if (!(centreDepth > 1.0) || !(distance > 1.0) ||
        distance > (std::numeric_limits<float>::max)()) return out;
    double minX = (std::numeric_limits<double>::max)();
    double minY = minX, maxX = -minX, maxY = -minX;
    for (int corner = 0; corner < 8; ++corner) {
        const double x = cx + ((corner & 1) ? 1.0 : -1.0)*candidate.halfExtents.x;
        const double y = cy + ((corner & 2) ? 1.0 : -1.0)*candidate.halfExtents.y;
        const double z = cz + ((corner & 4) ? 1.0 : -1.0)*candidate.halfExtents.z;
        // Clip near-plane corners, preserving close visible parts of the body.
        const double depth = std::max(1.0, x*fx+y*fy+z*fz);
        const double screenX = (x*rx+z*rz)/depth;
        const double screenY = (x*ux+y*uy+z*uz)/depth;
        minX = std::min(minX, screenX); maxX = std::max(maxX, screenX);
        minY = std::min(minY, screenY); maxY = std::max(maxY, screenY);
    }
    const double nearestX = minX > 0 ? minX : (maxX < 0 ? maxX : 0.0);
    const double nearestY = minY > 0 ? minY : (maxY < 0 ? maxY : 0.0);
    out.valid = true;
    out.inBody = nearestX == 0.0 && nearestY == 0.0;
    out.angleDegrees = float(std::atan(std::hypot(nearestX, nearestY))*57.29577951308232);
    out.eyeDistance = float(distance);
    return out;
}

// Exact body overlap wins before retention. Otherwise a previous target gets
// at most two degrees of preference, so deliberately aiming nearer another
// actor still switches targets. Ties prefer raw angle and then nearer actors.
inline int PickAimAssist(const AimVec& eye, const AimVec& direction,
                         const std::vector<AimCandidate>& candidates,
                         const AimAssistSettings& settings = {}, int stickyIndex = -1) {
    if (!std::isfinite(settings.maxAngleDegrees) || settings.maxAngleDegrees < 0 ||
        settings.maxAngleDegrees >= 90 || !std::isfinite(settings.maxEyeDistance) ||
        settings.maxEyeDistance <= 1 || !std::isfinite(settings.retentionAngleDegrees) ||
        settings.retentionAngleDegrees < 0 || settings.retentionAngleDegrees >= 90 ||
        !std::isfinite(settings.stickyBonusDegrees)) return -1;
    const float retention = std::max(settings.maxAngleDegrees, settings.retentionAngleDegrees);
    const float bonus = std::clamp(settings.stickyBonusDegrees, 0.0f, 2.0f);
    int best = -1;
    AimAssistScore bestScore;
    float bestAdjusted = 0.0f;
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const auto score = ScoreAimCandidate(eye, direction, candidates[i]);
        if (!score.valid || score.eyeDistance > settings.maxEyeDistance) continue;
        const bool sticky = i == stickyIndex;
        const float limit = sticky ? retention : settings.maxAngleDegrees;
        if (score.angleDegrees > limit + 1e-5f) continue;
        const float adjusted = std::max(0.0f, score.angleDegrees-(sticky ? bonus : 0.0f));
        const bool sameBody = score.inBody == bestScore.inBody;
        const bool sameAdjusted = std::fabs(adjusted-bestAdjusted) <= 1e-5f;
        const bool sameRaw = std::fabs(score.angleDegrees-bestScore.angleDegrees) <= 1e-5f;
        const bool better = best < 0 || (score.inBody && !bestScore.inBody) ||
            (sameBody && (adjusted < bestAdjusted-1e-5f ||
                (sameAdjusted && (score.angleDegrees < bestScore.angleDegrees-1e-5f ||
                    (sameRaw && score.eyeDistance < bestScore.eyeDistance)))));
        if (better) { best = i; bestScore = score; bestAdjusted = adjusted; }
    }
    return best;
}

} // namespace ParryHotter
