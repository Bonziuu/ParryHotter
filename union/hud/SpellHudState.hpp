// Parry Hotter - deterministic input rules independent of ZenGin.
// Can be compiled/tested without Gothic or Union.
// A spell only fires on a fresh LMB click, while RMB is held, with a target.
#pragma once
#include <array>
#include <cstddef>

namespace ParryHotter {
enum class Spell : int {
    Flipendo = 0,
    Levicorpus,
    Rictusempra,
    Expelliarmus,
    BumbardoMaximus,
    AvadaKedavra,
    Empty7,
    Empty8,
    Empty9,
    Empty0,
};
// Ten visible/selectable slots, with six implemented spells and four blanks.
constexpr int kSpellCount = 10;
constexpr std::array<bool, kSpellCount> kSpellAvailable{{
    true, true, true, true, true, true, false, false, false, false
}};
constexpr std::array<const char*, kSpellCount> kNames{{
    "Flipendo", "Levicorpus", "Rictusempra",
    "Expelliarmus", "Bumbardo Maximus", "Avada Kedavra",
    "Wolne miejsce", "Wolne miejsce", "Wolne miejsce", "Wolne miejsce"
}};
constexpr std::array<const char*, kSpellCount> kTextures{{
    "PH_FLIPENDO.TGA", "PH_LEVICORPUS.TGA",
    "PH_RICTUSEMPRA.TGA", "PH_EXPELLIARMUS.TGA",
    "PH_BUMBARDO.TGA", "PH_AVADA.TGA",
    "PH_EMPTY.TGA", "PH_EMPTY.TGA", "PH_EMPTY.TGA", "PH_EMPTY.TGA"
}};
constexpr std::array<const char*, kSpellCount> kHotkeyLabels{{
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0"
}};

struct Input {
    bool gameplay = false;
    // Physical keys 1..9,0 map to 1..10. Zero means no new selection.
    int slotPressed = 0;
    bool rightHeld = false;
    bool leftHeld = false;
    bool hasLiveNpcTarget = false;
};

struct Output {
    int selected = 0;
    bool hudVisible = false;
    bool targetSymbolVisible = false;
    bool aiming = false;
    bool startAim = false;
    bool stopAim = false;
    bool cast = false;
};

class Controller final {
public:
    // Harry Potter 2 (PC) scheme: hold LMB to raise the wand and aim, the
    // spell's symbol appears on a valid target, and RELEASING LMB casts at
    // the target under the reticle. RMB is jump (handled by locomotion).
    Output Step(Input in) {
        Output out{};
        if (!in.gameplay) {
            out.stopAim = previousAim_;
            previousAim_ = false;
            // After a menu, cutscene or load, LMB must be seen released
            // once; a button held through the interruption never aims/casts.
            requireLeftRelease_ = true;
            out.selected = selected_;
            return out;
        }
        if (in.slotPressed >= 1 && in.slotPressed <= kSpellCount) {
            selected_ = in.slotPressed - 1;
        }
        if (!in.leftHeld) requireLeftRelease_ = false;
        const bool holding = in.leftHeld && !requireLeftRelease_;
        const bool aiming = holding && kSpellAvailable[selected_];
        out.hudVisible = true;
        out.selected = selected_;
        out.aiming = aiming;
        out.targetSymbolVisible = aiming && in.hasLiveNpcTarget;
        out.startAim = aiming && !previousAim_;
        out.stopAim = !aiming && previousAim_;
        // Only a real release of an aiming LMB casts. Switching to an empty
        // slot while holding stops aiming without casting.
        out.cast = previousAim_ && !in.leftHeld && in.hasLiveNpcTarget &&
                   kSpellAvailable[selected_];
        previousAim_ = aiming;
        return out;
    }
    int Selected() const { return selected_; }
    void Reset() {
        selected_ = 0;
        previousAim_ = false;
        requireLeftRelease_ = false;
    }
    void ResetAfterInterruption() {
        Reset();
        requireLeftRelease_ = true;
    }
private:
    int selected_ = 0;
    bool previousAim_ = false;
    bool requireLeftRelease_ = false;
};
} // namespace ParryHotter
