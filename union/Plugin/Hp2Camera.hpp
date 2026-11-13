// Gothic I camera adapter. Include after the Gothic/Union API headers.
#pragma once
#if __has_include("CameraPlacement.hpp")
#include "CameraPlacement.hpp"
#else
#include "../hud/CameraPlacement.hpp"
#endif

namespace ParryCamera {
using namespace Gothic_I_Classic;

inline ParryHotter::CameraPlacement placement;
inline ParryHotter::CameraPlacementOutput last;
inline zCWorld* trackedWorld = nullptr;
inline oCNpc* trackedActor = nullptr;
inline zCAICamera* trackedCamera = nullptr;

inline void Reset() {
    placement.Reset();
    last = {};
    trackedWorld = nullptr;
    trackedActor = nullptr;
    trackedCamera = nullptr;
}

inline const ParryHotter::CameraPlacementOutput& Place(
    zCAICamera* camera, oCNpc* actor, bool ownsInput,
    float yawDegrees, float pitchDegrees, float seconds
) {
    if (!camera || camera != zCAICamera::current || !camera->camVob || !actor || !ownsInput) {
        Reset();
        return last;
    }
    auto* world = actor->GetHomeWorld();
    if (!world) { Reset(); return last; }
    if (world != trackedWorld || actor != trackedActor || camera != trackedCamera) {
        Reset();
        trackedWorld = world;
        trackedActor = actor;
        trackedCamera = camera;
    }
    float radius = 18.0f;
    if (zCCamera::activeCam) {
        float horizontal = 90.0f, vertical = 60.0f;
        zCCamera::activeCam->GetFOV(horizontal, vertical); // read-only native getter, returns degrees
        const float nearClip = zCCamera::activeCam->nearClipZ;
        if (std::isfinite(nearClip) && nearClip > 0.0f &&
            std::isfinite(horizontal) && horizontal > 1 && horizontal < 170 &&
            std::isfinite(vertical) && vertical > 1 && vertical < 170) {
            const float width = nearClip*std::tan(horizontal*0.00872664626f);
            const float height = nearClip*std::tan(vertical*0.00872664626f);
            radius = std::clamp(std::hypot(width, height), 8.0f, 80.0f);
        }
    }
    auto position = actor->GetPositionWorld();
    const ParryHotter::CameraPlacementInput input{true,
        {position[0], actor->bbox3D.maxs[1], position[2]},
        yawDegrees, pitchDegrees, seconds, radius};
    const int flags = zTRACERAY_STAT_POLY | zTRACERAY_VOB_IGNORE_NO_CD_DYN |
                      zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP |
                      zTRACERAY_POLY_2SIDED;
    last = placement.Step(input, [&](ParryHotter::CameraPoint from, ParryHotter::CameraPoint to) {
        const zVEC3 start(from.x, from.y, from.z);
        const zVEC3 delta(to.x-from.x, to.y-from.y, to.z-from.z);
        const float length = std::sqrt(delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2]);
        if (length <= 0.0001f) return ParryHotter::CameraRayHit{};
        if (!world->TraceRayNearestHit(start, delta, static_cast<zCVob*>(actor), flags))
            return ParryHotter::CameraRayHit{};
        const zVEC3 hit = world->traceRayReport.foundIntersection-start;
        const float distance = std::sqrt(hit[0]*hit[0] + hit[1]*hit[1] + hit[2]*hit[2]);
        return ParryHotter::CameraRayHit{true, std::clamp(distance/length, 0.0f, 1.0f)};
    });
    if (last.active) {
        camera->camVob->SetPositionWorld(zVEC3(last.position.x, last.position.y, last.position.z));
        camera->camVob->SetHeadingAtWorld(zVEC3(last.direction.x, last.direction.y, last.direction.z));
    }
    return last;
}

} // namespace ParryCamera
