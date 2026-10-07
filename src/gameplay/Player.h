#pragma once

#include "gameplay/Aabb.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <vector>

namespace mc {

// Movement intent for one tick, already decoded from keys (no GLFW here).
struct PlayerInput {
    float forward = 0.0f; // +1 W, -1 S
    float strafe = 0.0f;  // +1 D (right), -1 A
    bool jump = false;    // space: jump, or fly up
    int jumpPresses = 0;  // space presses since the last tick (double-tap detection)
    bool sneak = false;   // shift: sneak, or fly down
    bool sprint = false;  // ctrl
};

// The local player: vanilla movement physics at 20 TPS. Constants per tick, from
// the wiki (Player, Sprinting, Sneaking, Flying, Jumping) and the MCPK wiki's public
// write-up of the movement formulas; the tests check the resulting wiki speeds.
class Player {
public:
    // Dimensions (wiki: Player). Sneaking lowers the box and eye since 1.14.
    static constexpr double kWidth = 0.6;
    static constexpr double kHeight = 1.8;
    static constexpr double kSneakHeight = 1.5;
    static constexpr double kEyeHeight = 1.62;
    static constexpr double kSneakEyeHeight = 1.27;
    static constexpr double kStepHeight = 0.6;
    // Movement constants (blocks per tick).
    static constexpr double kGravity = 0.08;
    static constexpr double kVerticalDrag = 0.98;
    static constexpr double kWalkSpeed = 0.1; // movement_speed attribute
    static constexpr double kAirAccel = 0.02;
    static constexpr double kSprintFactor = 1.3;
    static constexpr double kSneakFactor = 0.3;
    static constexpr double kInputScale = 0.98;        // vanilla scales WASD input by 0.98
    static constexpr double kGroundSlipperiness = 0.6; // default block slipperiness
    static constexpr double kAirFriction = 0.91;
    static constexpr double kJumpVelocity = 0.42;
    static constexpr double kSprintJumpBoost = 0.2;
    static constexpr double kFlySpeed = 0.05;    // abilities.flyingSpeed
    static constexpr double kFlyVertical = 0.15; // 3 x flying speed
    static constexpr double kFlyVerticalDamping = 0.6;
    static constexpr int kDoubleTapTicks = 7;

    void setPosition(const glm::dvec3& feet);
    void setRotation(float yawDeg, float pitchDeg);
    void setCreative(bool creative) { m_creative = creative; }
    void setFlying(bool flying) { m_flying = flying && m_creative; }
    // Benchmarks only (--auto-fly): scales flight acceleration.
    void setFlySpeedMultiplier(double k) { m_flyMultiplier = k; }

    // One game tick. Does nothing while the player's chunk isn't loaded (vanilla
    // keeps the player still until the terrain arrives).
    void tick(const world::World& world, const PlayerInput& input);

    // Mouse movement in screen pixels; sensitivity 0..1 (vanilla default 0.5).
    void turn(double dxPixels, double dyPixels, double sensitivity = 0.5);

    // Feet position between the previous and current tick (alpha 0..1).
    glm::dvec3 renderPosition(double alpha) const;
    glm::dvec3 eyePosition(double alpha) const {
        return renderPosition(alpha) + glm::dvec3(0, eyeHeight(), 0);
    }

    const glm::dvec3& position() const { return m_pos; }
    const glm::dvec3& velocity() const { return m_velocity; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    bool onGround() const { return m_onGround; }
    bool flying() const { return m_flying; }
    bool sprinting() const { return m_sprinting; }
    bool sneaking() const { return m_sneaking; }
    double eyeHeight() const { return m_sneaking ? kSneakEyeHeight : kEyeHeight; }
    Aabb box() const { return Aabb::fromFeet(m_pos, kWidth, m_sneaking ? kSneakHeight : kHeight); }

private:
    // Moves by `delta` with collision (vanilla Entity.move): returns the actual motion.
    glm::dvec3 move(const world::World& world, glm::dvec3 delta);
    glm::dvec3 collide(const world::World& world, const Aabb& box, const glm::dvec3& delta);
    void gatherBoxes(const world::World& world, const Aabb& region);
    bool hasGroundBelow(const world::World& world, const Aabb& box);
    glm::dvec3 backOffFromEdge(const world::World& world, glm::dvec3 delta);

    glm::dvec3 m_prevPos{0.0};
    glm::dvec3 m_pos{0.0};
    glm::dvec3 m_velocity{0.0};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_onGround = false;
    bool m_creative = true;
    bool m_flying = false;
    bool m_sprinting = false;
    bool m_sneaking = false;
    int m_ticksSinceJumpPress = 1000;
    double m_flyMultiplier = 1.0;
    std::vector<Aabb> m_boxes; // reused collision box buffer (reserved: no tick allocation)

public:
    Player() { m_boxes.reserve(1024); }
};

// Vanilla mouse look: degrees of rotation per pixel for sensitivity 0..1.
double degreesPerPixel(double sensitivity);

} // namespace mc
