// Parry Hotter — bounded, collision-aware spell impulse calculations.
// Does not depend on Gothic/Union. Motion is ultimately performed by
// ZenGin's own rigid-body physics, only where physics is already enabled.
// Never teleports NPCs or turns gravity/collision detection off.
#pragma once

#include <cmath>

namespace ParryHotter {

struct ImpactVec {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class ImpactKind {
    Levicorpus,
    Bumbardo
};

struct ImpactPlan {
    ImpactVec velocity{};
    bool apply = false;
    bool upward = false;
    bool outward = false;
};

inline bool Finite(ImpactVec v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline ImpactPlan PlanImpact(
    ImpactKind kind, ImpactVec currentVelocity,
    ImpactVec attackerPos, ImpactVec targetPos,
    bool ceilingClear, bool horizontalClear
) {
    ImpactPlan plan{};
    if (!Finite(currentVelocity) || !Finite(attackerPos) || !Finite(targetPos))
        return plan;

    const float upwardSpeed = kind == ImpactKind::Levicorpus ? 165.0f : 285.0f;
    if (ceilingClear) {
        // Controlled knock-up. Never stack unlimited vertical impulses.
        plan.velocity.y = currentVelocity.y > upwardSpeed
                          ? currentVelocity.y : upwardSpeed;
        if (plan.velocity.y > 340.0f) plan.velocity.y = 340.0f;
        plan.upward = true;
    } else {
        plan.velocity.y = currentVelocity.y;
    }

    if (kind == ImpactKind::Bumbardo && horizontalClear) {
        const float dx = targetPos.x - attackerPos.x;
        const float dz = targetPos.z - attackerPos.z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (std::isfinite(d) && d > 20.0f) {
            // Maximum outward impulse 155 cm/s; limit total horizontal speed.
            float vx = currentVelocity.x + dx / d * 155.0f;
            float vz = currentVelocity.z + dz / d * 155.0f;
            const float magnitude = std::sqrt(vx * vx + vz * vz);
            if (magnitude > 220.0f) {
                vx *= 220.0f / magnitude;
                vz *= 220.0f / magnitude;
            }
            plan.velocity.x = vx;
            plan.velocity.z = vz;
            plan.outward = true;
        } else {
            plan.velocity.x = currentVelocity.x;
            plan.velocity.z = currentVelocity.z;
        }
    } else {
        plan.velocity.x = currentVelocity.x;
        plan.velocity.z = currentVelocity.z;
    }

    if (!Finite(plan.velocity)) return {};
    plan.apply = plan.upward || plan.outward;
    return plan;
}

}  // namespace ParryHotter
