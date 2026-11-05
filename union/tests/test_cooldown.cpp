#include "../hud/SpellCooldown.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ParryHotter;
using namespace std::chrono;

int main() {
    SpellCooldown cd;
    using Clock = SpellCooldown::Clock;
    const auto t0 = Clock::time_point{} + seconds(600);
    assert(SpellCooldown::Count == 6);
    assert(SpellCooldown::ManaCosts[0] == 3);
    assert(SpellCooldown::ManaCosts[2] == 10);
    assert(SpellCooldown::ManaCosts[5] == 5);

    assert(!cd.Ready(-1, 200, t0));
    assert(!cd.Ready(6, 200, t0));
    assert(!cd.Record(7, 200, t0));
    assert(!cd.Ready(2, 9, t0));
    assert(cd.Ready(2, 10, t0));
    assert(!cd.Record(2, 9, t0));
    assert(cd.Record(2, 10, t0));
    assert(!cd.Ready(2, 200, t0));
    assert(cd.RemainingMs(2, t0) == 2500);
    assert(cd.RemainingMs(2, t0 + milliseconds(500)) == 2000);
    assert(!cd.Record(2, 200, t0 + milliseconds(500)));
    // Gothic can consume the required mana *during* investment.
    // A spell already prepared must still be allowed to finish at zero mana,
    // while the UI disallows starting an unrelated new spell.
    assert(!cd.Ready(0, 0, t0));
    assert(cd.ReadyByTime(3, t0));
    assert(cd.MarkPreparedCast(3, t0));
    assert(!cd.ReadyByTime(3, t0 + milliseconds(400)));
    assert(!cd.MarkPreparedCast(3, t0 + milliseconds(400)));
    assert(cd.RemainingMs(3, t0 + milliseconds(400)) == 1500);

    // Other slots have independent cooldowns; energy required per spell.
    assert(cd.Ready(0, 3, t0));
    assert(cd.Record(0, 3, t0));
    assert(cd.Ready(5, 5, t0));
    assert(cd.Record(5, 5, t0));
    assert(!cd.Ready(5, 4, t0 + seconds(10)));
    assert(cd.Ready(0, 200, t0 + milliseconds(600)));
    assert(cd.Ready(2, 200, t0 + milliseconds(2500)));
    assert(cd.Ready(5, 200, t0 + milliseconds(5200)));

    // Never count a negative time gap as ready; time can only go forward.
    assert(!cd.Ready(5, 200, t0 - seconds(1)));
    cd.Reset();
    for (int i = 0; i < 6; ++i) {
        assert(cd.Ready(i, 200, t0));
        assert(cd.RemainingMs(i, t0) == 0);
    }
    std::cout << "Parry Hotter mana/cooldown logic: OK\n";
}
