// Gothic I adapter for the portable HP-style camera and locomotion intent.
// Include after the Union/Gothic headers. Native AI still owns gravity,
// animations, floor contact and collision response; this adapter supplies
// horizontal movement through BeginMovement/MoveWorld/EndMovement.
#pragma once
#include "Hp2Motion.hpp"
#include "MovingCasting.hpp"
#if __has_include("Locomotion.hpp")
#include "Locomotion.hpp"
#include "FreeCamera.hpp"
#include "InputEvents.hpp"
#else
#include "../hud/Locomotion.hpp"
#include "../hud/FreeCamera.hpp"
#include "../hud/InputEvents.hpp"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ParryControls {
using namespace Gothic_I_Classic;

struct InputSnapshot {
    float forward = 0.0f;
    float right = 0.0f;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool jumpHeld = false;
    bool jumpPressed = false;
    bool rightHeld = false;
    bool leftHeld = false;
    int slotPressed = 0; // 1..10; physical 0 maps to 10
};

inline InputSnapshot input;
inline bool enabled = true;
inline bool ownsInput = false;
// Set by the spell code while Gothic's magic AI invests or casts: it does
// nothing while the caster's walk animation runs.
inline bool castPose = false;
inline ParryHotter::InputEvents events;
inline void (*captureReleaseTarget)(std::uint64_t) = nullptr;
inline bool pendingSpaceJump = false;
inline bool previousSpace = false;
inline bool requireSpaceRelease = true;
// The engine input hook may not run during gameplay, so the player's DoAI
// captures input when no capture happened since its previous call. Exactly
// one capture per player AI step: a second one would find the number-key
// toggles already consumed and drop the selected slot.
inline bool capturedSinceAI = false;
inline unsigned inputHookCalls = 0;
inline unsigned fallbackCaptures = 0;
inline ParryHotter::FreeCamera orbit;
inline ParryHotter::Locomotion locomotion;
inline float planarX = 0.0f;
inline float planarZ = 0.0f;
inline zCAICamera* trackedCamera = nullptr;
inline float savedOffsetX = 0.0f;
inline float savedOffsetY = 0.0f;
inline float initialCameraYaw = 0.0f;
inline float initialCameraPitch = 0.0f;
inline float initialPlayerYaw = 0.0f;
inline float currentCameraYaw = 0.0f;
inline float currentCameraPitch = 0.0f; // degrees, positive looks up
inline bool mouseConsumed = false;
// Gothic polls DirectInput in every ProcessInputEvents call (thousands per
// second here): each poll resets the mouse delta and the key-toggle table.
// Mouse movement is therefore summed over polls until the camera uses it,
// and a number key stays latched until the spell HUD consumes it.
// A recent nearby floor contact can recover a small collision penetration.
// Expired or distant contacts never pull a falling actor across the world.
inline zVEC3 lastSafePosition;
inline bool hasSafePosition = false;
inline float safePositionAge = 0.0f;
inline unsigned floorRecoveries = 0;
inline float pendingMouseX = 0.0f;
inline float pendingMouseY = 0.0f;
inline int pendingSlot = 0;
inline oCAIHuman* movingAI = nullptr;
inline zCModel* scaledModel = nullptr;
inline float savedScaleX = 1.0f;
inline float savedScaleZ = 1.0f;
inline ParryHotter::LocomotionOutput pendingMove;
inline float lastDesiredX = 0.0f;
inline float lastDesiredZ = 0.0f;
inline float movementDelta = 0.0f;
inline Hp2Motion motion;
inline ParryHotter::PlanarFeedback movementFeedback;
inline bool resumedNativeFall = false;

// HP2 controls: LMB aims/casts, RMB jumps. Gothic I binds both mouse
// buttons to native actions (LMB = GAME_ACTION attack/take, RMB =
// GAME_SMOVE jump), and zeroing logical states after input processing was
// not enough in the real engine. While spells own LMB (any controllable
// state, F11 included) LMB is removed from every native binding; while
// our movement owns input, RMB is removed too and requests one native jump
// through Hp2Motion. The exact original lists are restored whenever the
// game regains control (menus, options, dialogue, inventory, save/load,
// exit), so the controls menu never sees or saves the reduced bindings.
struct MouseBindingGuard {
    static constexpr int kFirstAction = GAME_LEFT;
    static constexpr int kLastAction = 29; // GAME_LAME_HEAL
    std::array<std::vector<unsigned short>, kLastAction + 1> saved{};
    std::array<bool, kLastAction + 1> changed{};
    zCInput_Win32* owner = nullptr;
    bool left = false;
    bool right = false;

