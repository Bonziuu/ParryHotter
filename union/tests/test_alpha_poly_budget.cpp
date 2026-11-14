#include "../hud/AlphaPolyBudget.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>

using ParryHotter::AlphaPolyBudget;
int main() {
    AlphaPolyBudget policy;
    std::array<int, AlphaPolyBudget::Capacity> nativePool{};
    int nativeCount = 0;
    unsigned alarms = 0;
    // Reproduce the full fixed native pool, then submit many more polygons.
    for (int polygon = 0; polygon < 5000; ++polygon) {
        const auto decision = policy.Before(nativeCount);
        alarms += decision.report ? 1 : 0;
        if (decision.admit) {
            assert(nativeCount < AlphaPolyBudget::Capacity);
            nativePool[nativeCount++] = polygon;
            policy.AfterQueued(nativeCount);
        }
    }
    assert(nativeCount == 2048 && alarms == 1);
    assert(policy.PeakObserved() == 2048 && policy.BatchDropped() == 2952);
    assert(policy.TotalDropped() == 2952);
    for (int i = 0; i < nativeCount; ++i) assert(nativePool[i] == i);

    // An undrained sort cannot reset a batch or admit more geometry.
    const auto retained = policy.AfterSort(nativeCount, 10);
    assert(!retained.drainObserved && retained.report);
    assert(policy.BatchDropped() == 2952 && !policy.Before(nativeCount).admit);
    assert(nativeCount == 2048 && nativePool.back() == 2047);
    const auto stillFull = policy.AfterSort(nativeCount, 11);
    assert(!stillFull.report && !stillFull.drainObserved);

    // Only the external native sort clears its own pool; policy observes it.
    nativeCount = 0;
    const auto drained = policy.AfterSort(nativeCount, 12);
    assert(drained.drainObserved && !drained.report);
    assert(drained.peakObserved == 2048 && drained.dropped == 2953);
    assert(policy.PeakObserved() == 0 && policy.BatchDropped() == 0);
    assert(policy.LifetimePeakObserved() == 2048);
    assert(policy.Before(nativeCount).admit);
    nativePool[nativeCount++] = 12345;
    policy.AfterQueued(nativeCount);
    const auto pending = policy.AfterSort(0, 1010);
    assert(pending.report && pending.intervalDropped == 2953);
    assert(pending.totalDropped == 2953 && pending.intervalPeakObserved == 2048);
    assert(pending.intervalBatches == 2);

    // Corrupt/out-of-range observations cannot become fake peaks or writes.
    policy.Reset();
    for (const int count : {-1, 2049, (std::numeric_limits<int>::max)()}) {
        const auto rejected = policy.Before(count);
        assert(!rejected.admit && rejected.badCounter);
    }
    policy.AfterQueued((std::numeric_limits<int>::max)());
    assert(policy.PeakObserved() == 0 && policy.TotalDropped() == 3);
    assert(policy.LifetimePeakObserved() == 0);
    const auto bad = policy.AfterSort(0, 1);
    assert(bad.report && bad.badCounters == 3 && bad.intervalBadCounters == 3);

    // Summaries remain bounded, but no dropped count is lost across drains.
    policy.Reset();
    unsigned summaries = 0, immediate = 0;
    for (std::uint64_t ms = 0; ms < 3000; ++ms) {
        immediate += policy.Before(2048).report ? 1 : 0;
        const auto sample = policy.AfterSort(0, ms);
        summaries += sample.report ? 1 : 0;
    }
    assert(immediate == 1 && summaries == 3 && policy.TotalDropped() == 3000);
    const auto backwards = policy.AfterSort(0, 10);
    assert(backwards.report && backwards.intervalDropped == 999);
    assert(backwards.totalDropped == 3000);
    std::cout << "Alpha pool guard: fixed capacity, preserved queue, observed drains and bounded logs OK\n";
}
