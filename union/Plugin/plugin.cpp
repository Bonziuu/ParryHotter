// Parry Hotter: Union plugin for the ORIGINAL Gothic I engine.
// Integration source for Gratt-5r2/UnionProject, G1 Release | Win32.
//
// This is a G1 ENGINE INTEGRATION CANDIDATE, not yet a tested release DLL.
// It uses the Gothic I API, not Windows screen overlays. Target glyphs,
// reticle and all ten slots are zCView layers rendered inside ZenGin.
//
// Build template supplies "plugin.h" and UnionAfx.h.
// See union/README.md before installing the experimental plugin.
#include "plugin.h"
#include "UnionAfx.h"
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "SpellHudState.hpp"
#include "AimCamera.hpp"
#include "SpellCooldown.hpp"
#include "SpellImpactTag.hpp"
#include "ImpactImpulse.hpp"
#include "FlightRecovery.hpp"
#include "ImpactTelemetry.hpp"
#include "AimTargetState.hpp"
#include "TargetGlyph.hpp"
#include "CastLedger.hpp"
#include "CastLog.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <array>
#include <cstdint>

#ifdef __G1
#include "PlayerControls.hpp"
#include "CastingAnimations.hpp"
using namespace Gothic_I_Classic;

// Custom damage hook has access to the real victim, attacker and spell ID.
// Unlike moving a focused target on click, this only acts on a genuine hit.
namespace {
    void ApplyParryImpact(
        oCNpc* victim, const oCNpc::oSDamageDescriptor& descriptor
    );
    int CastTracked(oCSpell* spell);
    void BeginDamage(oCNpc*, const oCNpc::oSDamageDescriptor&);
    void EndDamage();
    void NotifyCastResult(int slot, bool success);
    bool OwnsMagicInput();
}

namespace Gothic_I_Classic {
    auto& Ivk_PH_OnDamage = UnionCore::CreateHook(
        static_cast<void (oCNpc::*)(oCNpc::oSDamageDescriptor&)>(&oCNpc::OnDamage),
        &oCNpc::PH_OnDamage, UnionCore::Hook_Auto);
    void oCNpc::PH_OnDamage(oSDamageDescriptor& descriptor) {
        BeginDamage(this, descriptor);
        THISCALL(Ivk_PH_OnDamage)(descriptor);
        EndDamage();
    }
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

    auto& Ivk_PH_Cast = UnionCore::CreateHook(
        &oCSpell::Cast, &oCSpell::PH_Cast, UnionCore::Hook_Auto);
    int oCSpell::PH_Cast() { return CastTracked(this); }

    auto& Ivk_PH_HumanAI = UnionCore::CreateHook(
        &oCAIHuman::DoAI, &oCAIHuman::PH_DoAI, UnionCore::Hook_Auto);
    void oCAIHuman::PH_DoAI(zCVob* vob, int& state) {
        // Hide owned mouse buttons from the player's native AI only
        // (RMB + W jumped in-game; LMB would attack).
        const bool own = this->npc && this->npc == player &&
            ParryControls::ContextAvailable();
        ParryControls::maskNativeLeft = own && ParryControls::mouseGuard.StripsLeft();
        ParryControls::maskNativeRight = own && ParryControls::mouseGuard.StripsRight();
        ParryControls::BeforeHumanAI(this);
        THISCALL(Ivk_PH_HumanAI)(vob, state);
        ParryControls::AfterHumanAI(this);
        ParryControls::maskNativeLeft = ParryControls::maskNativeRight = false;
    }
    auto& Ivk_PH_WeaponChoose = UnionCore::CreateHook(
        &oCAIHuman::WeaponChoose, &oCAIHuman::PH_WeaponChoose, UnionCore::Hook_Auto);
    void oCAIHuman::PH_WeaponChoose() {
        // Number keys belong to spells even when F11 restores native movement.
        if (npc == player && ParryControls::ContextAvailable()) return;
        THISCALL(Ivk_PH_WeaponChoose)();
    }
    auto& Ivk_PH_MagicMode = UnionCore::CreateHook(
        &oCAIHuman::MagicMode, &oCAIHuman::PH_MagicMode, UnionCore::Hook_Auto);
    int oCAIHuman::PH_MagicMode() {
        // Invest/cast are driven exactly once by our hold-LMB/release controller.
        if (npc == player && OwnsMagicInput()) return 1;
        return THISCALL(Ivk_PH_MagicMode)();
    }
    auto& Ivk_PH_CameraAI = UnionCore::CreateHook(
        &zCAICamera::DoAI, &zCAICamera::PH_DoAI, UnionCore::Hook_Auto);
    void zCAICamera::PH_DoAI(zCVob* vob, int& state) {
        ParryControls::UpdateCamera(this);
        THISCALL(Ivk_PH_CameraAI)(vob, state);
    }
    auto& Ivk_PH_CameraKeys = UnionCore::CreateHook(
        &zCAICamera::CheckKeys, &zCAICamera::PH_CheckKeys, UnionCore::Hook_Auto);
    void zCAICamera::PH_CheckKeys() {
        if (!ParryControls::OwnsInput()) THISCALL(Ivk_PH_CameraKeys)();
    }
}

