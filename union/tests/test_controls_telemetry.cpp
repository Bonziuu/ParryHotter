#include "../hud/ControlsTelemetry.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace ParryHotter;

int main() {
    ControlsTelemetry telemetry;
    ControlsSnapshot state;
    unsigned logged = 0;
    // High-rate input, animation changes and float noise do not exhaust the
    // log. Two seconds at 1 kHz produce only the four scheduled samples.
    for (unsigned t = 0; t < 2000; ++t) {
        state.inputSequence = t;
        state.desiredX = float(t) * 0.001f;
        state.actualX = state.desiredX - 0.0001f;
        state.body = int(t % 3);
        if (telemetry.ShouldLog(t, state)) {
            assert(t % 500 == 0);
            ++logged;
        }
    }
    assert(logged == 4 && telemetry.SnapshotCount() == 4);
    assert(telemetry.Latest().inputSequence == 1999);
    assert(telemetry.Latest().actualX == state.actualX);

    // A complete quick tap and native cast may happen between two periodic
    // samples. Their individual stages must be immediately available.
    telemetry.Reset();
    assert(telemetry.ShouldLog(0, state));
    assert(telemetry.NoteEvent(10, ControlsEvent::InputPress, 1));
    assert(telemetry.NoteEvent(11, ControlsEvent::InputRelease, 2));
    assert(telemetry.NoteEvent(12, ControlsEvent::CastRequested, 42));
    assert(telemetry.NoteEvent(13, ControlsEvent::CastReady, 42));
    assert(telemetry.NoteEvent(14, ControlsEvent::CastCompleted, 42));
    assert(!telemetry.NoteEvent(15, ControlsEvent::CastCompleted, 42));
    assert(telemetry.Count(ControlsEvent::CastCompleted) == 1);
    assert(telemetry.DuplicateEvents() == 1);
    assert(!telemetry.ShouldLog(499, state));
    assert(telemetry.ShouldLog(500, state));

    // Holding a blocked movement/jump can produce many native notifications.
    // Output remains bounded, but the counters retain all distinct attempts.
    telemetry.Reset();
    unsigned eventsWritten = 0;
    for (unsigned sequence = 0; sequence < 100; ++sequence) {
        if (telemetry.NoteEvent(sequence, ControlsEvent::JumpRefused, sequence))
            ++eventsWritten;
    }
    assert(eventsWritten == ControlsTelemetry::MaxEventsPerSecond);
    assert(telemetry.Count(ControlsEvent::JumpRefused) == 100);
    assert(telemetry.SuppressedEvents() == 100 - eventsWritten);
    // Repeated frame reports of a suppressed event are still deduplicated.
    assert(!telemetry.NoteEvent(100, ControlsEvent::JumpRefused, 99));
    assert(telemetry.Count(ControlsEvent::JumpRefused) == 100);
    assert(telemetry.NoteEvent(1000, ControlsEvent::JumpAccepted, 101));

    // A load can restart elapsed time and event IDs. Gates reopen cleanly;
    // already collected statistics survive until an explicit reset.
    assert(telemetry.ShouldLog(1200, state));
    assert(!telemetry.ShouldLog(1300, state));
    assert(telemetry.NoteEvent(10, ControlsEvent::JumpRefused, 99));
    assert(telemetry.Count(ControlsEvent::JumpRefused) == 101);
    assert(telemetry.ShouldLog(10, state));
    assert(!telemetry.ShouldLog(509, state));
    assert(telemetry.ShouldLog(510, state));
    assert(!telemetry.NoteEvent(511, static_cast<ControlsEvent>(999), 1));
    assert(telemetry.Count(static_cast<ControlsEvent>(999)) == 0);
    assert(std::strcmp(ControlsTelemetry::Label(ControlsEvent::CastCancelled), "cast_cancelled") == 0);

    telemetry.Reset();
    assert(telemetry.SnapshotCount() == 0 && telemetry.SuppressedEvents() == 0);
    assert(telemetry.DuplicateEvents() == 0);
    for (unsigned i = 0; i < ControlsTelemetry::EventCount; ++i)
        assert(telemetry.Count(static_cast<ControlsEvent>(i)) == 0);
    assert(telemetry.ShouldLog(0, state));
    assert(telemetry.NoteEvent(0, ControlsEvent::InputPress, 0));

    std::cout << "Controls telemetry: bounded snapshots, action events, counters and clock reset OK\n";
}
