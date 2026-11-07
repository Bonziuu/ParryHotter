#include "../hud/SpellImpactTag.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace ParryHotter;
using namespace std::chrono;
int main() {
    SpellImpactTag tag;
    const auto now = SpellImpactTag::Clock::time_point{} + seconds(50);
    assert(tag.Active(now) == 0);
    tag.MarkCast(2, now);
    assert(tag.Active(now) == 3);
    assert(tag.Active(now + seconds(6)) == 3);
    assert(tag.Active(now + seconds(7)) == 0);
    assert(tag.Expired(now + seconds(7)));
    tag.MarkCast(5, now + seconds(8));
    assert(tag.Active(now + seconds(8)) == 6);
    for (int slot = 6; slot < 10; ++slot) {
        tag.MarkCast(slot, now + seconds(9)); // selectable blanks cannot tag hits
        assert(tag.Active(now + seconds(9)) == 6);
    }
    tag.MarkCast(10, now + seconds(9)); // invalid input ignored
    assert(tag.Active(now + seconds(9)) == 6);
    tag.MarkCast(-1, now + seconds(9));
    assert(tag.Active(now + seconds(9)) == 6);
    tag.Reset();
    assert(tag.Active(now + seconds(10)) == 0);
    std::cout << "Parry Hotter impact tag window: OK\n";
}