    bool Engaged() const { return owner != nullptr; }
    bool StripsLeft() const { return owner && left; }
    bool StripsRight() const { return owner && right; }

    void Engage(zCInput_Win32* in, bool stripLeft, bool stripRight) {
        if (!in || (!stripLeft && !stripRight)) { Release(); return; }
        if (owner == in && left == stripLeft && right == stripRight) return;
        Release();
        for (int logical = kFirstAction; logical <= kLastAction; ++logical) {
            const auto action = static_cast<unsigned short>(logical);
            const bool hasLeft = stripLeft && in->IsBinded(action, MOUSE_BUTTONLEFT);
            const bool hasRight = stripRight && in->IsBinded(action, MOUSE_BUTTONRIGHT);
            if (!hasLeft && !hasRight) continue;
            zCArray<unsigned short> current;
            in->GetBinding(action, current);
            zCArray<unsigned short> filtered;
            saved[logical].clear();
            for (int i = 0; i < current.GetNumInList(); ++i) {
                const unsigned short key = current[i];
                saved[logical].push_back(key);
                if ((stripLeft && key == MOUSE_BUTTONLEFT) ||
                    (stripRight && key == MOUSE_BUTTONRIGHT)) continue;
                filtered.InsertEnd(key);
            }
            in->Bind(action, filtered);
            changed[logical] = true;
        }
        owner = in;
        left = stripLeft;
        right = stripRight;
    }

    void Release() {
        if (owner) {
            for (int logical = kFirstAction; logical <= kLastAction; ++logical) {
                if (!changed[logical]) continue;
                zCArray<unsigned short> original;
                for (const auto key : saved[logical]) original.InsertEnd(key);
                owner->Bind(static_cast<unsigned short>(logical), original);
            }
        }
        changed.fill(false);
        for (auto& list : saved) list.clear();
        owner = nullptr;
        left = right = false;
    }
};
inline MouseBindingGuard mouseGuard;

// Gothic I also reads the mouse buttons directly in the player's movement
// AI (e.g. RMB + forward = jump), bypassing every binding. While the
// player's native AI runs, the input hooks report the buttons the guard
// owns as released. Spells and locomotion sample them earlier.
inline bool maskNativeLeft = false;
inline bool maskNativeRight = false;

// G1 SetState(action, 0) only clears the forced key-mapping state: GetState
// still falls back to the physically held keys. Mask movement at its actual
// read site, only while the player's native AI runs. MagicMode temporarily
// supplies ACTION+UP without feeding those keys to native locomotion.
inline bool nativePlayerInput = false;
inline bool nativeMagicInput = false;
inline bool nativeCastKeys = false; // GAME_UP: invest while held
inline bool nativeCastAction = false; // GAME_ACTION: remains down for release/rearm
inline bool nativeMagicRearm = false;

// G1 clears spellCastedLastFrame only with ACTION held and UP released
// (MagicMode 0x46D3A1), not when both keys are released. Keep them distinct.
inline void SetNativeMagicKeys(bool action, bool up) {
    nativeCastAction = action;
    nativeCastKeys = up;
}

struct NativePlayerInputScope {
    bool previous;
    explicit NativePlayerInputScope(bool active) : previous(nativePlayerInput) {
        nativePlayerInput = active;
    }
    ~NativePlayerInputScope() { nativePlayerInput = previous; }
    NativePlayerInputScope(const NativePlayerInputScope&) = delete;
    NativePlayerInputScope& operator=(const NativePlayerInputScope&) = delete;
};

struct NativeMagicInputScope {
    bool previous;
    explicit NativeMagicInputScope(bool active) : previous(nativeMagicInput) {
        nativeMagicInput = active;
    }
    ~NativeMagicInputScope() { nativeMagicInput = previous; }
    NativeMagicInputScope(const NativeMagicInputScope&) = delete;
    NativeMagicInputScope& operator=(const NativeMagicInputScope&) = delete;
};