namespace Gothic_I_Classic {
    auto& Ivk_PH_MouseRight = UnionCore::CreateHook(
        &zCInput_Win32::GetMouseButtonPressedRight,
        &zCInput_Win32::PH_GetMouseButtonPressedRight, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_GetMouseButtonPressedRight() {
        if (ParryControls::maskNativeRight) return 0;
        return THISCALL(Ivk_PH_MouseRight)();
    }

    auto& Ivk_PH_MouseLeft = UnionCore::CreateHook(
        &zCInput_Win32::GetMouseButtonPressedLeft,
        &zCInput_Win32::PH_GetMouseButtonPressedLeft, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_GetMouseButtonPressedLeft() {
        if (ParryControls::maskNativeLeft) return 0;
        return THISCALL(Ivk_PH_MouseLeft)();
    }

    auto& Ivk_PH_KeyPressed = UnionCore::CreateHook(
        &zCInput_Win32::KeyPressed, &zCInput_Win32::PH_KeyPressed, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_KeyPressed(int key) {
        if ((ParryControls::maskNativeRight && key == MOUSE_BUTTONRIGHT) ||
            (ParryControls::maskNativeLeft && key == MOUSE_BUTTONLEFT)) return 0;
        return THISCALL(Ivk_PH_KeyPressed)(key);
    }

    auto& Ivk_PH_KeyToggled = UnionCore::CreateHook(
        &zCInput_Win32::KeyToggled, &zCInput_Win32::PH_KeyToggled, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_KeyToggled(int key) {
        if ((ParryControls::maskNativeRight && key == MOUSE_BUTTONRIGHT) ||
            (ParryControls::maskNativeLeft && key == MOUSE_BUTTONLEFT)) return 0;
        return THISCALL(Ivk_PH_KeyToggled)(key);
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
        ParryControls::CaptureInput(this);
        if (!ParryControls::ContextAvailable()) return;
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
    constexpr int kLeft = (8192 - (ParryHotter::kSpellCount * kSize +
                                  (ParryHotter::kSpellCount - 1) * kGap)) / 2;
    constexpr int kTargetSize = 720;
    int SlotHeight() { return screen->any(screen->nax(kSize)); }
    int HudTop() { return 8192 - SlotHeight() - 180; }

    // These are the existing spells used by mod/Magic/ParryHotter.d.
    constexpr std::array<int, ParryHotter::kSpellCount> kGothicSpellIds{{
        SPL_WINDFIST, SPL_ICECUBE, SPL_BERZERK,
        SPL_THUNDERBALL, SPL_STORMFIST, SPL_BREATHOFDEATH, -1, -1, -1, -1
    }};
    constexpr std::array<int, ParryHotter::kSpellCount> kHotkeys{{
        KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0
    }};
    constexpr std::array<const char*, ParryHotter::kSpellCount> kTargetTextures{{
        "PH_TARGET_FLIPENDO.TGA",
        "PH_TARGET_LEVICORPUS.TGA",
        "PH_TARGET_RICTUSEMPRA.TGA",
        "PH_TARGET_EXPELLIARMUS.TGA",
        "PH_TARGET_BUMBARDO.TGA",
        "PH_TARGET_AVADA.TGA", nullptr, nullptr, nullptr, nullptr
    }};

    ParryHotter::Controller gController;
    ParryHotter::AimCamera gCamera;
    // Development diagnostics: Gothic/System/ParryHotter_cast.log.
    ParryHotter::CastLog gLog;
    ParryHotter::SpellCooldown gCooldown;
    ParryHotter::CastLedger gCasts;
    struct CastReferences {
        ParryHotter::CastLedger::Id id = 0;
        std::array<oCVisualFX*, 4> effects{};
        oCNpc* target = nullptr;
    };
    std::array<CastReferences, ParryHotter::CastLedger::Capacity> gCastReferences{};
    oCSpell* gRequestedSpell = nullptr;
    // Set by a successful release-cast; the wand lowers once it resolves.
    bool gLowerAfterCast = false;
    int gRequestedSlot = -1;
    oCNpc* gRequestedTarget = nullptr;
    std::uint64_t gRequestGeneration = 0;
    bool gCastConfirmed = false;
    ParryHotter::CastLedger::TimePoint gRequestExpires{};
    ParryHotter::CastLedger::Id gHitCast = 0;
    int gHitSlot = -1;
    struct DamageFrame { ParryHotter::CastLedger::Id id; int slot; };
    std::array<DamageFrame, 32> gDamageFrames{};
    unsigned int gDamageDepth = 0;

    void WriteScriptSlot(int slot) {
        if (!parser) return;
        auto* symbol = parser->GetSymbol(zSTRING("PH_SELECTED_SPELL"));
        if (symbol) symbol->SetValue(slot >= 0 ? slot + 1 : 0, 0);
    }

    void ReleaseCastRequest() {
        auto* spell = gRequestedSpell;
        auto* target = gRequestedTarget;
        gRequestedSpell = nullptr;
        gRequestedTarget = nullptr;
        gRequestedSlot = -1;
        ++gRequestGeneration;
        if (spell) spell->Release();
        if (target) target->Release();
    }

    void ReapCasts(bool clear = false) {
        const auto now = ParryHotter::CastLedger::Clock::now();
        if (clear) gCasts.Clear(); else gCasts.Expire(now);
        for (auto& entry : gCastReferences) {
            if (!entry.id || gCasts.FindById(entry.id, now).id) continue;
            const auto expired = entry;
            entry = {}; // Destructors may call back into damage handling.
            for (auto* effect : expired.effects) if (effect) effect->Release();
            if (expired.target) expired.target->Release();
        }
    }

    ParryHotter::CastLedger::Id TrackEffect(
        oCSpell* spell, oCNpc* target, int slot, std::uint64_t generation,
        ParryHotter::CastLedger::Id existing = 0
    ) {
        if (!spell || !target || generation != gRequestGeneration) return existing;
        ReapCasts();
        // Releasing an expired native FX can invoke scripts/lifecycle hooks.
        if (generation != gRequestGeneration || !spell->effect) return existing;
        auto* effect = spell->effect;
        const auto now = ParryHotter::CastLedger::Clock::now();
        for (auto& entry : gCastReferences) {
            if ((existing && entry.id != existing) || (!existing && entry.id)) continue;
            auto id = existing;
            if (!id) {
                id = gCasts.Stage(
                    reinterpret_cast<ParryHotter::CastLedger::Token>(effect),
                    reinterpret_cast<ParryHotter::CastLedger::Token>(target),
                    spell->GetSpellID(), slot, now);
                if (!id) return 0;
                target->AddRef();
                entry.id = id;
                entry.target = target;
            }
            for (auto* retained : entry.effects) if (retained == effect) return id;
            for (auto& retained : entry.effects) {
                if (retained) continue;
                if (!gCasts.AddEffectAlias(id,
                        reinterpret_cast<ParryHotter::CastLedger::Token>(effect), now)) return id;
                effect->AddRef();
                retained = effect;
                return id;
            }
            return id;
        }
        return existing;
    }

    int CastTracked(oCSpell* spell) {
        const bool ours = spell && spell == gRequestedSpell && spell->spellCasterNpc == player &&
            gRequestedTarget && gRequestedSlot >= 0 &&
            gRequestedSlot < ParryHotter::kSpellCount &&
            spell->GetSpellID() == kGothicSpellIds[gRequestedSlot];
        const auto generation = gRequestGeneration;
        const int slot = gRequestedSlot;
        auto* target = ours ? gRequestedTarget : nullptr;
        // Native Cast runs scripts and damage hooks. Those callbacks can cancel
        // or replace the request; retain the objects and identify this request.
        if (ours) { spell->AddRef(); target->AddRef(); }
        const auto sameRequest = [&] {
            return generation == gRequestGeneration && spell == gRequestedSpell;
        };
        auto id = ours ? TrackEffect(spell, target, slot, generation) : 0;
        const int result = (spell->*Gothic_I_Classic::Ivk_PH_Cast)();
        if (ours) {
            if (result != 0) {
                if (sameRequest()) id = TrackEffect(spell, target, slot, generation, id);
                // A completed projectile remains identifiable after aiming
                // was cancelled. Clear() never reuses pre-transition IDs.
                if (id) gCasts.Confirm(id, ParryHotter::CastLedger::Clock::now());
            } else if (id) {
                gCasts.Revoke(id);
                ReapCasts();
            }
            gLog.Line("engine Cast slot=%d result=%d sameRequest=%d", slot, result, sameRequest() ? 1 : 0);
            if (sameRequest()) {
                gCastConfirmed = result != 0;
                NotifyCastResult(slot, result != 0);
                ReleaseCastRequest();
            }
        }
        if (ours) { target->Release(); spell->Release(); }
        return result;
    }

    ParryHotter::CastLedger::Match IdentifyHit(
        oCNpc* victim, const oCNpc::oSDamageDescriptor& descriptor
    ) {
        if (!victim || descriptor.pNpcAttacker != player ||
            !(descriptor.dwFieldsValid & oCNpc::oEDamageDescFlag_SpellID) ||
            !(descriptor.dwFieldsValid & oCNpc::oEDamageDescFlag_VisualFX)) return {};
        const auto now = ParryHotter::CastLedger::Clock::now();
        const auto victimToken = reinterpret_cast<ParryHotter::CastLedger::Token>(victim);
        ParryHotter::CastLedger::Match identified{};
        for (auto* original : {descriptor.pFXHit, descriptor.pVisualFX}) {
            auto* effect = original;
            for (int depth = 0; effect && depth < 8; ++depth) {
                const auto match = gCasts.Resolve(
                    reinterpret_cast<ParryHotter::CastLedger::Token>(effect),
                    victimToken, static_cast<int>(descriptor.nSpellID), now);
                if (match.state == ParryHotter::CastLedger::MatchState::Ambiguous)
                    return match;
                if (match.state == ParryHotter::CastLedger::MatchState::Matched) {
                    if (identified.id && identified.id != match.id)
                        return {ParryHotter::CastLedger::MatchState::Ambiguous, 0, -1, false};
                    identified = match;
                }
                if (effect->root && effect->root != effect) effect = effect->root;
                else effect = effect->parent;
            }
        }
        return identified;
    }

    void BeginDamage(oCNpc* victim, const oCNpc::oSDamageDescriptor& descriptor) {
        if (gDamageDepth < gDamageFrames.size())
            gDamageFrames[gDamageDepth] = {gHitCast, gHitSlot};
        ++gDamageDepth;
        const auto match = IdentifyHit(victim, descriptor);
        const bool exact = gDamageDepth <= gDamageFrames.size() &&
            match.state == ParryHotter::CastLedger::MatchState::Matched;
        gHitCast = exact ? match.id : 0;
        gHitSlot = exact ? match.slot : -1;
        WriteScriptSlot(gHitSlot);
    }

    void EndDamage() {
        if (gDamageDepth) --gDamageDepth;
        if (gDamageDepth < gDamageFrames.size()) {
            const auto frame = gDamageFrames[gDamageDepth];
            gHitCast = frame.id;
            gHitSlot = frame.slot;
        } else { gHitCast = 0; gHitSlot = -1; }
        WriteScriptSlot(gHitSlot);
    }
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
        // Print() takes coordinates of the panel itself (0..8191 over its own
        // height). Fixed offsets made the lines overlap in-game, so size the
        // panel for four font lines and step by the panel's font height.
        constexpr int kDiagnosticLines = 4;
        const int screenLine = std::max(screen->FontY(), 96);
        gDiagnosticPanel = new zCView(220, 420, 5850,
                                      420 + screenLine * kDiagnosticLines + screenLine / 2);
        screen->InsertItem(gDiagnosticPanel);
        const int line = std::max(gDiagnosticPanel->FontY(), 1);
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
        gDiagnosticPanel->Print(30, 0, zSTRING(title));
        gDiagnosticPanel->Print(30, line, zSTRING(stats));
        gDiagnosticPanel->Print(30, line * 2, zSTRING(causes));
        char targetLine[240]{};
        std::snprintf(targetLine, sizeof(targetLine), "Cel: %s",
                      ParryHotter::AimTargetDescription(gAimTargetState));
        gDiagnosticPanel->Print(30, line * 3, zSTRING(targetLine));
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
            !gHitCast ||
            victim->IsDead() || !victim->IsHuman()) return;
        if (descriptor.pNpcAttacker != player ||
            !(descriptor.dwFieldsValid & oCNpc::oEDamageDescFlag_SpellID))
            return;
        const auto now = ParryHotter::SpellImpactTag::Clock::now();
        const int active = gHitSlot + 1;
        const bool levi = active == 2 && descriptor.nSpellID == SPL_ICECUBE;
        const bool bomb = active == 5 && descriptor.nSpellID == SPL_STORMFIST;
        if (!levi && !bomb) return;
        if (!gCasts.TryMarkImpact(gHitCast, now)) return;
        // One diagnostic outcome per real spell hit, even if damage-over-time
        // reports the same impact again. Never spam the HUD with duplicates.
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

    std::array<zCView*, ParryHotter::kSpellCount> gSlots{};
    std::array<zCView*, ParryHotter::kSpellCount> gTargetIcons{};
    zCView* gSelection = nullptr;
    zCView* gReticle = nullptr;
    bool gAiming = false;
    bool gManaPrepared = false;
    bool OwnsMagicInput() { return gAiming && ParryControls::ContextAvailable(); }
    void NotifyCastResult(int slot, bool success) {
        if (success) {
            gCooldown.MarkPreparedCast(slot, ParryHotter::SpellCooldown::Clock::now());
            gManaPrepared = false;
        }
    }
    int gShownTargetSlot = -1;
    int gActiveSlot = 0;
    bool gAttached = false;
    std::array<int, ParryHotter::kSpellCount> gSlotDimmed{};
    // Opacity of an unavailable slot (no mana, cooldown, empty 7-0).
    constexpr int kDimmedSlotAlpha = 110;
    zCModel* gWandModel = nullptr;
    zCModelNodeInst* gWandSocket = nullptr;
    zCVisual* gWandVisual = nullptr;
    zCModel* gHarryModel = nullptr;
    oCItem* gHarryArmor = nullptr;
    void DetachWand();

    void KeepHarryAppearance() {
        if (!player || !parser || !player->IsHuman() ||
            !parser->GetSymbol(zSTRING("PH_ARMOR_HARRY"))) {
            gHarryModel = nullptr;
            gHarryArmor = nullptr;
            return;
        }
        auto* model = player->GetModel();
        auto* armor = player->GetEquippedArmor();
        if (!model || (model == gHarryModel && armor == gHarryArmor)) return;
        gHarryModel = model;
        gHarryArmor = armor;
        // Probe the real mesh first. A missing optional asset must never
        // erase the playable Gothic body. Equipment retains its normal stats.
        if (!model->ApplyMeshLib(zSTRING("PH_HARRY.ASC"))) return;
        DetachWand();
        model->RemoveMeshLibAll();
        if (!model->ApplyMeshLib(zSTRING("PH_HARRY.ASC"))) {
            // Rebuild the NPC's ordinary equipped model if the native mesh
            // loader fails after the probe; never leave an invisible player.
            player->InitModel();
            gHarryModel = player->GetModel();
            return;
        }
        auto* oldHead = model->SearchNode(zSTRING("BIP01 HEAD"));
        if (oldHead) model->SetNodeVisual(oldHead, nullptr, 0);
    }
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
        // Alpha-blend explicitly so zCView::alpha (opacity) always applies.
        const zTRnd_AlphaBlendFunc blend = zRND_ALPHA_FUNC_BLEND;
        view->SetAlphaBlendFunc(blend);
        view->SetTransparency(255);
        return view;
    }

    void BuildHud() {
        if (!screen || gReticle) return;
        for (int i = 0; i < ParryHotter::kSpellCount; ++i) {
            const int x = kLeft + i * (kSize + kGap);
            gSlots[i] = NewSprite(
                x, HudTop(), kSize, SlotHeight(), ParryHotter::kTextures[i]
            );
            if (kTargetTextures[i]) gTargetIcons[i] = NewSprite(
                4096 + kSpace, 4096 - kTargetSize / 2,
                kTargetSize, kTargetSize, kTargetTextures[i]
            );
            // Text is created once, rather than leaking text every frame.
            gSlots[i]->Print(35, 30, zSTRING(ParryHotter::kHotkeyLabels[i]));
        }
        gSelection = NewSprite(kLeft, HudTop(), kSize, SlotHeight(), "PH_SELECTED.TGA");
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
        gHarryModel = nullptr;
        gHarryArmor = nullptr;
        gSlotDimmed.fill(-1);
        ParryCasting::Forget();
        gController.ResetAfterInterruption();
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
        for (int i = 0; i < ParryHotter::kSpellCount; ++i) {
            const bool preparing = gAiming && gManaPrepared &&
                                    gController.Selected() == i;
            const int dim = (preparing || gCooldown.Ready(i, mana, now)) ? 0 : 1;
            if (gSlots[i] && gSlotDimmed[i] != dim) {
                // Visual availability, never removes an icon from HUD.
                // SetTransparency takes opacity (zCView::alpha): 255 = solid.
                gSlots[i]->SetTransparency(dim ? kDimmedSlotAlpha : 255);
                gSlotDimmed[i] = dim;
            }
        }
    }

    void ShowSelection(int spell) {
        if (!gSelection || !screen || spell == gActiveSlot) return;
        screen->RemoveItem(gSelection);
        gSelection->SetPos(kLeft + spell * (kSize + kGap), HudTop());
        screen->InsertItem(gSelection);
        gActiveSlot = spell;
    }

    void ShowTarget(bool hasTarget, int spell, oCNpc* npc = nullptr) {
        if (!screen || !gReticle) return;
        ParryHotter::TargetGlyphLayout glyph{};
        auto* camera = zCCamera::activeCam;
        if (hasTarget && npc && camera && gAiming && gManaPrepared &&
            spell >= 0 && spell < ParryHotter::kSpellCount &&
            kTargetTextures[spell] && gCooldown.ReadyByTime(
                spell, ParryHotter::SpellCooldown::Clock::now())) {
            auto* book = player ? player->GetSpellBook() : nullptr;
            auto* prepared = book ? book->GetSelectedSpell() : nullptr;
            if (!prepared || prepared->GetSpellID() != kGothicSpellIds[spell] ||
                !prepared->GetEnabled() || !prepared->IsValidTarget(npc) ||
                prepared->GetSpellStatus() == SPL_STATUS_STOP ||
                prepared->GetSpellStatus() == SPL_STATUS_DONTINVEST) {
                hasTarget = false;
            }
        }
        if (hasTarget && npc && camera && gAiming && gManaPrepared &&
            spell >= 0 && spell < ParryHotter::kSpellCount && kTargetTextures[spell] &&
            gCooldown.ReadyByTime(spell, ParryHotter::SpellCooldown::Clock::now())) {
            // Anchor the glyph at the NPC's upper torso, not at the crosshair.
            zVEC3 anchor = npc->bbox3D.mins +
                (npc->bbox3D.maxs - npc->bbox3D.mins) * zVEC3(0.5f);
            anchor[1] += (npc->bbox3D.maxs[1] - npc->bbox3D.mins[1]) * 0.18f;
            const zVEC3 cameraSpace = camera->camMatrix * anchor;
            if (cameraSpace[2] > camera->nearClipZ) {
                float x = 0.0f, y = 0.0f;
                camera->Project(&cameraSpace, x, y);
                ParryHotter::TargetGlyphInput input{};
                input.aiming = input.targetValid = input.spellReady = true;
                input.inFront = input.lineOfSight = true; // center ray was unobstructed
                input.anchorX = x;
                input.anchorY = y;
                input.viewportWidth = float(screen->psizex);
                input.viewportHeight = float(screen->psizey);
                input.baseSize = float(screen->nax(kTargetSize));
                input.seconds = std::chrono::duration<double>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                glyph = ParryHotter::ComputeTargetGlyph(input);
            }
        }
        const int targetIndex = glyph.visible ? spell : -1;
        if (gShownTargetSlot >= 0 && targetIndex != gShownTargetSlot) {
            screen->RemoveItem(gTargetIcons[gShownTargetSlot]);
        }
        if (targetIndex >= 0) {
            auto* icon = gTargetIcons[targetIndex];
            icon->SetPos(screen->anx(glyph.x), screen->any(glyph.y));
            icon->SetSize(screen->anx(glyph.width), screen->any(glyph.height));
            icon->SetTransparency(glyph.alpha);
            if (targetIndex != gShownTargetSlot) screen->InsertItem(icon);
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

    // Use the native spellbook so that mana, target rules and Gothic visual FX
    // still apply. Test the call ordering on a LOCAL Gothic I installation;
    // engine variations (Steam + Union) may require an event hook.
    void RaiseWand(int spellSlot) {
        if (!ParryControls::ContextAvailable() ||
            spellSlot < 0 || spellSlot >= ParryHotter::kSpellCount ||
            kGothicSpellIds[spellSlot] < 0) {
            gLog.Line("raise slot=%d REFUSED context=%d", spellSlot,
                      ParryControls::ContextAvailable() ? 1 : 0);
            return;
        }
        ReleaseCastRequest();
        const int mana = player->GetAttribute(NPC_ATR_MANA);
        const auto now = ParryHotter::SpellCooldown::Clock::now();
        if (gAiming) {
            auto* actor = player;
            const auto generation = gRequestGeneration;
            gAiming = false;
            gManaPrepared = false;
            DetachWand();
            ParryCasting::Restore(player->human_ai, player->GetModel());
            if (generation != gRequestGeneration || player != actor || gAiming)
                return;
            player->UnreadySpell();
        }
        // Check mana BEFORE Gothic invests it. Do not re-check during casting.
        if (!gCooldown.Ready(spellSlot, mana, now)) {
            gLog.Line("raise slot=%d REFUSED cooldown/mana mana=%d", spellSlot, mana);
            return;
        }
        const int id = kGothicSpellIds[spellSlot];
        // ReadySpell returns an undocumented integer in Gothic 1. Never
        // assume 0 means failure; native Spell_Cast performs the final check.
        player->ReadySpell(id, 0);
        auto* book = player->GetSpellBook();
        auto* prepared = book ? book->GetSelectedSpell() : nullptr;
        if (!prepared || prepared->GetSpellID() != id || !prepared->GetEnabled()) {
            gLog.Line("raise slot=%d REFUSED ReadySpell(%d) book=%d prepared=%d preparedId=%d enabled=%d",
                      spellSlot, id, book ? 1 : 0, prepared ? 1 : 0,
                      prepared ? prepared->GetSpellID() : -1,
                      prepared ? prepared->GetEnabled() : -1);
            player->UnreadySpell();
            return;
        }
        // Native invest animations work on the Gothic skeleton used by Harry.
        if (player->human_ai) {
            // Optional player overlay supplies the same native transition
            // handles for all six spells, including one-phase Gothic spells.
            // Incomplete/unloaded animation assets keep the native defaults.
            ParryCasting::Bind(player->human_ai, player->GetModel());
            player->human_ai->TransitionToInvest();
        }
        gManaPrepared = true;
        gAiming = true;
        AttachWand();
        gLog.Line("raise slot=%d OK spellId=%d mana=%d wandAttached=%d", spellSlot, id,
                  mana, gWandVisual ? 1 : 0);
    }

    void LowerWand() {
        if (gAiming || gRequestedSpell) gLog.Line("lower wand aiming=%d request=%d",
                                                  gAiming ? 1 : 0, gRequestedSpell ? 1 : 0);
        gLowerAfterCast = false;
        const bool wasAiming = gAiming;
        auto* actor = player;
        gAiming = false;
        gManaPrepared = false;
        ReleaseCastRequest();
        if (player != actor || gAiming || gRequestedSpell) return;
        const auto generation = gRequestGeneration;
        DetachWand();
        if (player) ParryCasting::Restore(player->human_ai, player->GetModel());
        else ParryCasting::Forget();
        // A cancellation callback may have loaded another world or started a
        // new request. Do not unready that replacement actor/request on return.
        if (generation != gRequestGeneration || player != actor || gAiming ||
            gRequestedSpell) return;
        if (player && wasAiming) player->UnreadySpell();
    }

    void SetUnionPresence(bool enabled) {
        if (!parser) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_UNION_LOADED"));
        if (symbol) symbol->SetValue(enabled ? 1 : 0, 0);
    }

    void ResetSelectedSpellForScript() {
        ReleaseCastRequest();
        ClearFlight();
        ReapCasts(true);
        gHitCast = 0;
        gHitSlot = -1;
        gDamageDepth = 0;
        if (!parser) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_SELECTED_SPELL"));
        if (symbol) symbol->SetValue(0, 0);
    }

    bool CastAtCenteredNpc(int slot) {
        if (!ParryControls::ContextAvailable() || gRequestedSpell ||
            slot < 0 || slot >= ParryHotter::kSpellCount ||
            !ParryHotter::kSpellAvailable[slot]) {
            gLog.Line("cast slot=%d REFUSED context=%d pendingRequest=%d", slot,
                      ParryControls::ContextAvailable() ? 1 : 0, gRequestedSpell ? 1 : 0);
            return false;
        }
        oCNpc* target = GetCenteredNpcTarget();
        if (!target) {
            gLog.Line("cast slot=%d REFUSED no target (%s)", slot,
                      ParryHotter::AimTargetDescription(gAimTargetState));
            return false;
        }
        oCMag_Book* spellbook = player->GetSpellBook();
        if (!spellbook) {
            SetAimTargetState(ParryHotter::AimTargetState::NoSpellBook);
            gLog.Line("cast slot=%d REFUSED no spellbook", slot);
            return false;
        }
        auto* spell = spellbook->GetSelectedSpell();
        if (!spell || spell->GetSpellID() != kGothicSpellIds[slot] ||
            !spell->GetEnabled() || !spell->IsValidTarget(target)) {
            gLog.Line("cast slot=%d REFUSED spell=%d id=%d want=%d enabled=%d validTarget=%d",
                      slot, spell ? 1 : 0, spell ? spell->GetSpellID() : -1,
                      kGothicSpellIds[slot], spell ? spell->GetEnabled() : -1,
                      spell ? spell->IsValidTarget(target) : -1);
            return false;
        }
        const int status = spell->GetSpellStatus();
        if (status == SPL_STATUS_STOP || status == SPL_STATUS_DONTINVEST) {
            gLog.Line("cast slot=%d REFUSED spellStatus=%d", slot, status);
            return false;
        }
        // Bind the game engine's target to the actual center-screen NPC.
        // Gothic's own focus detection may otherwise select a neighbor.
        player->SetFocusVob(static_cast<zCVob*>(target));
        // Selecting a focus alone does not rebind an already prepared spell.
        spell->spellTarget = target;
        spell->spellTargetNpc = target;
        if (spell->effect) spell->effect->SetTarget(static_cast<zCVob*>(target));
        gRequestedSpell = spell;
        gRequestedSlot = slot;
        gRequestedTarget = target;
        spell->AddRef();
        target->AddRef();
        gRequestExpires = ParryHotter::CastLedger::Clock::now() +
            std::chrono::seconds(2);
        gCastConfirmed = false;
        if (player->human_ai) {
            ParryCasting::Bind(player->human_ai, player->GetModel());
            player->human_ai->TransitionToCast();
        }
        gLog.Line("cast slot=%d REQUEST target=%p status=%d", slot,
                  static_cast<void*>(target), status);
        spellbook->Spell_Cast();
        gLog.Line("cast slot=%d after Spell_Cast confirmed=%d pending=%d", slot,
                  gCastConfirmed ? 1 : 0, gRequestedSpell ? 1 : 0);
        // The native animation may call oCSpell::Cast on a later frame.
        // Its hook is the sole place that starts the cooldown on success.
        return gCastConfirmed || gRequestedSpell != nullptr;
    }

    void GameTick() {
        const bool gameplay =
            ogame && screen && player && zinput &&
            !ogame->inLoadSaveGame && !ogame->inLevelChange &&
            !player->openinventory &&
            !ogame->IsOnPause() && !player->IsDead() &&
            oCInformationManager::GetInformationManager().HasFinished() &&
            (!zCAICamera::current || !zCAICamera::current->inCutsceneMode);
        if (!gameplay) {
            // Opening inventory, changing levels or entering a cutscene
            // cancels unfinished air tracking. Never animate a paused NPC.
            if (gFlightRecovery.Active())
                NoteImpact(ParryHotter::ImpactEvent::Interrupted);
            if (gFlightRecovery.Active() || gCasts.Size(ParryHotter::CastLedger::Clock::now()))
                ResetSelectedSpellForScript();
            HideDiagnostics();
            ParryControls::Disable();
            gCamera.Restore(zCAICamera::current);
            if (gAiming || gRequestedSpell) LowerWand();
            gController.Step({});
            ShowTarget(false, gActiveSlot);
            return;
        }
        if (zinput->KeyToggled(KEY_F10)) {
            gDiagnosticVisible = !gDiagnosticVisible;
            gDiagnosticDirty = true;
        }
        const bool inputAllowed = ParryControls::ContextAvailable();
        int hotkey = inputAllowed && ParryControls::OwnsInput()
            ? ParryControls::GetInput().slotPressed : 0;
        for (int i = 0; inputAllowed && !hotkey && i < ParryHotter::kSpellCount; ++i) {
            if (zinput->KeyToggled(kHotkeys[i])) {
                hotkey = i + 1;
                break;
            }
        }
        const bool right = zinput->GetMouseButtonPressedRight() != 0;
        const bool left = zinput->GetMouseButtonPressedLeft() != 0;
        oCNpc* targetNpc = inputAllowed ? GetCenteredNpcTarget() : nullptr;
        const bool target = targetNpc != nullptr;
        ParryHotter::Input input{inputAllowed, hotkey, right, left, target};
        auto out = gController.Step(input);
        {
            // Log every change of the casting chain's observable state.
            struct Snapshot {
                int left, right, aiming, wand, mana, target, slot, body, castAni, request;
                bool operator!=(const Snapshot& o) const {
                    return std::memcmp(this, &o, sizeof(*this)) != 0;
                }
            };
            static Snapshot last{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
            const Snapshot now{left, right, out.aiming, gAiming, gManaPrepared,
                               target, out.selected, player->GetBodyState(),
                               player->human_ai && player->human_ai->IsInCastAni() ? 1 : 0,
                               gRequestedSpell ? 1 : 0};
            if (now != last || out.cast || out.startAim || out.stopAim) {
                gLog.Line("frame LMB=%d RMB=%d aim=%d wand=%d manaPrep=%d target=%d(%s) "
                          "slot=%d body=%d castAni=%d request=%d start=%d stop=%d cast=%d mana=%d",
                          now.left, now.right, now.aiming, now.wand, now.mana, now.target,
                          ParryHotter::AimTargetDescription(gAimTargetState), now.slot,
                          now.body, now.castAni, now.request, out.startAim, out.stopAim,
                          out.cast, player->GetAttribute(NPC_ATR_MANA));
                last = now;
            }
        }

        // Falling unconscious, swimming or entering a scripted action cancels
        // preparation. Already fired projectiles retain their hit records.
        if (!inputAllowed && (gAiming || gRequestedSpell)) LowerWand();

        const auto now = ParryHotter::SpellCooldown::Clock::now();
        UpdateFlight(now);
        ReapCasts();
        if (gRequestedSpell && now >= gRequestExpires) LowerWand();
        KeepHarryAppearance();
        const int mana = player->GetAttribute(NPC_ATR_MANA);
        ShowHud();
        ShowSlotAvailability(mana, now);
        ShowSelection(out.selected);
        ShowTarget(out.targetSymbolVisible, out.selected, targetNpc);
        DrawDiagnostics();
        // HP2 keeps the standard camera framing while aiming (no zoom or
        // shoulder shift). Gothic's CAMMODNORMAL (2.5 m) already matches
        // HP2's distance, so the framing helper only restores old values.
        gCamera.Restore(zCAICamera::current);

        // HP2: the release of LMB both ends aiming and casts. Cast first,
        // while the wand is still raised and the mana prepared; lower the
        // wand only after the native cast resolved (confirmed or expired).
        if (out.cast && gAiming && gManaPrepared &&
            gCooldown.ReadyByTime(out.selected, now)) {
            gLowerAfterCast = CastAtCenteredNpc(out.selected);
        }
        if (out.startAim) gLowerAfterCast = false;
        if (out.stopAim && !gLowerAfterCast) {
            LowerWand();
        }
        if (gLowerAfterCast && !gRequestedSpell &&
            !(player->human_ai && player->human_ai->IsInCastAni())) {
            gLowerAfterCast = false;
            if (!out.aiming) LowerWand();
        }
        if (out.startAim || (hotkey && out.aiming)) {
            RaiseWand(out.selected);
        }
        if (out.aiming && gAiming) {
            if (player->human_ai)
                ParryCasting::Bind(player->human_ai, player->GetModel());
            AttachWand(); // Model may have changed with a clothing swap.
        }
        if (out.aiming && !gManaPrepared &&
            gCooldown.Ready(out.selected, mana, now)) {
            // Holding LMB can re-arm after the previous cooldown expires.
            RaiseWand(out.selected);
        }
        if (out.aiming && gAiming && gManaPrepared && !gRequestedSpell && player->GetSpellBook() &&
            gCooldown.ReadyByTime(out.selected, now)) {
            // Gothic invests mana internally. Keep the prepared cast valid
            // even if the visible mana counter reaches zero mid-charge.
            player->GetSpellBook()->Spell_Invest();
        }
    }
} // anonymous namespace

// Standard Union DLL exports, called by engine event hooks in template.
cexport const char* ParryHotter_BuildId() {
    return "PH_BUILD_013_TEN_SLOTS_PROJECTILE_INPUT_CAST_ANIM";
}
cexport void Game_Entry() {}
cexport void Game_Init() {
    ParryControls::Reset();
    ParryCasting::Forget();
    gTelemetry.Reset();
    gDiagnosticDirty = true;
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    gCooldown.Reset();
}
cexport void Game_Exit() {
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
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
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Exit casting mode before the game records its save. Both mouse
    // buttons must be released before re-arming on return.
    gController.Step({});
    if (gAiming) LowerWand();
    gCamera.Restore(zCAICamera::current);
    HideDiagnostics();
    // Never serialize PH_UNION_LOADED=1 into a save used later without the
    // plugin. DEV mode should still recognize its own six rune placeholders.
    ResetSelectedSpellForScript();
    SetUnionPresence(false);
}
cexport void Game_SaveEnd() {
    SetUnionPresence(true);
}
cexport void Game_LoadBegin_NewGame() {
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    ResetSelectedSpellForScript();
    if (gAiming) LowerWand();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_NewGame() {
    ParryControls::Forget();
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    gCooldown.Reset();
}
cexport void Game_LoadBegin_SaveGame() {
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    ResetSelectedSpellForScript();
    if (gAiming) LowerWand();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_SaveGame() {
    ParryControls::Forget();
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    gCooldown.Reset();
}
cexport void Game_LoadBegin_ChangeLevel() {
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    ResetSelectedSpellForScript();
    if (gAiming) LowerWand();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_ChangeLevel() {
    ParryControls::Forget();
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    gCooldown.Reset();
}
cexport void Game_LoadBegin_Trigger() {}
cexport void Game_LoadEnd_Trigger() {}
cexport void Game_Pause() {
    ParryControls::Disable();
    // The game loop may stop during pause and never call GameTick's
    // !gameplay branch. Cancel pending input here, too.
    gController.Step({});
    if (gAiming) LowerWand();
    ResetSelectedSpellForScript();
    HideDiagnostics();
    gCamera.Restore(zCAICamera::current);
}
cexport void Game_Unpause() {}
cexport void Game_DefineExternals() {}
cexport void Game_ApplyOptions() {
    if (gAiming) LowerWand();
    RemoveHud(); // Recalculate pixel-square HUD geometry after a resize.
}
#endif
