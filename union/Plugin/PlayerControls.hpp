// Gothic I adapter for the portable HP-style camera and locomotion intent.
// Include after the Union/Gothic headers. Native AI still owns gravity,
// animations, floor contact and collision response; this adapter supplies
// horizontal movement through BeginMovement/MoveWorld/EndMovement.
#pragma once
#if __has_include("Locomotion.hpp")
#include "Locomotion.hpp"
#include "FreeCamera.hpp"
#else
#include "../hud/Locomotion.hpp"
#include "../hud/FreeCamera.hpp"
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
    bool rightHeld = false;
    bool leftHeld = false;
    int slotPressed = 0; // 1..10; physical 0 maps to 10
};

inline InputSnapshot input;
inline bool enabled = true;
inline bool ownsInput = false;
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
// Last position where Harry stood on (not inside) the floor. If the engine
// lets him sink through the terrain he is put back instead of falling out
// of the world.
inline zVEC3 lastSafePosition;
inline bool hasSafePosition = false;
inline float pendingMouseX = 0.0f;
inline float pendingMouseY = 0.0f;
inline int pendingSlot = 0;
inline oCAIHuman* movingAI = nullptr;
inline zCModel* scaledModel = nullptr;
inline float savedScaleX = 1.0f;
inline float savedScaleZ = 1.0f;
inline ParryHotter::LocomotionOutput pendingMove;
inline float movementDelta = 0.0f;
inline float nativeJumpBuffer = 0.0f;
inline bool previousJump = false;
inline bool requireJumpRelease = false;

// HP2 controls: LMB aims/casts, RMB jumps. Gothic I binds both mouse
// buttons to native actions (LMB = GAME_ACTION attack/take, RMB =
// GAME_SMOVE jump), and zeroing logical states after input processing was
// not enough in the real engine. While spells own LMB (any controllable
// state, F11 included) LMB is removed from every native binding; while
// our movement owns input, RMB is removed too and jumps through the
// locomotion model. The exact original lists are restored whenever the
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
inline bool ContextAvailable() {
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
    planarX = planarZ = nativeJumpBuffer = 0.0f;
    previousJump = false;
    requireJumpRelease = true;
    ownsInput = false;
    mouseConsumed = false;
    pendingMouseX = pendingMouseY = 0.0f;
    pendingSlot = 0;
}

// World load hooks must call Forget before freeing the old world if they
// cannot guarantee that its camera/model is still alive.
inline void Forget() {
    scaledModel = nullptr;
    movingAI = nullptr;
    trackedCamera = nullptr;
    pendingMove = {};
    orbit.Reset();
    locomotion.Step({});
    input = {};
    planarX = planarZ = nativeJumpBuffer = 0.0f;
    hasSafePosition = false;
    previousJump = ownsInput = mouseConsumed = false;
    pendingMouseX = pendingMouseY = 0.0f;
    pendingSlot = 0;
    requireJumpRelease = true;
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
    if (controls->KeyToggled(KEY_F11)) {
        enabled = !enabled;
        Disable();
    }
    if (ContextAvailable()) mouseGuard.Engage(controls, true, enabled);
    else ReleaseNativeBindings();
    // Spells retain LMB ownership when F11 restores native movement.
    // Leave native action input untouched in dialogue and other unsafe states.
    if (ContextAvailable() && controls->GetMouseButtonPressedLeft())
        controls->SetState(GAME_ACTION, 0);
    if (!Available()) { Disable(); return; }
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
    constexpr int keys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
                            KEY_6, KEY_7, KEY_8, KEY_9, KEY_0};
    for (int i = 0; i < 10; ++i) {
        if (controls->KeyToggled(keys[i])) { pendingSlot = i + 1; break; }
    }
    input.slotPressed = pendingSlot;
    // Clear logical actions only. Physical keys/mouse remain readable to the
    // spell controller; inventory, dialogue and menu key buffers are untouched.
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

inline void BeforeHumanAI(oCAIHuman* ai) {
    if (!ai || ai->npc != player) return;
    RestoreRootMotion();
    if (!OwnsInput()) { Disable(); return; }
    if (ai->IsInWater() || ai->state == zCAIPlayer::zMV_STATE_SWIM ||
        ai->state == zCAIPlayer::zMV_STATE_DIVE) { Disable(); return; }
    movementDelta = FrameSeconds();
    if (ai->state == zCAIPlayer::zMV_STATE_STAND && ai->aboveFloor >= -10.0f &&
        ai->aboveFloor <= 20.0f) {
        lastSafePosition = player->GetPositionWorld();
        hasSafePosition = true;
    } else if (hasSafePosition && ai->aboveFloor < -45.0f) {
        // Feet far below the floor under him: he went through the ground.
        player->SetPositionWorld(lastSafePosition);
        planarX = planarZ = 0.0f;
        return;
    }
    const bool grounded = ai->state == zCAIPlayer::zMV_STATE_STAND &&
                          ai->aboveFloor >= -8.0f && ai->aboveFloor <= 14.0f;
    pendingMove = locomotion.Step({true, movementDelta, currentCameraYaw,
        input.forward, input.right, planarX, planarZ,
        grounded, input.jumpHeld, input.leftHeld});
    planarX = pendingMove.velocityX;
    planarZ = pendingMove.velocityZ;
    nativeJumpBuffer = std::max(0.0f, nativeJumpBuffer - movementDelta);
    if (!input.jumpHeld) requireJumpRelease = false;
    if (input.jumpHeld && !previousJump && !requireJumpRelease)
        nativeJumpBuffer = 0.15f;
    previousJump = input.jumpHeld;
    bool jumpStarted = false;
    if ((pendingMove.jump || nativeJumpBuffer > 0.0f) &&
        ai->CanJump(0)) {
        jumpStarted = ai->JumpForward() != 0;
        if (jumpStarted) nativeJumpBuffer = 0.0f;
    }
    // JumpForward can seed native horizontal ballistic velocity. Planar
    // travel has one owner here; leave its vertical impulse/gravity intact.
    ai->velocity[0] = ai->velocity[2] = 0.0f;
    if (player->physicsEnabled) {
        auto* body = player->GetRigidBody();
        if (body) body->SetVelocity(zVEC3(0.0f, body->v[1], 0.0f));
    }
    // grounded was sampled before JumpForward. Do not immediately replace
    // its jump animation, or an invest/cast animation, with a ground command.
    if (grounded && !jumpStarted && !input.leftHeld && !ai->IsInCastAni()) {
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
        // Native animations retain their vertical motion/gravity. Horizontal
        // travel is supplied once below, avoiding doubled running speed.
        scaledModel->aniTransScale[0] = 0.0f;
        scaledModel->aniTransScale[2] = 0.0f;
    }
}

inline void AfterHumanAI(oCAIHuman* ai) {
    if (!ai || movingAI != ai) return;
    const auto move = pendingMove;
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
    for (int i = 0; i < steps; ++i) {
        player->BeginMovement();
        player->MoveWorld(x / steps, 0.0f, z / steps);
        player->EndMovement();
    }
    // Gothic does not start a fall during a spell animation, so carrying
    // Harry over an edge while casting left him hanging in the air. Lower
    // him onto the floor beneath instead (at most a fall speed per frame).
    if ((input.leftHeld || ai->IsInCastAni()) &&
        ai->state == zCAIPlayer::zMV_STATE_STAND && ai->aboveFloor > 20.0f) {
        const float drop = std::min(ai->aboveFloor, 900.0f * movementDelta);
        player->BeginMovement();
        player->MoveWorld(0.0f, -drop, 0.0f);
        player->EndMovement();
    }
}
} // namespace ParryControls
