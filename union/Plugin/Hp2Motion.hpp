// Gothic I 1.08k native physics adapter. Include after the Union SDK.
#pragma once
#if __has_include("MotionIntent.hpp")
#include "MotionIntent.hpp"
#else
#include "../hud/MotionIntent.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ParryControls {
using namespace Gothic_I_Classic;

struct JumpTelemetry {
    // New/pending request, current flight, and flight ended this frame are
    // separate IDs: landing and a fresh accepted press can share one frame.
    std::uint64_t requestId = 0;
    std::uint64_t activeId = 0;
    std::uint64_t endedId = 0;
    bool requested = false;
    bool ignored = false;
    bool attempted = false;
    bool accepted = false;
    bool landed = false;
    bool interrupted = false;
    bool active = false;
    bool physics = false;
    float velocityY = 0.0f;
    float aboveFloor = 0.0f;
    float age = 0.0f;
    float peakHeight = 0.0f;
    float endedAge = 0.0f;
    float endedPeakHeight = 0.0f;
    float requestRemaining = 0.0f;
    const char* reason = "idle";
};

class Hp2Motion final {
public:
    // Tuned low jump, not a measurement of HP2. Gothic's native 981 cm/s²
    // gives a 70 cm jump about 0.76 s of flight on a level floor.
    static constexpr float JumpHeightCm = 70.0f;

    bool Before(oCAIHuman* ai, oCNpc* npc, bool enabled, float dt,
                bool held, bool pressed) {
        Restore(npc);
        if (!enabled || !ai || !npc || ai->npc != npc) {
            Reset(npc);
            frame_.reason = "context";
            return false;
        }
        frame_ = {};
        const float seconds = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
        Observe(ai, npc, seconds);
        const bool requested = intent_.Update(true, seconds, held, pressed);
        if (requested) {
            pendingId_ = ++nextRequestId_;
            frame_.requested = true;
            frame_.requestId = pendingId_;
            if (active_) {
                // A new click during a flight is observable but cannot
                // replace its identity or queue an automatic landing jump.
                frame_.ignored = true;
                frame_.reason = "active_jump";
                intent_.Accepted();
                pendingId_ = 0;
            }
        }
        if (!intent_.Pending()) pendingId_ = 0;
        bool accepted = false;
        if (intent_.Pending() && !active_) {
            frame_.requestId = pendingId_;
            frame_.attempted = true;
            accepted = TryLaunch(ai, npc);
            if (accepted) {
                intent_.Accepted();
                pendingId_ = 0;
            }
        }
        // Root translation is suppressed only on our own jump animation.
        // A hit animation, fall, external launch or scripted action retains Y.
        zCModel* mdl = npc->GetModel();
        if (active_ && mdl && (AnimationActive(mdl, ai->_t_stand_2_jump) ||
                              AnimationActive(mdl, ai->_s_jump))) {
            scaled_ = mdl;
            savedY_ = mdl->aniTransScale[1];
            mdl->aniTransScale[1] = 0.0f;
        }
        Sample(ai, npc);
        return accepted;
    }

    void After(oCAIHuman* ai, oCNpc* npc) {
        Restore(npc);
        if (ai && npc && ai->npc == npc) Sample(ai, npc);
    }

    // Call before discarding a live actor; Forget() is safe after world teardown.
    void Reset(oCNpc* npc) {
        Restore(npc);
        if (active_) {
            frame_ = {};
            frame_.interrupted = true;
            frame_.endedId = activeId_;
            frame_.endedAge = age_;
            frame_.endedPeakHeight = peak_;
            frame_.reason = "context";
        } else if (!frame_.interrupted) {
            frame_ = {};
            frame_.reason = "context";
        }
        intent_.Reset();
        active_ = leftGround_ = false;
        pendingId_ = activeId_ = 0;
        frame_.active = false;
        frame_.activeId = 0;
        frame_.requestRemaining = 0.0f;
        tracked_ = nullptr;
        trackedModel_ = nullptr;
        age_ = peak_ = 0.0f;
    }
    void Forget() {
        scaled_ = nullptr;
        Reset(nullptr);
    }
    bool Active() const { return active_; }
    const JumpTelemetry& Telemetry() const { return frame_; }

