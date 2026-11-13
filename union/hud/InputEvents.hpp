// Bounded input history between Gothic's high-frequency polls and game AI.
// No engine objects are retained here. The adapter captures the target at a
// release separately; this queue preserves button order and selected slots.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ParryHotter {

enum class InputEventKind {
    LeftPressed, LeftReleased, RightPressed, RightReleased, SlotSelected, Interrupted
};

struct InputEvent {
    std::uint64_t sequence = 0;
    InputEventKind kind = InputEventKind::Interrupted;
    int slotPressed = 0; // 1..10 only for SlotSelected, otherwise zero
    int slot = 0;        // absolute selected slot, zero based
    bool leftHeld = false;
    bool rightHeld = false;
};

class InputEvents final {
public:
    static constexpr std::size_t Capacity = 64;

    // A valid slot press is an edge supplied by the engine's KeyToggled.
    // Release edges precede a simultaneous available-spell selection;
    // selecting an empty slot explicitly cancels before release. Presses
    // always follow selection.
    void Capture(bool gameplay, bool leftHeld, bool rightHeld, int slotPressed = 0) {
        captureLeftRelease_ = 0;
        if (!gameplay) {
            if (!captured_ || gameplay_) Interrupt();
            captured_ = true;
            gameplay_ = false;
            requireLeftRelease_ = requireRightRelease_ = true;
            return;
        }
        captured_ = gameplay_ = true;
        const bool leftGuardReleased = requireLeftRelease_ && !leftHeld;
        const bool rightGuardReleased = requireRightRelease_ && !rightHeld;
        if (!leftHeld) requireLeftRelease_ = false;
        if (!rightHeld) requireRightRelease_ = false;
        const bool left = leftHeld && !requireLeftRelease_;
        const bool right = rightHeld && !requireRightRelease_;
        const bool selecting = slotPressed >= 1 && slotPressed <= 10 &&
                               slotPressed - 1 != selected_;
        const bool selectingEmpty = selecting && slotPressed >= 7;

        if (selectingEmpty) {
            selected_ = slotPressed - 1;
            if (!Push(InputEventKind::SlotSelected, slotPressed)) return;
        }

        // A release after an interruption must also reach the consumer when
        // the corresponding held press was deliberately suppressed. This
        // lets a controller re-arm without relying on a separate frame poll.
        if ((left_ && !left) || leftGuardReleased) {
            left_ = false;
            if (!Push(InputEventKind::LeftReleased)) return;
        }
        if ((right_ && !right) || rightGuardReleased) {
            right_ = false;
            if (!Push(InputEventKind::RightReleased)) return;
        }
        if (selecting && !selectingEmpty) {
            selected_ = slotPressed - 1;
            if (!Push(InputEventKind::SlotSelected, slotPressed)) return;
        }
        if (!left_ && left) {
            left_ = true;
            if (!Push(InputEventKind::LeftPressed)) return;
        }
        if (!right_ && right) {
            right_ = true;
            if (!Push(InputEventKind::RightPressed)) return;
            rightPressed_ = true;
        }
    }

    bool Pop(InputEvent& event) {
        if (!size_) return false;
        event = events_[head_];
        head_ = (head_ + 1) % Capacity;
        --size_;
        deliveredSelected_ = event.slot;
        return true;
    }

    // AI consumes the press independently of the HUD event stream. Several
    // presses before one AI step coalesce, avoiding deferred automatic jumps.
    bool ConsumeRightPressed() {
        const bool pressed = rightPressed_;
        rightPressed_ = false;
        return pressed;
    }

    void Reset(int selectedSlot = 0, bool requireRelease = true) {
        head_ = size_ = 0;
        selected_ = selectedSlot >= 0 && selectedSlot < 10 ? selectedSlot : 0;
        deliveredSelected_ = selected_;
        left_ = right_ = rightPressed_ = false;
        captured_ = gameplay_ = false;
        captureLeftRelease_ = 0;
        requireLeftRelease_ = requireRightRelease_ = requireRelease;
        // Sequence and dropped count survive lifecycle resets for diagnostics.
    }

    void Interrupt() {
        head_ = size_ = 0;
        captureLeftRelease_ = 0;
        // Drop unconsumed selections with the discarded button history.
        // Otherwise the producer could remain on a slot the HUD never saw,
        // and would never emit its selection again on the next press.
        selected_ = deliveredSelected_;
        left_ = right_ = rightPressed_ = false;
        requireLeftRelease_ = requireRightRelease_ = true;
        Store(InputEventKind::Interrupted, 0);
    }

    std::size_t Size() const { return size_; }
    std::uint64_t Dropped() const { return dropped_; }
    std::uint64_t LastSequence() const { return sequence_; }
    // Exact queued release produced by the last Capture, or zero. In
    // particular an empty-slot cancellation can precede release, while
    // overflow can discard a release already emitted earlier in that poll.
    std::uint64_t LastLeftReleaseSequence() const { return captureLeftRelease_; }
    int Selected() const { return selected_; }
    bool LeftHeld() const { return left_; }
    bool RightHeld() const { return right_; }
    bool RequiresLeftRelease() const { return requireLeftRelease_; }
    bool RequiresRightRelease() const { return requireRightRelease_; }

private:
    bool Push(InputEventKind kind, int slotPressed = 0) {
        if (size_ == Capacity) {
            dropped_ += size_ + 1;
            Interrupt();
            return false;
        }
        Store(kind, slotPressed);
        return true;
    }

    void Store(InputEventKind kind, int slotPressed) {
        events_[(head_ + size_) % Capacity] =
            {++sequence_, kind, slotPressed, selected_, left_, right_};
        if (kind == InputEventKind::LeftReleased) captureLeftRelease_ = sequence_;
        ++size_;
    }

    std::array<InputEvent, Capacity> events_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::uint64_t sequence_ = 0;
    std::uint64_t dropped_ = 0;
    std::uint64_t captureLeftRelease_ = 0;
    int selected_ = 0;
    int deliveredSelected_ = 0;
    bool left_ = false;
    bool right_ = false;
    bool rightPressed_ = false;
    bool requireLeftRelease_ = false;
    bool requireRightRelease_ = false;
    bool captured_ = false;
    bool gameplay_ = false;
};

} // namespace ParryHotter
