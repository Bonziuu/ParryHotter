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
};
constexpr int kSpellCount = 6;
constexpr std::array<const char*, kSpellCount> kNames{{
    "Flipendo", "Levicorpus", "Rictusempra",
    "Expelliarmus", "Bumbardo Maximus", "Avada Kedavra"
}};
constexpr std::array<const char*, kSpellCount> kTextures{{
    "PH_FLIPENDO.TGA", "PH_LEVICORPUS.TGA",
    "PH_RICTUSEMPRA.TGA", "PH_EXPELLIARMUS.TGA",
    "PH_BUMBARDO.TGA", "PH_AVADA.TGA"
}};

struct Input {
    bool gameplay = false;
    int slotPressed = 0;     // 0 means no selection, 1..6 are shortcuts
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
    Output Step(Input in) {
        Output out{};
        if (!in.gameplay) {
            out.stopAim = previousAim_;
            previousAim_ = false;
            previousLeft_ = false;
            out.selected = selected_;
            return out;
        }
        if (in.slotPressed >= 1 && in.slotPressed <= kSpellCount) {
            selected_ = in.slotPressed - 1;
        }
        out.hudVisible = true;
        out.selected = selected_;
        out.aiming = in.rightHeld;
        out.targetSymbolVisible = in.hasLiveNpcTarget;
        out.startAim = in.rightHeld && !previousAim_;
        out.stopAim = !in.rightHeld && previousAim_;
        // Never cast simply from moving the mouse over a target; a deliberate
        // fresh click is required. Holding LMB does not trigger repeated casts.
        out.cast = in.rightHeld && in.leftHeld && !previousLeft_
                    && in.hasLiveNpcTarget;
        previousAim_ = in.rightHeld;
        previousLeft_ = in.leftHeld;
        return out;
    }
    int Selected() const { return selected_; }
    void Reset() { selected_ = 0; previousAim_ = false; previousLeft_ = false; }
private:
    int selected_ = 0;
    bool previousAim_ = false;
    bool previousLeft_ = false;
};
} // namespace ParryHotter
