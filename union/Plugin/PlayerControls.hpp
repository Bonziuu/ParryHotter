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
inline bool mouseConsumed = false;
inline oCAIHuman* movingAI = nullptr;
inline zCModel* scaledModel = nullptr;
inline float savedScaleX = 1.0f;
inline float savedScaleZ = 1.0f;
inline ParryHotter::LocomotionOutput pendingMove;
inline float movementDelta = 0.0f;
inline float nativeJumpBuffer = 0.0f;
inline bool previousJump = false;
inline bool requireJumpRelease = false;

// Gothic I binds RMB to native actions (GAME_SMOVE = jump). Zeroing the
// logical state after input processing was not enough in the real engine:
// RMB still made the hero jump and moved the camera. While spells own RMB,
// RMB is removed from every native binding; the exact original lists are
// restored whenever the game regains control (menus, options, dialogue,
// inventory, save/load, exit), so the controls menu never sees or saves
// the reduced bindings.
struct RmbBindingGuard {
    static constexpr int kFirstAction = GAME_LEFT;
    static constexpr int kLastAction = 29; // GAME_LAME_HEAL
    std::array<std::vector<unsigned short>, kLastAction + 1> saved{};
    std::array<bool, kLastAction + 1> changed{};
    zCInput_Win32* owner = nullptr;

    bool Engaged() const { return owner != nullptr; }

    void Engage(zCInput_Win32* in) {
        if (!in || owner == in) return;
        Release();
        for (int logical = kFirstAction; logical <= kLastAction; ++logical) {
            const auto action = static_cast<unsigned short>(logical);
            if (!in->IsBinded(action, MOUSE_BUTTONRIGHT)) continue;
            zCArray<unsigned short> current;
            in->GetBinding(action, current);
            zCArray<unsigned short> filtered;
            saved[logical].clear();
            for (int i = 0; i < current.GetNumInList(); ++i) {
                saved[logical].push_back(current[i]);
                if (current[i] != MOUSE_BUTTONRIGHT) filtered.InsertEnd(current[i]);
            }
            in->Bind(action, filtered);
            changed[logical] = true;
        }
        owner = in;
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
    }
};
inline RmbBindingGuard rmbGuard;

// Call from every lifecycle hook where the game takes control back.
inline void ReleaseNativeBindings() { rmbGuard.Release(); }

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
    previousJump = ownsInput = mouseConsumed = false;
    requireJumpRelease = true;
}

inline void Reset() { Disable(); enabled = true; }
inline const InputSnapshot& GetInput() { return input; }
inline bool OwnsInput() { return ownsInput && Available(); }

inline void CaptureInput(zCInput_Win32* controls) {
    if (!controls) { ReleaseNativeBindings(); Disable(); return; }
    // Spells own RMB in both movement modes (F11 included) whenever the
    // player is in a controllable state; elsewhere the game's bindings rule.
    if (ContextAvailable()) rmbGuard.Engage(controls);
    else ReleaseNativeBindings();
    if (controls->KeyToggled(KEY_F11)) {
        enabled = !enabled;
        Disable();
    }
    // Spells retain RMB/LMB ownership when F11 restores native movement.
    // Leave native action input untouched in dialogue and other unsafe states.
    if (ContextAvailable() && controls->GetMouseButtonPressedRight())
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
    input.jumpHeld = controls->KeyPressed(KEY_SPACE) != 0;
    input.rightHeld = controls->GetMouseButtonPressedRight() != 0;
    input.leftHeld = controls->GetMouseButtonPressedLeft() != 0;
    float wheel = 0.0f;
    controls->GetMousePos(input.mouseX, input.mouseY, wheel);
    constexpr int keys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
                            KEY_6, KEY_7, KEY_8, KEY_9, KEY_0};
    for (int i = 0; i < 10; ++i) {
        if (controls->KeyToggled(keys[i])) { input.slotPressed = i + 1; break; }
    }
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
        initialCameraYaw, initialCameraPitch});
    mouseConsumed = true;
    currentCameraYaw = view.yawDegrees;
    // Offset changes steer the native collision-aware orbit rather than
    // teleporting the camera VOB or disabling its path/obstacle handling.
    camera->rotOffsetY = savedOffsetY +
        ParryHotter::WrapDegrees(view.yawDegrees - initialCameraYaw -
                               (Yaw(player) - initialPlayerYaw));
    camera->rotOffsetX = savedOffsetX - (view.pitchDegrees - initialCameraPitch);
}

inline void BeforeHumanAI(oCAIHuman* ai) {
    if (!ai || ai->npc != player) return;
    RestoreRootMotion();
    if (!OwnsInput()) { Disable(); return; }
    if (ai->IsInWater() || ai->state == zCAIPlayer::zMV_STATE_SWIM ||
        ai->state == zCAIPlayer::zMV_STATE_DIVE) { Disable(); return; }
    movementDelta = FrameSeconds();
    const bool grounded = ai->state == zCAIPlayer::zMV_STATE_STAND &&
                          ai->aboveFloor >= -8.0f && ai->aboveFloor <= 14.0f;
    pendingMove = locomotion.Step({true, movementDelta, currentCameraYaw,
        input.forward, input.right, planarX, planarZ,
        grounded, input.jumpHeld, input.rightHeld});
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
    if (grounded && !jumpStarted && !input.rightHeld && !ai->IsInCastAni()) {
        if (std::hypot(input.forward, input.right) > 0.001f) ai->PC_GoForward();
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
}
} // namespace ParryControls
