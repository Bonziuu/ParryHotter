// Local in-game diagnostic counters for Gothic I Union spell physics.
// No file output or personal data; optional F10 overlay is off by default.
// Testable without the game SDK.
#pragma once
#include <array>
#include <cstddef>
#include <limits>

namespace ParryHotter {

enum class ImpactEvent : unsigned {
    Recognized = 0,
    NoNativePhysics,
    NoRigidBody,
    WrongWorld,
    NoClearance,
    Launched,
    Landed,
    Interrupted,
    TimedOut,
    Count
};

class ImpactTelemetry final {
public:
    static constexpr unsigned Size = static_cast<unsigned>(ImpactEvent::Count);

    void Note(ImpactEvent event) {
        const auto i = static_cast<unsigned>(event);
        if (i >= Size) return;
        if (counts_[i] < (std::numeric_limits<unsigned>::max)())
            ++counts_[i];
        last_ = event;
    }

    unsigned Get(ImpactEvent event) const {
        const auto i = static_cast<unsigned>(event);
        return i < Size ? counts_[i] : 0u;
    }

    ImpactEvent Last() const { return last_; }

    static const char* Label(ImpactEvent event) {
        switch (event) {
            case ImpactEvent::Recognized: return "rozpoznane trafienie";
            case ImpactEvent::NoNativePhysics: return "NPC bez fizyki (ZenGin)";
            case ImpactEvent::NoRigidBody: return "NPC bez zCRigidBody";
            case ImpactEvent::WrongWorld: return "cel bez aktualnego swiata";
            case ImpactEvent::NoClearance: return "przeszkoda lub natywny odrzut";
            case ImpactEvent::Launched: return "impuls nadany";
            case ImpactEvent::Landed: return "ladowanie";
            case ImpactEvent::Interrupted: return "przerwany lot";
            case ImpactEvent::TimedOut: return "limit czasu lotu";
            default: return "brak";
        }
    }

    void Reset() {
        counts_.fill(0);
        last_ = ImpactEvent::Count;
    }

private:
    std::array<unsigned, Size> counts_{};
    ImpactEvent last_ = ImpactEvent::Count;
};

} // namespace ParryHotter
