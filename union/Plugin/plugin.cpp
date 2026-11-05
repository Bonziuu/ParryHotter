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
#include "AimCamera.hpp"
#include "SpellCooldown.hpp"
#include "SpellImpactTag.hpp"
#include "ImpactImpulse.hpp"
#include "FlightRecovery.hpp"
#include "ImpactTelemetry.hpp"
#include "AimTargetState.hpp"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <array>

#ifdef __G1
using namespace Gothic_I_Classic;

// Custom damage hook has access to the real victim, attacker and spell ID.
// Unlike moving a focused target on click, this only acts on a genuine hit.
namespace {
    void ApplyParryImpact(
        oCNpc* victim, const oCNpc::oSDamageDescriptor& descriptor
    );
}

namespace Gothic_I_Classic {
    auto& Ivk_PH_OnDamage_Hit = UnionCore::CreateHook(
        &oCNpc::OnDamage_Hit,
        &oCNpc::PH_OnDamage_Hit,
        UnionCore::Hook_Auto
    );

    void oCNpc::PH_OnDamage_Hit(oSDamageDescriptor& descriptor) {
        // Native damage/collision/AI always has priority.
        THISCALL(Ivk_PH_OnDamage_Hit)(descriptor);
        ApplyParryImpact(this, descriptor);
    }
}

