// Admission and bounded diagnostics for Gothic I's fixed alpha polygon pool.
// No engine memory is changed here. A batch ends only after an observed drain.
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace ParryHotter {

struct AlphaPolyDecision {
    bool admit = false;
    bool report = false;
    bool badCounter = false;
};

struct AlphaPolySummary {
    bool report = false;
    bool drainObserved = false;
    std::uint64_t batch = 0;
    int peakObserved = 0;
    std::uint64_t admittedCalls = 0;
    std::uint64_t dropped = 0;
    std::uint64_t badCounters = 0;
    std::uint64_t intervalBatches = 0;
    std::uint64_t intervalDropped = 0;
    std::uint64_t intervalBadCounters = 0;
    int intervalPeakObserved = 0;
    std::uint64_t totalDropped = 0;
};

class AlphaPolyBudget final {
public:
    static constexpr int Capacity = 2048;
    static constexpr std::uint64_t ReportIntervalMs = 1000;

    AlphaPolyDecision Before(int nativeCount) {
        const bool valid = ObserveCount(nativeCount);
        AlphaPolyDecision out;
        out.badCounter = !valid;
        out.admit = valid && nativeCount < Capacity;
        if (out.admit) Increment(admitted_);
        else {
            Increment(dropped_);
            Increment(totalDropped_);
            if (!valid) Increment(badCounters_);
            // One immediate alarm per renderer lifetime, then batch summaries.
            out.report = !alarmWritten_;
            alarmWritten_ = true;
        }
        return out;
    }

    // Call after the actual native store; never predict count + 1.
    void AfterQueued(int nativeCount) { ObserveCount(nativeCount); }

    AlphaPolySummary AfterSort(int nativeCount, std::uint64_t nowMs) {
        if (clockStarted_ && nowMs < lastClockMs_) summaryWritten_ = false;
        clockStarted_ = true;
        lastClockMs_ = nowMs;
        AlphaPolySummary out;
        out.batch = batches_;
        out.peakObserved = peak_;
        out.admittedCalls = admitted_;
        out.dropped = dropped_;
        out.badCounters = badCounters_;
        out.totalDropped = totalDropped_;
        out.drainObserved = nativeCount == 0;
        if (out.drainObserved) {
            Increment(batches_);
            Increment(intervalBatches_);
            Add(intervalDropped_, dropped_);
            Add(intervalBad_, badCounters_);
            intervalPeak_ = std::max(intervalPeak_, peak_);
            peak_ = 0;
            admitted_ = dropped_ = badCounters_ = 0;
        }
        // A hook/renderer replacement can return without draining. Preserve
        // the batch and report the actual failure, never fake a reset.
        const bool issue = intervalDropped_ || intervalBad_ || !out.drainObserved;
        out.report = issue && (!summaryWritten_ || nowMs-lastSummaryMs_ >= ReportIntervalMs);
        out.intervalBatches = intervalBatches_;
        out.intervalDropped = intervalDropped_;
        out.intervalBadCounters = intervalBad_;
        out.intervalPeakObserved = intervalPeak_;
        if (out.report) {
            summaryWritten_ = true;
            lastSummaryMs_ = nowMs;
            intervalBatches_ = intervalDropped_ = intervalBad_ = 0;
            intervalPeak_ = 0;
        }
        return out;
    }

    int PeakObserved() const { return peak_; }
    int LifetimePeakObserved() const { return lifetimePeak_; }
    std::uint64_t BatchDropped() const { return dropped_; }
    std::uint64_t TotalDropped() const { return totalDropped_; }
    void Reset() { *this = AlphaPolyBudget{}; }

private:
    bool ObserveCount(int count) {
        if (count < 0 || count > Capacity) return false;
        peak_ = std::max(peak_, count);
        lifetimePeak_ = std::max(lifetimePeak_, count);
        return true;
    }
    static void Increment(std::uint64_t& value) { Add(value, 1); }
    static void Add(std::uint64_t& value, std::uint64_t amount) {
        const auto limit = (std::numeric_limits<std::uint64_t>::max)();
        value = amount > limit-value ? limit : value+amount;
    }
    int peak_ = 0, intervalPeak_ = 0, lifetimePeak_ = 0;
    std::uint64_t admitted_ = 0, dropped_ = 0, badCounters_ = 0;
    std::uint64_t totalDropped_ = 0, batches_ = 0;
    std::uint64_t intervalBatches_ = 0, intervalDropped_ = 0, intervalBad_ = 0;
    std::uint64_t lastClockMs_ = 0, lastSummaryMs_ = 0;
    bool alarmWritten_ = false, clockStarted_ = false, summaryWritten_ = false;
};
} // namespace ParryHotter
