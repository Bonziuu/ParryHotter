#include "../hud/SpellHudState.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace ParryHotter;

// Input field order: gameplay, slotPressed, rightHeld, leftHeld, hasLiveNpcTarget.
int main() {
    Controller ctl;
    assert(kSpellCount == 10);
    assert(kNames.size() == 10 && kTextures.size() == 10);
    assert(std::string(kHotkeyLabels.front()) == "1");
    assert(std::string(kHotkeyLabels.back()) == "0");

    auto o = ctl.Step({true, 0, false, false, false});
    assert(o.hudVisible && o.selected == 0 && !o.cast && !o.aiming);
    o = ctl.Step({true, 3, false, false, true});
    assert(o.selected == 2 && !o.targetSymbolVisible && !o.cast);

    // HP2: pressing LMB raises the wand and aims; the symbol shows on a target.
    o = ctl.Step({true, 0, false, true, true});
    assert(o.startAim && o.aiming && !o.cast && o.targetSymbolVisible);
    o = ctl.Step({true, 0, false, true, true});
    assert(o.aiming && !o.startAim && !o.cast); // holding never fires
    // Releasing LMB on a valid target casts exactly once.
    o = ctl.Step({true, 0, false, false, true});
    assert(o.cast && o.stopAim && !o.aiming);
    o = ctl.Step({true, 0, false, false, true});
    assert(!o.cast && !o.stopAim);

    // Releasing without a valid target (wall, dead NPC, out of range) misses
    // silently: no cast, wand lowers.
    o = ctl.Step({true, 0, false, true, true});
    assert(o.startAim);
    o = ctl.Step({true, 0, false, true, false});
    assert(o.aiming && !o.targetSymbolVisible);
    o = ctl.Step({true, 0, false, false, false});
    assert(!o.cast && o.stopAim);

    // RMB (jump) never aims or casts.
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.aiming && !o.cast && !o.startAim);
    // RMB held while aiming does not change the LMB result.
    o = ctl.Step({true, 0, true, true, true});
    assert(o.startAim && o.aiming);
    o = ctl.Step({true, 0, true, false, true});
    assert(o.cast);

    // Switching slots while holding: another spell keeps aiming; an empty
    // slot stops aiming and the later release cannot cast.
    o = ctl.Step({true, 0, false, true, true});
    assert(o.startAim);
    o = ctl.Step({true, 2, false, true, true});
    assert(o.selected == 1 && o.aiming && !o.startAim && !o.cast);
    o = ctl.Step({true, 7, false, true, true});
    assert(o.selected == 6 && !o.aiming && o.stopAim && !o.cast);
    o = ctl.Step({true, 0, false, false, true});
    assert(!o.cast && !o.stopAim);
    for (int shortcut = 1; shortcut <= 10; ++shortcut) {
        o = ctl.Step({true, shortcut, false, false, true});
        assert(o.selected == shortcut - 1 && !o.aiming && !o.cast);
    }
    o = ctl.Step({true, 11, false, false, true});
    assert(o.selected == 9); // out-of-range input must preserve selection
    // Empty slots 7-0: pressing and releasing LMB does nothing.
    o = ctl.Step({true, 0, false, true, true});
    assert(!o.aiming && !o.startAim && !o.targetSymbolVisible);
    o = ctl.Step({true, 0, false, false, true});
    assert(!o.cast);

    // Interruption while aiming lowers the wand and never casts; LMB held
    // through the menu must be released before aiming again.
    o = ctl.Step({true, 1, false, false, true});
    o = ctl.Step({true, 0, false, true, true});
    assert(o.startAim);
    o = ctl.Step({false, 0, false, true, true});
    assert(o.stopAim && !o.hudVisible && !o.cast);
    o = ctl.Step({true, 0, false, true, true});
    assert(!o.aiming && !o.startAim && !o.cast);
    o = ctl.Step({true, 0, false, false, true});
    assert(!o.cast); // releasing the stale press cannot cast
    o = ctl.Step({true, 0, false, true, true});
    assert(o.startAim);
    o = ctl.Step({true, 0, false, false, true});
    assert(o.cast);

    ctl.Reset();
    assert(ctl.Selected() == 0);
    // HUD rebuild/options and load-end can reset the controller repeatedly.
    // They must preserve the release requirement across the full lifecycle.
    ctl.ResetAfterInterruption();
    ctl.ResetAfterInterruption();
    o = ctl.Step({true, 0, false, true, true});
    assert(!o.startAim && !o.aiming && !o.cast);
    o = ctl.Step({true, 0, false, false, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, false, true, true});
    assert(o.startAim && !o.cast);
    o = ctl.Step({true, 0, false, false, true});
    assert(o.cast);
    std::cout << "Parry Hotter HP2 hold-LMB aim, release-LMB cast: OK\n";
}
