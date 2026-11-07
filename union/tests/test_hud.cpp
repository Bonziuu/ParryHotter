#include "../hud/SpellHudState.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace ParryHotter;

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

    // RMB must be held BEFORE a separate LMB click.
    o = ctl.Step({true, 0, true, false, true});
    assert(o.startAim && o.aiming && !o.cast && o.targetSymbolVisible);
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.cast); // now LMB is released while aiming, so it is armed
    o = ctl.Step({true, 0, true, true, true});
    assert(o.cast && o.aiming);
    o = ctl.Step({true, 0, true, true, true});
    assert(!o.cast); // holding LMB cannot repeat
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, true, true, false});
    assert(!o.cast); // no ray-target, no firing
    o = ctl.Step({true, 0, true, false, true});
    o = ctl.Step({true, 0, true, true, true});
    assert(o.cast);

    // All ten shortcuts are selectable, including physical key 0 -> slot 10.
    for (int shortcut = 1; shortcut <= kSpellCount; ++shortcut) {
        o = ctl.Step({true, shortcut, true, false, true});
        assert(o.selected == shortcut - 1 && !o.cast);
        assert(o.aiming == kSpellAvailable[shortcut - 1]);
        assert(o.targetSymbolVisible == kSpellAvailable[shortcut - 1]);
        if (shortcut == 7) assert(o.stopAim); // changing to blank lowers wand
    }
    o = ctl.Step({true, 11, true, false, true});
    assert(o.selected == 9); // out-of-range input must preserve selection
    o = ctl.Step({true, 0, false, false, true});
    assert(!o.aiming && !o.targetSymbolVisible && o.selected == 9);

    // Empty slot 0 stays selectable but cannot prepare or cast a hidden spell.
    o = ctl.Step({true, 10, true, false, true});
    assert(o.selected == 9 && !o.aiming && !o.startAim && !o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(!o.cast && !o.targetSymbolVisible);

    // The sign disappears immediately when the ray loses the NPC.
    o = ctl.Step({true, 1, true, false, false});
    assert(o.aiming && !o.targetSymbolVisible && !o.cast);

    // RMB + LMB on the same frame MUST NOT be considered prepared.
    ctl.Reset();
    o = ctl.Step({true, 0, true, true, true});
    assert(o.startAim && o.aiming && !o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(o.cast); // only fresh LMB AFTER it was released

    // Holding LMB before pressing RMB also cannot accidentally cast.
    ctl.Reset();
    o = ctl.Step({true, 0, false, true, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(o.startAim && !o.cast);
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(o.cast);

    // Leave gameplay with buttons down (inventory, dialogue, cutscene).
    // Returning with RMB still held must require a full release first.
    o = ctl.Step({false, 0, true, true, true});
    assert(o.stopAim && !o.hudVisible && !o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(!o.aiming && !o.startAim && !o.cast && !o.targetSymbolVisible);
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.aiming && !o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(!o.aiming && !o.cast);
    o = ctl.Step({true, 0, false, true, true});
    assert(!o.cast && !o.aiming); // finally release RMB
    o = ctl.Step({true, 0, true, true, true});
    assert(o.startAim && !o.cast);
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(o.cast);

    ctl.Reset();
    assert(ctl.Selected() == 0);
    // HUD rebuild/options and load-end can reset the controller repeatedly.
    // They must preserve the release requirement across the full lifecycle.
    ctl.ResetAfterInterruption();
    ctl.ResetAfterInterruption();
    o = ctl.Step({true, 0, true, false, true});
    assert(!o.startAim && !o.aiming && !o.cast);
    o = ctl.Step({true, 0, true, true, true});
    assert(!o.aiming && !o.cast);
    ctl.Step({true, 0, false, false, true});
    o = ctl.Step({true, 0, true, false, true});
    assert(o.startAim && !o.cast);
    ctl.Step({true, 0, true, false, true});
    o = ctl.Step({true, 0, true, true, true});
    assert(o.cast);
    std::cout << "Parry Hotter strict RMB-then-LMB input: OK\n";
}
