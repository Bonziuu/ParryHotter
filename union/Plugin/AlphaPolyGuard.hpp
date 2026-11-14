// G1 1.08k native renderer guard. Existing queued geometry is never changed.
#pragma once
#if __has_include("AlphaPolyBudget.hpp")
#include "AlphaPolyBudget.hpp"
#include "CastLog.hpp"
#else
#include "../hud/AlphaPolyBudget.hpp"
#include "../hud/CastLog.hpp"
#endif
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace ParryAlphaGuard {
using namespace Gothic_I_Classic;
inline constexpr std::size_t CounterOffset = 0x80A18;
inline bool supported = false;
inline zCRnd_D3D* renderer = nullptr; // Compared only; never retained.
inline ParryHotter::AlphaPolyBudget budget;

inline std::uint64_t NowMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline void Configure(ParryHotter::CastLog& log) {
    // These instructions lie beyond the entry detour installed by Union.
    // A modified pool/limit must keep its own policy; never read its layout.
    constexpr unsigned char load[] = {0x8B, 0x86, 0x18, 0x0A, 0x08, 0x00};
    constexpr unsigned char limit[] = {0x3D, 0x00, 0x08, 0x00, 0x00};
    supported = std::memcmp(reinterpret_cast<const void*>(0x0071633B), load, sizeof(load)) == 0 &&
        std::memcmp(reinterpret_cast<const void*>(0x00716341), limit, sizeof(limit)) == 0;
    renderer = nullptr;
    budget.Reset();
    log.Line("alpha_guard enabled=%d capacity=2048 counterOffset=0x80A18 reason=%s",
             supported ? 1 : 0, supported ? "native_g1_signature" : "unsupported_renderer_patch");
}

inline bool Supported() { return supported; }

inline int ReadCount(const zCRnd_D3D* value) {
    int count = 0;
    // The pinned SDK comments have a different offset; use the native
    // instruction above as the authority, and read without writing/aliasing.
    std::memcpy(&count, reinterpret_cast<const unsigned char*>(value)+CounterOffset, sizeof(count));
    return count;
}

inline void Select(zCRnd_D3D* value) {
    if (renderer == value) return;
    renderer = value;
    budget.Reset();
}

inline bool Before(zCRnd_D3D* value, zCPolygon* poly, zCMaterial* material,
                   ParryHotter::CastLog& log) {
    if (!supported) return true;
    if (!value) return false;
    Select(value);
    const int count = ReadCount(value);
    const auto result = budget.Before(count);
    if (result.report) {
        char materialName[96] = "<none>", textureName[96] = "<none>";
        if (material) {
            // Copy both names while native arguments are live. No engine
            // object or pointer to its text is kept after this callback.
            const char* name = material->GetName().ToChar();
            const char* texture = material->GetTextureName().ToChar();
            std::snprintf(materialName, sizeof(materialName), "%s", name ? name : "<empty>");
            std::snprintf(textureName, sizeof(textureName), "%s", texture ? texture : "<empty>");
        }
        log.Line("alpha_guard phase=drop native=%d capacity=2048 peak=%d droppedBatch=%llu "
                 "droppedTotal=%llu badCounter=%d poly=%p material=%p verts=%d name=[%s] texture=[%s]",
                 count, budget.PeakObserved(), static_cast<unsigned long long>(budget.BatchDropped()),
                 static_cast<unsigned long long>(budget.TotalDropped()), result.badCounter ? 1 : 0,
                 static_cast<void*>(poly), static_cast<void*>(material), poly ? poly->numClipVert : 0,
                 materialName, textureName);
    }
    return result.admit;
}

inline void AfterQueued(zCRnd_D3D* value) {
    if (!supported || !value) return;
    Select(value);
    budget.AfterQueued(ReadCount(value));
}

inline void AfterSort(zCRnd_D3D* value, ParryHotter::CastLog& log) {
    if (!supported || !value) return;
    Select(value);
    const int count = ReadCount(value);
    const auto result = budget.AfterSort(count, NowMs());
    if (!result.report) return;
    log.Line("alpha_guard phase=batch batch=%llu nativeAfterSort=%d drain=%d peak=%d "
             "admittedCalls=%llu droppedBatch=%llu badCounterSamples=%llu intervalBatches=%llu "
             "intervalDropped=%llu intervalBadCounters=%llu intervalPeak=%d droppedTotal=%llu",
             static_cast<unsigned long long>(result.batch), count, result.drainObserved ? 1 : 0,
             result.peakObserved, static_cast<unsigned long long>(result.admittedCalls),
             static_cast<unsigned long long>(result.dropped), static_cast<unsigned long long>(result.badCounters),
             static_cast<unsigned long long>(result.intervalBatches),
             static_cast<unsigned long long>(result.intervalDropped),
             static_cast<unsigned long long>(result.intervalBadCounters), result.intervalPeakObserved,
             static_cast<unsigned long long>(result.totalDropped));
}
} // namespace ParryAlphaGuard
