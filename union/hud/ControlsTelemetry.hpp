// Bounded control diagnostics, independent of Gothic/Union and file output.
// Keep continuous snapshots at <= 2 Hz; record meaningful actions separately.
// All times are caller-supplied monotonic milliseconds. A backwards clock
// reopens the gates (e.g. after loading), without losing accumulated counters.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace ParryHotter {

struct ControlsSnapshot {
    std::uint64_t inputSequence = 0;
    std::uint64_t requestId = 0;
    int slot = -1;
    int castStage = 0;
    int contextReason = 0;
    int npcMode = -1;
    int aiMode = -1;
    int body = -1;
    bool leftHeld = false;
    bool rightHeld = false;
    bool grounded = false;
    // cm/s, apart from floor/camera measurements in cm.
    float desiredX = 0.0f;
    float desiredZ = 0.0f;
    float actualX = 0.0f;
    float actualZ = 0.0f;
    float verticalVelocity = 0.0f;
    float aboveFloor = 0.0f;
    float cameraDistance = 0.0f;
    float cameraPivotHeight = 0.0f;
};

enum class ControlsEvent : unsigned {
    InputPress = 0,
    InputRelease,
    SlotChanged,
    CastRequested,
    CastReady,
    CastCompleted,
    CastCancelled,
    CastRefused,
    JumpRequested,
    JumpAccepted,
    JumpRefused,
    JumpLanded,
    ContextChanged,
    MovementBlocked,
    CameraBlocked,
    QueueOverflow,
    Count
};

class ControlsTelemetry final {
public:
    static constexpr std::uint64_t SnapshotIntervalMs = 500;
    static constexpr unsigned MaxEventsPerSecond = 12;
    static constexpr std::size_t EventCount = static_cast<std::size_t>(ControlsEvent::Count);

    // Do not compare continuous floats or animation/body-state churn: those
    // could otherwise create one log entry per frame. The latest sample is
    // retained even if this call does not print it.
    bool ShouldLog(std::uint64_t nowMs, const ControlsSnapshot& snapshot) {
        ObserveClock(nowMs);
        latest_ = snapshot;
        if (snapshotWritten_ && nowMs - lastSnapshotMs_ < SnapshotIntervalMs) return false;
        snapshotWritten_ = true;
        lastSnapshotMs_ = nowMs;
        Increment(snapshots_);
        return true;
    }

    // sequence identifies one actual action (input edge/request/jump ID),
    // rather than the current frame. Repeated notification of the same kind
    // and sequence is coalesced. Different stages of the same request remain
    // distinct events. Counters include events omitted by the output budget.
    bool NoteEvent(std::uint64_t nowMs, ControlsEvent event, std::uint64_t sequence) {
        ObserveClock(nowMs);
        const auto index = static_cast<std::size_t>(event);
        if (index >= EventCount) return false;
        if (seen_[index] && lastSequence_[index] == sequence) {
            Increment(duplicates_);
            return false;
        }
        seen_[index] = true;
        lastSequence_[index] = sequence;
        Increment(counts_[index]);
        if (!eventWindowStarted_ || nowMs - eventWindowMs_ >= 1000) {
            eventWindowStarted_ = true;
            eventWindowMs_ = nowMs;
            eventsWrittenInWindow_ = 0;
        }
        if (eventsWrittenInWindow_ >= MaxEventsPerSecond) {
            Increment(suppressed_);
            return false;
        }
        ++eventsWrittenInWindow_;
        return true;
    }

    const ControlsSnapshot& Latest() const { return latest_; }
    std::uint64_t Count(ControlsEvent event) const {
        const auto index = static_cast<std::size_t>(event);
        return index < EventCount ? counts_[index] : 0;
    }
    std::uint64_t SnapshotCount() const { return snapshots_; }
    std::uint64_t SuppressedEvents() const { return suppressed_; }
    std::uint64_t DuplicateEvents() const { return duplicates_; }

    static const char* Label(ControlsEvent event) {
        switch (event) {
        case ControlsEvent::InputPress: return "input_press";
        case ControlsEvent::InputRelease: return "input_release";
        case ControlsEvent::SlotChanged: return "slot_changed";
        case ControlsEvent::CastRequested: return "cast_requested";
        case ControlsEvent::CastReady: return "cast_ready";
        case ControlsEvent::CastCompleted: return "cast_completed";
        case ControlsEvent::CastCancelled: return "cast_cancelled";
        case ControlsEvent::CastRefused: return "cast_refused";
        case ControlsEvent::JumpRequested: return "jump_requested";
        case ControlsEvent::JumpAccepted: return "jump_accepted";
        case ControlsEvent::JumpRefused: return "jump_refused";
        case ControlsEvent::JumpLanded: return "jump_landed";
        case ControlsEvent::ContextChanged: return "context_changed";
        case ControlsEvent::MovementBlocked: return "movement_blocked";
        case ControlsEvent::CameraBlocked: return "camera_blocked";
        case ControlsEvent::QueueOverflow: return "queue_overflow";
        default: return "unknown_event";
        }
    }

    void Reset() { *this = ControlsTelemetry{}; }

private:
    static void Increment(std::uint64_t& value) {
        if (value != (std::numeric_limits<std::uint64_t>::max)()) ++value;
    }

    void ObserveClock(std::uint64_t nowMs) {
        if (clockStarted_ && nowMs < lastClockMs_) {
            snapshotWritten_ = eventWindowStarted_ = false;
            eventsWrittenInWindow_ = 0;
            seen_.fill(false);
        }
        clockStarted_ = true;
        lastClockMs_ = nowMs;
    }

    ControlsSnapshot latest_{};
    std::array<std::uint64_t, EventCount> counts_{};
    std::array<std::uint64_t, EventCount> lastSequence_{};
    std::array<bool, EventCount> seen_{};
    std::uint64_t snapshots_ = 0;
    std::uint64_t suppressed_ = 0;
    std::uint64_t duplicates_ = 0;
    std::uint64_t lastClockMs_ = 0;
    std::uint64_t lastSnapshotMs_ = 0;
    std::uint64_t eventWindowMs_ = 0;
    unsigned eventsWrittenInWindow_ = 0;
    bool clockStarted_ = false;
    bool snapshotWritten_ = false;
    bool eventWindowStarted_ = false;
};

} // namespace ParryHotter
