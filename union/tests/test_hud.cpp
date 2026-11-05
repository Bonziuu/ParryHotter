#include "../hud/SpellHudState.hpp"
#include <cassert>
#include <iostream>
using namespace ParryHotter;

int main() {
    Controller ctl;
    assert(kSpellCount == 6);
    assert(kNames.size() == 6 && kTextures.size() == 6);

    auto o = ctl.Step({true,0,false,false,false});
    assert(o.hudVisible && o.selected == 0 && !o.cast && !o.aiming);
    o = ctl.Step({true,3,false,false,true});
    assert(o.selected == 2 && o.targetSymbolVisible && !o.cast);

    // PPM raises wand; moving over a target only shows its glyph.
    o = ctl.Step({true,0,true,false,true});
    assert(o.startAim && o.aiming && !o.cast && o.targetSymbolVisible);
    o = ctl.Step({true,0,true,true,true});
    assert(o.cast && o.aiming);
    o = ctl.Step({true,0,true,true,true});
    assert(!o.cast); // no machine-gun effect when LMB held
    o = ctl.Step({true,0,true,false,true});
    assert(!o.cast);
    o = ctl.Step({true,0,true,true,false});
    assert(!o.cast); // do not shoot if cursor is not on living NPC
    o = ctl.Step({true,0,true,false,true});
    o = ctl.Step({true,0,true,true,true});
    assert(o.cast);

    o = ctl.Step({true,6,true,false,true});
    assert(o.selected == 5);
    o = ctl.Step({true,9,true,false,true});
    assert(o.selected == 5); // never allow slot outside 1..6
    o = ctl.Step({true,0,false,false,true});
    assert(o.stopAim && !o.aiming);
    o = ctl.Step({false,0,true,true,true});
    assert(!o.hudVisible && !o.targetSymbolVisible && !o.cast);
    o = ctl.Step({true,0,true,true,true});
    assert(o.startAim && o.cast);
    ctl.Reset();
    assert(ctl.Selected() == 0);
    std::cout << "Parry Hotter input/HUD state: OK\n";
}
