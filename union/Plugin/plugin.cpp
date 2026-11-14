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
#include <cstdio>
// A member pointer to a virtual SDK method is an MSVC vcall thunk, not the
// engine function: hooking it caught no engine call (in-game counters stayed
// 0). Virtual methods are therefore hooked at their Gothic I 1.08k address.
#define PH_VIRTUAL(address) reinterpret_cast<void*>(address)
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
#include "AimAssist.hpp"
#include "ControlsTelemetry.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <array>
#include <cstdint>

#ifdef __G1
#include "PlayerControls.hpp"
#include "CastingAnimations.hpp"
#include "SpellTargeting.hpp"
#include "NativeWand.hpp"
#include "Hp2Camera.hpp"
#include "MovingCasting.hpp"
using namespace Gothic_I_Classic;

// Custom damage hook has access to the real victim, attacker and spell ID.
// Unlike moving a focused target on click, this only acts on a genuine hit.
namespace {
    void ApplyParryImpact(
        oCNpc* victim, const oCNpc::oSDamageDescriptor& descriptor
    );
    int CastTracked(oCSpell* spell);
    int InvestTracked(oCSpell* spell);
    bool DeferChargedSpell(oCSpell* spell);
    void LogJumpInput(oCAIHuman* ai);
    void PlaceHp2Camera(zCAICamera* camera);
    void BeginDamage(oCNpc*, const oCNpc::oSDamageDescriptor&);
    void EndDamage();
    void NotifyCastResult(int slot, bool success);
    bool OwnsMagicInput();
    bool CastRequestPending();
    int gOwnMagicCall = 0;
    struct OwnMagicCall {
        OwnMagicCall() { ++gOwnMagicCall; }
        ~OwnMagicCall() { --gOwnMagicCall; }
    };
    bool DriveNativeMagic();
    void CaptureReleaseTarget(std::uint64_t sequence);
    oCNpc* GetCenteredNpcTarget();
    void ClearReleaseTargets();
    const char* TargetFailure(oCNpc* target, bool lineOfSight = true);
    void LowerWand();
    void UpdateSpellPreparation();
    void LogNativeMagic(oCAIHuman* ai, bool after);
    void ObserveNativeRelease();
    void LogPlayerMagicAI(oCAIHuman* ai, bool after, bool driving);
    unsigned gPlayerMagicCalls = 0;
}

namespace Gothic_I_Classic {
    auto& Ivk_PH_OnDamage = UnionCore::CreateHook(
        PH_VIRTUAL(0x00730FE0) /* oCNpc::OnDamage(oSDamageDescriptor&) */,
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
        PH_VIRTUAL(0x0047CFB0) /* oCSpell::Cast */,
        &oCSpell::PH_Cast, UnionCore::Hook_Auto);
    int oCSpell::PH_Cast() { return CastTracked(this); }

    auto& Ivk_PH_Invest = UnionCore::CreateHook(
        PH_VIRTUAL(0x0047CD40) /* oCSpell::Invest */,
        &oCSpell::PH_Invest, UnionCore::Hook_Auto);
    int oCSpell::PH_Invest() { return InvestTracked(this); }

    auto& Ivk_PH_SpellStatus = UnionCore::CreateHook(
        PH_VIRTUAL(0x0047E390) /* oCSpell::GetSpellStatus */,
        &oCSpell::PH_GetSpellStatus, UnionCore::Hook_Auto);
    int oCSpell::PH_GetSpellStatus() {
        if (DeferChargedSpell(this)) return SPL_STATUS_CANINVEST;
        return THISCALL(Ivk_PH_SpellStatus)();
    }

    // AdvanceAnis contains the main node-blending loop inline. Hooking only
    // CalcNodeListAniBlending would miss normal model updates in G1 1.08k.
    auto& Ivk_PH_ModelAdvance = UnionCore::CreateHook(
        PH_VIRTUAL(0x00562CD0) /* zCModel::AdvanceAnis */,
        &zCModel::PH_AdvanceAnis, UnionCore::Hook_Auto);
    void zCModel::PH_AdvanceAnis() {
        ParryMovingCasting::BeforeBlend(this);
        THISCALL(Ivk_PH_ModelAdvance)();
        ParryMovingCasting::AfterBlend(this);
    }

    auto& Ivk_PH_HumanAI = UnionCore::CreateHook(
        PH_VIRTUAL(0x00615A50) /* oCAIHuman::DoAI */, &oCAIHuman::PH_DoAI, UnionCore::Hook_Auto);
    void oCAIHuman::PH_DoAI(zCVob* vob, int& state) {
        // Hide owned mouse buttons from the player's native AI only
        // (RMB + W jumped in-game; LMB would attack).
        if (this->npc && this->npc == player)
            ParryControls::CaptureIfMissed(static_cast<zCInput_Win32*>(zinput));
        const bool own = this->npc && this->npc == player &&
            ParryControls::ContextAvailable();
        struct NativeMouseScope {
            bool left = ParryControls::maskNativeLeft;
            bool right = ParryControls::maskNativeRight;
            NativeMouseScope(bool owns) {
                ParryControls::maskNativeLeft = owns && ParryControls::mouseGuard.StripsLeft();
                ParryControls::maskNativeRight = owns && ParryControls::mouseGuard.StripsRight();
            }
            ~NativeMouseScope() {
                ParryControls::maskNativeLeft = left;
                ParryControls::maskNativeRight = right;
            }
        } mouseScope(own);
        const bool driving = own && DriveNativeMagic();
        LogPlayerMagicAI(this, false, driving);
        ParryControls::BeforeHumanAI(this);
        // F11 restores native movement, but native magic still cannot invest
        // while walking. Give it a stationary animation during this request.
        if (driving && !ParryControls::OwnsInput() && !IsInCastAni() && IsWalking())
            _Stand();
        {
            ParryControls::NativePlayerInputScope movementInput(
                own && (ParryControls::OwnsInput() || driving));
            // A nested NPC AI must not inherit the player's synthetic spell keys.
            ParryControls::NativeMagicInputScope outsideMagic(false);
            if (driving) {
                OwnMagicCall nativeMagic;
                THISCALL(Ivk_PH_HumanAI)(vob, state);
            } else {
                THISCALL(Ivk_PH_HumanAI)(vob, state);
            }
        }
        LogPlayerMagicAI(this, true, driving);
        ParryControls::AfterHumanAI(this);
        if (this->npc && this->npc == player) LogJumpInput(this);
    }
    auto& Ivk_PH_WeaponChoose = UnionCore::CreateHook(
        &oCAIHuman::WeaponChoose, &oCAIHuman::PH_WeaponChoose, UnionCore::Hook_Auto);
    void oCAIHuman::PH_WeaponChoose() {
        // Number keys belong to spells even when F11 restores native movement.
        if (npc == player && ParryControls::ContextAvailable()) return;
        THISCALL(Ivk_PH_WeaponChoose)();
    }
    auto& Ivk_PH_MagicMode = UnionCore::CreateHook(
        PH_VIRTUAL(0x0046D260) /* oCAIHuman::MagicMode */,
        &oCAIHuman::PH_MagicMode, UnionCore::Hook_Auto);
    int oCAIHuman::PH_MagicMode() {
        // Only MagicMode sees synthetic ACTION+UP. The rest of native player
        // DoAI sees zero movement, so it cannot replace the cast with a run.
        const bool own = npc == player && ParryControls::ContextAvailable();
        if (npc == player) ++gPlayerMagicCalls;
        ParryControls::NativePlayerInputScope movementInput(
            own && ParryControls::nativePlayerInput);
        ParryControls::NativeMagicInputScope magicInput(own);
        // G1 leaves bit 8 latched after a successful Cast. Its own release
        // branch clears it only for ACTION=1, UP=0 (0x46D363/0x46D3A1).
        // Supply that native transition before investing the next request.
        const bool rearm = own && OwnsMagicInput() && spellCastedLastFrame;
        ParryControls::NativeMagicRearmScope rearmInput(rearm);
        auto* book = own && OwnsMagicInput() ? player->GetSpellBook() : nullptr;
        ParryTargeting::Scope targeting(book ? book->GetSelectedSpell() : nullptr);
        LogNativeMagic(this, false);
        if (own) ObserveNativeRelease();
        const int result = THISCALL(Ivk_PH_MagicMode)();
        LogNativeMagic(this, true);
        return result;
    }
    auto& Ivk_PH_CameraAI = UnionCore::CreateHook(
        PH_VIRTUAL(0x0049D6A0) /* zCAICamera::DoAI */, &zCAICamera::PH_DoAI, UnionCore::Hook_Auto);
    void zCAICamera::PH_DoAI(zCVob* vob, int& state) {
        ParryControls::UpdateCamera(this);
        THISCALL(Ivk_PH_CameraAI)(vob, state);
        PlaceHp2Camera(this);
    }
    auto& Ivk_PH_CameraKeys = UnionCore::CreateHook(
        &zCAICamera::CheckKeys, &zCAICamera::PH_CheckKeys, UnionCore::Hook_Auto);
    void zCAICamera::PH_CheckKeys() {
        if (!ParryControls::OwnsInput()) THISCALL(Ivk_PH_CameraKeys)();
    }
}