    // Replace the downward MoveWorld hack while a magic pose blocks falling.
    // The next native physics tick moves the actor; no position correction,
    // repeated acceleration or altered gravity is introduced here.
    bool ResumeNativeFall(oCAIHuman* ai, oCNpc* npc, bool magicPose) {
        if (!magicPose || !ai || !npc || ai->npc != npc || active_ ||
            npc->physicsEnabled || ai->IsInWater() || !BodyAvailable(npc) ||
            ai->state != zCAIPlayer::zMV_STATE_STAND ||
            !std::isfinite(ai->aboveFloor) ||
            ai->aboveFloor <= std::max(20.0f, ai->config.zMV_STEP_HEIGHT)) return false;
        const zVEC3 velocity = npc->GetVelocity();
        if (!Finite(velocity) || velocity[1] > 0.0f) return false;
        zCRigidBody* body = npc->GetRigidBody();
        if (!body || !body->gravityOn) return false;
        ai->StartFallDownAni(); // native virtual dispatch, no hook involved
        ai->velocity = velocity;
        ai->StartPhysicsWithVel();
        body->SetMode(zPHY_RBMODE_FLY);
        return npc->physicsEnabled != 0;
    }

private:
    static bool Finite(const zVEC3& velocity) {
        return std::isfinite(velocity[0]) && std::isfinite(velocity[1]) &&
               std::isfinite(velocity[2]);
    }
    static bool AnimationActive(zCModel* mdl, int id) {
        zCModelAni* ani = mdl && id >= 0 ? mdl->GetAniFromAniID(id) : nullptr;
        return ani && mdl->IsAniActive(ani);
    }
    static bool Contact(oCAIHuman* ai) {
        return ai->state == zCAIPlayer::zMV_STATE_STAND &&
               std::isfinite(ai->aboveFloor) &&
               ai->aboveFloor >= -8.0f && ai->aboveFloor <= 14.0f;
    }
    static bool BodyAvailable(oCNpc* npc) {
        const int body = npc->GetBodyState();
        if (body & (BS_MOD_TRANSFORMED | BS_MOD_CONTROLLED | BS_MOD_CONTROLLING))
            return false;
        const int state = body & BS_ONLY_STATE;
        return state == BS_STAND || state == BS_WALK || state == BS_RUN ||
               state == BS_SPRINT || state == BS_JUMP || state == BS_FALL ||
               state == BS_CASTING;
    }
    void Restore(oCNpc* npc) {
        // Compare a possibly stale world pointer without dereferencing it.
        if (scaled_ && npc && npc->GetModel() == scaled_)
            scaled_->aniTransScale[1] = savedY_;
        scaled_ = nullptr;
    }
    void Observe(oCAIHuman* ai, oCNpc* npc, float dt) {
        if (!active_) return;
        if (tracked_ != npc || trackedModel_ != npc->GetModel() ||
            !BodyAvailable(npc) || ai->IsInWater()) {
            active_ = false;
            frame_.interrupted = true;
            EndFlight();
            frame_.reason = "native_action";
            return;
        }
        age_ += dt;
        const float rise = npc->GetPositionWorld()[1] - startY_;
        if (std::isfinite(rise)) peak_ = std::max(peak_, rise);
        const zVEC3 velocity = npc->GetVelocity();
        if (!Contact(ai)) leftGround_ = true;
        // Native landing can happen immediately beneath a low ceiling. Do
        // not require an airborne sample, and never relaunch on held RMB.
        if (age_ >= 0.10f && Contact(ai) && velocity[1] <= 1.0f &&
            (leftGround_ || age_ >= 0.20f)) {
            active_ = false;
            frame_.landed = true;
            EndFlight();
            frame_.reason = "landed";
        }
    }
    void EndFlight() {
        frame_.endedId = activeId_;
        frame_.endedAge = age_;
        frame_.endedPeakHeight = peak_;
        activeId_ = 0;
    }
    bool TryLaunch(oCAIHuman* ai, oCNpc* npc) {
        frame_.reason = "no_contact";
        if (!Contact(ai) || !BodyAvailable(npc) || ai->IsInWater()) return false;
        const zVEC3 velocity = npc->GetVelocity();
        frame_.reason = "vertical_motion";
        if (!Finite(velocity) || std::fabs(velocity[1]) > 25.0f) return false;
        frame_.reason = "animation_unavailable";
        zCModel* mdl = npc->GetModel();
        // CanJump(0) only checks the animation ID in G1; contact is checked
        // above. JumpForward also chooses ledge climbing, so use its ordinary
        // jump animation directly, keeping native landing and collision.
        if (!mdl || !ai->CanJump(0) || ai->_t_stand_2_jump < 0 || ai->_s_jump < 0)
            return false;
        zCRigidBody* body = npc->GetRigidBody();
        frame_.reason = "native_gravity_disabled";
        if (!body || !body->gravityOn || !std::isfinite(body->gravityScale) ||
            std::fabs(body->gravityScale - 1.0f) > 0.01f) return false;
        mdl->StartAni(ai->_t_stand_2_jump, 0);
        if (!AnimationActive(mdl, ai->_t_stand_2_jump)) {
            frame_.reason = "animation_rejected";
            return false;
        }
        // StartAni does not refresh isFlying until AdvanceAnis. CheckPhysics
        // at the start of this same AI call would otherwise switch the new
        // physics off while feet still touch the floor. Synchronize only at
        // takeoff; AdvanceAnis thereafter derives this flag from native poses.
        mdl->isFlying = 1;
        npc->SetBodyState(BS_JUMP);
        ai->velocity = velocity; // retain existing X/Z, never zero an impulse
        ai->velocity[1] = ParryHotter::LowJumpLaunchSpeed(JumpHeightCm);
        // 0x5021F0 enables native physics then seeds body velocity exactly
        // once. CheckJump(0x62BCD0) reseeds ONLY when physics is disabled, so
        // it cannot add a second animation impulse after this launch.
        ai->StartPhysicsWithVel();
        body->SetMode(zPHY_RBMODE_FLY);
        if (!npc->physicsEnabled) {
            frame_.reason = "native_physics_rejected";
            return false;
        }
        active_ = true;
        activeId_ = pendingId_;
        leftGround_ = false;
        age_ = peak_ = 0.0f;
        startY_ = npc->GetPositionWorld()[1];
        tracked_ = npc;
        trackedModel_ = mdl;
        frame_.accepted = true;
        frame_.reason = "accepted";
        return true;
    }
    void Sample(oCAIHuman* ai, oCNpc* npc) {
        frame_.active = active_;
        frame_.activeId = activeId_;
        if (!frame_.requestId && intent_.Pending()) frame_.requestId = pendingId_;
        frame_.physics = npc->physicsEnabled != 0;
        frame_.velocityY = npc->GetVelocity()[1];
        frame_.aboveFloor = ai->aboveFloor;
        frame_.age = age_;
        frame_.peakHeight = peak_;
        frame_.requestRemaining = intent_.Remaining();
    }

    ParryHotter::JumpIntent intent_;
    JumpTelemetry frame_;
    // Diagnostic identities remain monotonic through F11/menu/world resets.
    std::uint64_t nextRequestId_ = 0;
    std::uint64_t pendingId_ = 0;
    std::uint64_t activeId_ = 0;
    oCNpc* tracked_ = nullptr;
    zCModel* trackedModel_ = nullptr;
    zCModel* scaled_ = nullptr;
    float savedY_ = 1.0f;
    float startY_ = 0.0f;
    float age_ = 0.0f;
    float peak_ = 0.0f;
    bool active_ = false;
    bool leftGround_ = false;
};
} // namespace ParryControls
