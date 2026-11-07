// Camera-relative movement intent, independent of Gothic/Union headers.
// Units: centimeters, seconds, degrees. The adapter must apply planar intent
// through the native collision-aware mover; jump is a native jump request,
// never a teleport or a replacement for engine gravity/vertical collision.
#pragma once
#include <algorithm>
#include <cmath>
#include "FreeCamera.hpp"

namespace ParryHotter {

struct LocomotionInput {
    bool enabled = false; // false in menus/dialogue/death/swimming/climbing
    float deltaSeconds = 1.0f / 60.0f;
    float cameraYawDegrees = 0.0f;
    float forward = 0.0f; // W positive, S negative
    float right = 0.0f; // D positive, A negative
    float velocityX = 0.0f;
    float velocityZ = 0.0f;
    bool grounded = false;
    bool jumpHeld = false;
    bool aiming = false;
};

struct LocomotionOutput {
    bool active = false;
    float velocityX = 0.0f;
    float velocityZ = 0.0f;
    bool hasFacing = false;
    float facingYawDegrees = 0.0f;
    bool jump = false;
};

class Locomotion final {
public:
    struct Settings {
        float runSpeed = 650.0f;
        float aimSpeed = 450.0f;
        float groundAcceleration = 4200.0f;
        float groundBraking = 5000.0f;
        float airAcceleration = 2600.0f;
        float airBraking = 120.0f;
        float coyoteSeconds = 0.12f;
        float jumpBufferSeconds = 0.15f;
    };

    Locomotion() = default;
    explicit Locomotion(Settings settings) : settings_(settings) {}

    LocomotionOutput Step(const LocomotionInput& input) {
        if (!input.enabled) {
            Reset();
            requireJumpRelease_ = true;
            return {};
        }
        const float dt = std::clamp(Finite(input.deltaSeconds), 0.0f, 0.1f);
        if (!input.jumpHeld) requireJumpRelease_ = false;
        if (!input.grounded) {
            // Do not allow a second coyote jump while the native mover is
            // still reporting contact during the takeoff frame.
            leftGroundAfterJump_ = true;
        } else if (leftGroundAfterJump_) {
            jumpConsumed_ = false;
            leftGroundAfterJump_ = false;
        }
        if (input.grounded && !jumpConsumed_)
            coyoteRemaining_ = settings_.coyoteSeconds;
        else
            coyoteRemaining_ = std::max(0.0f, coyoteRemaining_ - dt);

        jumpBufferRemaining_ = std::max(0.0f, jumpBufferRemaining_ - dt);
        if (input.jumpHeld && !previousJump_ && !requireJumpRelease_)
            jumpBufferRemaining_ = settings_.jumpBufferSeconds;
        previousJump_ = input.jumpHeld;

        float forward = std::clamp(Finite(input.forward), -1.0f, 1.0f);
        float right = std::clamp(Finite(input.right), -1.0f, 1.0f);
        const float magnitude = std::sqrt(forward * forward + right * right);
        if (magnitude > 1.0f) { forward /= magnitude; right /= magnitude; }
        const float yaw = Finite(input.cameraYawDegrees) * 0.01745329252f;
        const float speed = input.aiming ? settings_.aimSpeed : settings_.runSpeed;
        // +Z is camera forward at yaw=0; +X is camera right.
        const float desiredX = (forward * std::sin(yaw) + right * std::cos(yaw)) * speed;
        const float desiredZ = (forward * std::cos(yaw) - right * std::sin(yaw)) * speed;
        const bool hasInput = magnitude > 0.001f;
        const float acceleration = input.grounded
            ? (hasInput ? settings_.groundAcceleration : settings_.groundBraking)
            : (hasInput ? settings_.airAcceleration : settings_.airBraking);
        float velocityX = Finite(input.velocityX);
        float velocityZ = Finite(input.velocityZ);
        const float changeX = desiredX - velocityX;
        const float changeZ = desiredZ - velocityZ;
        const float change = std::sqrt(changeX * changeX + changeZ * changeZ);
        if (change > 0.0001f) {
            const float factor = std::min(1.0f, acceleration * dt / change);
            velocityX += changeX * factor;
            velocityZ += changeZ * factor;
        }
        const bool jump = jumpBufferRemaining_ > 0.0f &&
            coyoteRemaining_ > 0.0f && !jumpConsumed_;
        if (jump) {
            jumpBufferRemaining_ = 0.0f;
            coyoteRemaining_ = 0.0f;
            jumpConsumed_ = true;
        }
        const bool hasFacing = input.aiming || hasInput;
        const float facing = input.aiming ? Finite(input.cameraYawDegrees)
            : hasInput ? std::atan2(desiredX, desiredZ) * 57.29577951f : 0.0f;
        return {true, velocityX, velocityZ, hasFacing, WrapDegrees(facing), jump};
    }

    void Reset() {
        coyoteRemaining_ = jumpBufferRemaining_ = 0.0f;
        previousJump_ = jumpConsumed_ = leftGroundAfterJump_ = false;
        requireJumpRelease_ = false;
    }

private:
    static float Finite(float value) { return std::isfinite(value) ? value : 0.0f; }
    Settings settings_{};
    float coyoteRemaining_ = 0.0f;
    float jumpBufferRemaining_ = 0.0f;
    bool previousJump_ = false;
    bool jumpConsumed_ = false;
    bool leftGroundAfterJump_ = false;
    bool requireJumpRelease_ = false;
};
} // namespace ParryHotter