// A newly prepared spell may need one native ACTION-only step to clear
// the previous cast latch, even when its own charge is already requested.
struct NativeMagicRearmScope {
    bool previous;
    explicit NativeMagicRearmScope(bool active) : previous(nativeMagicRearm) {
        nativeMagicRearm = active;
    }
    ~NativeMagicRearmScope() { nativeMagicRearm = previous; }
    NativeMagicRearmScope(const NativeMagicRearmScope&) = delete;
    NativeMagicRearmScope& operator=(const NativeMagicRearmScope&) = delete;
};

// Physical capture reads these keys before player AI. The native player AI
// must not also select a Gothic weapon/spell while our queued cast owns them.
// A nested NPC, a UI or a physical polling scope keeps its original keys.
inline bool BlocksNativeSpellHotkey(int key) {
    if (!nativePlayerInput) return false;
    constexpr int spellKeys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
                                 KEY_6, KEY_7, KEY_8, KEY_9, KEY_0};
    for (const int spellKey : spellKeys) if (key == spellKey) return true;
    return false;
}

inline bool OverrideNativeState(unsigned short action, float& value) {
    if (nativeMagicInput && (action == GAME_ACTION || action == GAME_UP)) {
        const bool down = action == GAME_ACTION
            ? (nativeMagicRearm || nativeCastAction)
            : (nativeCastKeys && !nativeMagicRearm);
        value = down ? 1.0f : 0.0f;
        return true;
    }
    if (!nativePlayerInput) return false;
    switch (action) {
    case GAME_UP: case GAME_DOWN: case GAME_LEFT: case GAME_RIGHT:
    case GAME_STRAFELEFT: case GAME_STRAFERIGHT: case GAME_SMOVE: case GAME_SMOVE2:
        value = 0.0f;
        return true;
    default:
        return false;
    }
}

// Native MagicMode also fires automatically at full charge. Keep the ready
// state hidden from that AI until the LMB release has captured a target.
// The actual spell status, invested mana and visual effect stay intact.
inline bool DeferNativeSpellCast(bool prepared, bool requested, int status) {
    return nativeMagicInput && prepared && !requested && status == SPL_STATUS_CAST;
}

// Some native spells reject a partial release even after investment began.
// BreathOfDeath (Avada) needs its native CAST status; its first free setup
// point alone is not enough. The engine still decides readiness and mana.
inline bool ShouldHoldNativeCast(bool requireCastStatus, bool confirmed,
                                 int status, int invested, bool releaseStarted = false) {
    if (confirmed || releaseStarted || status == SPL_STATUS_STOP) return false;
    if (status == SPL_STATUS_DONTINVEST) return true;
    return requireCastStatus ? status != SPL_STATUS_CAST : invested <= 0;
}

// Call from every lifecycle hook where the game takes control back.
inline void ReleaseNativeBindings() { mouseGuard.Release(); }

inline float FrameSeconds() {
    const float seconds = ztimer ? ztimer->frameTimeFloat * 0.001f : 0.0f;
    return std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 0.1f) : 0.0f;
}

inline float Yaw(zCVob* vob) {
    if (!vob) return 0.0f;
    const zVEC3 direction = vob->GetAtVectorWorld();
    return std::atan2(direction[0], direction[2]) * 57.29577951f;
}

// Shared gameplay safety gate for spells as well as movement. F11 only
// switches the movement/camera adapter; it must not change spell eligibility.
inline bool ConsoleVisible() {
    return zCConsole::cur_console && zCConsole::cur_console->IsVisible();
}

inline bool ContextAvailable() {
    if (ConsoleVisible()) return false;
    if (!ogame || !player || !zinput || ogame->IsOnPause() ||
        ogame->inLoadSaveGame || ogame->inLevelChange || player->openinventory ||
        player->IsDead() || !player->IsHuman() || player->IsMovLock()) return false;
    zCAICamera* camera = zCAICamera::current;
    if (!camera || !camera->camVob || camera->inCutsceneMode || camera->dialogCam ||
        camera->underWater) return false;
    if (!oCInformationManager::GetInformationManager().HasFinished()) return false;
    if (player->GetBodyState() &
        (BS_MOD_TRANSFORMED | BS_MOD_CONTROLLED | BS_MOD_CONTROLLING)) return false;
    const int state = player->GetBodyState() & BS_ONLY_STATE;
    // Relinquish control during ladders, swimming, hit reactions, interaction,
    // sitting, death, transformation and every other scripted movement mode.
    return state == BS_STAND || state == BS_WALK || state == BS_RUN ||
           state == BS_SPRINT || state == BS_JUMP || state == BS_FALL ||
           state == BS_CASTING;
}

