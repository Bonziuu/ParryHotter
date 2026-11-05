// Protect vanilla Gothic magic: tag only a short window after a Parry cast.
// The script global PH_SELECTED_SPELL is cleared when this expires.
// Pure C++17: no Union/Zengin headers required.
#pragma once
#include <chrono>

namespace ParryHotter {

class SpellImpactTag final {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr int Count = 6;
    static constexpr auto Duration = std::chrono::seconds(7);

    void MarkCast(int slot, TimePoint now) {
        if (slot < 0 || slot >= Count) return;
        slot_ = slot + 1;
        expires_ = now + Duration;
    }

    bool Expired(TimePoint now) const {
        return slot_ != 0 && now >= expires_;
    }

    int Active(TimePoint now) const {
        return (slot_ != 0 && now < expires_) ? slot_ : 0;
    }

    void Reset() {
        slot_ = 0;
        expires_ = TimePoint{};
    }

private:
    int slot_ = 0; // 0 = vanilla/default DEV handling
    TimePoint expires_{};
};
} // namespace ParryHotter
