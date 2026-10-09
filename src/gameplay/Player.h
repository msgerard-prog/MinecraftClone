#pragma once

#include "gameplay/Aabb.h"
#include "world/World.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace mc {

// Movement intent for one tick, already decoded from keys (no GLFW here).
struct PlayerInput {
    float forward = 0.0f;  // +1 W, -1 S
    float strafe = 0.0f;   // +1 D (right), -1 A
    bool jump = false;     // space: jump, or fly up
    int jumpPresses = 0;   // space presses since the last tick (double-tap detection)
    bool sneak = false;    // shift: sneak, or fly down
    bool sprint = false;   // ctrl
    bool canSprint = true; // false when too hungry (food <= 6): ends a sprint (wiki: Sprinting)
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
    // Fluids (public write-ups of vanilla's fluid travel; the swim speeds they give
    // match the wiki: 0.02 x 0.98 / (1 - 0.8) = 1.96 b/s in water vs 1.97, Swimming).
    static constexpr double kSwimAccel = 0.02, kSwimUp = 0.04, kFluidGravity = 0.02;
    static constexpr double kWaterDrag = 0.8, kLavaDrag = 0.5;
    static constexpr double kWaterPush = 0.014; // current, blocks/tick per tick
    static constexpr double kSprintFactor = 1.3;
    static constexpr double kSneakFactor = 0.3;
    static constexpr double kInputScale = 0.98;        // vanilla scales WASD input by 0.98
    static constexpr double kGroundSlipperiness = 0.6; // default block slipperiness
    static constexpr double kAirFriction = 0.91;
    static constexpr double kJumpVelocity = 0.42;
    static constexpr double kFluidJumpThreshold = 0.4; // deeper fluid: swim up instead
    static constexpr double kSprintJumpBoost = 0.2;
    static constexpr double kFlySpeed = 0.05;    // creative flying speed (wiki: Flying speeds)
    static constexpr double kFlyVertical = 0.15; // unverified (gives 7.5 b/s; not on the wiki)
    static constexpr double kFlyVerticalDamping = 0.6;    // unverified
    static constexpr int kDoubleTapTicks = 7;             // unverified (not on the wiki)
    static constexpr double kMomentumThreshold = 0.003;   // MCPK: smaller speeds become 0 (1.9+)
    static constexpr int kJumpDelay = 10;                 // wiki: Jumping (holding jump, MC-184409)
    static constexpr double kMinorCollisionDegrees = 8.0; // wiki: Sprinting (21w41a)

