#include "../hud/FlightRecovery.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ParryHotter;
using namespace std::chrono;

int main() {
    const auto t = FlightRecovery::Clock::time_point{} + seconds(10);
    FlightRecovery flight;
    assert(flight.Step(t, true, true, 0) == FlightResult::Inactive);
    flight.Start(t);
    assert(flight.Active());
    assert(flight.Step(t + milliseconds(100), true, true, 0) ==
           FlightResult::Flying); // avoid instantaneous "landing" at impact
    assert(flight.Step(t + milliseconds(470), true, true, -5) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(500), true, true, -5) ==
           FlightResult::Landed);
    assert(!flight.Active());
    assert(flight.Step(t + milliseconds(600), true, true, 0) ==
           FlightResult::Inactive);

    // Rebound resets consecutive contact: do not mistake a bounce as landing.
    flight.Start(t);
    assert(flight.Step(t + milliseconds(470), true, true, 0) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(500), true, false, 50) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(530), true, true, 0) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(560), true, true, 0) ==
           FlightResult::Landed);

    // Flying continuously expires; no stuck persistent state.
    flight.Start(t);
    assert(flight.Step(t + milliseconds(4599), true, false, -40) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(4600), true, false, -40) ==
           FlightResult::TimedOut);
    assert(!flight.Active());

    // Corpse/world unloading cancels without forcing AI, velocity or position.
    flight.Start(t);
    assert(flight.Step(t + milliseconds(1000), false, true, 0) ==
           FlightResult::Cancelled);
    flight.Start(t);
    flight.Reset();
    assert(flight.Step(t + seconds(1), true, true, 0) ==
           FlightResult::Inactive);

    flight.Start(t);
    assert(flight.Step(t - milliseconds(1), true, true, 0) ==
           FlightResult::Cancelled);

    // Do not report landing during high-speed upward/downward movement.
    flight.Start(t);
    assert(flight.Step(t + milliseconds(500), true, true, 115) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(550), true, true, -220) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(600), true, true, 20) ==
           FlightResult::Flying);
    assert(flight.Step(t + milliseconds(650), true, true, 15) ==
           FlightResult::Landed);

    std::cout << "Parry Hotter flight landing/timeout: OK\n";
}
