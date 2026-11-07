// Optional player-only casting animation bindings for the Gothic I adapter.
// Include after the Gothic/Union headers. Native TransitionToInvest/Cast/Stand
// still owns playback; this helper supplies six IDs from the validated overlay.
// Restore must run while its supplied AI/model are live, before world teardown.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ParryCasting {
using namespace Gothic_I_Classic;

namespace Detail {
constexpr std::size_t kCount = 6;
using Field = int oCAIHuman::*;
inline constexpr std::array<Field, kCount> kFields{{
    &oCAIHuman::t_stand_2_cast,
    &oCAIHuman::s_cast,
    &oCAIHuman::t_cast_2_shoot,
    &oCAIHuman::t_cast_2_stand,
    &oCAIHuman::s_shoot,
    &oCAIHuman::t_shoot_2_stand,
}};
inline constexpr std::array<const char*, kCount> kNames{{
    "T_PH_PREPARE", "S_PH_CHARGE", "T_PH_RELEASE",
    "T_PH_CANCEL", "S_PH_RELEASE", "T_PH_RECOVER",
}};

struct Binding {
    oCAIHuman* ai = nullptr;
    zCModel* model = nullptr;
    std::array<int, kCount> originals{};
    std::array<int, kCount> custom{};
};
inline Binding current;
inline std::uint64_t generation = 0;

inline bool Resolve(zCModel* model, std::array<int, kCount>& ids) {
    for (std::size_t i = 0; i < kCount; ++i) {
        // SearchAni belongs to zCModelPrototype, not zCModel. These are the
        // actual Gothic I model APIs, including the valid animation ID zero.
        const int id = model->GetAniIDFromAniName(zSTRING(kNames[i]));
        if (id < 0) return false;
        auto* animation = model->GetAniFromAniID(id);
        if (!animation || animation->GetAniID() != id) return false;
        ids[i] = id;
    }
    return true;
}
} // namespace Detail

// Forget only: stale native pointers are never dereferenced on a model/world
// replacement. The caller may use Restore instead while the old pair is live.
inline void Forget() {
    Detail::current = {};
    ++Detail::generation;
}

inline void Restore(oCAIHuman* liveAI, zCModel* liveModel) {
    const auto old = Detail::current;
    Forget(); // Native StopAni may call back into plugin lifecycle handling.
    const auto generation = Detail::generation;
    if (!old.ai || !old.model || old.ai != liveAI || old.model != liveModel)
        return;

    // Do not replace handles changed by the engine or another plugin since
    // our last Bind. Clear our ownership before any native cancellation call.
    for (std::size_t i = 0; i < Detail::kCount; ++i) {
        const auto field = Detail::kFields[i];
        if (liveAI->*field == old.custom[i]) liveAI->*field = old.originals[i];
    }
    for (std::size_t i = 0; i < Detail::kCount; ++i) {
        const int id = old.custom[i];
        // An overlay can change ID mappings without changing the model pointer.
        // Never stop a foreign animation now occupying one of our former IDs.
        if (liveModel->GetAniIDFromAniName(zSTRING(Detail::kNames[i])) != id)
            continue;
        auto* animation = liveModel->GetAniFromAniID(id);
        if (animation && animation->GetAniID() == id &&
            liveModel->IsAniActive(animation)) liveModel->StopAni(id);
        // A cancellation callback can forget this pair during world teardown
        // without binding a replacement. Any lifecycle change invalidates the
        // supplied pointers for the remaining native calls in this loop.
        if (Detail::generation != generation) return;
    }
}

inline bool Bind(oCAIHuman* ai, zCModel* model) {
    if (!ai || !model) {
        Restore(ai, model);
        return false;
    }
    std::array<int, Detail::kCount> ids{};
    // An incomplete/unloaded overlay is an all-or-nothing native fallback.
    if (!Detail::Resolve(model, ids)) {
        Restore(ai, model);
        return false;
    }

    auto& binding = Detail::current;
    if (binding.ai != ai || binding.model != model) {
        // A different live pair does not establish that the old pair survived.
        Forget();
        binding.ai = ai;
        binding.model = model;
        for (std::size_t i = 0; i < Detail::kCount; ++i)
            binding.originals[i] = ai->*Detail::kFields[i];
    } else {
        // Native SetAnimations may refresh its handles while aiming. Preserve
        // those new native values for restoration instead of caching our IDs.
        for (std::size_t i = 0; i < Detail::kCount; ++i) {
            const int value = ai->*Detail::kFields[i];
            if (value != binding.custom[i]) binding.originals[i] = value;
        }
    }
    ++Detail::generation;
    binding.custom = ids;
    for (std::size_t i = 0; i < Detail::kCount; ++i)
        ai->*Detail::kFields[i] = ids[i];
    return true;
}
} // namespace ParryCasting
