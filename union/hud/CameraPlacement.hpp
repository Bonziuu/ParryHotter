// Collision-aware HP-style camera placement, independent of Gothic/Union.
// Trace callbacks return the first hit on a finite segment (fraction 0..1).
// The nine swept rays approximate the camera near-plane cross section.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace ParryHotter {

struct CameraPoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    CameraPoint operator+(CameraPoint b) const { return {x+b.x, y+b.y, z+b.z}; }
    CameraPoint operator-(CameraPoint b) const { return {x-b.x, y-b.y, z-b.z}; }
    CameraPoint operator*(float s) const { return {x*s, y*s, z*s}; }
};

struct CameraRayHit {
    bool hit = false;
    float fraction = 1.0f;
};

struct CameraPlacementInput {
    bool enabled = false;
    CameraPoint head;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float deltaSeconds = 0.0f;
    float radius = 18.0f;
};

struct CameraPlacementOutput {
    bool active = false;
    CameraPoint position;
    CameraPoint direction;
    CameraPoint pivot;
    float distance = 0.0f;
    float desiredDistance = 0.0f;
    float pivotAboveHead = 0.0f;
    bool ceilingLimited = false;
    bool collisionLimited = false;
    unsigned traceCount = 0;
    unsigned sampleHits = 0;
};

class CameraPlacement final {
public:
    static constexpr float kDistance = 300.0f;
    static constexpr float kAboveHead = 75.0f;
    static constexpr float kGap = 8.0f;
    static constexpr float kReturnSpeed = 400.0f;

    void Reset() { initialized_ = false; distance_ = 0.0f; }

    template<class Trace>
    CameraPlacementOutput Step(const CameraPlacementInput& input, Trace trace) {
        CameraPlacementOutput out;
        if (!input.enabled || !Finite(input.head) ||
            !std::isfinite(input.yawDegrees) || !std::isfinite(input.pitchDegrees)) {
            Reset();
            return out;
        }
        const float radius = std::clamp(Sane(input.radius), 1.0f, 80.0f);
        const float yaw = input.yawDegrees * 0.01745329252f;
        const float pitch = std::clamp(input.pitchDegrees, -89.0f, 89.0f) * 0.01745329252f;
        out.direction = {std::sin(yaw)*std::cos(pitch), std::sin(pitch),
                         std::cos(yaw)*std::cos(pitch)};
        const CameraPoint back = out.direction * -1.0f;
        const CameraPoint right{std::cos(yaw), 0.0f, -std::sin(yaw)};
        const CameraPoint up{-std::sin(yaw)*std::sin(pitch), std::cos(pitch),
                             -std::cos(yaw)*std::sin(pitch)};

        const auto ray = [&](CameraPoint a, CameraPoint b) {
            ++out.traceCount;
            auto hit = trace(a, b);
            if (!std::isfinite(hit.fraction)) hit = {true, 0.0f};
            hit.fraction = std::clamp(hit.fraction, 0.0f, 1.0f);
            if (hit.hit) ++out.sampleHits;
            return hit;
        };
        // Start below the head, so the ray can detect a low roof even when
        // the old head+75 pivot was already above/inside its polygon.
        const CameraPoint roofStart = input.head + CameraPoint{0.0f, -60.0f, 0.0f};
        const CameraPoint roofEnd = input.head + CameraPoint{0.0f, kAboveHead + radius + kGap, 0.0f};
        const auto roof = ray(roofStart, roofEnd);
        float heightLimit = kAboveHead;
        if (roof.hit) {
            const float roofY = roofStart.y + (roofEnd.y-roofStart.y) * roof.fraction;
            heightLimit = std::clamp(roofY-input.head.y-radius-kGap, -60.0f, kAboveHead);
            out.ceilingLimited = heightLimit < kAboveHead;
        }
        const auto pivotAt = [&](float distance) {
            const float above = std::min(kAboveHead * distance/kDistance, heightLimit);
            return input.head + CameraPoint{0.0f, above, 0.0f};
        };
        const auto sweptDistance = [&](CameraPoint pivot, float distance) {
            constexpr float diagonal = 0.70710678118f;
            const std::array<std::array<float, 2>, 9> samples{{
                {{0,0}}, {{1,0}}, {{-1,0}}, {{0,1}}, {{0,-1}},
                {{diagonal,diagonal}}, {{diagonal,-diagonal}},
                {{-diagonal,diagonal}}, {{-diagonal,-diagonal}}
            }};
            float available = distance;
            for (const auto& sample : samples) {
                const auto offset = right*(sample[0]*radius) + up*(sample[1]*radius);
                const auto start = pivot + offset;
                // Verify the shifted start is on the same free side of the
                // geometry, rather than tracing outwards from inside a wall.
                if ((sample[0] != 0 || sample[1] != 0) && ray(pivot, start).hit) {
                    available = 0.0f;
                    continue;
                }
                const float length = distance + kGap;
                const auto hit = ray(start, start + back*length);
                if (hit.hit)
                    available = std::min(available, std::max(0.0f, hit.fraction*length-kGap));
            }
            return available;
        };

        // Shrink monotonically while lowering the pivot. Every new height
        // gets its own sweep: a clear ray from the former height is insufficient.
        // If an adversarial staircase of obstacles exhausts the trace budget,
        // use the head pivot instead of returning a final unchecked placement.
        const auto fit = [&](float requested) {
            float distance = requested;
            for (int pass=0; pass<8; ++pass) {
                const float available = sweptDistance(pivotAt(distance), distance);
                if (available >= distance-0.001f) return distance;
                distance = available;
            }
            return 0.0f;
        };
        const float wanted = fit(kDistance);
        out.desiredDistance = wanted;
        const float dt = std::clamp(Sane(input.deltaSeconds), 0.0f, 0.1f);
        float placed = !initialized_ || wanted < distance_ ? wanted
            : std::min(wanted, distance_ + kReturnSpeed*dt);
        // Smoothing changes the pivot too. Recheck that actual pose, not only
        // the final desired pose, when the camera moves out of a tight space.
        // The desired pose was already checked; avoid repeating its rays
        // in open space. A smoothed intermediate pose has its own lower pivot.
        if (placed != wanted) placed = fit(placed);
        distance_ = placed;
        initialized_ = true;
        out.pivot = pivotAt(placed);
        out.position = out.pivot + back*placed;
        out.distance = placed;
        out.pivotAboveHead = out.pivot.y-input.head.y;
        out.collisionLimited = placed < kDistance-0.01f;
        out.active = Finite(out.position);
        return out;
    }

private:
    static float Sane(float value) { return std::isfinite(value) ? value : 0.0f; }
    static bool Finite(CameraPoint p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    bool initialized_ = false;
    float distance_ = 0.0f;
};

} // namespace ParryHotter
