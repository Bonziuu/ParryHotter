// Parry Hotter - deterministic input rules independent of ZenGin.
// Can be compiled/tested without Gothic or Union.
// Hold LMB to aim, release it to cast at a live target. RMB is jump.
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
    // A slot selection and release can arrive together. The release belongs
    // to the spell aimed before that selection, not an unprepared new spell.
    int castSlot = -1;
    int aimSlot = -1;
    bool slotChanged = false;
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
            aimedSlot_ = -1;
            out.selected = selected_;
            return out;
        }
        const int previousSlot = selected_;
        const int releaseSlot = aimedSlot_;
        if (in.slotPressed >= 1 && in.slotPressed <= kSpellCount) {
            selected_ = in.slotPressed - 1;
        }
        if (!in.leftHeld) requireLeftRelease_ = false;
        const bool holding = in.leftHeld && !requireLeftRelease_;
        const bool aiming = holding && kSpellAvailable[selected_];
        out.hudVisible = true;
        out.selected = selected_;
        out.slotChanged = selected_ != previousSlot;
        out.aiming = aiming;
        out.aimSlot = aiming ? selected_ : -1;
        out.targetSymbolVisible = aiming && in.hasLiveNpcTarget;
        const bool reprepare = previousAim_ && aiming && releaseSlot != selected_;
        out.startAim = aiming && (!previousAim_ || reprepare);
        out.stopAim = previousAim_ && (!aiming || reprepare);
        // Only a real release of an aiming LMB casts. Switching to an empty
        // slot while holding stops aiming without casting.
        out.cast = previousAim_ && !in.leftHeld && in.hasLiveNpcTarget &&
                   releaseSlot >= 0 && releaseSlot < kSpellCount &&
                   kSpellAvailable[releaseSlot] && kSpellAvailable[selected_];
        out.castSlot = out.cast ? releaseSlot : -1;
        previousAim_ = aiming;
        aimedSlot_ = aiming ? selected_ : -1;
        return out;
    }
    int Selected() const { return selected_; }
    void Reset() {
        selected_ = 0;
        previousAim_ = false;
        aimedSlot_ = -1;
        requireLeftRelease_ = false;
    }
    void ResetAfterInterruption() {
        Reset();
        requireLeftRelease_ = true;
    }
private:
    int selected_ = 0;
    bool previousAim_ = false;
    int aimedSlot_ = -1;
    bool requireLeftRelease_ = false;
};
} // namespace ParryHotter
