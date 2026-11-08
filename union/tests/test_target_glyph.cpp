#include "../hud/TargetGlyph.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace ParryHotter;

TargetGlyphInput ValidInput() {
    TargetGlyphInput in{};
    in.aiming = in.targetValid = in.spellReady = true;
    in.inFront = in.lineOfSight = true;
    in.anchorX = 4096.0f;
    in.anchorY = 3000.0f;
    return in;
}

void AssertFits(const TargetGlyphInput& in, const TargetGlyphLayout& out) {
    assert(out.visible);
    assert(out.width > 0 && out.height == out.width);
    assert(out.x >= 0 && out.y >= 0);
    assert(out.x + out.width <= in.viewportWidth);
    assert(out.y + out.height <= in.viewportHeight);
    assert(out.alpha >= 219 && out.alpha <= 255);
}

int main() {
    auto in = ValidInput();
    const auto initial = ComputeTargetGlyph(in);
    AssertFits(in, initial);
    assert(initial.x == 3896 && initial.y == 2800 && initial.width == 400);

    // Every cast-readiness and projection condition actually gates the glyph.
    for (int flag = 0; flag < 5; ++flag) {
        auto blocked = in;
        switch (flag) {
            case 0: blocked.aiming = false; break;
            case 1: blocked.targetValid = false; break;
            case 2: blocked.spellReady = false; break;
            case 3: blocked.inFront = false; break;
            case 4: blocked.lineOfSight = false; break;
        }
        const auto hidden = ComputeTargetGlyph(blocked);
        assert(!hidden.visible && hidden.alpha == 0);
    }

    // Bob, pulse and glow all change; there is no fixed reticle icon.
    in.seconds = 0.6;
    const auto raised = ComputeTargetGlyph(in);
    assert(raised.width > initial.width && raised.y < initial.y);
    assert(raised.alpha > initial.alpha);
    in.seconds = 1.8;
    const auto lowered = ComputeTargetGlyph(in);
    assert(lowered.width < initial.width && lowered.y > initial.y);
    assert(lowered.alpha < initial.alpha);

    // Same elapsed time gives the same layout at 30 and 144 FPS.
    for (const int fps : {30, 144}) {
        double elapsed = 0.0;
        for (int frame = 0; frame < fps * 3; ++frame) elapsed += 1.0 / fps;
        in.seconds = elapsed;
        const auto sampled = ComputeTargetGlyph(in);
        in.seconds = 3.0;
        const auto exact = ComputeTargetGlyph(in);
        assert(sampled.x == exact.x && sampled.y == exact.y);
        assert(sampled.width == exact.width);
        assert(sampled.alpha == exact.alpha);
    }

    // Projection tracks the NPC's new position.
    in = ValidInput();
    in.anchorX += 500;
    const auto moved = ComputeTargetGlyph(in);
    assert(moved.x == initial.x + 500 && moved.y == initial.y);

    // An offscreen anchor must hide, not become a misleading edge indicator.
    for (const float value : {-1.0f, 8192.0f, 10000.0f}) {
        in = ValidInput();
        in.anchorX = value;
        assert(!ComputeTargetGlyph(in).visible);
        in = ValidInput();
        in.anchorY = value;
        assert(!ComputeTargetGlyph(in).visible);
    }
    for (const float x : {0.0f, 8191.0f}) {
        for (const float y : {0.0f, 8191.0f}) {
            in = ValidInput();
            in.anchorX = x;
            in.anchorY = y;
            in.seconds = 0.6;
            AssertFits(in, ComputeTargetGlyph(in));
        }
    }

    // Pixel-sized viewports and an oversized glyph stay within the bounds.
    in = ValidInput();
    in.viewportWidth = 1920;
    in.viewportHeight = 1080;
    in.anchorX = 1900;
    in.anchorY = 20;
    in.baseSize = 80;
    AssertFits(in, ComputeTargetGlyph(in));
    in.baseSize = 100000;
    AssertFits(in, ComputeTargetGlyph(in));
    in.viewportWidth = 2.9f;
    in.viewportHeight = 3.1f;
    in.anchorX = 1.0f;
    in.anchorY = 1.0f;
    AssertFits(in, ComputeTargetGlyph(in));

    // Bad dimensions/timestamps cannot generate invalid renderer coordinates.
    for (const float bad : {0.0f, -1.0f,
                            std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()}) {
        in = ValidInput();
        in.viewportWidth = bad;
        assert(!ComputeTargetGlyph(in).visible);
        in = ValidInput();
        in.viewportHeight = bad;
        assert(!ComputeTargetGlyph(in).visible);
        in = ValidInput();
        in.baseSize = bad;
        assert(!ComputeTargetGlyph(in).visible);
    }
    in = ValidInput();
    in.anchorX = std::numeric_limits<float>::quiet_NaN();
    assert(!ComputeTargetGlyph(in).visible);
    in = ValidInput();
    in.seconds = std::numeric_limits<double>::infinity();
    assert(!ComputeTargetGlyph(in).visible);
    in.seconds = 1.0e12;
    AssertFits(in, ComputeTargetGlyph(in));
    std::cout << "Parry Hotter projected floating spell glyph: OK\n";
}