// Gothic I hardcodes RMB as jump (GAME_SMOVE). Intercept input after
// DirectInput processing, then clear that logical jump state only when the
// user didn't also press an actual key bound to jump.
// The original right-mouse state stays readable by our aiming code.
namespace Gothic_I_Classic {
    auto& Ivk_PH_ProcessInputEvents = UnionCore::CreateHook(
        &zCInput_Win32::ProcessInputEvents,
        &zCInput_Win32::PH_ProcessInputEvents,
        UnionCore::Hook_Auto
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
    ParryHotter::AimCamera gCamera;
    ParryHotter::SpellCooldown gCooldown;
    ParryHotter::SpellImpactTag gImpactTag;
    oCNpc* gCastTarget = nullptr;
    bool gImpactApplied = false;
    bool gImpactAttempted = false;
    ParryHotter::ImpactTelemetry gTelemetry;
    bool gDiagnosticVisible = false;
    bool gDiagnosticDirty = true;
    ParryHotter::AimTargetState gAimTargetState = ParryHotter::AimTargetState::NoPlayer;

    void SetAimTargetState(ParryHotter::AimTargetState state) {
        if (gAimTargetState != state) {
            gAimTargetState = state;
            gDiagnosticDirty = true;
        }
    }
    zCView* gDiagnosticPanel = nullptr;

    void NoteImpact(ParryHotter::ImpactEvent event) {
        gTelemetry.Note(event);
        gDiagnosticDirty = true;
    }

    void HideDiagnostics() {
        if (gDiagnosticPanel) {
            if (screen) screen->RemoveItem(gDiagnosticPanel);
            delete gDiagnosticPanel;
            gDiagnosticPanel = nullptr;
        }
        gDiagnosticDirty = true;
    }

    void DrawDiagnostics() {
        if (!screen || !gDiagnosticVisible) {
            HideDiagnostics();
            return;
        }
        if (!gDiagnosticDirty) return;
        HideDiagnostics();
        gDiagnosticPanel = new zCView(220, 420, 5850, 1440);
        screen->InsertItem(gDiagnosticPanel);
        char title[220]{};
        char stats[240]{};
        char causes[260]{};
        std::snprintf(title, sizeof(title),
                      "PARRY HOTTER - F10: ukryj diagnostyke");
        std::snprintf(stats, sizeof(stats),
                      "Trafienia: %u / impulsy: %u / ladowania: %u",
                      gTelemetry.Get(ParryHotter::ImpactEvent::Recognized),
                      gTelemetry.Get(ParryHotter::ImpactEvent::Launched),
                      gTelemetry.Get(ParryHotter::ImpactEvent::Landed));
        std::snprintf(causes, sizeof(causes),
                      "Fizyka wyl.: %u / brak RB: %u / przeszkoda: %u",
                      gTelemetry.Get(ParryHotter::ImpactEvent::NoNativePhysics),
                      gTelemetry.Get(ParryHotter::ImpactEvent::NoRigidBody),
                      gTelemetry.Get(ParryHotter::ImpactEvent::NoClearance));
        gDiagnosticPanel->Print(30, 20, zSTRING(title));
        gDiagnosticPanel->Print(30, 250, zSTRING(stats));
        gDiagnosticPanel->Print(30, 490, zSTRING(causes));
        char targetLine[240]{};
        std::snprintf(targetLine, sizeof(targetLine), "Cel: %s",
                      ParryHotter::AimTargetDescription(gAimTargetState));
        gDiagnosticPanel->Print(30, 710, zSTRING(targetLine));
        gDiagnosticDirty = false;
    }

    ParryHotter::FlightRecovery gFlightRecovery;
    oCNpc* gFlightTarget = nullptr;
    bool gFlightIsBumbardo = false;

    void ClearFlight() {
        gFlightRecovery.Reset();
        if (gFlightTarget) {
            gFlightTarget->Release();
            gFlightTarget = nullptr;
        }
        gFlightIsBumbardo = false;
    }

    void BeginFlight(oCNpc* victim, bool bumbardo) {
        ClearFlight();
        if (!victim) return;
        victim->AddRef();
        gFlightTarget = victim;
        gFlightIsBumbardo = bumbardo;
        gFlightRecovery.Start(ParryHotter::FlightRecovery::Clock::now());
    }

    void ClearCastTarget() {
        if (gCastTarget) {
            gCastTarget->Release();
            gCastTarget = nullptr;
        }
        gImpactApplied = false;
        gImpactAttempted = false;
    }

    bool Clearance(zCWorld* world, oCNpc* npc,
                   const zVEC3& origin, const zVEC3& direction) {
        if (!world) return false;
        const int flags = zTRACERAY_STAT_POLY | zTRACERAY_VOB_BBOX |
                          zTRACERAY_FIRSTHIT;
        // The engine checks walls, ceilings and other VOB collision volumes.
        return world->TraceRayNearestHit(
            origin, direction, static_cast<zCVob*>(npc), flags
        ) == 0;
    }

    void ApplyParryImpact(
        oCNpc* victim, const oCNpc::oSDamageDescriptor& descriptor
    ) {
        if (!victim || victim == player || !player ||
            victim != gCastTarget || gImpactApplied ||
            victim->IsDead() || !victim->IsHuman()) return;
        if (descriptor.pNpcAttacker != player ||
            !(descriptor.dwFieldsValid & oCNpc::oEDamageDescFlag_SpellID))
            return;
        const auto now = ParryHotter::SpellImpactTag::Clock::now();
        const int active = gImpactTag.Active(now);
        const bool levi = active == 2 && descriptor.nSpellID == SPL_ICECUBE;
        const bool bomb = active == 5 && descriptor.nSpellID == SPL_STORMFIST;
        if (!levi && !bomb) return;
        if (gImpactAttempted) return;
        // One diagnostic outcome per real spell hit, even if damage-over-time
        // reports the same impact again. Never spam the HUD with duplicates.
        gImpactAttempted = true;
        NoteImpact(ParryHotter::ImpactEvent::Recognized);

        // ZenGin remains in control of falling, collisions and AI.
        // Never enable physics on an NPC that did not already have it,
        // and never disable gravity or teleport a person through geometry.
        if (!victim->physicsEnabled) {
            NoteImpact(ParryHotter::ImpactEvent::NoNativePhysics);
            return;
        }
        zCRigidBody* body = victim->GetRigidBody();
        if (!body) {
            NoteImpact(ParryHotter::ImpactEvent::NoRigidBody);
            return;
        }
        zCWorld* world = victim->GetHomeWorld();
        if (!world || world != player->GetHomeWorld()) {
            NoteImpact(ParryHotter::ImpactEvent::WrongWorld);
            return;
        }

        const zVEC3 pos = victim->GetPositionWorld();
        const zVEC3 caster = player->GetPositionWorld();
        const bool up = Clearance(world, victim, pos + zVEC3(0, 75, 0),
                                  zVEC3(0, 250, 0));
        zVEC3 away(pos[0] - caster[0], 0, pos[2] - caster[2]);
        bool outward = false;
        if (away.Length_Sqr() > 400.0f) {
            away.Normalize();
            away *= zVEC3(180.0f);
            outward = Clearance(world, victim, pos + zVEC3(0, 75, 0), away);
        }
        const zVEC3 current = victim->GetVelocity();
        const ParryHotter::ImpactPlan plan = ParryHotter::PlanImpact(
            levi ? ParryHotter::ImpactKind::Levicorpus
                 : ParryHotter::ImpactKind::Bumbardo,
            {current[0], current[1], current[2]},
            {caster[0], caster[1], caster[2]},
            {pos[0], pos[1], pos[2]},
            up, outward
        );
        if (!plan.apply) {
            NoteImpact(ParryHotter::ImpactEvent::NoClearance);
            return;
        }
        body->SetVelocity(zVEC3(
            plan.velocity.x, plan.velocity.y, plan.velocity.z
        ));
        BeginFlight(victim, bomb);
        NoteImpact(ParryHotter::ImpactEvent::Launched);
        gImpactApplied = true; // no repeated impulses from damage-over-time
    }

    bool NearSolidFloor(oCNpc* npc) {
        if (!npc) return false;
        zCWorld* world = npc->GetHomeWorld();
        if (!world) return false;
        const zVEC3 pos = npc->GetPositionWorld();
        const zVEC3 origin = pos + zVEC3(0, 65, 0);
        const zVEC3 down(0, -180, 0);
        const int flags = zTRACERAY_STAT_POLY |
                          zTRACERAY_POLY_NORMAL | zTRACERAY_FIRSTHIT;
        if (!world->TraceRayNearestHit(
                origin, down, static_cast<zCVob*>(npc), flags))
            return false;
        const zVEC3 hit = world->traceRayReport.foundIntersection;
        // A steep wall is not a landing surface. Use the floor directly
        // under the NPC, not the endpoint of a distant ray.
        if (world->traceRayReport.foundPolyNormal[1] < 0.50f) return false;
        const float height = pos[1] - hit[1];
        return height > -8.0f && height < 27.0f;
    }

    void UpdateFlight(ParryHotter::FlightRecovery::TimePoint now) {
        if (!gFlightRecovery.Active()) return;
        oCNpc* npc = gFlightTarget;
        const bool valid = npc && player && !npc->IsDead() &&
                           npc->GetHomeWorld() == player->GetHomeWorld();
        const float vertical = valid ? npc->GetVelocity()[1] : 0.0f;
        const bool floor = valid && NearSolidFloor(npc);
        const auto result = gFlightRecovery.Step(now, valid, floor, vertical);
        if (result == ParryHotter::FlightResult::Landed) {
            NoteImpact(ParryHotter::ImpactEvent::Landed);
            // Use an existing short Gothic one-shot animation. Do not
            // override the 4s Levicorpus state; it recovers via Daedalus.
            if (gFlightIsBumbardo && npc && !npc->IsDead()) {
                zCModel* model = npc->GetModel();
                if (model) {
                    model->StartAni(
                        zSTRING("T_STAND_2_LIGHTNING_VICTIM"),
                        zCModel::zMDL_STARTANI_DEFAULT
                    );
                }
            }
        }
        if (result == ParryHotter::FlightResult::Cancelled)
            NoteImpact(ParryHotter::ImpactEvent::Interrupted);
        if (result == ParryHotter::FlightResult::TimedOut)
            NoteImpact(ParryHotter::ImpactEvent::TimedOut);
        if (result == ParryHotter::FlightResult::Landed ||
            result == ParryHotter::FlightResult::Cancelled ||
            result == ParryHotter::FlightResult::TimedOut) {
            ClearFlight();
        }
    }

    std::array<zCView*, 6> gSlots{};
    std::array<zCView*, 6> gTargetIcons{};
    zCView* gSelection = nullptr;
    zCView* gReticle = nullptr;
    bool gAiming = false;
    bool gManaPrepared = false;
    int gShownTargetSlot = -1;
    int gActiveSlot = 0;
    bool gAttached = false;
    std::array<int, 6> gSlotDimmed{{-1, -1, -1, -1, -1, -1}};
    zCModel* gWandModel = nullptr;
    zCModelNodeInst* gWandSocket = nullptr;
    zCVisual* gWandVisual = nullptr;
    // Only render on a native, unused right-hand attachment point. Never
    // replace an equipped melee weapon or persistent character mesh.
    void DetachWand() {
        if (player && gWandModel && player->GetModel() == gWandModel &&
            gWandSocket && gWandSocket->nodeVisual == gWandVisual) {
            gWandModel->SetNodeVisual(gWandSocket, nullptr, 0);
        }
        gWandModel = nullptr;
        gWandSocket = nullptr;
        gWandVisual = nullptr;
    }

    void AttachWand() {
        if (!player) return;
        zCModel* model = player->GetModel();
        if (!model) return;
        if (gWandModel == model && gWandSocket &&
            gWandSocket->nodeVisual == gWandVisual) return;
        // When engine changes the model (loading/armor change) discard stale
        // socket pointers, never dereference a previous model's bone.
        gWandModel = nullptr;
        gWandSocket = nullptr;
        gWandVisual = nullptr;
        zCModelNodeInst* hand = model->SearchNode(zSTRING("ZS_RIGHTHAND"));
        if (!hand || hand->nodeVisual) return;
        zCVisual* visual = zCVisual::LoadVisual(zSTRING("PH_WAND.3DS"));
        if (!visual) return;
        model->SetNodeVisual(hand, visual, 0);
        if (hand->nodeVisual == visual) {
            gWandModel = model;
            gWandSocket = hand;
            gWandVisual = visual;
        }
    }

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
        HideDiagnostics();
        gDiagnosticVisible = false;
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
        gManaPrepared = false;
        gSlotDimmed.fill(-1);
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

    void ShowSlotAvailability(int mana, ParryHotter::SpellCooldown::TimePoint now) {
        for (int i = 0; i < 6; ++i) {
            const bool preparing = gAiming && gManaPrepared &&
                                    gController.Selected() == i;
            const int dim = (preparing || gCooldown.Ready(i, mana, now)) ? 0 : 1;
            if (gSlots[i] && gSlotDimmed[i] != dim) {
                // Visual availability, never removes an icon from HUD.
                gSlots[i]->SetTransparency(dim ? 145 : 0);
                gSlotDimmed[i] = dim;
            }
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

    // Camera-centered aiming is authoritative. The native Gothic focus may
    // point to another nearby NPC and must not silently block the crosshair.
    // The nearest ray hit must itself be a living NPC, not a wall or prop.
    oCNpc* GetCenteredNpcTarget() {
        if (!player) {
            SetAimTargetState(ParryHotter::AimTargetState::NoPlayer);
            return nullptr;
        }
        zCAICamera* cam = zCAICamera::current;
        zCVob* cameraVob = cam ? cam->camVob : nullptr;
        zCWorld* world = player->GetHomeWorld();
        if (!cameraVob) {
            SetAimTargetState(ParryHotter::AimTargetState::NoCamera);
            return nullptr;
        }
        if (!world) {
            SetAimTargetState(ParryHotter::AimTargetState::NoWorld);
            return nullptr;
        }

        zVEC3 direction = cameraVob->GetAtVectorWorld();
        if (direction.Length_Sqr() < 0.01f) {
            SetAimTargetState(ParryHotter::AimTargetState::NoCamera);
            return nullptr;
        }
        direction.Normalize();
        direction *= zVEC3(1900.0f);
        const int flags = zTRACERAY_VOB_BBOX |
                          zTRACERAY_STAT_POLY | zTRACERAY_FIRSTHIT;
        if (!world->TraceRayNearestHit(
                cameraVob->GetPositionWorld(), direction,
                static_cast<zCVob*>(player), flags)) {
            SetAimTargetState(ParryHotter::AimTargetState::NoRayHit);
            return nullptr;
        }
        zCVob* hitVob = world->traceRayReport.foundVob;
        oCNpc* npc = zDYNAMIC_CAST<oCNpc>(hitVob);
        if (!npc || npc == player) {
            SetAimTargetState(ParryHotter::AimTargetState::WallOrObject);
            return nullptr;
        }
        if (npc->IsDead()) {
            SetAimTargetState(ParryHotter::AimTargetState::DeadNpc);
            return nullptr;
        }
        if (npc->GetHomeWorld() != world) {
            SetAimTargetState(ParryHotter::AimTargetState::WrongWorld);
            return nullptr;
        }
        const float distance = player->GetDistanceToVob(*npc);
        if (distance < 0.0f || distance >= 1900.0f) {
            SetAimTargetState(ParryHotter::AimTargetState::OutOfRange);
            return nullptr;
        }
        SetAimTargetState(ParryHotter::AimTargetState::ValidNpc);
        return npc;
    }

    bool HasValidNpcTarget() {
        return GetCenteredNpcTarget() != nullptr;
    }

    // Use the native spellbook so that mana, target rules and Gothic visual FX
    // still apply. Test the call ordering on a LOCAL Gothic I installation;
    // engine variations (Steam + Union) may require an event hook.
    void RaiseWand(int spellSlot) {
        if (!player) return;
        const int mana = player->GetAttribute(NPC_ATR_MANA);
        const auto now = ParryHotter::SpellCooldown::Clock::now();
        if (gAiming) {
            DetachWand();
            player->UnreadySpell();
            gAiming = false;
            gManaPrepared = false;
        }
        // Check mana BEFORE Gothic invests it. Do not re-check during casting.
        if (!gCooldown.Ready(spellSlot, mana, now)) return;
        const int id = kGothicSpellIds[spellSlot];
        // ReadySpell returns an undocumented integer in Gothic 1. Never
        // assume 0 means failure; native Spell_Cast performs the final check.
        player->ReadySpell(id, 0);
        gManaPrepared = true;
        gAiming = true;
        AttachWand();
    }

    void LowerWand() {
        DetachWand();
        if (player && gAiming) player->UnreadySpell();
        gAiming = false;
        gManaPrepared = false;
    }

    void SetUnionPresence(bool enabled) {
        if (!parser) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_UNION_LOADED"));
        if (symbol) symbol->SetValue(enabled ? 1 : 0, 0);
    }

    void ResetSelectedSpellForScript() {
        ClearFlight();
        ClearCastTarget();
        gImpactTag.Reset();
        if (!parser) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_SELECTED_SPELL"));
        if (symbol) symbol->SetValue(0, 0);
    }

    void SyncSelectedSpellForScript(int slot, oCNpc* target) {
        // Keep the last genuine center-ray target so a later projectile
        // impact does not accidentally move a different focused NPC.
        if (!parser || slot < 0 || slot >= 6 || !target) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_SELECTED_SPELL"));
        if (!symbol) return;
        ClearCastTarget();
        target->AddRef();
        gCastTarget = target;
        symbol->SetValue(slot + 1, 0);
        gImpactTag.MarkCast(slot, ParryHotter::SpellImpactTag::Clock::now());
    }

    bool CastAtCenteredNpc(int slot) {
        if (!player) return false;
        oCNpc* target = GetCenteredNpcTarget();
        if (!target) return false;
        oCMag_Book* spellbook = player->GetSpellBook();
        if (!spellbook) {
            SetAimTargetState(ParryHotter::AimTargetState::NoSpellBook);
            return false;
        }
        // Bind the game engine's target to the actual center-screen NPC.
        // Gothic's own focus detection may otherwise select a neighbor.
        player->SetFocusVob(static_cast<zCVob*>(target));
        SyncSelectedSpellForScript(slot, target);
        spellbook->Spell_Cast();
        return true;
    }

    void GameTick() {
        const bool gameplay =
            ogame && screen && player && zinput &&
            !ogame->inLoadSaveGame && !ogame->inLevelChange &&
            !player->openinventory &&
            (!zCAICamera::current || !zCAICamera::current->inCutsceneMode);
        if (!gameplay) {
            // Opening inventory, changing levels or entering a cutscene
            // cancels unfinished air tracking. Never animate a paused NPC.
            if (gFlightRecovery.Active())
                NoteImpact(ParryHotter::ImpactEvent::Interrupted);
            if (gFlightRecovery.Active() || gCastTarget ||
                gImpactTag.Active(ParryHotter::SpellImpactTag::Clock::now()))
                ResetSelectedSpellForScript();
            HideDiagnostics();
            gCamera.Restore(zCAICamera::current);
            if (gAiming) LowerWand();
            gController.Step({});
            ShowTarget(false, gActiveSlot);
            return;
        }
        if (zinput->KeyToggled(KEY_F10)) {
            gDiagnosticVisible = !gDiagnosticVisible;
            gDiagnosticDirty = true;
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

        const auto now = ParryHotter::SpellCooldown::Clock::now();
        UpdateFlight(now);
        if (gImpactTag.Expired(now)) ResetSelectedSpellForScript();
        const int mana = player->GetAttribute(NPC_ATR_MANA);
        ShowHud();
        ShowSlotAvailability(mana, now);
        ShowSelection(out.selected);
        ShowTarget(out.targetSymbolVisible, out.selected);
        DrawDiagnostics();
        // Keep the Gothic camera's own mouse rotation and collision.
        // Our helper only moves the distance/shoulder for spell targeting,
        // and restores previous values after PPM is released.
        gCamera.Update(zCAICamera::current, out.aiming);

        if (out.stopAim) {
            LowerWand();
        }
        if (out.startAim || (hotkey && out.aiming)) {
            RaiseWand(out.selected);
        }
        if (out.aiming && gAiming) {
            AttachWand(); // Model may have changed with a clothing swap.
        }
        if (out.aiming && !gManaPrepared &&
            gCooldown.Ready(out.selected, mana, now)) {
            // Holding PPM can re-arm after the previous cooldown expires.
            RaiseWand(out.selected);
        }
        if (out.aiming && gAiming && gManaPrepared && player->GetSpellBook() &&
            gCooldown.ReadyByTime(out.selected, now)) {
            // Gothic invests mana internally. Keep the prepared cast valid
            // even if the visible mana counter reaches zero mid-charge.
            player->GetSpellBook()->Spell_Invest();
        }
        if (out.cast && gAiming && gManaPrepared &&
            gCooldown.ReadyByTime(out.selected, now)) {
            if (CastAtCenteredNpc(out.selected)) {
                gCooldown.MarkPreparedCast(out.selected, now);
                gManaPrepared = false;
                ShowSlotAvailability(player->GetAttribute(NPC_ATR_MANA), now);
            }
        }
    }
} // anonymous namespace

// Standard Union DLL exports, called by engine event hooks in template.
cexport void Game_Entry() {}
cexport void Game_Init() {
    gTelemetry.Reset();
    gDiagnosticDirty = true;
    SetUnionPresence(true);
    gController.Reset();
    gCooldown.Reset();
    gImpactTag.Reset();
}
cexport void Game_Exit() {
    ResetSelectedSpellForScript();
    SetUnionPresence(false);
    gCamera.Restore(zCAICamera::current);
    if (gAiming) LowerWand();
    RemoveHud();
}
cexport void Game_PreLoop() {}
cexport void Game_Loop() { GameTick(); }
cexport void Game_PostLoop() {}
cexport void Game_MenuLoop() {}
cexport void Game_SaveBegin() {
    // Never serialize PH_UNION_LOADED=1 into a save used later without the
    // plugin. DEV mode should still recognize its own six rune placeholders.
    ResetSelectedSpellForScript();
    SetUnionPresence(false);
}
cexport void Game_SaveEnd() {
    SetUnionPresence(true);
}
cexport void Game_LoadBegin_NewGame() {
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    ResetSelectedSpellForScript();
    if (gAiming) LowerWand();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_NewGame() {
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.Reset();
    gCooldown.Reset();
}
cexport void Game_LoadBegin_SaveGame() {
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    ResetSelectedSpellForScript();
    if (gAiming) LowerWand();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_SaveGame() {
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.Reset();
    gCooldown.Reset();
}
cexport void Game_LoadBegin_ChangeLevel() {
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    ResetSelectedSpellForScript();
    if (gAiming) LowerWand();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_ChangeLevel() {
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.Reset();
    gCooldown.Reset();
}
cexport void Game_LoadBegin_Trigger() {}
cexport void Game_LoadEnd_Trigger() {}
cexport void Game_Pause() { gCamera.Restore(zCAICamera::current); }
cexport void Game_Unpause() {}
cexport void Game_DefineExternals() {}
cexport void Game_ApplyOptions() {}
#endif
