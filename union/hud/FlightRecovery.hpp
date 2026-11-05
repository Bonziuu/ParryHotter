// Parry Hotter: track a brief spell-induced flight until a safe landing.
// Pure C++17 state, no ownership of NPCs and no physics overrides.
// Game integration only observes ZenGin movement; it never teleports or
// disables collisions, gravity or the victim's standard AI.
#pragma once
#include <chrono>

namespace ParryHotter {

enum class FlightResult {
    Inactive,
    Flying,
    Landed,
    Cancelled,
    TimedOut
};

class FlightRecovery {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto MaxDuration = std::chrono::milliseconds(4600);
    static constexpr auto MinFlight = std::chrono::milliseconds(450);

    void Start(TimePoint now) {
        started_ = now;
        consecutiveGround_ = 0;
        active_ = true;
    }

    FlightResult Step(TimePoint now, bool victimValid,
                      bool nearFloor, float verticalSpeed) {
        if (!active_) return FlightResult::Inactive;
        if (!victimValid) return Finish(FlightResult::Cancelled);
        if (now < started_) return Finish(FlightResult::Cancelled);
        if (now - started_ >= MaxDuration)
            return Finish(FlightResult::TimedOut);
        if (now - started_ < MinFlight) {
            consecutiveGround_ = 0;
            return FlightResult::Flying;
        }
        const bool settled = nearFloor && verticalSpeed <= 30.0f
                            && verticalSpeed >= -110.0f;
        consecutiveGround_ = settled ? consecutiveGround_ + 1 : 0;
        if (consecutiveGround_ >= 2)
            return Finish(FlightResult::Landed);
        return FlightResult::Flying;
    }

    void Reset() {
        active_ = false;
        consecutiveGround_ = 0;
    }

    bool Active() const { return active_; }

private:
    FlightResult Finish(FlightResult result) {
        Reset();
        return result;
    }

    TimePoint started_{};
    int consecutiveGround_ = 0;
    bool active_ = false;
};
} // namespace ParryHotter