namespace Gothic_I_Classic {
    auto& Ivk_PH_InputState = UnionCore::CreateHook(
        PH_VIRTUAL(0x004C8310) /* zCInput_Win32::GetState */,
        &zCInput_Win32::PH_GetState, UnionCore::Hook_Auto);
    float zCInput_Win32::PH_GetState(unsigned short action) {
        float value = 0.0f;
        if (ParryControls::OverrideNativeState(action, value)) return value;
        return THISCALL(Ivk_PH_InputState)(action);
    }

    auto& Ivk_PH_MouseRight = UnionCore::CreateHook(
        PH_VIRTUAL(0x004C8C50) /* zCInput_Win32::GetMouseButtonPressedRight */,
        &zCInput_Win32::PH_GetMouseButtonPressedRight, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_GetMouseButtonPressedRight() {
        if (ParryControls::maskNativeRight) return 0;
        return THISCALL(Ivk_PH_MouseRight)();
    }

    auto& Ivk_PH_MouseLeft = UnionCore::CreateHook(
        PH_VIRTUAL(0x004C8C30) /* zCInput_Win32::GetMouseButtonPressedLeft */,
        &zCInput_Win32::PH_GetMouseButtonPressedLeft, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_GetMouseButtonPressedLeft() {
        if (ParryControls::maskNativeLeft) return 0;
        return THISCALL(Ivk_PH_MouseLeft)();
    }

    auto& Ivk_PH_KeyPressed = UnionCore::CreateHook(
        PH_VIRTUAL(0x004C87A0) /* zCInput_Win32::KeyPressed */, &zCInput_Win32::PH_KeyPressed, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_KeyPressed(int key) {
        if (ParryControls::BlocksNativeSpellHotkey(key)) return 0;
        if ((ParryControls::maskNativeRight && key == MOUSE_BUTTONRIGHT) ||
            (ParryControls::maskNativeLeft && key == MOUSE_BUTTONLEFT)) return 0;
        return THISCALL(Ivk_PH_KeyPressed)(key);
    }

    auto& Ivk_PH_KeyToggled = UnionCore::CreateHook(
        PH_VIRTUAL(0x004C87B0) /* zCInput_Win32::KeyToggled */, &zCInput_Win32::PH_KeyToggled, UnionCore::Hook_Auto);
    int zCInput_Win32::PH_KeyToggled(int key) {
        if (ParryControls::BlocksNativeSpellHotkey(key)) return 0;
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
        PH_VIRTUAL(0x004C8BA0) /* zCInput_Win32::ProcessInputEvents */,
        &zCInput_Win32::PH_ProcessInputEvents,
        UnionCore::Hook_Auto
    );

    void zCInput_Win32::PH_ProcessInputEvents() {
        // The engine can poll again inside player AI. Read physical input
        // outside our AI masks, otherwise a held button looks released.
        ParryControls::NativePlayerInputScope physicalState(false);
        ParryControls::NativeMagicInputScope physicalMagic(false);
        struct PhysicalMouse {
            bool left = ParryControls::maskNativeLeft;
            bool right = ParryControls::maskNativeRight;
            PhysicalMouse() { ParryControls::maskNativeLeft = ParryControls::maskNativeRight = false; }
            ~PhysicalMouse() {
                ParryControls::maskNativeLeft = left;
                ParryControls::maskNativeRight = right;
            }
        } physicalMouse;
        THISCALL(Ivk_PH_ProcessInputEvents)();
        ++ParryControls::inputHookCalls;
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
    // IceCube (FRZ) and Berzerk (SLE) use a one-phase release. Wait for native
    // CAST status, not merely the first invested point. DONTINVEST can also
    // mean Setup has not yet received a valid target.
    constexpr std::array<bool, ParryHotter::kSpellCount> kInstantSpell{{
        false, true, true, false, false, false, false, false, false, false
    }};

    // Native cast keys are held while LMB charges an invest spell at a target.
    bool gChargeKeys = false;
    void SetNativeCastKeys(bool down, bool active = false) {
        // Read by the scoped GetState hook inside player MagicMode only.
        // Forcing the mappings here also exposed UP to native locomotion.
        ParryControls::SetNativeMagicKeys(active || down, down);
    }
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
    ParryHotter::ControlsTelemetry gControlsLog;
    std::uint64_t gInputSequence = 0;
    std::uint64_t gLastRequestId = 0;
    std::uint64_t gRequestId = 0;
    std::uint64_t gRequestSequence = 0;
    std::uint64_t gRequestGeneration = 0;
    int gPreparedSlot = -1;
    bool gPreparePending = false;
    bool gPreparationBound = false;
    ParryHotter::SpellCooldown::TimePoint gPrepareExpires{};
    oCNpc* gStickyTarget = nullptr; // Compared only against the current world NPC list.
    ParryHotter::CastLedger::TimePoint gRequestStarted{};
    struct ReleasedTarget { std::uint64_t sequence = 0; oCNpc* npc = nullptr; };
    std::array<ReleasedTarget, ParryHotter::InputEvents::Capacity> gReleasedTargets{};
    ParryHotter::SpellCooldown gCooldown;
    ParryHotter::CastLedger gCasts;
    struct CastReferences {
        ParryHotter::CastLedger::Id id = 0;
        std::array<oCVisualFX*, 4> effects{};
        oCNpc* target = nullptr;
    };
    std::array<CastReferences, ParryHotter::CastLedger::Capacity> gCastReferences{};
    oCSpell* gRequestedSpell = nullptr;
    oCSpell* gCancelledSpell = nullptr;
    // Set by a successful release-cast; the wand lowers once it resolves.
    bool gLowerAfterCast = false;
    enum class CastAttempt { Refused, Requested, NotReady };

    // Depth of the plugin's own/native-driven investment calls. In the first
    // recorded session Gothic invested and launched the player's Thunderball
    // twice on its own while LMB was held at a wall (no plugin request),
    // provoking the gate guards. Investment needs this scope; launching
    // always needs a matching release request, including inside this scope.

    bool BlocksForeignPlayerMagic(oCSpell* spell) {
        return spell && player && spell->spellCasterNpc == player &&
               (ParryControls::ContextAvailable() || ParryControls::ConsoleVisible() ||
                spell == gCancelledSpell);
    }

    int InvestTracked(oCSpell* spell) {
        if (gOwnMagicCall == 0 && BlocksForeignPlayerMagic(spell)) {
            static int blocked = 0;
            if (++blocked <= 20 || blocked % 200 == 0)
                gLog.Line("native INVEST blocked spellId=%d (count=%d)",
                          spell->GetSpellID(), blocked);
            return spell->GetSpellStatus();
        }
        // Already fully charged: keep the effect/mana until release, without
        // repeatedly running the native investment or consuming more mana.
        if (DeferChargedSpell(spell)) return 1;
        if (!spell) return 0;
        auto* actor = player;
        const auto requestGeneration = gRequestGeneration;
        spell->AddRef(); // Native investment runs script callbacks.
        const int manaBefore = player ? player->GetAttribute(NPC_ATR_MANA) : -1;
        const int result = (spell->*Gothic_I_Classic::Ivk_PH_Invest)();
        if (player && player == actor && requestGeneration == gRequestGeneration &&
            spell->spellCasterNpc == player && OwnsMagicInput()) {
            const int manaAfter = player->GetAttribute(NPC_ATR_MANA);
            const std::array<int, 4> state{{manaAfter, spell->manaInvested, spell->spellStatus, result}};
            static std::array<int, 4> last{};
            static std::uint64_t generation = ~std::uint64_t(0);
            if (state != last || generation != gRequestGeneration || manaBefore != manaAfter) {
                last = state; generation = gRequestGeneration;
                gLog.Line("native Invest request=%llu slot=%d manaBefore=%d manaAfter=%d invested=%d status=%d result=%d",
                          static_cast<unsigned long long>(gRequestId), gPreparedSlot,
                          manaBefore, manaAfter, state[1], state[2], result);
            }
        }
        spell->Release();
        return result;
    }

    // Each phase is emitted once per physical jump request/flight, including
    // taps shorter than one AI frame and failures before a buffered retry.
    void LogJumpInput(oCAIHuman* ai) {
        if (!ai) return;
        const auto& jump = ParryControls::motion.Telemetry();
        static std::uint64_t requested = 0, accepted = 0, refused = 0, ended = 0;
        const auto write = [&](std::uint64_t id, const char* phase, const char* reason,
                               float age, float peak) {
            gLog.Line("jump id=%llu seq=%llu phase=%s reason=%s held=%d physics=%d "
                      "vy=%.1f floor=%.1f ageSec=%.3f peakCm=%.1f targetHeightCm=70",
                      static_cast<unsigned long long>(id),
                      static_cast<unsigned long long>(ParryControls::events.LastSequence()),
                      phase, reason, ParryControls::GetInput().jumpHeld ? 1 : 0,
                      player && player->physicsEnabled ? 1 : 0, jump.velocityY,
                      jump.aboveFloor, age, peak);
        };
        if (jump.requested && jump.requestId != requested) {
            requested = jump.requestId;
            write(requested, "requested", "press", jump.age, jump.peakHeight);
        }
        if (jump.accepted && jump.activeId != accepted) {
            accepted = jump.activeId;
            write(accepted, "accepted", "accepted", jump.age, jump.peakHeight);
        }
        if ((jump.ignored || (jump.attempted && !jump.accepted)) && jump.requestId != refused) {
            refused = jump.requestId;
            write(refused, "refused", jump.reason, jump.age, jump.peakHeight);
        }
        if ((jump.landed || jump.interrupted) && jump.endedId && jump.endedId != ended) {
            ended = jump.endedId;
            write(ended, jump.landed ? "landed" : "interrupted",
                  jump.landed ? "landed" : jump.reason, jump.endedAge, jump.endedPeakHeight);
        }
        static unsigned recoveries = 0;
        if (recoveries != ParryControls::floorRecoveries) {
            recoveries = ParryControls::floorRecoveries;
            gLog.Line("movement seq=%llu floorRecovery=%u reason=nearby_floor_penetration",
                      static_cast<unsigned long long>(ParryControls::events.LastSequence()), recoveries);
        }
        if (ParryControls::resumedNativeFall)
            gLog.Line("movement seq=%llu nativeFallResumed=1 reason=magic_pose_over_edge",
                      static_cast<unsigned long long>(ParryControls::events.LastSequence()));
    }

    // Diagnostics: which model prototypes (base + overlays) the hero carries
    // and whether Harry's clip replaced the stock Windfist charge animation.
    void LogCastOverlay(int slot, bool bound) {
        zCModel* model = player ? player->GetModel() : nullptr;
        if (!model) { gLog.Line("raise slot=%d overlay model=none", slot); return; }
        char protos[256] = {};
        std::size_t used = 0;
        for (int i = 0; i < model->modelProtoList.GetNum() && used + 40 < sizeof(protos); ++i) {
            zCModelPrototype* proto = model->modelProtoList[i];
            const int written = std::snprintf(protos + used, sizeof(protos) - used, "%s%s",
                i ? "," : "", proto ? proto->modelProtoName.ToChar() : "?");
            if (written > 0) used += std::size_t(written);
        }
        const int id = model->GetAniIDFromAniName(zSTRING("S_WNDCAST"));
        zCModelAni* ani = id >= 0 ? model->GetAniFromAniID(id) : nullptr;
        gLog.Line("raise slot=%d harryCastAnims=%d overlay=%d protos=[%s] S_WNDCAST=%s",
                  slot, bound ? 1 : 0,
                  model->HasAppliedModelProtoOverlay(zSTRING("PH_HARRY_CAST")) ? 1 : 0,
                  protos, ani ? ani->ascName.ToChar() : "missing");
    }

    // Native camera AI runs first. The HP2 adapter rechecks the actual pivot
    // after lowering it under roofs or while easing away from an obstacle.
    void PlaceHp2Camera(zCAICamera* camera) {
        ParryCamera::Place(camera, player, ParryControls::OwnsInput(),
            ParryControls::currentCameraYaw, ParryControls::currentCameraPitch,
            ParryControls::FrameSeconds());
    }

    // HP2 quick tap: LMB released on a valid target before Gothic's invest
    // became castable. Keep the wand raised, invest, and cast at the target
    // captured on release as soon as the engine allows (or give up).
    oCNpc* gQueuedTarget = nullptr;
    int gQueuedSlot = -1;
    std::uint64_t gQueuedSequence = 0;
    ParryHotter::CastLedger::Clock::time_point gQueuedExpires{};

    void ClearQueuedCast() {
        auto* target = gQueuedTarget;
        gQueuedTarget = nullptr;
        gQueuedSlot = -1;
        gQueuedSequence = 0;
        if (target) target->Release();
    }

    void QueueCast(int slot, oCNpc* target) {
        ClearQueuedCast();
        if (!target) return;
        target->AddRef();
        gQueuedTarget = target;
        gQueuedSlot = slot;
        gQueuedSequence = gInputSequence;
        gQueuedExpires = ParryHotter::CastLedger::Clock::now() +
            std::chrono::milliseconds(3000);
        gLog.Line("cast slot=%d QUEUED until castable target=%p seq=%llu", slot,
                  static_cast<void*>(target), static_cast<unsigned long long>(gQueuedSequence));
    }
    int gRequestedSlot = -1;
    oCNpc* gRequestedTarget = nullptr;
    bool gCastConfirmed = false;
    bool gRequestReleased = false;
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
        gRequestReleased = false;
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
        // Investing is permitted inside native AI; launching a projectile
        // still requires a matching release request, even in that scope.
        if (!ours && BlocksForeignPlayerMagic(spell)) {
            gLog.Line("native CAST blocked spellId=%d spell=%p request=%p (no matching request)",
                      spell->GetSpellID(), static_cast<void*>(spell),
                      static_cast<void*>(gRequestedSpell));
            return 0;
        }
        const auto generation = gRequestGeneration;
        const int slot = gRequestedSlot;
        const auto requestId = gRequestId;
        const auto requestSequence = gRequestSequence;
        const auto requestStarted = gRequestStarted;
        auto* target = ours ? gRequestedTarget : nullptr;
        // Native Cast runs scripts and damage hooks. Those callbacks can cancel
        // or replace the request; retain the objects and identify this request.
        if (ours) { spell->AddRef(); target->AddRef(); }
        ParryTargeting::Scope targeting(ours ? spell : nullptr);
        if (ours) {
            const char* failure = ParryControls::ContextAvailable() ? TargetFailure(target) : "context";
            if (!failure && !spell->IsValidTarget(target)) failure = "native_target";
            if (failure) {
                gLog.Line("cast request=%llu slot=%d CANCEL reason=%s caster=%p targetType=%d targetAlgo=%d",
                          static_cast<unsigned long long>(gRequestId), slot, failure,
                          static_cast<void*>(spell->spellCasterNpc), spell->targetCollectType,
                          spell->targetCollectAlgo);
                target->Release(); spell->Release();
                LowerWand();
                return 0;
            }
        }
        const auto sameRequest = [&] {
            return generation == gRequestGeneration && spell == gRequestedSpell;
        };
        auto id = ours ? TrackEffect(spell, target, slot, generation) : 0;
        // Retiring old effects can run engine callbacks and change the world,
        // context or request. Never launch a request invalidated by one.
        if (ours && (!sameRequest() || !ParryControls::ContextAvailable() || TargetFailure(target))) {
            if (id) { gCasts.Revoke(id); ReapCasts(); }
            gLog.Line("cast request=%llu slot=%d CANCEL reason=changed_before_cast",
                      static_cast<unsigned long long>(requestId), slot);
            if (sameRequest()) LowerWand();
            target->Release(); spell->Release();
            return 0;
        }
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
            const double latency = std::chrono::duration<double, std::milli>(
                ParryHotter::CastLedger::Clock::now() - requestStarted).count();
            gLog.Line("engine Cast slot=%d result=%d sameRequest=%d request=%llu latencyMs=%.1f mana=%d seq=%llu",
                      slot, result, sameRequest() ? 1 : 0,
                      static_cast<unsigned long long>(requestId), latency,
                      player ? player->GetAttribute(NPC_ATR_MANA) : -1,
                      static_cast<unsigned long long>(requestSequence));
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
    bool gTargetReleaseReady = false;
    bool OwnsMagicInput() { return gAiming && ParryControls::ContextAvailable(); }
    bool CastRequestPending() { return gRequestedSpell != nullptr; }

    // Observe both NPC weapon mode and the separate animation-selection FSM.
    void LogPlayerMagicAI(oCAIHuman* ai, bool after, bool driving) {
        if (!ai || !player || ai->npc != player || (!gAiming && !gRequestedSpell)) return;
        auto* book = player->GetSpellBook();
        auto* spell = book ? book->GetSelectedSpell() : nullptr;
        const std::array<int, 13> state{{player->fmode, ai->wmode,
            ai->actionMode, ai->changeweapon, player->state.curState.valid,
            ai->IsWalking(), ai->IsInCastAni(), driving ? 1 : 0,
            spell ? spell->GetSpellID() : -1, spell ? spell->spellStatus : -1,
            spell ? spell->manaInvested : -1, player->state.curState.index,
            ai->spellCastedLastFrame ? 1 : 0}};
        static std::array<std::array<int, 13>, 2> last{};
        static std::array<std::uint64_t, 2> generation{{~std::uint64_t(0), ~std::uint64_t(0)}};
        const int phase = after ? 1 : 0;
        if (state == last[phase] && generation[phase] == gRequestGeneration) return;
        last[phase] = state;
        generation[phase] = gRequestGeneration;
        gLog.Line("player AI %s npcMode=%d aiMode=%d actionMode=%d changeWeapon=%d "
                  "scriptStateValid=%d walking=%d castAni=%d driving=%d spellId=%d "
                  "status=%d invested=%d scriptState=%d magicCalls=%u caster=%p lastCast=%d",
                  after ? "after" : "before", state[0], state[1], state[2], state[3],
                  state[4], state[5], state[6], state[7], state[8], state[9], state[10], state[11],
                  gPlayerMagicCalls, spell ? static_cast<void*>(spell->spellCasterNpc) : nullptr, state[12]);
    }

    bool DeferChargedSpell(oCSpell* spell) {
        return spell && ParryControls::DeferNativeSpellCast(
            gManaPrepared && OwnsMagicInput() && spell->spellCasterNpc == player,
            gRequestedSpell != nullptr, spell->spellStatus);
    }

    void LogNativeMagic(oCAIHuman* ai, bool after) {
        if (!ai || ai->npc != player || !zinput || (!gAiming && !gRequestedSpell)) return;
        auto* book = player->GetSpellBook();
        auto* spell = book ? book->GetSelectedSpell() : nullptr;
        if (!spell) return;
        const std::array<int, 12> state{{spell->GetSpellID(), spell->GetSpellStatus(),
            spell->manaInvested, zinput->GetState(GAME_ACTION) != 0.0f,
            zinput->GetState(GAME_UP) != 0.0f, ai->IsWalking(), ai->IsInCastAni(),
            ai->wmode, ai->spellReleased, spell->spellStatus,
            ai->spellCastedLastFrame ? 1 : 0, ParryControls::nativeMagicRearm ? 1 : 0}};
        static std::array<std::array<int, 12>, 2> last{};
        static std::array<std::uint64_t, 2> generation{{~std::uint64_t(0), ~std::uint64_t(0)}};
        const int phase = after ? 1 : 0;
        if (state == last[phase] && generation[phase] == gRequestGeneration) return;
        last[phase] = state;
        generation[phase] = gRequestGeneration;
        gLog.Line("magic %s spellId=%d status=%d invested=%d action=%d up=%d "
                  "walking=%d castAni=%d wmode=%d released=%d rawStatus=%d request=%p lastCast=%d rearm=%d",
                  after ? "after" : "before", state[0], state[1], state[2],
                  state[3], state[4], state[5], state[6], state[7], state[8], state[9],
                  static_cast<void*>(gRequestedSpell), state[10], state[11]);
    }

    void ObserveNativeRelease() {
        if (!gRequestedSpell || gRequestReleased || !OwnsMagicInput() ||
            ParryControls::nativeMagicRearm || ParryControls::nativeCastKeys ||
            !ParryControls::nativeCastAction) return;
        gRequestReleased = true;
        gLog.Line("cast request=%llu slot=%d RELEASE status=%d invested=%d",
                  static_cast<unsigned long long>(gRequestId), gRequestedSlot,
                  gRequestedSpell->spellStatus, gRequestedSpell->manaInvested);
    }

    // Gothic I's own magic AI (oCAIHuman::MagicMode) invests while ACTION and
    // UP are both pressed and fires on their release; it then plays the
    // shoot animation and casts at the focus. LMB is not bound to ACTION while
    // we own input. Set the keys here, but expose them via GetState only
    // inside the player's MagicMode. Returns true while driving.
    bool DriveNativeMagic() {
        UpdateSpellPreparation();
        if (!gAiming || !player) {
            SetNativeCastKeys(false);
            ParryControls::castPose = false;
            return false;
        }
        bool hold = gChargeKeys;
        if (gRequestedSpell) {
            const char* failure = TargetFailure(gRequestedTarget);
            if (failure) {
                gLog.Line("cast request=%llu slot=%d CANCEL reason=%s",
                          static_cast<unsigned long long>(gRequestId), gRequestedSlot, failure);
                LowerWand();
                ParryControls::castPose = false;
                return false;
            }
            const int status = gRequestedSpell->GetSpellStatus();
            const bool requireCastStatus = gRequestedSlot >= 0 &&
                gRequestedSlot < ParryHotter::kSpellCount &&
                (kInstantSpell[gRequestedSlot] ||
                 gRequestedSlot == static_cast<int>(ParryHotter::Spell::AvadaKedavra));
            // BreathOfDeath's release script sends STOP before native CAST,
            // unlike Wind/Thunder/Storm's legal partially invested release.
            hold = ParryControls::ShouldHoldNativeCast(requireCastStatus,
                gCastConfirmed, status, gRequestedSpell->manaInvested,
                gRequestReleased && gRequestedSlot ==
                    static_cast<int>(ParryHotter::Spell::AvadaKedavra));
            if (gRequestedTarget) {
                player->SetFocusVob(static_cast<zCVob*>(gRequestedTarget));
                gRequestedSpell->spellTarget = gRequestedTarget;
                gRequestedSpell->spellTargetNpc = gRequestedTarget;
            }
        }
        SetNativeCastKeys(hold, true);
        // Gothic invests only while the caster is not walking.
        ParryControls::castPose = hold || gRequestedSpell != nullptr;
        return hold || gRequestedSpell;
    }
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
        // HP2 centre dot: square on screen (virtual X and Y units differ).
        constexpr int kReticle = 150;
        const int reticleHeight = screen->any(screen->nax(kReticle));
        gReticle = NewSprite(4096 - kReticle / 2, 4096 - reticleHeight / 2,
                             kReticle, reticleHeight, "PH_RETICLE.TGA");
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
    ParryControls::events.Reset(gController.Selected(), true);
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
        gTargetReleaseReady = false;
        if (!screen || !gReticle) return;
        ParryHotter::TargetGlyphLayout glyph{};
        auto* camera = zCCamera::activeCam;
        const bool acquired = hasTarget && npc && camera &&
            spell >= 0 && spell < ParryHotter::kSpellCount && kTargetTextures[spell];
        bool ready = acquired && gAiming && gManaPrepared && gPreparedSlot == spell &&
            !gRequestedSpell && !gQueuedTarget &&
            gCooldown.ReadyByTime(spell, ParryHotter::SpellCooldown::Clock::now());
        if (ready) {
            auto* book = player ? player->GetSpellBook() : nullptr;
            auto* prepared = book ? book->GetSelectedSpell() : nullptr;
            ParryTargeting::Scope targeting(prepared);
            if (!prepared || prepared->GetSpellID() != kGothicSpellIds[spell] ||
                !prepared->GetEnabled() || !prepared->IsValidTarget(npc) ||
                prepared->GetSpellStatus() == SPL_STATUS_STOP) {
                ready = false;
            }
        }
        gTargetReleaseReady = ready;
        if (acquired) {
            // Anchor the glyph at the NPC's upper torso, not at the crosshair.
            zVEC3 anchor = npc->bbox3D.mins +
                (npc->bbox3D.maxs - npc->bbox3D.mins) * zVEC3(0.5f);
            anchor[1] += (npc->bbox3D.maxs[1] - npc->bbox3D.mins[1]) * 0.18f;
            const zVEC3 cameraSpace = camera->camMatrix * anchor;
            if (cameraSpace[2] > camera->nearClipZ) {
                float x = 0.0f, y = 0.0f;
                camera->Project(&cameraSpace, x, y);
                ParryHotter::TargetGlyphInput input{};
                input.aiming = input.targetValid = true;
                input.spellReady = ready;
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
        const bool locked = acquired;
        {
            // Diagnostics: a valid lock whose glyph is not drawn.
            static auto lastReport = std::chrono::steady_clock::time_point{};
            const auto clock = std::chrono::steady_clock::now();
            if (locked && !glyph.visible && camera && clock - lastReport > std::chrono::seconds(1)) {
                lastReport = clock;
                zVEC3 anchor = npc->bbox3D.mins + (npc->bbox3D.maxs - npc->bbox3D.mins) * zVEC3(0.5f);
                const zVEC3 cs = camera->camMatrix * anchor;
                float px = -1.0f, py = -1.0f;
                if (cs[2] > camera->nearClipZ) camera->Project(&cs, px, py);
                gLog.Line("glyph hidden: camZ=%.1f near=%.1f screen=%.0f,%.0f of %d,%d cooldown=%d texture=%d",
                          cs[2], camera->nearClipZ, px, py, screen->psizex, screen->psizey,
                          gCooldown.ReadyByTime(spell, ParryHotter::SpellCooldown::Clock::now()) ? 1 : 0,
                          kTargetTextures[spell] ? 1 : 0);
            }
        }
        // HP2 lock-on: a chime when the dot reaches a new NPC, and the
        // engine focus shows that NPC's name. The pointer is only compared.
        static const oCNpc* lockedNpc = nullptr;
        if (locked) {
            if (npc != lockedNpc) {
                lockedNpc = npc;
                if (zsound && player)
                    zsound->PlaySound3D(zSTRING("MFX_LIGHT_CAST"), static_cast<zCVob*>(player), 0, nullptr);
                gLog.Line("lock-on slot=%d npc=%p", spell, static_cast<void*>(npc));
            }
            if (player) player->SetFocusVob(static_cast<zCVob*>(npc));
        } else {
            lockedNpc = nullptr;
        }
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
    // True when nothing blocks the straight line from the camera to the NPC.
    struct AimProbe {
        const char* mode = "none";
        const char* reason = "no_target";
        unsigned scanned = 0, eligible = 0, tested = 0;
        unsigned cameraBlocked = 0, actorBlocked = 0;
        float angle = -1.0f;
    };
    AimProbe gAimProbe;

    bool CameraSeesNpc(zCWorld* world, const zVEC3& eye, oCNpc* npc) {
        const zVEC3 size = npc->bbox3D.maxs - npc->bbox3D.mins;
        const zVEC3 centre = npc->bbox3D.mins + size * zVEC3(0.5f);
        // A visible head or lower body can acquire the target too. The
        // actor-to-target native LOS is still required separately.
        constexpr float heights[] = {0.5f, 0.8f, 0.2f};
        const int flags = zTRACERAY_VOB_BBOX | zTRACERAY_STAT_POLY |
                          zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE_NO_CD_DYN;
        for (const float height : heights) {
            zVEC3 aim = centre;
            aim[1] = npc->bbox3D.mins[1] + size[1] * height;
            if (!world->TraceRayNearestHit(eye, aim-eye, static_cast<zCVob*>(player), flags) ||
                world->traceRayReport.foundVob == npc) return true;
        }
        return false;
    }

    bool InSpellRange(oCNpc* npc) {
        const float distance = player->GetDistanceToVob(*npc);
        return distance >= 0.0f && distance < float(ParryTargeting::kRangeCm);
    }

    const char* TargetFailure(oCNpc* target, bool lineOfSight) {
        if (!player || !target || target == player) return "no_target";
        if (target->IsDead()) return "target_dead";
        if (target->GetHomeWorld() != player->GetHomeWorld()) return "target_world";
        if (!InSpellRange(target)) return "target_range";
        if (lineOfSight && !player->FreeLineOfSight(target)) return "target_occluded";
        return nullptr;
    }

    void ClearReleaseTargets() {
        for (auto& entry : gReleasedTargets) {
            auto* target = entry.npc;
            entry = {};
            if (target) target->Release();
        }
    }

    void CaptureReleaseTarget(std::uint64_t sequence) {
        auto& entry = gReleasedTargets[sequence % gReleasedTargets.size()];
        if (entry.npc) entry.npc->Release();
        entry = {sequence, GetCenteredNpcTarget()};
        if (entry.npc) entry.npc->AddRef();
        gLog.Line("aim seq=%llu phase=release mode=%s target=%p reason=%s angle=%.2f "
                  "scanned=%u eligible=%u tested=%u cameraBlocked=%u actorBlocked=%u",
                  static_cast<unsigned long long>(sequence), gAimProbe.mode,
                  static_cast<void*>(entry.npc), gAimProbe.reason, gAimProbe.angle,
                  gAimProbe.scanned, gAimProbe.eligible, gAimProbe.tested,
                  gAimProbe.cameraBlocked, gAimProbe.actorBlocked);
    }

    oCNpc* TakeReleaseTarget(std::uint64_t sequence) {
        auto& entry = gReleasedTargets[sequence % gReleasedTargets.size()];
        if (entry.sequence != sequence) return nullptr;
        auto* target = entry.npc;
        entry = {};
        return target; // Caller owns the retained reference.
    }

    // Acquire around the projected body rather than one point on the chest.
    // Range and both lines of sight remain authoritative for every candidate.
    oCNpc* AimAssistTarget(zCWorld* world, const zVEC3& eye, const zVEC3& direction) {
        oCWorld* gameWorld = ogame ? ogame->GetGameWorld() : nullptr;
        if (!gameWorld || static_cast<zCWorld*>(gameWorld) != world ||
            !gameWorld->voblist_npcs) return nullptr;
        std::vector<ParryHotter::AimCandidate> candidates;
        std::vector<oCNpc*> npcs;
        int sticky = -1;
        for (auto* node = gameWorld->voblist_npcs->next; node; node = node->next) {
            ++gAimProbe.scanned;
            oCNpc* npc = node->data;
            if (!npc || npc == player || npc->GetHomeWorld() != world ||
                npc->IsDead() || !InSpellRange(npc)) continue;
            const zVEC3 half = (npc->bbox3D.maxs-npc->bbox3D.mins) * zVEC3(0.5f);
            const zVEC3 centre = npc->bbox3D.mins + half;
            if (npc == gStickyTarget) sticky = static_cast<int>(npcs.size());
            candidates.push_back({{centre[VX], centre[VY], centre[VZ]}, true,
                                  {half[VX], half[VY], half[VZ]}});
            npcs.push_back(npc);
        }
        gAimProbe.eligible = static_cast<unsigned>(candidates.size());
        const ParryHotter::AimVec eyeV{eye[VX], eye[VY], eye[VZ]};
        const ParryHotter::AimVec dirV{direction[VX], direction[VY], direction[VZ]};
        gAimProbe.reason = candidates.empty() ? "no_living_npc_in_range" : "outside_assist";
        for (const auto& candidate : candidates) {
            const auto score = ParryHotter::ScoreAimCandidate(eyeV, dirV, candidate);
            if (score.valid && (gAimProbe.angle < 0.0f || score.angleDegrees < gAimProbe.angle))
                gAimProbe.angle = score.angleDegrees;
        }
        // Test every geometrically eligible candidate in priority order. An
        // obscured crowd must not hide the next valid NPC after four attempts.
        for (std::size_t attempt = 0; attempt < candidates.size(); ++attempt) {
            const int best = ParryHotter::PickAimAssist(eyeV, dirV, candidates, {}, sticky);
            if (best < 0) break;
            ++gAimProbe.tested;
            oCNpc* npc = npcs[best];
            const char* failure = TargetFailure(npc);
            if (failure) {
                ++gAimProbe.actorBlocked;
                gAimProbe.reason = failure;
            } else if (!CameraSeesNpc(world, eye, npc)) {
                ++gAimProbe.cameraBlocked;
                gAimProbe.reason = "camera_occluded";
            } else {
                gAimProbe.mode = "assist";
                gAimProbe.reason = "acquired";
                gAimProbe.angle = ParryHotter::ScoreAimCandidate(eyeV, dirV, candidates[best]).angleDegrees;
                return npc;
            }
            candidates[best].alive = false;
        }
        return nullptr;
    }

    oCNpc* GetCenteredNpcTarget() {
        gAimProbe = {};
        if (!player) {
            gAimProbe.reason = "no_player";
            SetAimTargetState(ParryHotter::AimTargetState::NoPlayer);
            return nullptr;
        }
        zCAICamera* cam = zCAICamera::current;
        zCVob* cameraVob = cam ? cam->camVob : nullptr;
        zCWorld* world = player->GetHomeWorld();
        if (!cameraVob) {
            gAimProbe.reason = "no_camera";
            SetAimTargetState(ParryHotter::AimTargetState::NoCamera);
            return nullptr;
        }
        if (!world) {
            gAimProbe.reason = "no_world";
            SetAimTargetState(ParryHotter::AimTargetState::NoWorld);
            return nullptr;
        }

        zVEC3 direction = cameraVob->GetAtVectorWorld();
        if (direction.Length_Sqr() < 0.01f) {
            gAimProbe.reason = "no_camera_direction";
            SetAimTargetState(ParryHotter::AimTargetState::NoCamera);
            return nullptr;
        }
        direction.Normalize();
        const zVEC3 eye = cameraVob->GetPositionWorld();
        zVEC3 ray = direction;
        // The camera is behind the actor; only the actor's 1900 cm gate
        // defines spell range. The picking ray must reach beyond that offset.
        ray *= zVEC3(ParryHotter::AimAssistSettings{}.maxEyeDistance);
        // Nearest hit (no FIRSTHIT): any first-found far polygon must not
        // hide an NPC standing right under the dot.
        const int flags = zTRACERAY_VOB_BBOX | zTRACERAY_STAT_POLY |
                          zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE_NO_CD_DYN;
        auto state = ParryHotter::AimTargetState::NoRayHit;
        if (world->TraceRayNearestHit(eye, ray, static_cast<zCVob*>(player), flags)) {
            oCNpc* npc = zDYNAMIC_CAST<oCNpc>(world->traceRayReport.foundVob);
            if (!npc || npc == player) state = ParryHotter::AimTargetState::WallOrObject;
            else if (npc->IsDead()) state = ParryHotter::AimTargetState::DeadNpc;
            else if (npc->GetHomeWorld() != world) state = ParryHotter::AimTargetState::WrongWorld;
            else if (!InSpellRange(npc)) state = ParryHotter::AimTargetState::OutOfRange;
            else if (!TargetFailure(npc)) {
                SetAimTargetState(ParryHotter::AimTargetState::ValidNpc);
                gStickyTarget = npc;
                gAimProbe.mode = "ray";
                gAimProbe.reason = "acquired";
                gAimProbe.angle = 0.0f;
                return npc;
            }
        }
        if (oCNpc* snapped = AimAssistTarget(world, eye, direction)) {
            SetAimTargetState(ParryHotter::AimTargetState::ValidNpc);
            gStickyTarget = snapped;
            return snapped;
        }
        SetAimTargetState(state);
        gStickyTarget = nullptr;
        return nullptr;
    }

    // Use the native spellbook so that mana, target rules and Gothic visual FX
    // still apply. Test the call ordering on a LOCAL Gothic I installation;
    // engine variations (Steam + Union) may require an event hook.
    void RefreshManaCosts() {
        constexpr const char* names[] = {"SPL_SENDCAST_WINDFIST", "SPL_SENDCAST_ICECUBE",
            "SPL_SENDCAST_BERZERK", "SPL_SENDCAST_THUNDERBALL", "SPL_SENDCAST_STORMFIST",
            "SPL_SENDCAST_BREATHOFDEATH"};
        for (int slot = 0; slot < 6; ++slot) {
            int cost = ParryHotter::SpellCooldown::ManaCosts[slot];
            auto* symbol = parser ? parser->GetSymbol(zSTRING(names[slot])) : nullptr;
            if (symbol && symbol->type == zPAR_TYPE_INT && symbol->ele >= 1) {
                int parsed = -1;
                symbol->GetValue(parsed, 0);
                if (parsed >= 0) cost = parsed;
            }
            if (cost != gCooldown.ManaCost(slot)) {
                gLog.Line("mana slot=%d required=%d source=%s", slot, cost,
                          symbol ? "scripts" : "fallback");
            }
            gCooldown.SetManaCost(slot, cost);
        }
    }

    void UpdateSpellPreparation() {
        if (!gPreparePending || !gAiming || !player) return;
        if (!ParryControls::ContextAvailable() ||
            ParryHotter::SpellCooldown::Clock::now() >= gPrepareExpires) {
            gLog.Line("prepare slot=%d CANCEL reason=%s", gPreparedSlot,
                      ParryControls::ContextAvailable() ? "ready_timeout" : "context");
            LowerWand();
            return;
        }
        auto* book = player->GetSpellBook();
        auto* prepared = book ? book->GetSelectedSpell() : nullptr;
        if (!prepared || prepared->GetSpellID() != kGothicSpellIds[gPreparedSlot] ||
            !prepared->GetEnabled()) return;
        if (!gPreparationBound && player->human_ai) {
            gPreparationBound = true;
            LogCastOverlay(gPreparedSlot,
                ParryCasting::Bind(player->human_ai, player->GetModel()));
        }
        // Immediate preparation must initialize the caster and mode together.
        if (player->fmode > 6) {
            gPreparePending = false;
            gLog.Line("prepare slot=%d READY npcMode=%d spellId=%d", gPreparedSlot,
                      player->fmode, prepared->GetSpellID());
        }
    }

    void RaiseWand(int spellSlot) {
        if (!ParryControls::ContextAvailable() ||
            spellSlot < 0 || spellSlot >= ParryHotter::kSpellCount ||
            kGothicSpellIds[spellSlot] < 0) {
            gLog.Line("raise slot=%d REFUSED context=%d", spellSlot,
                      ParryControls::ContextAvailable() ? 1 : 0);
            return;
        }
        ReleaseCastRequest();
        gChargeKeys = false;
        SetNativeCastKeys(false);
        RefreshManaCosts();
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
            ParryWand::Cancel(player);
        }
        // Check mana BEFORE Gothic invests it. Do not re-check during casting.
        if (!gCooldown.Ready(spellSlot, mana, now)) {
            gLog.Line("raise slot=%d REFUSED cooldown/mana mana=%d", spellSlot, mana);
            return;
        }
        const int id = kGothicSpellIds[spellSlot];
        // Stop locomotion before starting the prepare animation, never by
        // replacing an already active invest/shoot animation every AI step.
        if (player->human_ai && player->human_ai->IsWalking() &&
            !player->human_ai->IsInCastAni()) player->human_ai->_Stand();
        // Select/setup the native rune and enter magic without queued weapon
        // drawing. LMB is the aim gesture, never a draw/sheath toggle.
        auto* actor = player;
        const auto generation = gRequestGeneration;
        const bool readyResult = ParryWand::Prepare(actor, id, GetCenteredNpcTarget());
        if (player != actor || generation != gRequestGeneration) return;
        if (!ParryControls::ContextAvailable()) {
            ParryWand::Cancel(actor);
            return;
        }
        if (!readyResult) {
            gLog.Line("raise slot=%d REFUSED reason=%s", spellSlot, ParryWand::Reason());
            ParryWand::Cancel(actor);
            return;
        }
        gManaPrepared = true;
        gAiming = true;
        gPreparedSlot = spellSlot;
        gPreparePending = true;
        gPreparationBound = false;
        gPrepareExpires = now + std::chrono::seconds(3);
        UpdateSpellPreparation();
        AttachWand();
        auto* selected = player->GetSpellBook()->GetSelectedSpell();
        gLog.Line("raise slot=%d OK spellId=%d mana=%d wandAttached=%d instantPrepared=1 npcMode=%d caster=%p",
                  spellSlot, id, mana, gWandVisual ? 1 : 0, player->fmode,
                  selected ? static_cast<void*>(selected->spellCasterNpc) : nullptr);
    }

    void LowerWand() {
        ClearQueuedCast();
        gChargeKeys = false;
        SetNativeCastKeys(false);
        if (gAiming || gRequestedSpell) gLog.Line("lower wand aiming=%d request=%d",
                                                  gAiming ? 1 : 0, gRequestedSpell ? 1 : 0);
        gLowerAfterCast = false;
        const bool wasAiming = gAiming;
        auto* actor = player;
        if (player && gAiming) {
            auto* book = player->GetSpellBook();
            auto* cancelled = book ? book->GetSelectedSpell() : nullptr;
            if (cancelled) cancelled->AddRef();
            auto* previous = gCancelledSpell;
            gCancelledSpell = cancelled;
            if (previous) previous->Release();
        }
        gAiming = false;
        gManaPrepared = false;
        gPreparedSlot = -1;
        gPreparePending = gPreparationBound = false;
        ReleaseCastRequest();
        if (player != actor || gAiming || gRequestedSpell) return;
        const auto generation = gRequestGeneration;
        DetachWand();
        if (player) ParryCasting::Restore(player->human_ai, player->GetModel());
        else ParryCasting::Forget();
        // A cancellation callback may have loaded another world or started a
        // new request. Do not cancel that replacement actor/request on return.
        if (generation != gRequestGeneration || player != actor || gAiming ||
            gRequestedSpell) return;
        if (player && (wasAiming || ParryWand::Owns(player))) ParryWand::Cancel(player);
    }

    void SetUnionPresence(bool enabled) {
        if (!parser) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_UNION_LOADED"));
        if (symbol) symbol->SetValue(enabled ? 1 : 0, 0);
    }

    void ResetSelectedSpellForScript() {
        ParryWand::Forget();
        ClearReleaseTargets();
        gStickyTarget = nullptr;
        ReleaseCastRequest();
        auto* cancelled = gCancelledSpell;
        gCancelledSpell = nullptr;
        if (cancelled) cancelled->Release();
        ClearFlight();
        ReapCasts(true);
        gHitCast = 0;
        gHitSlot = -1;
        gDamageDepth = 0;
        if (!parser) return;
        zCPar_Symbol* symbol = parser->GetSymbol(zSTRING("PH_SELECTED_SPELL"));
        if (symbol) symbol->SetValue(0, 0);
    }

    CastAttempt CastAtNpc(int slot, oCNpc* target, std::uint64_t sequence) {
        const char* failure = TargetFailure(target);
        if (!ParryControls::ContextAvailable()) failure = "context";
        else if (gRequestedSpell) failure = "busy";
        else if (slot < 0 || slot >= ParryHotter::kSpellCount ||
                 !ParryHotter::kSpellAvailable[slot] || gPreparedSlot != slot) failure = "prepared_slot";
        if (failure) {
            gLog.Line("cast slot=%d REFUSED reason=%s seq=%llu", slot, failure,
                      static_cast<unsigned long long>(gInputSequence));
            return CastAttempt::Refused;
        }
        auto* book = player->GetSpellBook();
        auto* spell = book ? book->GetSelectedSpell() : nullptr;
        // Preserve the exact release target if a callback delays preparation.
        if (!spell || spell->GetSpellID() != kGothicSpellIds[slot]) return CastAttempt::NotReady;
        if (!spell->GetEnabled() || spell->GetSpellStatus() == SPL_STATUS_STOP) {
            if (gPreparePending) return CastAttempt::NotReady;
            gLog.Line("cast slot=%d REFUSED reason=disabled seq=%llu", slot,
                      static_cast<unsigned long long>(gInputSequence));
            return CastAttempt::Refused;
        }
        player->SetFocusVob(target);
        spell->spellTarget = target;
        spell->spellTargetNpc = target;
        if (spell->effect) spell->effect->SetTarget(target);
        gRequestedSpell = spell;
        gRequestedSlot = slot;
        gRequestedTarget = target;
        spell->AddRef(); target->AddRef();
        gRequestStarted = ParryHotter::CastLedger::Clock::now();
        gRequestExpires = gRequestStarted + std::chrono::seconds(4);
        gRequestId = ++gLastRequestId;
        gRequestSequence = sequence;
        gCastConfirmed = false;
        gRequestReleased = false;
        gChargeKeys = false;
        // IsValidTarget may require a caster/effect created by native setup.
        // Validate geometry now; native eligibility is checked before Cast.
        gLog.Line("cast request=%llu seq=%llu slot=%d REQUEST target=%p status=%d invested=%d npcMode=%d",
                  static_cast<unsigned long long>(gRequestId),
                  static_cast<unsigned long long>(sequence), slot,
                  static_cast<void*>(target), spell->GetSpellStatus(), spell->manaInvested, player->fmode);
        return CastAttempt::Requested;
    }

    CastAttempt CastAtCenteredNpc(int slot) {
        return CastAtNpc(slot, GetCenteredNpcTarget(), gInputSequence);
    }


    void ApplySpellInput(const ParryHotter::Output& out, oCNpc* target,
                         ParryHotter::SpellCooldown::TimePoint now) {
        if (out.cast) {
            if (gAiming && gManaPrepared && gCooldown.ReadyByTime(out.castSlot, now)) {
                const auto attempt = CastAtNpc(out.castSlot, target, gInputSequence);
                if (attempt == CastAttempt::Requested) gLowerAfterCast = true;
                else if (attempt == CastAttempt::NotReady) {
                    QueueCast(out.castSlot, target);
                    gChargeKeys = true;
                }
            } else {
                gLog.Line("cast slot=%d REFUSED reason=cooldown_or_unprepared seq=%llu", out.castSlot,
                          static_cast<unsigned long long>(gInputSequence));
            }
        }
        if (out.stopAim && !gLowerAfterCast && !gQueuedTarget && !gRequestedSpell) LowerWand();
        if (out.startAim && !gQueuedTarget && !gRequestedSpell) {
            gLowerAfterCast = false;
            RaiseWand(out.aimSlot);
        }
    }

    const char* ControlContext() {
        if (ParryControls::ConsoleVisible()) return "console";
        if (!ogame || !player || !zinput) return "no_game";
        if (ogame->IsOnPause()) return "pause";
        if (ogame->inLoadSaveGame || ogame->inLevelChange) return "loading";
        if (player->openinventory) return "inventory";
        if (player->IsDead()) return "dead";
        return ParryControls::ContextAvailable() ? "gameplay" : "native_state";
    }

    void GameTick() {
        const char* context = ControlContext();
        static const char* previousContext = nullptr;
        if (context != previousContext) {
            gLog.Line("context reason=%s seq=%llu", context,
                      static_cast<unsigned long long>(ParryControls::events.LastSequence()));
            previousContext = context;
        }
        const bool gameplay = ogame && screen && player && zinput &&
            !ogame->inLoadSaveGame && !ogame->inLevelChange && !player->openinventory &&
            !ogame->IsOnPause() && !player->IsDead();
        if (!gameplay) {
            if (gRequestedSpell)
                gLog.Line("cast request=%llu slot=%d CANCEL reason=%s",
                          static_cast<unsigned long long>(gRequestId), gRequestedSlot, context);
            ParryControls::events.Capture(false, false, false);
            ClearReleaseTargets();
            if (gAiming || gRequestedSpell || gQueuedTarget || ParryWand::Owns(player)) LowerWand();
            if (gFlightRecovery.Active() || gCasts.Size(ParryHotter::CastLedger::Clock::now()))
                ResetSelectedSpellForScript();
            ParryControls::Disable();
            gCamera.Restore(zCAICamera::current);
            gController.Step({});
            ShowTarget(false, gActiveSlot);
            HideDiagnostics();
            return;
        }
        const bool allowed = ParryControls::ContextAvailable();
        if (!ParryControls::ConsoleVisible() && zinput->KeyToggled(KEY_F10)) {
            gDiagnosticVisible = !gDiagnosticVisible;
            gDiagnosticDirty = true;
        }
        if (!allowed) {
            ParryControls::events.Capture(false, false, false);
            gAimProbe = {};
            gAimProbe.reason = context;
        }
        const auto now = ParryHotter::SpellCooldown::Clock::now();
        static const auto clockStart = now;
        const auto nowMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - clockStart).count());
        UpdateSpellPreparation();
        if (!allowed && (gAiming || gRequestedSpell || gQueuedTarget)) {
            gLog.Line("cast request=%llu slot=%d CANCEL reason=%s",
                      static_cast<unsigned long long>(gRequestId), gRequestedSlot, context);
            LowerWand();
        }
        oCNpc* target = allowed ? GetCenteredNpcTarget() : nullptr;
        const AimProbe targetProbe = gAimProbe;
        ParryHotter::InputEvent event;
        while (ParryControls::events.Pop(event)) {
            gInputSequence = event.sequence;
            if (event.kind == ParryHotter::InputEventKind::Interrupted) {
                gController.Step({});
                ClearReleaseTargets();
                if (gAiming || gRequestedSpell || gQueuedTarget || ParryWand::Owns(player)) LowerWand();
                gLog.Line("input seq=%llu kind=Interrupted slot=%d dropped=%llu",
                          static_cast<unsigned long long>(event.sequence), event.slot,
                          static_cast<unsigned long long>(ParryControls::events.Dropped()));
                continue;
            }
            const bool release = event.kind == ParryHotter::InputEventKind::LeftReleased;
            oCNpc* released = release ? TakeReleaseTarget(event.sequence) : nullptr;
            const char* kind = release ? "LeftReleased" :
                event.kind == ParryHotter::InputEventKind::LeftPressed ? "LeftPressed" :
                event.kind == ParryHotter::InputEventKind::RightPressed ? "RightPressed" :
                event.kind == ParryHotter::InputEventKind::RightReleased ? "RightReleased" : "SlotSelected";
            gLog.Line("input seq=%llu kind=%s slot=%d target=%p",
                      static_cast<unsigned long long>(event.sequence), kind, event.slot,
                      static_cast<void*>(release ? released : target));
            const auto output = gController.Step({allowed, event.slotPressed,
                event.rightHeld, event.leftHeld, (release ? released : target) != nullptr});
            ApplySpellInput(output, release ? released : target, now);
            if (released) released->Release();
        }
        ParryControls::ConsumeSlotPressed();
        const auto out = gController.Step({allowed, 0, ParryControls::events.RightHeld(),
            ParryControls::events.LeftHeld(), target != nullptr});
        ApplySpellInput(out, target, now);
        if (gQueuedTarget) {
            const char* failure = TargetFailure(gQueuedTarget);
            if (!allowed || !gAiming || failure || now >= gQueuedExpires) {
                gLog.Line("cast slot=%d QUEUE DROPPED reason=%s", gQueuedSlot,
                          failure ? failure : (!allowed ? context : "ready_timeout"));
                LowerWand();
            } else {
                const auto attempt = CastAtNpc(gQueuedSlot, gQueuedTarget, gQueuedSequence);
                if (attempt == CastAttempt::Requested) { ClearQueuedCast(); gLowerAfterCast = true; }
                else if (attempt == CastAttempt::Refused) LowerWand();
            }
        }
        if (gRequestedSpell && now >= gRequestExpires) {
            gLog.Line("cast request=%llu slot=%d TIMEOUT status=%d invested=%d magicCalls=%u",
                      static_cast<unsigned long long>(gRequestId), gRequestedSlot,
                      gRequestedSpell->GetSpellStatus(), gRequestedSpell->manaInvested, gPlayerMagicCalls);
            LowerWand();
        }
        // A jump/hit can replace Avada's shoot pose and reset its full cost.
        // Do not charge that release again and spend its mana twice.
        if (gRequestedSpell && gRequestedSlot == static_cast<int>(ParryHotter::Spell::AvadaKedavra) &&
            gRequestReleased && gRequestedSpell->manaInvested <= 0 &&
            gRequestedSpell->spellStatus == SPL_STATUS_CANINVEST) {
            gLog.Line("cast request=%llu slot=%d CANCEL reason=shoot_interrupted",
                      static_cast<unsigned long long>(gRequestId), gRequestedSlot);
            LowerWand();
        }
        if (gRequestedSpell && (gRequestedSpell->manaInvested < 0 ||
            gRequestedSpell->GetSpellStatus() == SPL_STATUS_STOP)) {
            gLog.Line("cast request=%llu slot=%d CANCEL reason=engine_stop status=%d invested=%d",
                      static_cast<unsigned long long>(gRequestId), gRequestedSlot,
                      gRequestedSpell->GetSpellStatus(), gRequestedSpell->manaInvested);
            LowerWand();
        }
        if (out.aiming && gAiming && gManaPrepared && !gRequestedSpell && target &&
            gPreparedSlot == out.aimSlot && !kInstantSpell[gPreparedSlot] &&
            gCooldown.ReadyByTime(gPreparedSlot, now)) {
            if (!gChargeKeys) gLog.Line("charge slot=%d start seq=%llu", gPreparedSlot,
                                     static_cast<unsigned long long>(gInputSequence));
            gChargeKeys = true;
        }
        if (gLowerAfterCast && !gRequestedSpell &&
            !(player->human_ai && player->human_ai->IsInCastAni())) LowerWand();
        UpdateFlight(now);
        ReapCasts();
        KeepHarryAppearance();
        ShowHud();
        ShowSlotAvailability(player->GetAttribute(NPC_ATR_MANA), now);
        ShowSelection(out.selected);
        ShowTarget(out.targetSymbolVisible, out.selected, target);
        DrawDiagnostics();
        gCamera.Restore(zCAICamera::current);
        ParryHotter::ControlsSnapshot snapshot;
        snapshot.inputSequence = gInputSequence;
        snapshot.requestId = gRequestedSpell ? gRequestId : 0;
        snapshot.slot = out.selected;
        snapshot.leftHeld = ParryControls::events.LeftHeld();
        snapshot.rightHeld = ParryControls::events.RightHeld();
        snapshot.npcMode = player->fmode;
        snapshot.body = player->GetBodyState();
        snapshot.desiredX = ParryControls::lastDesiredX;
        snapshot.desiredZ = ParryControls::lastDesiredZ;
        snapshot.actualX = ParryControls::movementFeedback.velocityX;
        snapshot.actualZ = ParryControls::movementFeedback.velocityZ;
        snapshot.verticalVelocity = player->GetVelocity()[1];
        snapshot.cameraDistance = ParryCamera::last.distance;
        snapshot.cameraPivotHeight = ParryCamera::last.pivotAboveHead;
        if (gControlsLog.ShouldLog(nowMs, snapshot)) {
            auto* ai = player->human_ai;
            gLog.Line("controls seq=%llu request=%llu slot=%d context=%s LMB=%d RMB=%d "
                      "npcMode=%d aiMode=%d body=%d castAni=%d invested=%d mana=%d floor=%.1f "
                      "desiredX=%.1f desiredZ=%.1f actualX=%.1f actualZ=%.1f vy=%.1f "
                      "movementBlocked=%d cameraActive=%d cameraDist=%.1f cameraWant=%.1f "
                      "pivotHeight=%.1f cameraBlocked=%d roofLimited=%d cameraHits=%d cameraTraces=%d "
                      "gaitSupported=%d gaitApplied=%d dropped=%llu",
                      static_cast<unsigned long long>(gInputSequence),
                      static_cast<unsigned long long>(snapshot.requestId), out.selected, context,
                      snapshot.leftHeld ? 1 : 0, snapshot.rightHeld ? 1 : 0, snapshot.npcMode,
                      ai ? ai->wmode : -1, snapshot.body, ai ? ai->IsInCastAni() : 0,
                      gRequestedSpell ? gRequestedSpell->manaInvested : 0,
                      player->GetAttribute(NPC_ATR_MANA), ai ? ai->aboveFloor : 0.0f,
                      snapshot.desiredX, snapshot.desiredZ, snapshot.actualX, snapshot.actualZ,
                      snapshot.verticalVelocity, ParryControls::movementFeedback.blocked ? 1 : 0,
                      ParryCamera::last.active ? 1 : 0, snapshot.cameraDistance,
                      ParryCamera::last.desiredDistance, snapshot.cameraPivotHeight,
                      ParryCamera::last.collisionLimited ? 1 : 0, ParryCamera::last.ceilingLimited ? 1 : 0,
                      ParryCamera::last.sampleHits, ParryCamera::last.traceCount,
                      ParryMovingCasting::Supported() ? 1 : 0, ParryMovingCasting::Applied() ? 1 : 0,
                      static_cast<unsigned long long>(ParryControls::events.Dropped()));
            gLog.Line("aim seq=%llu phase=snapshot mode=%s target=%p reason=%s angle=%.2f "
                      "scanned=%u eligible=%u tested=%u cameraBlocked=%u actorBlocked=%u ready=%d",
                      static_cast<unsigned long long>(gInputSequence), targetProbe.mode,
                      static_cast<void*>(target), targetProbe.reason, targetProbe.angle,
                      targetProbe.scanned, targetProbe.eligible, targetProbe.tested,
                      targetProbe.cameraBlocked, targetProbe.actorBlocked, gTargetReleaseReady ? 1 : 0);
        }
    }

} // anonymous namespace

// Standard Union DLL exports, called by engine event hooks in template.
cexport const char* ParryHotter_BuildId() {
    return "PH_BUILD_013_TEN_SLOTS_PROJECTILE_INPUT_CAST_ANIM";
}
cexport void Game_Entry() {}
cexport void Game_Init() {
    ParryCamera::Reset();
    gLog.Line("build PH_CAST_V5_BODY_AIM_AVADA_READY");
    ParryControls::captureReleaseTarget = CaptureReleaseTarget;
    gControlsLog.Reset();
    gLog.Line("hooks MagicMode=%p Invest=%p Cast=%p SpellStatus=%p",
              Gothic_I_Classic::Ivk_PH_MagicMode.GetSourceAddress(),
              Gothic_I_Classic::Ivk_PH_Invest.GetSourceAddress(),
              Gothic_I_Classic::Ivk_PH_Cast.GetSourceAddress(),
              Gothic_I_Classic::Ivk_PH_SpellStatus.GetSourceAddress());
    ParryControls::Reset();
    ParryCasting::Forget();
    gTelemetry.Reset();
    gDiagnosticDirty = true;
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    ParryControls::events.Reset(gController.Selected(), true);
    gCooldown.Reset();
    RefreshManaCosts();
}
cexport void Game_Exit() {
    ParryCamera::Reset();
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    if (gAiming || ParryWand::Owns(player)) LowerWand();
    ResetSelectedSpellForScript();
    SetUnionPresence(false);
    gCamera.Restore(zCAICamera::current);
    RemoveHud();
}
cexport void Game_PreLoop() {}
cexport void Game_Loop() { GameTick(); }
cexport void Game_PostLoop() {}
cexport void Game_MenuLoop() {
    ParryCamera::Reset();
    // Menus (options/controls included) must always see the player's own
    // key bindings, even if no gameplay input capture runs any more.
    ParryControls::ReleaseNativeBindings();
}
cexport void Game_SaveBegin() {
    ParryCamera::Reset();
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Exit casting mode before the game records its save. Both mouse
    // buttons must be released before re-arming on return.
    gController.Step({});
    if (gAiming || ParryWand::Owns(player)) LowerWand();
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
    ParryCamera::Reset();
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    if (gAiming || ParryWand::Owns(player)) LowerWand();
    ResetSelectedSpellForScript();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_NewGame() {
    ParryCamera::Reset();
    ParryControls::Forget();
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    ParryControls::events.Reset(gController.Selected(), true);
    gCooldown.Reset();
    RefreshManaCosts();
}
cexport void Game_LoadBegin_SaveGame() {
    ParryCamera::Reset();
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    if (gAiming || ParryWand::Owns(player)) LowerWand();
    ResetSelectedSpellForScript();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_SaveGame() {
    ParryCamera::Reset();
    ParryControls::Forget();
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    ParryControls::events.Reset(gController.Selected(), true);
    gCooldown.Reset();
    RefreshManaCosts();
}
cexport void Game_LoadBegin_ChangeLevel() {
    ParryCamera::Reset();
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // Drop any AddRef'd target BEFORE ZenGin frees the previous world.
    if (gAiming || ParryWand::Owns(player)) LowerWand();
    ResetSelectedSpellForScript();
    gCamera.Forget();
    RemoveHud();
}
cexport void Game_LoadEnd_ChangeLevel() {
    ParryCamera::Reset();
    ParryControls::Forget();
    ResetSelectedSpellForScript();
    SetUnionPresence(true);
    gController.ResetAfterInterruption();
    ParryControls::events.Reset(gController.Selected(), true);
    gCooldown.Reset();
    RefreshManaCosts();
}
cexport void Game_LoadBegin_Trigger() { ParryCamera::Reset(); }
cexport void Game_LoadEnd_Trigger() { ParryCamera::Reset(); }
cexport void Game_Pause() {
    ParryCamera::Reset();
    ParryControls::ReleaseNativeBindings();
    ParryControls::Disable();
    // The game loop may stop during pause and never call GameTick's
    // !gameplay branch. Cancel pending input here, too.
    gController.Step({});
    if (gAiming || ParryWand::Owns(player)) LowerWand();
    ResetSelectedSpellForScript();
    HideDiagnostics();
    gCamera.Restore(zCAICamera::current);
}
cexport void Game_Unpause() {}
cexport void Game_DefineExternals() {}
cexport void Game_ApplyOptions() {
    ParryCamera::Reset();
    if (gAiming || ParryWand::Owns(player)) LowerWand();
    RemoveHud(); // Recalculate pixel-square HUD geometry after a resize.
}
#endif
