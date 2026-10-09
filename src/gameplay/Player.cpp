#include "gameplay/Player.h"

#include "gameplay/BlockCollision.h"
#include "gameplay/FluidContact.h"
#include "world/Blocks.h"
#include "world/Rotation.h"

#include <algorithm>
#include <cmath>

namespace mc {

double degreesPerPixel(double sensitivity) {
    // Community documentation of vanilla mouse handling (not on the wiki):
    // f = s*0.6 + 0.2; degrees per pixel = f^3 * 8 * 0.15.
    const double f = sensitivity * 0.6 + 0.2;
    return f * f * f * 8.0 * 0.15;
}

void Player::setPosition(const glm::dvec3& feet) {
    m_pos = m_prevPos = feet;
    m_velocity = glm::dvec3(0.0);
}

void Player::setRotation(float yawDeg, float pitchDeg) {
    m_yaw = yawDeg;
    m_pitch = std::clamp(pitchDeg, -world::kMaxPitch, world::kMaxPitch);
}

void Player::turn(double dxPixels, double dyPixels, double sensitivity) {
    const double scale = degreesPerPixel(sensitivity);
    const float yaw = std::remainder(m_yaw + static_cast<float>(dxPixels * scale), 360.0f);
    setRotation(yaw, m_pitch + static_cast<float>(dyPixels * scale));
}

glm::dvec3 Player::renderPosition(double alpha) const {
    return m_prevPos + (m_pos - m_prevPos) * alpha;
}

void Player::gatherBoxes(const world::World& world, const Aabb& region) {
    // Block shapes (doors, fences...); unloaded chunks count as solid: never move into
    // terrain that hasn't been generated yet (it would appear around the player).
    gatherBlockBoxes(world, region, m_boxes, true);
}

glm::dvec3 Player::collide(const world::World& world, const Aabb& start, const glm::dvec3& delta) {
    gatherBoxes(world, start.expandedTowards(delta));
    Aabb b = start;
    glm::dvec3 d = delta;
    // Vanilla order: Y first, then the larger horizontal axis first (X before Z
    // unless |x| < |z|).
    const int order[3] = {1, std::abs(d.x) < std::abs(d.z) ? 2 : 0,
                          std::abs(d.x) < std::abs(d.z) ? 0 : 2};
    for (int axis : order) {
        if (d[axis] == 0.0) continue;
        for (const Aabb& wall : m_boxes)
            d[axis] = b.clip(wall, axis, d[axis]);
        glm::dvec3 step(0.0);
        step[axis] = d[axis];
        b = b.moved(step);
    }
    return d;
}

bool Player::hasGroundBelow(const world::World& world, const Aabb& box) {
    const Aabb probe = box.moved({0.0, -kStepHeight, 0.0});
    gatherBoxes(world, probe);
    for (const Aabb& wall : m_boxes)
        if (probe.intersects(wall)) return true;
    return false;
}

glm::dvec3 Player::backOffFromEdge(const world::World& world, glm::dvec3 d) {
    // Sneaking on the ground: shrink horizontal motion in 0.05 steps until the box
    // would still stand on something (MCPK: Sneaking).
    constexpr double kStep = 0.05;
    const Aabb b = box();
    auto shrink = [&](double v) {
        return v < kStep && v >= -kStep ? 0.0 : v > 0 ? v - kStep : v + kStep;
    };
    while (d.x != 0.0 && !hasGroundBelow(world, b.moved({d.x, 0.0, 0.0})))
        d.x = shrink(d.x);
    while (d.z != 0.0 && !hasGroundBelow(world, b.moved({0.0, 0.0, d.z})))
        d.z = shrink(d.z);
    while (d.x != 0.0 && d.z != 0.0 && !hasGroundBelow(world, b.moved({d.x, 0.0, d.z}))) {
        d.x = shrink(d.x);
        d.z = shrink(d.z);
    }
    return d;
}

glm::dvec3 Player::move(const world::World& world, glm::dvec3 delta) {
    if (m_spectator) { // no collision at all
        m_pos += delta;
        m_onGround = false;
        return delta;
    }
    // Sneak edge protection: on the ground or up to 0.6 above it, but never on the
    // tick a jump starts (jumping off an edge while sneaking works, wiki: Sneaking).
    if (m_sneaking && !m_flying && delta.y <= 0.0 && (m_onGround || hasGroundBelow(world, box()))) {
        delta = backOffFromEdge(world, delta);
    }
    const Aabb start = box();
    glm::dvec3 moved = collide(world, start, delta);

    // Step up (stairs-less ledges up to 0.6): if a horizontal move was blocked while
    // on or landing on the ground, retry from 0.6 higher and keep the farther result.
    const bool hitHorizontal = moved.x != delta.x || moved.z != delta.z;
    const bool grounded = m_onGround || (delta.y < 0 && moved.y != delta.y);
    if (hitHorizontal && grounded && !m_flying) {
        glm::dvec3 up = collide(world, start, {0.0, kStepHeight, 0.0});
        Aabb raised = start.moved(up);
        glm::dvec3 across = collide(world, raised, {delta.x, 0.0, delta.z});
        raised = raised.moved(across);
        const glm::dvec3 down = collide(world, raised, {0.0, -up.y + delta.y, 0.0});
        const glm::dvec3 stepped = up + across + down;
        if (stepped.x * stepped.x + stepped.z * stepped.z > moved.x * moved.x + moved.z * moved.z) {
            moved = stepped;
        }
    }

    m_pos += moved;
    m_onGround = delta.y < 0.0 && moved.y != delta.y;
    if (moved.x != delta.x) m_velocity.x = 0.0;
    if (moved.z != delta.z) m_velocity.z = 0.0;
    if (moved.y != delta.y) m_velocity.y = 0.0;
    // Landing on a slime block bounces back up as fast as it fell (not when sneaking),
    // and the fall does no damage either way (wiki: Slime Block).
    if (m_onGround && delta.y < 0.0 && !m_flying) {
        const world::BlockPos under{int(std::floor(m_pos.x)), int(std::floor(m_pos.y - 0.01)),
                                    int(std::floor(m_pos.z))};
        if (world::blockRegistry().blockOf(world.getBlock(under)) == world::blocks::SlimeBlock) {
            m_bounced = true; // (no fall damage, sneaking or not - wiki: Slime Block)
            if (delta.y < -0.1 && !m_sneaking) {
                m_velocity.y = -delta.y;
                m_onGround = false;
            }
        }
    }
    return moved;
}

void Player::tick(const world::World& world, const PlayerInput& input) {
    m_prevPos = m_pos;
    // MCPK: since 1.9 any speed below 0.003 is set to 0, so motion fully stops.
    for (int a = 0; a < 3; ++a) {
        if (std::abs(m_velocity[a]) < kMomentumThreshold) m_velocity[a] = 0.0;
    }
    const world::ChunkPos here{world::blockToChunk(static_cast<int32_t>(std::floor(m_pos.x))),
                               world::blockToChunk(static_cast<int32_t>(std::floor(m_pos.z)))};
    if (!world.chunk(here)) return;

    // Double-tap jump toggles creative flight (second press within 7 ticks, vanilla).
    // Presses are counted by the window, so a quick tap between ticks isn't missed.
    ++m_ticksSinceJumpPress;
    for (int k = 0; k < input.jumpPresses; ++k) {
        if (m_creative && m_ticksSinceJumpPress <= kDoubleTapTicks) {
            m_flying = !m_flying || m_spectator; // (spectators never land)
            m_ticksSinceJumpPress = 1000;
        } else {
            m_ticksSinceJumpPress = 0;
        }
    }

    // Sneaking pose (not while flying): only stand up again if there is headroom.
    const bool wantSneak = input.sneak && !m_flying;
    if (!wantSneak && m_sneaking) {
        gatherBoxes(world, Aabb::fromFeet(m_pos, kWidth, kHeight));
        const Aabb standing = Aabb::fromFeet(m_pos, kWidth, kHeight);
        bool blocked = false;
        for (const Aabb& wall : m_boxes)
            blocked |= standing.intersects(wall);
        m_sneaking = blocked;
    } else {
        m_sneaking = wantSneak;
    }

    // Sprinting: needs forward input; stops when forward is released or after running
    // into a wall (wiki: Sprinting). Sneaking doesn't end it (1.21.5+): a sprint that
    // goes on while sneaking is a faster sneak. It can't start while sneaking.
    if (input.sprint && input.canSprint && input.forward > 0.0f && !m_sneaking) m_sprinting = true;
    if (input.forward <= 0.0f || !input.canSprint) m_sprinting = false;

    // Horizontal input, scaled like vanilla (0.98, sneak 0.3), normalised if > 1.
    glm::dvec2 in(input.strafe * kInputScale, input.forward * kInputScale);
    if (m_sneaking) in *= m_sneakFactor;
    if (glm::dot(in, in) > 1.0) in = glm::normalize(in);

    // In water or lava (not flying): swim (M14; vanilla travel in fluids - acceleration
    // 0.02, jump rises 0.04 a tick, sneak sinks, drag 0.8 in water / 0.5 in lava,
    // gravity 0.02, pushed by the current; a wall bump while moving hops out at 0.3).
    const FluidContact fluid = fluidContact(world, box());
    m_inWater = fluid.water;
    m_inLava = fluid.lava;
    // A shallow layer (up to 0.4 deep: vanilla's fluid jump threshold) still lets a
    // player on the ground jump normally.
    const bool shallowJump = m_onGround && fluid.height <= kFluidJumpThreshold;
    if (fluid.water || fluid.lava) m_gliding = false; // (water and lava end a glide)
    if (!m_flying && (fluid.water || fluid.lava)) {
        if (!input.jump) m_jumpDelay = 0;
        if (m_jumpDelay > 0) --m_jumpDelay;
        if (input.jump && shallowJump && m_jumpDelay == 0) {
            m_velocity.y = kJumpVelocity;
            m_jumpDelay = kJumpDelay;
        } else if (input.jump) {
            m_velocity.y += kSwimUp;
        }
        if (input.sneak && fluid.water) m_velocity.y -= kSwimUp;
        const glm::dvec3 fwd(world::forwardFlat(m_yaw));
        const glm::dvec3 rgt(world::rightFlat(m_yaw));
        // Depth Strider (M29.2b; wiki): water slows a third less a level - drag and
        // acceleration move toward the ground's (half as much off the ground).
        const double strider = fluid.water ? double(m_depthStrider) / 3.0 * (m_onGround ? 1.0 : 0.5) : 0.0;
        const double swimAccel = kSwimAccel + (kWalkSpeed * m_walkMultiplier - kSwimAccel) * strider;
        m_velocity += (fwd * in.y + rgt * in.x) * swimAccel;
        if (fluid.water) m_velocity += fluid.flow * kWaterPush;
        const glm::dvec3 wanted = m_velocity;
        move(world, m_velocity);
        const bool bumped = m_velocity.x != wanted.x || m_velocity.z != wanted.z;
        // Water slows all axes by 0.8; lava horizontal 0.5, vertical 0.8 (wiki: Lava -
        // "horizontal speed -50%, vertical -20%").
        if (fluid.water && m_dolphinsGrace)
            m_velocity *= glm::dvec3(0.96, kWaterDrag, 0.96); // (Dolphin's Grace)
        else if (fluid.water) {
            const double drag = kWaterDrag + (0.546 - kWaterDrag) * strider;
            m_velocity *= glm::dvec3(drag, kWaterDrag, drag);
        }
        else
            m_velocity *= glm::dvec3(kLavaDrag, kWaterDrag, kLavaDrag);
        m_velocity.y -= kFluidGravity;
        if (bumped) { // climb out over a ledge (vanilla: if the space 0.6 up is free)
            const Aabb up = box().moved({wanted.x, 0.6, wanted.z});
            gatherBoxes(world, up);
            bool free = true;
            for (const Aabb& b : m_boxes)
                free = free && !up.intersects(b);
            if (free) m_velocity.y = 0.3;
        }
        return;
    }

    // Gliding (M20.4): the elytra motion as measured and documented by the community
    // (MCPK wiki: Elytra) - lift from the pitch's cosine squared, diving trades height
    // for speed, pulling up trades speed for height, the velocity turns toward the look;
    // drag 0.99 / 0.98 / 0.99.
    if (m_gliding && (m_onGround || !m_canGlide || m_flying)) m_gliding = false;
    if (!m_gliding && m_canGlide && !m_flying && !m_onGround &&
        input.jumpPresses > 0) // (jumping or falling)
        m_gliding = true;
    if (m_gliding) {
        const double pitch = m_pitch * 3.14159265358979 / 180.0;
        const glm::dvec3 look(world::lookVector(m_yaw, m_pitch));
        const double hlook = std::sqrt(look.x * look.x + look.z * look.z);
        const double hvel = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
        const double lift = std::cos(pitch) * std::cos(pitch);
        m_velocity.y += -kGravity + lift * 0.06;
        if (m_velocity.y < 0.0 && hlook > 0.0) {
            const double yacc = m_velocity.y * -0.1 * lift;
            m_velocity.y += yacc;
            m_velocity.x += look.x * yacc / hlook;
            m_velocity.z += look.z * yacc / hlook;
        }
        if (pitch < 0.0 && hlook > 0.0) {
            const double yacc = hvel * -std::sin(pitch) * 0.04;
            m_velocity.y += yacc * 3.2;
            m_velocity.x -= look.x * yacc / hlook;
            m_velocity.z -= look.z * yacc / hlook;
        }
        if (hlook > 0.0) {
            m_velocity.x += (look.x / hlook * hvel - m_velocity.x) * 0.1;
            m_velocity.z += (look.z / hlook * hvel - m_velocity.z) * 0.1;
        }
        const double before = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
        move(world, m_velocity);
        const double after = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
        const double lost = (before - after) * 10.0 - 3.0; // flying into a wall (wiki: Elytra)
        if (lost > 0.0) m_impact = static_cast<float>(lost);
        m_velocity *= glm::dvec3(0.99, 0.98, 0.99);
        return;
    }

    // The block underfoot (M29.2b): its slipperiness (ice slides) and speed (soul sand slows
    // to 0.4 unless the boots have Soul Speed, which speeds it up instead - wiki).
    const world::BlockId under = world::blockRegistry().blockOf(
        world.getBlock({int(std::floor(m_pos.x)), int(std::floor(m_pos.y - 0.5)), int(std::floor(m_pos.z))}));
    const double groundSlip = m_onGround && !m_flying ? slipperinessOf(under) : kGroundSlipperiness;
    m_onSoul = m_onGround && !m_flying && (under == world::blocks::SoulSand || under == world::blocks::SoulSoil);
    double accel;
    if (m_flying) {
        accel = kFlySpeed * (m_sprinting ? 2.0 : 1.0) * m_flyMultiplier;
        if (input.jump) m_velocity.y += kFlyVertical;
        if (input.sneak) m_velocity.y -= kFlyVertical;
    } else if (m_onGround) {
        const double slip = groundSlip;
        accel = kWalkSpeed * m_walkMultiplier * (m_sprinting ? kSprintFactor : 1.0) *
                std::pow(0.6 / slip, 3.0);
        if (m_onSoul && m_soulSpeed > 0) accel *= 1.0 + 0.105 * double(m_soulSpeed); // (ours: +10.5% a level)
    } else {
        accel = kAirAccel * (m_sprinting ? kSprintFactor : 1.0);
    }

    // Jumping (before moving, as vanilla): 0.42 up; sprint-jumping adds a boost forward.
    // Holding jump re-jumps only every 10 ticks; releasing it resets the delay.
    if (!input.jump) m_jumpDelay = 0;
    if (m_jumpDelay > 0) --m_jumpDelay;
    // A honey block underfoot (M26.3b; wiki: Honey Block): half the jump, 0.4 the speed.
    const bool onHoney = !m_flying && m_onGround &&
                         world::blockRegistry().blockOf(world.getBlock(
                             {int(std::floor(m_pos.x)), int(std::floor(m_pos.y - 0.5)),
                              int(std::floor(m_pos.z))})) == world::blocks::HoneyBlock;
    if (!m_flying && input.jump && m_onGround && m_jumpDelay == 0) {
        m_jumpDelay = kJumpDelay;
        m_velocity.y = (kJumpVelocity + 0.1 * m_jumpBoost) * (onHoney ? 0.5 : 1.0);
        if (m_sprinting) {
            const glm::dvec3 f(world::forwardFlat(m_yaw));
            m_velocity += f * kSprintJumpBoost;
        }
    }

    // moveRelative: input rotated by yaw, times acceleration.
    const glm::dvec3 forward(world::forwardFlat(m_yaw));
    const glm::dvec3 right(world::rightFlat(m_yaw));
    m_velocity += (forward * in.y + right * in.x) * accel;

    // Ladders (M23.2; wiki: Ladder): in a ladder's cell, horizontal speed and the fall
    // are capped at 0.15; sneaking holds on (no sliding); walking into the wall or
    // holding jump climbs at 0.2.
    const world::BlockPos feetCell{int(std::floor(m_pos.x)), int(std::floor(m_pos.y)),
                                   int(std::floor(m_pos.z))};
    const world::BlockId climbBlock = world::blockRegistry().blockOf(world.getBlock(feetCell));
    m_climbing = !m_flying && (climbBlock == world::blocks::Ladder ||
                               climbBlock == world::blocks::Vine); // (M28.5a: vines too)
    if (m_climbing) {
        m_velocity.x = std::clamp(m_velocity.x, -0.15, 0.15);
        m_velocity.z = std::clamp(m_velocity.z, -0.15, 0.15);
        m_velocity.y = std::max(m_velocity.y, -0.15);
        if (m_sneaking && m_velocity.y < 0.0) m_velocity.y = 0.0;
    }

    const glm::dvec3 before = m_velocity;
    move(world, m_velocity);
    if (m_climbing && (m_velocity.x != before.x || m_velocity.z != before.z || input.jump))
        m_velocity.y = 0.2;
    // Sprinting stops on a wall hit steeper than 8 degrees; glancing contact while
    // running along a wall keeps it (wiki: Sprinting, since 21w41a).
    if (m_sprinting && (m_velocity.x != before.x || m_velocity.z != before.z)) {
        const bool both = m_velocity.x != before.x && m_velocity.z != before.z;
        const double blocked = m_velocity.x != before.x ? before.x : before.z;
        const double free = m_velocity.x != before.x ? before.z : before.x;
        const double angle = glm::degrees(std::atan2(std::abs(blocked), std::abs(free)));
        if (both || angle > kMinorCollisionDegrees) m_sprinting = false;
    }

    // After moving: gravity and drag, then friction (MCPK: Horizontal/Vertical Movement Formulas).
    const double friction =
        m_onGround && !m_flying ? groundSlip * kAirFriction : kAirFriction;
    if (m_flying) {
        m_velocity.y *= kFlyVerticalDamping;
        if (m_onGround) m_flying = false; // landing ends creative flight
    } else if (m_levitation > 0) {
        m_velocity.y += (0.05 * m_levitation - m_velocity.y) * 0.2;
    } else {
        m_velocity.y = (m_velocity.y - (m_slowFalling && m_velocity.y <= 0.0 ? 0.01 : kGravity)) *
                       kVerticalDrag;
    }
    m_velocity.x *= friction;
    m_velocity.z *= friction;
    if (onHoney) { // (its speed factor: vanilla scales the motion after moving)
        m_velocity.x *= 0.4;
        m_velocity.z *= 0.4;
    }
    if (m_onSoul && under == world::blocks::SoulSand && m_soulSpeed == 0) { // (speed factor 0.4)
        m_velocity.x *= 0.4;
        m_velocity.z *= 0.4;
    }
}

double Player::slipperinessOf(world::BlockId b) {
    using namespace world;
    switch (b) {
    case blocks::Ice: case blocks::PackedIce: return 0.98;
    case blocks::BlueIce: return 0.989;
    case blocks::SlimeBlock: return 0.8;
    case blocks::FrostedIce: return 0.98;
    default: return kGroundSlipperiness;
    }
}

} // namespace mc
