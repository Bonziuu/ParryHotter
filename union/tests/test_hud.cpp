#include "../hud/SpellHudState.hpp"
#include <cassert>
#include <iostream>
using namespace ParryHotter;

int main() {
    Controller ctl;
    assert(kSpellCount == 6);
    assert(kNames.size() == 6 && kTextures.size() == 6);

    auto o = ctl.Step({true, 0, false, false, false});
    assert(o.hudVisible && o.selected == 0 && !o.cast && !o.aiming);
    o = ctl.Step({true, 3, false, false, true});
    assert(o.selected == 2 && o.targetSymbolVisible && !o.cast);

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

    // Hotkeys stay within the six-slot HUD.
    o = ctl.Step({true, 6, true, false, true});
    assert(o.selected == 5);
    o = ctl.Step({true, 9, true, false, true});
    assert(o.selected == 5);
    o = ctl.Step({true, 0, false, false, true});
    assert(o.stopAim && !o.aiming);

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
    assert(!o.aiming && !o.startAim && !o.cast);
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
    std::cout << "Parry Hotter strict RMB-then-LMB input: OK\n";
}
