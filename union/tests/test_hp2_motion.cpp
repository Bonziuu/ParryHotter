// Native-call contract doubles, not an SDK ABI or in-game physics test.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace Gothic_I_Classic {
struct zVEC3 {
    float v[3];
    zVEC3(float x = 0, float y = 0, float z = 0) : v{x,y,z} {}
    float& operator[](int i) { return v[i]; }
    float operator[](int i) const { return v[i]; }
};
enum zTRigidBodyMode { zPHY_RBMODE_FLY, zPHY_RBMODE_SLIDE };
struct zCRigidBody {
    zVEC3 v;
    bool gravityOn = true;
    float gravityScale = 1.0f;
    int sets = 0;
    void SetVelocity(zVEC3 value) { v = value; ++sets; }
    void SetMode(zTRigidBodyMode) {}
};
struct zCModelAni { int id = 0; };
struct zCModel {
    zVEC3 aniTransScale{1, 1.75f, 1};
    bool isFlying = false, allow = true;
    int active = -1;
    zCModelAni ani;
    zCModelAni* GetAniFromAniID(int id) { ani.id = id; return &ani; }
    int IsAniActive(zCModelAni* value) { return value->id == active; }
    void StartAni(int id, int) { if (allow) active = id; }
};
constexpr int BS_ONLY_STATE = 127, BS_STAND = 0, BS_WALK = 1, BS_RUN = 3,
    BS_SPRINT = 4, BS_JUMP = 8, BS_FALL = 10, BS_CASTING = 28,
    BS_MOD_CONTROLLED = 2048, BS_MOD_TRANSFORMED = 4096,
    BS_MOD_CONTROLLING = 8192;
struct oCNpc {
    bool physicsEnabled = false;
    zCRigidBody body;
    zCModel model;
    zVEC3 position, unphysicalVelocity;
    int state = BS_STAND;
    zCRigidBody* GetRigidBody() { return &body; }
    zCModel* GetModel() { return &model; }
    zVEC3 GetVelocity() { return physicsEnabled ? body.v : unphysicalVelocity; }
    zVEC3 GetPositionWorld() { return position; }
    int GetBodyState() { return state; }
    void SetBodyState(int value) { state = value; }
};
struct zCAIPlayer {
    enum State { zMV_STATE_STAND, zMV_STATE_FLY, zMV_STATE_SWIM, zMV_STATE_DIVE };
};
struct oCAIHuman {
    oCNpc* npc;
    zVEC3 velocity;
    int state = zCAIPlayer::zMV_STATE_STAND;
    float aboveFloor = 0;
    int _t_stand_2_jump = 10, _s_jump = 11;
    struct Config { float zMV_STEP_HEIGHT = 30.0f; } config;
    bool canJump = true, water = false;
    int launches = 0, fallAnis = 0;
    int CanJump(int) { return canJump; }
    bool IsInWater() { return water; }
    void StartFallDownAni() { ++fallAnis; npc->state = BS_FALL; }
    void StartPhysicsWithVel() {
        ++launches;
        npc->physicsEnabled = true;
        npc->body.SetVelocity(velocity);
    }
};
} // namespace Gothic_I_Classic

#include "../Plugin/Hp2Motion.hpp"
using namespace Gothic_I_Classic;
using ParryControls::Hp2Motion;
static bool close(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a-b) < epsilon;
}

static void contractFlight(float dt, float ceiling) {
    oCNpc npc;
    oCAIHuman ai;
    ai.npc = &npc;
    Hp2Motion motion;
    assert(motion.Before(&ai, &npc, true, dt, true, true));
    assert(npc.model.isFlying); // first AI tick cannot switch physics off
    assert(npc.model.aniTransScale[1] == 0.0f);
    assert(close(npc.body.v[1], std::sqrt(1962.0f * 70.0f)));
    motion.After(&ai, &npc);
    assert(close(npc.model.aniTransScale[1], 1.75f));

    // A small collision/ballistic double lets us verify the adapter never
    // reapplies upward velocity after a ceiling stops the initial impulse.
    // Gothic's actual integrator, animation order and collision mesh still
    // require the recorded in-game test; this is not their implementation.
    float peak = 0.0f, time = 0.0f;
    bool touchedCeiling = false, landed = false;
    for (int i = 0; i < int(3.0f / dt); ++i) {
        if (npc.physicsEnabled) {
            npc.position[1] += npc.body.v[1] * dt - 0.5f * 981.0f * dt * dt;
            npc.body.v[1] -= 981.0f * dt;
            if (npc.position[1] >= ceiling && npc.body.v[1] > 0) {
                npc.position[1] = ceiling;
                npc.body.v[1] = 0;
                touchedCeiling = true;
            }
            if (npc.position[1] <= 0) {
                npc.position[1] = 0;
                npc.body.v[1] = 0;
                npc.physicsEnabled = false;
                npc.state = BS_STAND;
                npc.model.active = -1;
                npc.model.isFlying = false;
                landed = true;
            }
        }
        ai.aboveFloor = npc.position[1];
        ai.state = landed ? zCAIPlayer::zMV_STATE_STAND : zCAIPlayer::zMV_STATE_FLY;
        time += dt;
        peak = std::max(peak, npc.position[1]);
        // Even malformed repeated held-press notifications cannot renew it.
        assert(!motion.Before(&ai, &npc, true, dt, true, true));
        if (touchedCeiling) assert(npc.body.v[1] <= 0.0f);
        motion.After(&ai, &npc);
        if (landed && !motion.Active()) break;
    }
    assert(landed && !motion.Active());
    assert(ai.launches == 1 && npc.body.sets == 1);
    assert(peak <= ceiling + 0.01f);
    if (ceiling > 70.0f) {
        assert(peak > 69.0f && peak <= 70.01f);
        assert(time > 0.70f && time < 0.85f);
    }
    // Release + a fresh edge is a new jump; remaining on ground with a held
    // button never creates another impulse, including immediate roof contact.
    motion.Before(&ai, &npc, true, dt, false, false);
    motion.After(&ai, &npc);
    assert(motion.Before(&ai, &npc, true, dt, true, true));
    motion.After(&ai, &npc);
    assert(ai.launches == 2);
}

