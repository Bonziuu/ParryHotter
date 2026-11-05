#include "../hud/ImpactTelemetry.hpp"
#include <cassert>
#include <iostream>
#include <limits>

using namespace ParryHotter;

int main() {
    ImpactTelemetry diagnostics;
    assert(diagnostics.Get(ImpactEvent::Launched) == 0);
    assert(diagnostics.Last() == ImpactEvent::Count);
    assert(diagnostics.Get(static_cast<ImpactEvent>(255)) == 0);

    diagnostics.Note(ImpactEvent::Recognized);
    diagnostics.Note(ImpactEvent::NoNativePhysics);
    diagnostics.Note(ImpactEvent::Recognized);
    diagnostics.Note(ImpactEvent::Launched);
    diagnostics.Note(ImpactEvent::Landed);
    assert(diagnostics.Get(ImpactEvent::Recognized) == 2);
    assert(diagnostics.Get(ImpactEvent::NoNativePhysics) == 1);
    assert(diagnostics.Get(ImpactEvent::Launched) == 1);
    assert(diagnostics.Get(ImpactEvent::Landed) == 1);
    assert(diagnostics.Last() == ImpactEvent::Landed);
    assert(ImpactTelemetry::Label(ImpactEvent::NoNativePhysics)[0] != '\0');
    diagnostics.Note(static_cast<ImpactEvent>(255));
    assert(diagnostics.Last() == ImpactEvent::Landed);
    diagnostics.Reset();
    for (unsigned i = 0; i < ImpactTelemetry::Size; ++i) {
        assert(diagnostics.Get(static_cast<ImpactEvent>(i)) == 0);
    }
    assert(diagnostics.Last() == ImpactEvent::Count);
    std::cout << "Parry Hotter impact diagnostics: OK\n";
}
