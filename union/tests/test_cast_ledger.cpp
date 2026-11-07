#include "../hud/CastLedger.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace ParryHotter;
using namespace std::chrono;

int main() {
    using State = CastLedger::MatchState;
    const CastLedger::TimePoint start{};
    CastLedger ledger;

    // An actual effect is mandatory, even for a synchronous provisional hit.
    assert(ledger.Stage(0, 20, 1, 0, start) == CastLedger::NoId);
    assert(ledger.Stage(10, 0, 1, 0, start) == CastLedger::NoId);
    assert(ledger.Stage(10, 20, -1, 0, start) == CastLedger::NoId);
    assert(ledger.Stage(10, 20, 1, -1, start) == CastLedger::NoId);
    assert(ledger.Size(start) == 0);

    const auto old = ledger.Stage(10, 20, 1, 0, start);
    assert(old != CastLedger::NoId);
    auto hit = ledger.Resolve(10, 20, 1, start);
    assert(hit.state == State::Matched && hit.id == old && hit.slot == 0);
    assert(!hit.confirmed); // native Cast() has not returned yet
    assert(ledger.FindById(old, start).slot == 0);
    assert(ledger.TryMarkImpact(old, start)); // synchronous damage is supported
    assert(!ledger.TryMarkImpact(old, start));
    assert(ledger.Confirm(old, start));
    assert(ledger.Resolve(10, 20, 1, start).confirmed);

    // A second projectile must not overwrite the first one's spell or target.
    const auto newer = ledger.Stage(11, 21, 2, 9, start + milliseconds(600));
    assert(ledger.Confirm(newer, start + milliseconds(600)));
    hit = ledger.Resolve(10, 20, 1, start + seconds(2));
    assert(hit.state == State::Matched && hit.id == old && hit.slot == 0);
    hit = ledger.Resolve(11, 21, 2, start + seconds(2));
    assert(hit.state == State::Matched && hit.id == newer && hit.slot == 9);
    assert(ledger.TryMarkImpact(newer, start + seconds(2)));

    // Neither a different victim nor a matching spell ID is enough evidence.
    assert(ledger.Resolve(10, 21, 1, start).state == State::NoMatch);
    assert(ledger.Resolve(10, 20, 2, start).state == State::NoMatch);
    assert(ledger.Resolve(0, 20, 1, start).state == State::NoMatch);
    assert(ledger.Resolve(999, 20, 1, start).state == State::NoMatch);

    // Exact duplicate identities are ambiguous, even if the slots are equal.
    const auto duplicate = ledger.Stage(10, 20, 1, 0, start + seconds(1));
    hit = ledger.Resolve(10, 20, 1, start + seconds(2));
    assert(hit.state == State::Ambiguous && hit.id == CastLedger::NoId);
    assert(hit.slot == -1 && !hit.confirmed);
    assert(!ledger.TryMarkImpact(duplicate, start + seconds(2)));
    assert(ledger.Revoke(duplicate));
    assert(ledger.Resolve(10, 20, 1, start + seconds(2)).id == old);

    // Native rejected casts lose the provisional entry and cannot hit later.
    const auto rejected = ledger.Stage(12, 20, 3, 2, start + seconds(2));
    assert(ledger.Revoke(rejected));
    assert(!ledger.Confirm(rejected, start + seconds(2)));
    assert(!ledger.TryMarkImpact(rejected, start + seconds(2)));
    assert(ledger.Resolve(12, 20, 3, start + seconds(2)).state == State::NoMatch);

    // Expiry is exactly seven seconds per cast, not seven since latest cast.
    assert(ledger.Resolve(10, 20, 1, start + seconds(7) - milliseconds(1)).state
           == State::Matched);
    assert(ledger.Resolve(10, 20, 1, start + seconds(7)).state == State::NoMatch);
    assert(ledger.Resolve(11, 21, 2, start + seconds(7)).state == State::Matched);
    assert(!ledger.Confirm(old, start + seconds(7)));
    assert(ledger.FindById(old, start + seconds(7)).state == State::NoMatch);
    assert(ledger.Size(start + seconds(7)) == 1);
    ledger.Expire(start + seconds(8));
    assert(ledger.Size(start + seconds(8)) == 0);

    // Reusing freed storage does not resurrect stale handles.
    const auto recycled = ledger.Stage(10, 20, 1, 5, start + seconds(8));
    assert(recycled != old && recycled != newer);
    assert(!ledger.Revoke(old));
    assert(ledger.Resolve(10, 20, 1, start + seconds(8)).id == recycled);
    ledger.Clear();
    assert(ledger.FindById(recycled, start + seconds(8)).state == State::NoMatch);
    const auto afterLoad = ledger.Stage(10, 20, 1, 5, start + seconds(8));
    assert(afterLoad != recycled);
    assert(!ledger.TryMarkImpact(recycled, start + seconds(8)));
    assert(ledger.Size(start + seconds(8)) == 1);

    // Capacity exhaustion must preserve all earlier live projectiles.
    ledger.Clear();
    CastLedger::Id first = CastLedger::NoId;
    for (std::size_t i = 0; i < CastLedger::Capacity; ++i) {
        const auto id = ledger.Stage(100 + i, 200 + i, 0, 0, start);
        assert(id != CastLedger::NoId);
        if (i == 0) first = id;
    }
    assert(ledger.Size(start) == CastLedger::Capacity);
    assert(ledger.Stage(999, 888, 2, 7, start) == CastLedger::NoId);
    assert(ledger.Resolve(100, 200, 0, start).id == first);
    assert(ledger.Stage(999, 888, 2, 7, start + seconds(7)) != CastLedger::NoId);
    assert(ledger.Size(start + seconds(7)) == 1);
    assert(!ledger.Confirm(CastLedger::NoId, start));
    assert(!ledger.Revoke(CastLedger::NoId));

    // Charging and launched projectile FX remain one cast, even when the
    // native spell swaps the effect object inside Cast().
    ledger.Clear();
    const auto swapped = ledger.Stage(1000, 2000, 9, 0, start);
    assert(ledger.AddEffectAlias(swapped, 1001, start));
    assert(ledger.AddEffectAlias(swapped, 1001, start));
    assert(ledger.Resolve(1000, 2000, 9, start).id == swapped);
    assert(ledger.Resolve(1001, 2000, 9, start).id == swapped);
    assert(ledger.TryMarkImpact(swapped, start));
    assert(!ledger.TryMarkImpact(swapped, start));
    assert(ledger.AddEffectAlias(swapped, 1002, start));
    assert(ledger.AddEffectAlias(swapped, 1003, start));
    assert(!ledger.AddEffectAlias(swapped, 1004, start));
    assert(!ledger.AddEffectAlias(swapped, 0, start));
    const auto overlapping = ledger.Stage(1001, 2000, 9, 1, start);
    assert(ledger.Resolve(1001, 2000, 9, start).state == State::Ambiguous);
    assert(ledger.Revoke(overlapping));
    assert(ledger.Resolve(1001, 2000, 9, start).id == swapped);
    assert(!ledger.AddEffectAlias(swapped, 1004, start + seconds(7)));
    std::cout << "Parry Hotter per-projectile spell identity ledger: OK\n";
}