    void setPosition(const glm::dvec3& feet);
    // Knockback from a hit (wiki: Knockback): pushed away from the attacker.
    void knockback(double dx, double dz, double strength = 0.4) {
        const double len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-6) return;
        m_velocity.x = m_velocity.x / 2.0 + dx / len * strength;
        m_velocity.z = m_velocity.z / 2.0 + dz / len * strength;
        if (m_onGround)
            m_velocity.y = std::min(0.4, m_velocity.y / 2.0 + strength); // wiki: Knockback
    }
    // A push (explosions): added to the velocity.
    void push(const glm::dvec3& v) { m_velocity += v; }
    void setRotation(float yawDeg, float pitchDeg);
    void setCreative(bool creative) { m_creative = creative; }
    void setVelocity(const glm::dvec3& v) { m_velocity = v; }
    void setFlying(bool flying) { m_flying = (flying && m_creative) || m_spectator; }
    // Spectator mode (M28.1c; wiki: Spectator): always flying, through blocks.
    void setSpectator(bool on) {
        m_spectator = on;
        if (on) m_creative = m_flying = true;
    }
    bool spectator() const { return m_spectator; }
    // Benchmarks only (--auto-fly): scales flight acceleration.
    void setFlySpeedMultiplier(double k) { m_flyMultiplier = k; }
    // Effects (M19.4; wiki: Speed, Slowness, Jump Boost, Slow Falling): walking speed
    // x (1 + 0.2 speed) x (1 - 0.15 slowness); jumps 0.1 higher per jump boost level;
    // slow falling: gravity 0.01 while falling; levitation (M20.4): drifts up toward
    // 0.05 blocks a tick per level instead of falling (wiki: Levitation, ~0.9 b/s).
    // Dolphin's Grace (M25.3b): water keeps 0.96 of the speed a tick instead of 0.8.
    void setDolphinsGrace(bool on) { m_dolphinsGrace = on; }
    // (M29.2b) the worn boots' Depth Strider and Soul Speed levels.
    void setBootEnchants(int depthStrider, int soulSpeed) {
        m_depthStrider = std::min(depthStrider, 3);
        m_soulSpeed = soulSpeed;
    }
    // (M29.2b) slipperiness of the block underfoot (wiki: Ice, Slime Block - vanilla's
    // block friction: ice 0.98, blue ice 0.989, slime 0.8, else 0.6).
    static double slipperinessOf(world::BlockId b);
    bool onSoulBlock() const { return m_onSoul; } // (soul sand/soil underfoot: Soul Speed wears the boots)
    // Swift Sneak (M27.3; wiki): sneaking speed 0.3 + 0.15 a level of the leggings'.
    void setSwiftSneak(int level) { m_sneakFactor = std::min(1.0, kSneakFactor + 0.15 * level); }
    void setEffects(int speed, int slowness, int jumpBoost, bool slowFalling, int levitation = 0) {
        m_walkMultiplier = std::max(0.0, (1.0 + 0.2 * speed) * (1.0 - 0.15 * slowness));
        m_jumpBoost = jumpBoost;
        m_slowFalling = slowFalling;
        m_levitation = levitation;
    }

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
    // Elytra (M20.4; wiki: Elytra): with a working elytra worn, pressing jump while
    // falling starts gliding; landing, water or taking it off ends it.
    void setCanGlide(bool can) { m_canGlide = can; }
    bool gliding() const { return m_gliding; }
    // Damage from flying into a wall this tick (horizontal speed lost x 10 - 3), once.
    float takeImpact() { return std::exchange(m_impact, 0.0f); }
    // Landed on a slime block this tick (the fall is forgiven: main resets it), once.
    bool takeBounce() { return std::exchange(m_bounced, false); }
    bool climbing() const { return m_climbing; } // on a ladder this tick (M23.2)
    bool inWater() const { return m_inWater; }   // touching water (last tick)
    bool inLava() const { return m_inLava; }
    bool sprinting() const { return m_sprinting; }
    bool sneaking() const { return m_sneaking; }
    double eyeHeight() const { return m_sneaking ? kSneakEyeHeight : kEyeHeight; }
    Aabb box() const { return Aabb::fromFeet(m_pos, kWidth, m_sneaking ? kSneakHeight : kHeight); }

private:
    bool m_climbing = false;
    // Moves by `delta` with collision (MCPK: Collisions): returns the actual motion.
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
    bool m_inWater = false, m_inLava = false;
    bool m_creative = true;
    bool m_spectator = false;
    bool m_flying = false;
    bool m_sprinting = false;
    bool m_sneaking = false;
    double m_sneakFactor = kSneakFactor;
    int m_ticksSinceJumpPress = 1000;
    int m_jumpDelay = 0;
    double m_flyMultiplier = 1.0;
    bool m_dolphinsGrace = false;
    int m_depthStrider = 0, m_soulSpeed = 0; // (M29.2b)
    bool m_onSoul = false;
    double m_walkMultiplier = 1.0;
    int m_jumpBoost = 0;
    bool m_slowFalling = false;
    int m_levitation = 0;
    bool m_canGlide = false, m_gliding = false;
    float m_impact = 0.0f;
    bool m_bounced = false;
    std::vector<Aabb> m_boxes; // reused collision box buffer (reserved: no tick allocation)

public:
    Player() { m_boxes.reserve(1024); }
};

// Vanilla mouse look: degrees of rotation per pixel for sensitivity 0..1.
double degreesPerPixel(double sensitivity);

} // namespace mc