inline bool Available() { return enabled && ContextAvailable(); }

inline void RestoreRootMotion() {
    // Old-world model pointers are compared only, never dereferenced.
    if (scaledModel && player && player->GetModel() == scaledModel) {
        scaledModel->aniTransScale[0] = savedScaleX;
        scaledModel->aniTransScale[2] = savedScaleZ;
    }
    scaledModel = nullptr;
    movingAI = nullptr;
    pendingMove = {};
}

inline void Disable() {
    motion.Reset(player);
    ParryMovingCasting::Restore(player ? player->GetModel() : nullptr);
    RestoreRootMotion();
    // A load can already have destroyed the previous camera. Restore only
    // when it is still the engine's live camera, otherwise forget it.
    if (trackedCamera && trackedCamera == zCAICamera::current) {
        trackedCamera->rotOffsetX = savedOffsetX;
        trackedCamera->rotOffsetY = savedOffsetY;
    }
    trackedCamera = nullptr;
    orbit.Reset();
    locomotion.Step({});
    input = {};
    planarX = planarZ = 0.0f;
    lastDesiredX = lastDesiredZ = 0.0f;
    movementFeedback = {};
    resumedNativeFall = false;
    hasSafePosition = false;
    safePositionAge = 0.0f;
    ownsInput = false;
    mouseConsumed = false;
    pendingMouseX = pendingMouseY = 0.0f;
    pendingSlot = 0;
}

// World load hooks must call Forget before freeing the old world if they
// cannot guarantee that its camera/model is still alive.
inline void Forget() {
    motion.Forget();
    ParryMovingCasting::Forget();
    scaledModel = nullptr;
    movingAI = nullptr;
    trackedCamera = nullptr;
    pendingMove = {};
    orbit.Reset();
    locomotion.Step({});
    input = {};
    planarX = planarZ = 0.0f;
    lastDesiredX = lastDesiredZ = 0.0f;
    movementFeedback = {};
    resumedNativeFall = false;
    hasSafePosition = false;
    safePositionAge = 0.0f;
    ownsInput = mouseConsumed = false;
    pendingMouseX = pendingMouseY = 0.0f;
    pendingSlot = 0;
    events.Reset(events.Selected(), true);
    pendingSpaceJump = previousSpace = false;
    requireSpaceRelease = true;
}

inline void Reset() { Disable(); enabled = true; }
inline const InputSnapshot& GetInput() { return input; }
// The spell HUD takes a latched number key exactly once.
inline int ConsumeSlotPressed() {
    const int slot = pendingSlot;
    pendingSlot = 0;
    input.slotPressed = 0;
    return slot;
}
inline bool OwnsInput() { return ownsInput && Available(); }