int main() {
    for (float fps : {30.0f, 60.0f, 144.0f}) {
        contractFlight(1.0f / fps, 1000.0f);
        contractFlight(1.0f / fps, 18.0f);
        contractFlight(1.0f / fps, 0.0f);
    }
    oCNpc npc;
    oCAIHuman ai;
    ai.npc = &npc;
    Hp2Motion motion;
    // A short tap is retained after its button has already been released.
    ai.canJump = false;
    assert(!motion.Before(&ai, &npc, true, 0.016f, false, true));
    assert(motion.Telemetry().attempted);
    ai.canJump = true;
    assert(motion.Before(&ai, &npc, true, 0.016f, false, false));
    motion.After(&ai, &npc);
    assert(ai.launches == 1);

    // Native refusal is retried only within the one 150 ms buffer.
    motion.Reset(&npc);
    npc.physicsEnabled = false;
    npc.state = BS_STAND;
    ai.canJump = false;
    motion.Before(&ai, &npc, true, 0.016f, false, true);
    motion.Before(&ai, &npc, true, 0.16f, false, false);
    ai.canJump = true;
    assert(!motion.Before(&ai, &npc, true, 0.016f, false, false));
    // An externally launched actor keeps every component of native velocity.
    npc.physicsEnabled = true;
    npc.body.v = {123.0f, 250.0f, -87.0f};
    assert(!motion.Before(&ai, &npc, true, 0.016f, false, true));
    assert(close(npc.body.v[0], 123) && close(npc.body.v[1], 250) &&
           close(npc.body.v[2], -87));
    assert(!motion.ResumeNativeFall(&ai, &npc, true));
    // Standing contact may have physics enabled: no false hard requirement.
    motion.Reset(&npc);
    npc.body.v = {10, 0, -20};
    assert(motion.Before(&ai, &npc, true, 0.016f, false, true));
    assert(close(npc.body.v[0], 10) && close(npc.body.v[2], -20));
    motion.After(&ai, &npc);
    // Hit reactions relinquish root Y and never cancel the native impulse.
    npc.state = 7; // a native state outside the controls' allowed set
    npc.body.v = {12, -42, 19};
    assert(!motion.Before(&ai, &npc, true, 0.016f, false, false));
    assert(motion.Telemetry().interrupted && !motion.Active());
    assert(close(npc.body.v[1], -42));
    assert(close(npc.model.aniTransScale[1], 1.75f));

    // Walking over an edge in a casting pose resumes the engine's fall once.
    npc.state = BS_CASTING;
    npc.physicsEnabled = false;
    npc.unphysicalVelocity = {9, -5, -4};
    ai.aboveFloor = 100;
    assert(motion.ResumeNativeFall(&ai, &npc, true));
    assert(ai.fallAnis == 1 && close(npc.body.v[1], -5));
    assert(close(npc.body.v[0], 9) && close(npc.body.v[2], -4));
    assert(!motion.ResumeNativeFall(&ai, &npc, true));

    const auto wall = ParryHotter::ResolvePlanarFeedback(6, 8, 0, 8, 0.02f);
    assert(wall.blocked && close(wall.velocityX, 0) && close(wall.velocityZ, 400));
    const auto corner = ParryHotter::ResolvePlanarFeedback(6, 8, 0, 0, 0.02f);
    assert(corner.blocked && corner.velocityX == 0 && corner.velocityZ == 0);
    const auto correction = ParryHotter::ResolvePlanarFeedback(6, 8, 10, 10, 0.02f);
    assert(close(std::hypot(correction.velocityX, correction.velocityZ), 500));
    for (float fps : {30.0f, 60.0f, 144.0f}) {
        const float dt = 1.0f / fps;
        const auto out = ParryHotter::ResolvePlanarFeedback(0, 540*dt, 0, 270*dt, dt);
        assert(close(out.velocityZ, 270));
    }
    assert(ParryHotter::ResolvePlanarFeedback(1, 1, 1, 1, 0).velocityX == 0);
    std::cout << "Parry Hotter single native low jump, ceiling/hold/retry and collision feedback: OK\n";
}
