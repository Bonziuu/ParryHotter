// Temporary camera-based targeting limits for an already validated player
// spell. Include after the Gothic/Union headers. Target type and native line
// of sight checks remain owned by Gothic; callers still validate the reticle.
#pragma once

namespace ParryTargeting {

inline constexpr int kRangeCm = 1900;

class Scope final {
public:
    explicit Scope(Gothic_I_Classic::oCSpell* spell) : spell_(spell) {
        if (!spell_) return;
        spell_->AddRef();
        range_ = spell_->targetCollectRange;
        azimuth_ = spell_->targetCollectAzi;
        elevation_ = spell_->targetCollectElev;
        spell_->targetCollectRange = kRangeCm;
        spell_->targetCollectAzi = kAngleDegrees;
        spell_->targetCollectElev = kAngleDegrees;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    ~Scope() {
        if (!spell_) return;
        // Nested scopes restore the outer scope's limits. A native callback
        // or another plugin may change any field independently: restore only
        // the values we still own, before Release can destroy the spell.
        if (spell_->targetCollectRange == kRangeCm)
            spell_->targetCollectRange = range_;
        if (spell_->targetCollectAzi == kAngleDegrees)
            spell_->targetCollectAzi = azimuth_;
        if (spell_->targetCollectElev == kAngleDegrees)
            spell_->targetCollectElev = elevation_;
        spell_->Release();
    }

private:
    static constexpr int kAngleDegrees = 360;
    Gothic_I_Classic::oCSpell* spell_ = nullptr;
    int range_ = 0;
    int azimuth_ = 0;
    int elevation_ = 0;
};

} // namespace ParryTargeting