inline void CaptureInput(zCInput_Win32* controls) {
    capturedSinceAI = true;
    if (!controls) { ReleaseNativeBindings(); Disable(); return; }
    // Spells own LMB in both movement modes (F11 included) whenever the
    // player is in a controllable state; RMB is our jump only while our
    // movement owns input. Elsewhere the game's bindings rule.
    // F11 first, so bindings match the movement mode in the same frame.
    if (!ConsoleVisible() && controls->KeyToggled(KEY_F11)) {
        enabled = !enabled;
        Disable();
    }
    const bool context = ContextAvailable();
    const bool left = controls->GetMouseButtonPressedLeft() != 0;
    const bool right = controls->GetMouseButtonPressedRight() != 0;
    int spellSlot = 0;
    if (context) {
        constexpr int keys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
                                KEY_6, KEY_7, KEY_8, KEY_9, KEY_0};
        for (int i = 0; i < 10; ++i)
            if (controls->KeyToggled(keys[i])) { spellSlot = i + 1; break; }
    }
    events.Capture(context, left, right, spellSlot);
    if (events.LastLeftReleaseSequence() && captureReleaseTarget)
        captureReleaseTarget(events.LastLeftReleaseSequence());
    if (spellSlot) pendingSlot = spellSlot;
    const bool space = context && controls->KeyPressed(KEY_SPACE) != 0;
    if (!space) requireSpaceRelease = false;
    if (space && !previousSpace && !requireSpaceRelease) pendingSpaceJump = true;
    previousSpace = space;
    if (!context) { pendingSpaceJump = false; requireSpaceRelease = true; }
    if (context) mouseGuard.Engage(controls, true, enabled);
    else ReleaseNativeBindings();
    // Spells retain LMB ownership when F11 restores native movement.
    // Leave native action input untouched in dialogue and other unsafe states.
    if (ContextAvailable() && controls->GetMouseButtonPressedLeft())
        controls->SetState(GAME_ACTION, 0);
    if (!Available()) {
        // The HUD keeps its events in F11 mode, but our jump must not replay
        // an old RMB/Space tap when custom movement is enabled again.
        events.ConsumeRightPressed();
        pendingSpaceJump = false;
        Disable();
        return;
    }
    ownsInput = true;
    if (!trackedCamera) currentCameraYaw = Yaw(zCAICamera::current->camVob);
    input = {};
    mouseConsumed = false;
    input.forward = float(controls->KeyPressed(KEY_W) || controls->KeyPressed(KEY_UP)) -
                    float(controls->KeyPressed(KEY_S) || controls->KeyPressed(KEY_DOWN));
    input.right = float(controls->KeyPressed(KEY_D) || controls->KeyPressed(KEY_RIGHT)) -
                  float(controls->KeyPressed(KEY_A) || controls->KeyPressed(KEY_LEFT));
    // HP2: RMB jumps (no auto-jump); Space stays as a keyboard jump.
    input.jumpHeld = controls->KeyPressed(KEY_SPACE) != 0 ||
                     controls->GetMouseButtonPressedRight() != 0;
    input.rightHeld = controls->GetMouseButtonPressedRight() != 0;
    input.leftHeld = controls->GetMouseButtonPressedLeft() != 0;
    float wheel = 0.0f, mouseX = 0.0f, mouseY = 0.0f;
    controls->GetMousePos(mouseX, mouseY, wheel);
    if (std::isfinite(mouseX) && std::isfinite(mouseY)) {
        pendingMouseX = std::clamp(pendingMouseX + mouseX, -4000.0f, 4000.0f);
        pendingMouseY = std::clamp(pendingMouseY + mouseY, -4000.0f, 4000.0f);
    }
    input.mouseX = pendingMouseX;
    input.mouseY = pendingMouseY;
    input.slotPressed = pendingSlot;
    // Clear forced logical states. G1 still reads physical bindings when the
    // forced value is zero; OverrideNativeState masks those during native AI.
    constexpr unsigned short actions[] = {
        GAME_UP, GAME_DOWN, GAME_LEFT, GAME_RIGHT,
        GAME_STRAFELEFT, GAME_STRAFERIGHT, GAME_SMOVE, GAME_SMOVE2
    };
    for (const auto action : actions) controls->SetState(action, 0);
}

inline void UpdateCamera(zCAICamera* camera) {
    if (camera != zCAICamera::current) return;
    if (!OwnsInput() || !camera || !camera->camVob) {
        Disable();
        return;
    }
    if (trackedCamera != camera) {
        // Never restore fields on an earlier world/camera while switching.
        trackedCamera = camera;
        savedOffsetX = camera->rotOffsetX;
        savedOffsetY = camera->rotOffsetY;
        initialCameraYaw = Yaw(camera->camVob);
        const zVEC3 at = camera->camVob->GetAtVectorWorld();
        const float length = std::sqrt(at[0]*at[0] + at[1]*at[1] + at[2]*at[2]);
        initialCameraPitch = length > 0.001f
            ? std::asin(std::clamp(at[1] / length, -1.0f, 1.0f)) * 57.29577951f : 0.0f;
        initialPlayerYaw = Yaw(player);
        currentCameraYaw = initialCameraYaw;
        orbit.Reset();
    }
    const auto view = orbit.Step({true, FrameSeconds(),
        mouseConsumed ? 0.0f : input.mouseX,
        mouseConsumed ? 0.0f : input.mouseY,
        initialCameraYaw, initialCameraPitch,
        false}); // free look while moving (no pitch auto-centre)
    mouseConsumed = true;
    pendingMouseX = pendingMouseY = 0.0f;
    currentCameraYaw = view.yawDegrees;
    currentCameraPitch = view.pitchDegrees;
    // Offset changes steer the native collision-aware orbit rather than
    // teleporting the camera VOB or disabling its path/obstacle handling.
    camera->rotOffsetY = savedOffsetY +
        ParryHotter::WrapDegrees(view.yawDegrees - initialCameraYaw -
                               (Yaw(player) - initialPlayerYaw));
    camera->rotOffsetX = savedOffsetX - (view.pitchDegrees - initialCameraPitch);
}

