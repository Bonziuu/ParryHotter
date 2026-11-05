// Parry Hotter: Union plugin for the ORIGINAL Gothic I engine.
// Integration source for Gratt-5r2/UnionProject, G1 Release | Win32.
//
// This is a G1 ENGINE INTEGRATION CANDIDATE, not yet a tested release DLL.
// It uses the Gothic I API, not Windows screen overlays. Target glyphs,
// reticle and all six slots are zCView layers rendered inside ZenGin.
//
// Build template supplies "plugin.h" and UnionAfx.h.
// See union/README.md before installing the experimental plugin.
#include "plugin.h"
#include "UnionAfx.h"
#include "SpellHudState.hpp"
#include <array>

#ifdef __G1
using namespace Gothic_I_Classic;

// Gothic I hardcodes RMB as jump (GAME_SMOVE). Intercept input after
// DirectInput processing, then clear that logical jump state only when the
// user didn't also press an actual key bound to jump.
// The original right-mouse state stays readable by our aiming code.
namespace Gothic_I_Classic {
    HOOK Ivk_PH_ProcessInputEvents PATCH(
        &zCInput_Win32::ProcessInputEvents,
        &zCInput_Win32::PH_ProcessInputEvents
    );

    void zCInput_Win32::PH_ProcessInputEvents() {
        THISCALL(Ivk_PH_ProcessInputEvents)();
        if (!this->GetMouseButtonPressedRight()) return;
        if (this->GetState(GAME_SMOVE) == 0.0f) return;

        zCArray<unsigned short> jumpBindings;
        this->GetBinding(GAME_SMOVE, jumpBindings);
        for (int i = 0; i < jumpBindings.GetNumInList(); ++i) {
            const int key = jumpBindings[i];
            // A physical keyboard jump is still allowed during aiming.
            if (key < MIN_MOUSE_BTNS_AND_CODES && this->KeyPressed(key))
                return;
        }
        this->SetState(GAME_SMOVE, 0);
    }
}

namespace {
    constexpr int kSize = 510;     // 8192x8192 resolution-independent coords
    constexpr int kSpace = 92;
    constexpr int kGap = 32;
    constexpr int kLeft = (8192 - (6 * kSize + 5 * kGap)) / 2;
    constexpr int kTop = 7440;
    constexpr int kTargetSize = 400;

    // These are the existing spells used by mod/Magic/ParryHotter.d.
    constexpr std::array<int, 6> kGothicSpellIds{{
        SPL_WINDFIST, SPL_ICECUBE, SPL_BERZERK,
        SPL_THUNDERBALL, SPL_STORMFIST, SPL_BREATHOFDEATH
    }};
    constexpr std::array<int, 6> kHotkeys{{
        KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6
    }};
    constexpr std::array<const char*, 6> kTargetTextures{{
        "PH_TARGET_FLIPENDO.TGA",
        "PH_TARGET_LEVICORPUS.TGA",
        "PH_TARGET_RICTUSEMPRA.TGA",
        "PH_TARGET_EXPELLIARMUS.TGA",
        "PH_TARGET_BUMBARDO.TGA",
        "PH_TARGET_AVADA.TGA"
    }};

    ParryHotter::Controller gController;
    std::array<zCView*, 6> gSlots{};
    std::array<zCView*, 6> gTargetIcons{};
    zCView* gSelection = nullptr;
    zCView* gReticle = nullptr;
    bool gAiming = false;
    int gShownTargetSlot = -1;
    int gActiveSlot = 0;
    bool gAttached = false;

    zCView* NewSprite(int x, int y, int width, int height, const char* name) {
        zCView* view = new zCView(x, y, x + width, y + height);
        view->InsertBack(zSTRING(name));
        return view;
    }

    void BuildHud() {
        if (!screen || gReticle) return;
        for (int i = 0; i < 6; ++i) {
            const int x = kLeft + i * (kSize + kGap);
            gSlots[i] = NewSprite(
                x, kTop, kSize, kSize, ParryHotter::kTextures[i]
            );
            gTargetIcons[i] = NewSprite(
                4096 + kSpace, 4096 - kTargetSize / 2,
                kTargetSize, kTargetSize, kTargetTextures[i]
            );
            // Text is created once, rather than leaking text every frame.
            gSlots[i]->Print(35, 30, zSTRING(i + 1));
        }
        gSelection = NewSprite(kLeft, kTop, kSize, kSize, "PH_SELECTED.TGA");
        gReticle = NewSprite(4096 - 48, 4096 - 48, 96, 96, "PH_RETICLE.TGA");
    }

    void RemoveHud() {
        if (screen) {
            for (auto* view : gTargetIcons) {
                if (view) screen->RemoveItem(view);
            }
            for (auto* view : gSlots) {
                if (view) screen->RemoveItem(view);
            }
            if (gSelection) screen->RemoveItem(gSelection);
            if (gReticle) screen->RemoveItem(gReticle);
        }
        for (auto*& view : gTargetIcons) { delete view; view = nullptr; }
        for (auto*& view : gSlots) { delete view; view = nullptr; }
        delete gSelection; gSelection = nullptr;
        delete gReticle; gReticle = nullptr;
        gAttached = false;
        gShownTargetSlot = -1;
        gActiveSlot = 0;
        gAiming = false;
        gController.Reset();
    }

