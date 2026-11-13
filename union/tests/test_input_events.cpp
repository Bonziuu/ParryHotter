#include "../hud/InputEvents.hpp"
#include "../hud/SpellHudState.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace ParryHotter;

static std::vector<InputEvent> Drain(InputEvents& events) {
    std::vector<InputEvent> result;
    InputEvent event;
    while (events.Pop(event)) result.push_back(event);
    return result;
}

static std::vector<int> Replay(InputEvents& events, Controller& controller,
                               bool hasTarget = true) {
    std::vector<int> casts;
    InputEvent event;
    while (events.Pop(event)) {
        const auto out = event.kind == InputEventKind::Interrupted
            ? controller.Step({})
            : controller.Step({true, event.slotPressed, event.rightHeld,
                               event.leftHeld, hasTarget});
        if (out.cast) casts.push_back(out.castSlot);
    }
    return casts;
}

int main() {
    InputEvents events;
    Controller controller;

    // Polls preserve a complete press/release between game frames. Reading
    // only the final held state would miss both the spell and the jump.
    events.Capture(true, false, false);
    for (int poll = 0; poll < 100; ++poll) events.Capture(true, false, false);
    assert(events.Size() == 0);
    events.Capture(true, true, true);
    assert(events.LastLeftReleaseSequence() == 0);
    events.Capture(true, false, false);
    const auto quickRelease = events.LastLeftReleaseSequence();
    assert(quickRelease != 0);
    assert(!events.LeftHeld() && !events.RightHeld());
    assert(events.ConsumeRightPressed());
    assert(!events.ConsumeRightPressed());
    assert((Replay(events, controller) == std::vector<int>{0}));
    assert(events.Size() == 0 && Replay(events, controller).empty());
    events.Capture(true, false, false);
    assert(events.LastLeftReleaseSequence() == 0);

    // Several high-frequency polls of a held button never duplicate edges.
    events.Capture(true, true, true);
    for (int poll = 0; poll < 1000; ++poll) events.Capture(true, true, true);
    assert(events.Size() == 2);
    auto held = Drain(events);
    assert(held[0].kind == InputEventKind::LeftPressed);
    assert(held[1].kind == InputEventKind::RightPressed);
    assert(held[0].sequence < held[1].sequence);
    assert(events.ConsumeRightPressed());
    assert(!events.ConsumeRightPressed());
    events.Capture(true, false, false);
    Drain(events);

    // Release and a new slot in the same poll: fire the prepared old slot,
    // then select the new one for the next press. Preserve absolute slots.
    controller.Reset();
    events.Reset(0, false);
    events.Capture(true, true, false);
    events.Capture(true, false, false, 2);
    auto releaseAndSlot = Drain(events);
    assert(releaseAndSlot.size() == 3);
    assert(releaseAndSlot[0].kind == InputEventKind::LeftPressed);
    assert(releaseAndSlot[1].kind == InputEventKind::LeftReleased);
    assert(releaseAndSlot[1].slot == 0 && releaseAndSlot[1].slotPressed == 0);
    assert(releaseAndSlot[2].kind == InputEventKind::SlotSelected);
    assert(releaseAndSlot[2].slot == 1 && releaseAndSlot[2].slotPressed == 2);
    for (const auto& event : releaseAndSlot) {
        const auto out = controller.Step({true, event.slotPressed, event.rightHeld,
                                          event.leftHeld, true});
        if (out.cast) assert(out.castSlot == 0 && out.selected == 0);
    }
    assert(controller.Selected() == 1 && events.Selected() == 1);
    events.Capture(true, true, false);
    events.Capture(true, false, false);
    assert((Replay(events, controller) == std::vector<int>{1}));

    // Simultaneous selection and press must instead prepare the new spell.
    controller.Reset();
    events.Reset(0, false);
    events.Capture(true, true, false, 4);
    events.Capture(true, false, false);
    assert((Replay(events, controller) == std::vector<int>{3}));

    // Empty-slot selection is an explicit cancellation, even when it arrives
    // in the same input poll as the release of a prepared available spell.
    events.Capture(true, true, false);
    const auto beforeEmptyRelease = events.LastSequence();
    events.Capture(true, false, false, 7);
    assert(events.LastLeftReleaseSequence() == beforeEmptyRelease + 2);
    assert(Replay(events, controller).empty());
    assert(events.Selected() == 6 && controller.Selected() == 6);
    events.Capture(true, false, false, 4);
    Replay(events, controller);

    // An empty slot chosen while held cancels the aim; later release cannot
    // resurrect the previous spell, including a new press on that empty slot.
    events.Capture(true, true, false);
    events.Capture(true, true, false, 7);
    events.Capture(true, false, false);
    assert(Replay(events, controller).empty());
    events.Capture(true, true, false);
    events.Capture(true, false, false);
    assert(Replay(events, controller).empty());

    // A miss is still a release, never a deferred shot when a target later
    // enters the reticle. Menu interruption drops any queued pre-menu click.
    controller.Reset();
    events.Reset(0, false);
    events.Capture(true, true, false);
    events.Capture(true, false, false);
    assert(Replay(events, controller, false).empty());
    assert(Replay(events, controller, true).empty());
    events.Capture(true, true, true);
    events.Capture(true, false, false);
    events.Capture(false, false, false);
    auto interrupted = Drain(events);
    assert(interrupted.size() == 1);
    assert(interrupted[0].kind == InputEventKind::Interrupted);
    assert(!events.ConsumeRightPressed());
    controller.Step({});
    events.Capture(false, true, true, 3);
    assert(events.Size() == 0 && events.Selected() == 0);
    events.Capture(true, true, true);
    assert(events.Size() == 0);
    assert(!events.ConsumeRightPressed());
    // The first actual release acknowledges the ignored held press to both
    // consumers, so a purely event-driven controller can re-arm correctly.
    events.Capture(true, false, false);
    assert(events.Size() == 2);
    assert(Replay(events, controller).empty());
    events.Capture(true, true, true);
    events.Capture(true, false, false);
    assert(events.ConsumeRightPressed());
    assert((Replay(events, controller) == std::vector<int>{0}));

    // RHS presses coalesce only in the independent jump consumer. The HUD
    // stream still contains every button event, in its original order.
    events.Reset(0, false);
    for (int click = 0; click < 3; ++click) {
        events.Capture(true, false, true);
        events.Capture(true, false, false);
    }
    assert(events.Size() == 6);
    assert(events.ConsumeRightPressed());
    assert(!events.ConsumeRightPressed());
    auto rightClicks = Drain(events);
    for (std::size_t i = 0; i < rightClicks.size(); ++i) {
        assert(rightClicks[i].kind == (i % 2 == 0
            ? InputEventKind::RightPressed : InputEventKind::RightReleased));
        if (i) assert(rightClicks[i].sequence > rightClicks[i - 1].sequence);
    }

    // Wrap the ring head repeatedly and ensure no stale sequence or slot is
    // replayed when the producer and consumer run at different frequencies.
    events.Reset(0, false);
    std::uint64_t previousSequence = events.LastSequence();
    for (int frame = 0; frame < 100; ++frame) {
        events.Capture(true, true, false, frame % 6 + 1);
        events.Capture(true, false, false);
        auto frameEvents = Drain(events);
        assert(!frameEvents.empty());
        for (const auto& event : frameEvents) {
            assert(event.sequence > previousSequence);
            previousSequence = event.sequence;
        }
    }

    // Overflow fails closed: discard every queued click and the triggering
    // edge, emit one interruption and require fresh button releases.
    events.Reset(0, false);
    controller.Reset();
    const auto droppedBefore = events.Dropped();
    const auto sequenceBefore = events.LastSequence();
    for (std::size_t click = 0; click < InputEvents::Capacity / 2; ++click) {
        events.Capture(true, true, false);
        events.Capture(true, false, false);
    }
    assert(events.Size() == InputEvents::Capacity);
    events.Capture(true, true, true);
    assert(events.Size() == 1);
    assert(events.LastLeftReleaseSequence() == 0);
    assert(events.Dropped() == droppedBefore + InputEvents::Capacity + 1);
    assert(events.LastSequence() > sequenceBefore);
    assert(events.RequiresLeftRelease() && events.RequiresRightRelease());
    assert(!events.LeftHeld() && !events.RightHeld());
    assert(!events.ConsumeRightPressed());
    assert(Replay(events, controller).empty());
    events.Capture(true, true, true);
    assert(events.Size() == 0);
    events.Capture(true, false, false);
    assert(Replay(events, controller).empty());
    events.Capture(true, true, true);
    events.Capture(true, false, false);
    assert(events.ConsumeRightPressed());
    assert((Replay(events, controller) == std::vector<int>{0}));

    // A release can fit into the final queue slot and then be discarded by
    // the simultaneous selection. Do not capture a target for that vanished
    // release: the sole remaining event is Interrupted.
    events.Reset(0, false);
    for (std::size_t click = 0; click < InputEvents::Capacity / 2 - 1; ++click) {
        events.Capture(true, true, false);
        events.Capture(true, false, false);
    }
    events.Capture(true, true, false);
    assert(events.Size() == InputEvents::Capacity - 1);
    events.Capture(true, false, false, 2);
    assert(events.Size() == 1 && events.LastLeftReleaseSequence() == 0);
    auto overflowAfterRelease = Drain(events);
    assert(overflowAfterRelease[0].kind == InputEventKind::Interrupted);
    controller.Step({});
    events.Capture(true, false, false);
    Replay(events, controller);

    // Discarded slot changes must not silently desynchronize the producer
    // and HUD after a menu/overflow: retain the last delivered selection.
    events.Capture(true, false, false, 4);
    assert(events.Selected() == 3 && controller.Selected() == 0);
    events.Interrupt();
    assert(events.Selected() == 0);
    assert(Replay(events, controller).empty());
    events.Capture(true, false, false);
    Replay(events, controller);
    events.Capture(true, true, false);
    events.Capture(true, false, false);
    assert((Replay(events, controller) == std::vector<int>{0}));

    // A lifecycle reset suppresses held buttons and preserves diagnostics.
    const auto lastSequence = events.LastSequence();
    const auto dropped = events.Dropped();
    events.Reset(5);
    assert(events.Selected() == 5 && events.Size() == 0);
    assert(events.LastSequence() == lastSequence && events.Dropped() == dropped);
    events.Capture(true, true, true);
    assert(events.Size() == 0 && !events.ConsumeRightPressed());
    events.Capture(true, false, false);
    Drain(events);
    events.Capture(true, true, false);
    InputEvent first;
    assert(events.Pop(first) && first.slot == 5);
    assert(first.kind == InputEventKind::LeftPressed && first.sequence > lastSequence);

    std::cout << "Parry Hotter buffered input order, quick taps and interruption safety: OK\n";
}
