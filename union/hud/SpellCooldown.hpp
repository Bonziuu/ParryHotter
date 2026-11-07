// Parry Hotter: independent spell cooldowns and mana requirements.
// Pure C++17 logic, testable without Gothic I, Union, or Windows.
// This does not deduct mana: native Gothic spellbook is responsible.
#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include "SpellHudState.hpp"

namespace ParryHotter {

class SpellCooldown final {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr int Count = kSpellCount;
    static constexpr std::array<int, Count> ManaCosts{{3, 3, 10, 5, 8, 5, 0, 0, 0, 0}};
    static constexpr std::array<int, Count> CooldownMs{{
        600, 3600, 2500, 1900, 2800, 5200, 0, 0, 0, 0
    }};

    bool ReadyByTime(int slot, TimePoint now) const {
        if (slot < 0 || slot >= Count || !kSpellAvailable[slot]) return false;
        if (!used_[slot]) return true;
        return now - last_[slot] >= std::chrono::milliseconds(CooldownMs[slot]);
    }

    bool Ready(int slot, int mana, TimePoint now) const {
        return slot >= 0 && slot < Count && mana >= ManaCosts[slot] &&
               ReadyByTime(slot, now);
    }

    int RemainingMs(int slot, TimePoint now) const {
        if (slot < 0 || slot >= Count || !used_[slot]) return 0;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_[slot]
        ).count();
        const auto left = static_cast<long long>(CooldownMs[slot]) - elapsed;
        return left > 0 ? static_cast<int>(left) : 0;
    }

    // Called after Gothic has already invested (deducted) mana.
    // No second mana check: a spell must still be castable after investment.
    bool MarkPreparedCast(int slot, TimePoint now) {
        if (!ReadyByTime(slot, now)) return false;
        last_[slot] = now;
        used_[slot] = true;
        return true;
    }

    bool Record(int slot, int mana, TimePoint now) {
        return Ready(slot, mana, now) && MarkPreparedCast(slot, now);
    }

    void Reset() {
        used_.fill(false);
        last_.fill(TimePoint{});
    }

private:
    std::array<TimePoint, Count> last_{};
    std::array<bool, Count> used_{};
};
} // namespace ParryHotter