    void ShowHud() {
        BuildHud();
        if (!gReticle || !screen) return;
        if (!gAttached) {
            for (auto* view : gSlots) screen->InsertItem(view);
            screen->InsertItem(gSelection);
            screen->InsertItem(gReticle);
            gAttached = true;
        }
    }

    void ShowSelection(int spell) {
        if (!gSelection || !screen || spell == gActiveSlot) return;
        screen->RemoveItem(gSelection);
        gSelection->SetPos(kLeft + spell * (kSize + kGap), kTop);
        screen->InsertItem(gSelection);
        gActiveSlot = spell;
    }

    void ShowTarget(bool hasTarget, int spell) {
        if (!screen || !gReticle) return;
        const int targetIndex = hasTarget ? spell : -1;
        if (targetIndex == gShownTargetSlot) return;
        if (gShownTargetSlot >= 0) {
            screen->RemoveItem(gTargetIcons[gShownTargetSlot]);
        }
        if (targetIndex >= 0) {
            screen->InsertItem(gTargetIcons[targetIndex]);
        }
        gShownTargetSlot = targetIndex;
    }

    // Aiming and cast only use NPCs the Gothic engine has already focused.
    // Do not cast through walls simply because a cursor is nearby.
    bool HasValidNpcTarget() {
        if (!player) return false;
        oCNpc* focus = player->GetFocusNpc();
        return focus && focus != player && !focus->IsDead();
    }

    // Use the native spellbook so that mana, target rules and Gothic visual FX
    // still apply. Test the call ordering on a LOCAL Gothic I installation;
    // engine variations (Steam + Union) may require an event hook.
    void RaiseWand(int spellSlot) {
        if (!player) return;
        const int id = kGothicSpellIds[spellSlot];
        player->ReadySpell(id, 0);
        gAiming = true;
    }

    void LowerWand() {
        if (player && gAiming) player->UnreadySpell();
        gAiming = false;
    }

    void CastAtFocusedNpc() {
        if (!player || !HasValidNpcTarget()) return;
        oCMag_Book* spellbook = player->GetSpellBook();
        if (!spellbook) return;
        spellbook->Spell_Cast();
    }

    void GameTick() {
        const bool gameplay = (ogame && screen && player && zinput);
        if (!gameplay) {
            if (gAiming) LowerWand();
            gController.Step({});
            return;
        }
        int hotkey = 0;
        for (int i = 0; i < 6; ++i) {
            if (zinput->KeyToggled(kHotkeys[i])) {
                hotkey = i + 1;
                break;
            }
        }
        const bool right = zinput->GetMouseButtonPressedRight() != 0;
        const bool left = zinput->GetMouseButtonPressedLeft() != 0;
        const bool target = HasValidNpcTarget();
        ParryHotter::Input input{true, hotkey, right, left, target};
        auto out = gController.Step(input);

        ShowHud();
        ShowSelection(out.selected);
        ShowTarget(out.targetSymbolVisible, out.selected);

        if (out.stopAim) {
            LowerWand();
        }
        if (out.startAim || (hotkey && right)) {
            RaiseWand(out.selected);
        }
        if (out.aiming && gAiming && player->GetSpellBook()) {
            // Gothic itself decides mana investment and cast readiness.
            player->GetSpellBook()->Spell_Invest();
        }
        if (out.cast && gAiming) {
            CastAtFocusedNpc();
        }
    }
} // anonymous namespace

// Standard Union DLL exports, called by engine event hooks in template.
cexport void Game_Entry() {}
cexport void Game_Init() { gController.Reset(); }
cexport void Game_Exit() { RemoveHud(); }
cexport void Game_PreLoop() {}
cexport void Game_Loop() { GameTick(); }
cexport void Game_PostLoop() {}
cexport void Game_MenuLoop() {}
cexport void Game_SaveBegin() {}
cexport void Game_SaveEnd() {}
cexport void Game_LoadBegin_NewGame() { RemoveHud(); }
cexport void Game_LoadEnd_NewGame() { gController.Reset(); }
cexport void Game_LoadBegin_SaveGame() { RemoveHud(); }
cexport void Game_LoadEnd_SaveGame() { gController.Reset(); }
cexport void Game_LoadBegin_ChangeLevel() { RemoveHud(); }
cexport void Game_LoadEnd_ChangeLevel() { gController.Reset(); }
cexport void Game_LoadBegin_Trigger() {}
cexport void Game_LoadEnd_Trigger() {}
cexport void Game_Pause() {}
cexport void Game_Unpause() {}
cexport void Game_DefineExternals() {}
cexport void Game_ApplyOptions() {}
#endif
