// Animated spell glyph anchored to a projected NPC, independent of ZenGin.
// The engine adapter supplies the projection and checks line of sight.
#pragma once
#include <algorithm>
#include <cmath>

namespace ParryHotter {

struct TargetGlyphInput {
    bool aiming = false;
    bool targetValid = false;
    bool spellReady = false;
    bool inFront = false;
    bool lineOfSight = false;
    // All dimensions use the same coordinates: pixels or ZenGin's 0..8192.
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float viewportWidth = 8192.0f;
    float viewportHeight = 8192.0f;
    float baseSize = 400.0f;
    double seconds = 0.0;
};

struct TargetGlyphLayout {
    bool visible = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    // zCView uses 0 for opaque and 255 for fully transparent.
    int transparency = 255;
};

inline TargetGlyphLayout ComputeTargetGlyph(const TargetGlyphInput& in) {
    TargetGlyphLayout out{};
    if (!in.aiming || !in.targetValid || !in.spellReady || !in.inFront ||
        !in.lineOfSight || !std::isfinite(in.anchorX) ||
        !std::isfinite(in.anchorY) || !std::isfinite(in.viewportWidth) ||
        !std::isfinite(in.viewportHeight) || !std::isfinite(in.baseSize) ||
        !std::isfinite(in.seconds) || in.viewportWidth < 2.0f ||
        in.viewportHeight < 2.0f || in.viewportWidth > 1000000.0f ||
        in.viewportHeight > 1000000.0f || in.baseSize <= 0.0f ||
        in.anchorX < 0.0f || in.anchorX >= in.viewportWidth ||
        in.anchorY < 0.0f || in.anchorY >= in.viewportHeight) {
        return out;
    }

    // Absolute time avoids frame-rate drift, even when rendering is paused.
    constexpr double pi = 3.14159265358979323846;
    const double phase = std::fmod(in.seconds, 2.4) * (2.0 * pi / 2.4);
    const float pulse = static_cast<float>(std::sin(phase));
    const float sway = static_cast<float>(std::sin(phase * 2.0));
    const float maxSize = std::floor(std::min(in.viewportWidth, in.viewportHeight));
    const float base = std::min(in.baseSize, maxSize / 1.06f);
    const float animatedSize = std::clamp(base * (1.0f + 0.06f * pulse),
                                        1.0f, maxSize);
    const int size = std::max(1, static_cast<int>(std::lround(animatedSize)));
    const float centerX = in.anchorX + base * 0.03f * sway;
    const float centerY = in.anchorY - base * 0.08f * pulse;
    const int maxX = std::max(0, static_cast<int>(in.viewportWidth) - size);
    const int maxY = std::max(0, static_cast<int>(in.viewportHeight) - size);
    out.visible = true;
    out.x = std::clamp(static_cast<int>(std::lround(centerX - size * 0.5f)),
                       0, maxX);
    out.y = std::clamp(static_cast<int>(std::lround(centerY - size * 0.5f)),
                       0, maxY);
    out.width = size;
    out.height = size;
    out.transparency = static_cast<int>(std::lround(18.0f * (1.0f - pulse)));
    return out;
}

} // namespace ParryHotter