inline void CaptureIfMissed(zCInput_Win32* controls) {
    if (!capturedSinceAI) {
        ++fallbackCaptures;
        CaptureInput(controls);
    }
    capturedSinceAI = false;
}

// Gothic's strafe is a one-shot step animation: restart it only after the
// previous step ended, never every frame.
inline void Strafe(oCAIHuman* ai, bool right) {
    zCModel* model = player ? player->GetModel() : nullptr;
    const int id = right ? ai->_t_strafer : ai->_t_strafel;
    zCModelAni* animation = model && id >= 0 ? model->GetAniFromAniID(id) : nullptr;
    if (animation && model->IsAniActive(animation)) return;
    if (right) ai->StrafeRight();
    else ai->StrafeLeft();
}

inline bool RecoverNearbyFloorPenetration(oCAIHuman* ai) {
    // Use elapsed wall frame time here: a long frame must expire this small
    // recovery window, even though locomotion clamps its simulation step.
    const float rawSeconds = ztimer ? ztimer->frameTimeFloat * 0.001f : 0.0f;
    safePositionAge += std::isfinite(rawSeconds) ? std::max(0.0f, rawSeconds) : 0.5f;
    if (safePositionAge > 0.5f) hasSafePosition = false;
    const int body = player->GetBodyState() & BS_ONLY_STATE;
    const bool groundBody = body == BS_STAND || body == BS_WALK ||
        body == BS_RUN || body == BS_SPRINT || body == BS_CASTING;
    if (!groundBody || motion.Active()) {
        hasSafePosition = false;
        return false;
    }
    const zVEC3 here = player->GetPositionWorld();
    const zVEC3 velocity = player->GetVelocity();
    if (ai->state == zCAIPlayer::zMV_STATE_STAND && ai->aboveFloor >= -10.0f &&
        ai->aboveFloor <= 20.0f && std::isfinite(velocity[1]) &&
        std::fabs(velocity[1]) <= 25.0f) {
        lastSafePosition = here;
        hasSafePosition = true;
        safePositionAge = 0.0f;
        return false;
    }
    if (!hasSafePosition || !std::isfinite(ai->aboveFloor) || ai->aboveFloor >= -45.0f)
        return false;
    const float horizontal = std::hypot(here[0] - lastSafePosition[0],
                                         here[2] - lastSafePosition[2]);
    const float dropped = lastSafePosition[1] - here[1];
    // Preserve native falls, knockback and scripted/external physics. The
    // recovery is only a one-off fix for a small non-physical floor overlap.
    const bool recover = !player->physicsEnabled && std::isfinite(horizontal) &&
        std::isfinite(dropped) && horizontal <= 100.0f &&
        dropped >= 45.0f && dropped <= 150.0f;
    hasSafePosition = false;
    if (!recover) return false;
    player->SetPositionWorld(lastSafePosition);
    planarX = planarZ = 0.0f;
    movementFeedback = {};
    motion.Reset(player);
    ParryMovingCasting::Restore(player->GetModel());
    ++floorRecoveries;
    return true;
}

