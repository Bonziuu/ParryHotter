// Player-instance-only leg motion. Include after Gothic/Union headers.
// The native upper-body cast, animation handles, root and physics are untouched.
#pragma once
#if __has_include("CastingGait.hpp")
#include "CastingGait.hpp"
#else
#include "../hud/CastingGait.hpp"
#endif
#include <array>
#include <cmath>
#include <cstddef>

namespace ParryMovingCasting {
using namespace Gothic_I_Classic;

namespace Detail {
inline constexpr std::array<const char*, 6> kNames{{
    "Bip01 L Thigh", "Bip01 R Thigh", "Bip01 L Calf",
    "Bip01 R Calf", "Bip01 L Foot", "Bip01 R Foot"
}};
struct SavedNode {
    zCModelNodeInst* node = nullptr;
    zMAT4 original;
    zMAT4 written;
};
struct State {
    zCModel* model = nullptr;
    ParryHotter::CastingGait gait;
    ParryHotter::CastingGaitPose pose;
    std::array<SavedNode, 6> nodes{};
    bool applied = false;
    bool supported = false;
};
inline State current;

inline zCModelNodeInst* Find(zCModel* model, const char* name) {
    if (auto* node = model->SearchNode(zSTRING(name))) return node;
    // Compiled Gothic skeletons can use uppercase names, while the local
    // converter preserves "Bip01 L Thigh". Native SearchNode is case-sensitive.
    char upper[40]{};
    std::size_t i = 0;
    for (; name[i] && i + 1 < sizeof(upper); ++i)
        upper[i] = name[i] >= 'a' && name[i] <= 'z' ? char(name[i] - 'a' + 'A') : name[i];
    return model->SearchNode(zSTRING(upper));
}

inline bool Same(const zMAT4& a, const zMAT4& b) {
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (a[row][column] != b[row][column]) return false;
    return true;
}

inline bool SameRotation(const zMAT4& a, const zMAT4& b) {
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            if (a[row][column] != b[row][column]) return false;
    return true;
}

inline bool Finite(const zMAT4& matrix) {
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (!std::isfinite(matrix[row][column])) return false;
    return true;
}

inline void UndoOwned(zCModel* liveModel) {
    if (!liveModel || current.model != liveModel || !current.applied) return;
    current.applied = false;
    for (std::size_t i = 0; i < kNames.size(); ++i) {
        auto& saved = current.nodes[i];
        // Compare a stale pointer only; dereference the node returned by the
        // currently live model. Preserve newer native/plugin transformations.
        auto* node = Find(liveModel, kNames[i]);
        if (node && node == saved.node && SameRotation(node->trafo, saved.written)) {
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    node->trafo[row][column] = saved.original[row][column];
        }
        saved.node = nullptr;
    }
}

// Rotate only the 3x3 local orientation. Translate and homogeneous rows stay
// exact, so this correction cannot change root translation or collision motion.
inline bool Rotate(zMAT4& local, const zMAT4& parentToModel,
                   std::array<float, 3> modelAxis, float degrees) {
    if (!Finite(local) || !Finite(parentToModel) || !std::isfinite(degrees)) return false;
    std::array<float, 3> axis{};
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            axis[column] += parentToModel[row][column] * modelAxis[row];
    const float length = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
    if (!std::isfinite(length) || length < 0.0001f) return false;
    for (auto& value : axis) value /= length;
    const float angle = degrees * 0.01745329252f;
    const float c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
    const float x = axis[0], y = axis[1], z = axis[2];
    const float turn[3][3] = {
        {t*x*x+c, t*x*y-s*z, t*x*z+s*y},
        {t*x*y+s*z, t*y*y+c, t*y*z-s*x},
        {t*x*z-s*y, t*y*z+s*x, t*z*z+c}
    };
    const auto source = local;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            local[row][column] = 0.0f;
            for (int k = 0; k < 3; ++k)
                local[row][column] += turn[row][k] * source[k][column];
        }
    }
    return true;
}
} // namespace Detail

inline void Forget() { Detail::current = {}; }

inline void Restore(zCModel* liveModel) {
    Detail::UndoOwned(liveModel);
    Detail::current.pose = {};
    Detail::current.gait.Reset();
}

// actualX/Z are measured velocity (cm/s), not requested displacement. The
// caller owns context/ground checks and enables this only while casting.
inline const ParryHotter::CastingGaitPose& Update(
    zCModel* model, bool active, float actualX, float actualZ,
    float actorYawDegrees, float dt
) {
    if (Detail::current.model != model) {
        Forget(); // A new model does not establish that the old one survived.
        Detail::current.model = model;
    }
    if (!model || !active) Restore(model);
    Detail::current.pose = Detail::current.gait.Step(
        model && active, actualX, actualZ, actorYawDegrees, dt);
    return Detail::current.pose;
}

inline void BeforeBlend(zCModel* model) { Detail::UndoOwned(model); }

inline void AfterBlend(zCModel* model) {
    auto& state = Detail::current;
    if (!model || state.model != model) return;
    Detail::UndoOwned(model); // No accumulation even if a caller missed Before.
    state.supported = false;
    if (!state.pose.active) return;
    std::array<zCModelNodeInst*, 6> nodes{};
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        nodes[i] = Detail::Find(model, Detail::kNames[i]);
        if (!nodes[i] || !nodes[i]->parentNode || !Detail::Finite(nodes[i]->trafo)) return;
    }
    state.supported = true;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto* node = nodes[i];
        auto& saved = state.nodes[i];
        saved.original = node->trafo;
        const auto parentToModel = model->GetTrafoNodeToModel(node->parentNode);
        const auto axis = i < 2 ? state.pose.hipAxis
                               : std::array<float, 3>{{1.0f, 0.0f, 0.0f}};
        if (!Detail::Rotate(node->trafo, parentToModel, axis, state.pose.angles[i])) {
            // All-or-nothing: restore transformations already applied here.
            Detail::UndoOwned(model);
            state.supported = false;
            return;
        }
        saved.node = node;
        saved.written = node->trafo;
        state.applied = true;
    }
}

inline bool Applied() { return Detail::current.applied; }
inline bool Supported() { return Detail::current.supported; }
} // namespace ParryMovingCasting
