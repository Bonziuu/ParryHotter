// Parry Hotter: six independent spell cooldowns and mana requirements.
// Pure C++17 logic, testable without Gothic I, Union, or Windows.
// This does not deduct mana: native Gothic spellbook is responsible.
#pragma once
#include <array>
#include <chrono>
#include <cstddef>

namespace ParryHotter {

class SpellCooldown final {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr int Count = 6;
    static constexpr std::array<int, Count> ManaCosts{{3, 3, 10, 5, 8, 5}};
    static constexpr std::array<int, Count> CooldownMs{{600, 3600, 2500, 1900, 2800, 5200}};

    bool Ready(int slot, int mana, TimePoint now) const {
        if (slot < 0 || slot >= Count || mana < ManaCosts[slot]) return false;
        if (!used_[slot]) return true;
        return now - last_[slot] >= std::chrono::milliseconds(CooldownMs[slot]);
    }

    int RemainingMs(int slot, TimePoint now) const {
        if (slot < 0 || slot >= Count || !used_[slot]) return 0;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_[slot]
        ).count();
        const auto left = static_cast<long long>(CooldownMs[slot]) - elapsed;
        return left > 0 ? static_cast<int>(left) : 0;
    }

    bool Record(int slot, int mana, TimePoint now) {
        if (!Ready(slot, mana, now)) return false;
        last_[slot] = now;
        used_[slot] = true;
        return true;
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