inline void BeforeHumanAI(oCAIHuman* ai) {
    if (!ai || ai->npc != player) return;
    RestoreRootMotion();
    if (!OwnsInput()) { Disable(); return; }
    input.jumpPressed = events.ConsumeRightPressed() || pendingSpaceJump;
    pendingSpaceJump = false;
    if (ai->IsInWater() || ai->state == zCAIPlayer::zMV_STATE_SWIM ||
        ai->state == zCAIPlayer::zMV_STATE_DIVE) { Disable(); return; }
    movementDelta = FrameSeconds();
    resumedNativeFall = false;
    if (RecoverNearbyFloorPenetration(ai)) return;
    const bool grounded = ai->state == zCAIPlayer::zMV_STATE_STAND &&
                          ai->aboveFloor >= -8.0f && ai->aboveFloor <= 14.0f;
    pendingMove = locomotion.Step({true, movementDelta, currentCameraYaw,
        input.forward, input.right, planarX, planarZ,
        grounded, false, input.leftHeld}); // jump has one request owner below
    planarX = pendingMove.velocityX;
    planarZ = pendingMove.velocityZ;
    const bool jumpStarted = motion.Before(ai, player, true, movementDelta,
        input.jumpHeld, input.jumpPressed);
    // Native body velocity belongs to gravity, impacts and external launches;
    // planar command movement never erases those components.
    // grounded was sampled before takeoff. Do not immediately replace
    // its jump animation, or an invest/cast animation, with a ground command.
    if (grounded && !jumpStarted && !motion.Active() && !ai->IsInCastAni() &&
        (input.leftHeld || castPose)) {
        // Casting stance: stop the walk animation (Harry still moves).
        if (ai->IsWalking()) ai->_Stand();
    } else if (grounded && !jumpStarted && !motion.Active() && !ai->IsInCastAni()) {
        // Harry keeps facing the camera; the animation follows the key
        // direction (run, run backwards, strafe) instead of a moonwalk.
        if (std::fabs(input.forward) >= std::fabs(input.right) && input.forward > 0.0f)
            ai->PC_GoForward();
        else if (std::fabs(input.forward) >= std::fabs(input.right) && input.forward < 0.0f)
            ai->PC_GoBackward();
        else if (input.right != 0.0f) Strafe(ai, input.right > 0.0f);
        else if (!ai->IsStanding()) ai->_Stand();
    }
    movingAI = ai;
    scaledModel = player->GetModel();
    if (scaledModel) {
        savedScaleX = scaledModel->aniTransScale[0];
        savedScaleZ = scaledModel->aniTransScale[2];
        // Horizontal command travel is supplied once below. Hp2Motion only
        // suppresses root Y of its own jump, preserving native physics.
        scaledModel->aniTransScale[0] = 0.0f;
        scaledModel->aniTransScale[2] = 0.0f;
    }
}

inline void AfterHumanAI(oCAIHuman* ai) {
    if (ai && ai->npc == player) motion.After(ai, player);
    if (!ai || movingAI != ai) return;
    const auto move = pendingMove;
    lastDesiredX = move.velocityX;
    lastDesiredZ = move.velocityZ;
    RestoreRootMotion();
    if (!OwnsInput() || !move.active || ai->npc != player) return;
    if (move.hasFacing) {
        const float turn = std::clamp(
            ParryHotter::WrapDegrees(move.facingYawDegrees - Yaw(player)),
            -900.0f * movementDelta, 900.0f * movementDelta);
        player->BeginMovement();
        player->RotateWorldY(turn);
        player->EndMovement();
    }
    const float x = move.velocityX * movementDelta;
    const float z = move.velocityZ * movementDelta;
    const float distance = std::hypot(x, z);
    // Short collision-resolved steps handle corners without tunnelling at
    // a low frame rate. Gravity and vertical collision remain in native DoAI.
    const int steps = std::max(1, int(std::ceil(distance / 10.0f)));
    const zVEC3 beforePlanar = player->GetPositionWorld();
    for (int i = 0; i < steps; ++i) {
        player->BeginMovement();
        player->MoveWorld(x / steps, 0.0f, z / steps);
        player->EndMovement();
    }
    const zVEC3 afterPlanar = player->GetPositionWorld();
    movementFeedback = ParryHotter::ResolvePlanarFeedback(x, z,
        afterPlanar[0] - beforePlanar[0], afterPlanar[2] - beforePlanar[2], movementDelta);
    planarX = movementFeedback.velocityX;
    planarZ = movementFeedback.velocityZ;
    const bool casting = input.leftHeld || castPose || ai->IsInCastAni();
    resumedNativeFall = motion.ResumeNativeFall(ai, player, casting);
    const bool grounded = ai->state == zCAIPlayer::zMV_STATE_STAND &&
        ai->aboveFloor >= -8.0f && ai->aboveFloor <= 14.0f && !motion.Active() &&
        std::fabs(player->GetVelocity()[1]) <= 25.0f;
    ParryMovingCasting::Update(player->GetModel(), casting && grounded,
        movementFeedback.velocityX, movementFeedback.velocityZ, Yaw(player), movementDelta);
}
} // namespace ParryControls
