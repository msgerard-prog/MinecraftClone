#include "gameplay/Player.h"

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
    const auto& reg = world::blockRegistry();
    m_boxes.clear();
    const int x0 = static_cast<int>(std::floor(region.min.x)),
              x1 = static_cast<int>(std::floor(region.max.x));
    const int y0 = static_cast<int>(std::floor(region.min.y)),
              y1 = static_cast<int>(std::floor(region.max.y));
    const int z0 = static_cast<int>(std::floor(region.min.z)),
              z1 = static_cast<int>(std::floor(region.max.z));
    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (reg.collides(world.getBlock({x, y, z}))) {
                    m_boxes.push_back({{x, y, z}, {x + 1.0, y + 1.0, z + 1.0}});
                }
}

glm::dvec3 Player::collide(const world::World& world, const Aabb& start, const glm::dvec3& delta) {
    gatherBoxes(world, start.expandedTowards(delta));
    Aabb b = start;
    glm::dvec3 d = delta;
    // Vanilla order: Y first, then the larger horizontal axis last (X before Z
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
    // would still stand on something (vanilla Player.maybeBackOffFromEdge).
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
    if (m_sneaking && m_onGround && !m_flying) delta = backOffFromEdge(world, delta);
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
        const glm::dvec3 down =
            collide(world, raised, {0.0, -(up.y) + std::min(0.0, delta.y), 0.0});
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
    return moved;
}

void Player::tick(const world::World& world, const PlayerInput& input) {
    m_prevPos = m_pos;
    const world::ChunkPos here{world::blockToChunk(static_cast<int32_t>(std::floor(m_pos.x))),
                               world::blockToChunk(static_cast<int32_t>(std::floor(m_pos.z)))};
    if (!world.chunk(here)) return;

    // Double-tap jump toggles creative flight (within 7 ticks, vanilla).
    const bool jumpPressed = input.jump && !m_jumpWasDown;
    m_jumpWasDown = input.jump;
    ++m_ticksSinceJumpPress;
    if (jumpPressed) {
        if (m_creative && m_ticksSinceJumpPress <= kDoubleTapTicks) {
            m_flying = !m_flying;
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

    // Sprinting: needs forward input; stops when forward is released, when sneaking,
    // or after running into a wall (wiki: Sprinting).
    if (input.sprint && input.forward > 0.0f && !m_sneaking) m_sprinting = true;
    if (input.forward <= 0.0f || m_sneaking) m_sprinting = false;

    // Horizontal input, scaled like vanilla (0.98, sneak 0.3), normalised if > 1.
    glm::dvec2 in(input.strafe * kInputScale, input.forward * kInputScale);
    if (m_sneaking) in *= kSneakFactor;
    if (glm::dot(in, in) > 1.0) in = glm::normalize(in);

    double accel;
    if (m_flying) {
        accel = kFlySpeed * (m_sprinting ? 2.0 : 1.0) * m_flyMultiplier;
        if (input.jump) m_velocity.y += kFlyVertical;
        if (input.sneak) m_velocity.y -= kFlyVertical;
    } else if (m_onGround) {
        const double slip = kGroundSlipperiness;
        accel = kWalkSpeed * (m_sprinting ? kSprintFactor : 1.0) * std::pow(0.6 / slip, 3.0);
    } else {
        accel = kAirAccel * (m_sprinting ? kSprintFactor : 1.0);
    }

    // Jumping (before moving, as vanilla): 0.42 up; sprint-jumping adds a boost forward.
    if (!m_flying && input.jump && m_onGround) {
        m_velocity.y = kJumpVelocity;
        if (m_sprinting) {
            const glm::dvec3 f(world::forwardFlat(m_yaw));
            m_velocity += f * kSprintJumpBoost;
        }
    }

    // moveRelative: input rotated by yaw, times acceleration.
    const glm::dvec3 forward(world::forwardFlat(m_yaw));
    const glm::dvec3 right(world::rightFlat(m_yaw));
    m_velocity += (forward * in.y + right * in.x) * accel;

    const glm::dvec3 before = m_velocity;
    move(world, m_velocity);
    if (m_sprinting && (m_velocity.x != before.x || m_velocity.z != before.z)) m_sprinting = false;

    // After moving: gravity and drag, then friction (vanilla LivingEntity.travel).
    const double friction =
        m_onGround && !m_flying ? kGroundSlipperiness * kAirFriction : kAirFriction;
    if (m_flying) {
        m_velocity.y *= kFlyVerticalDamping;
        if (m_onGround) m_flying = false; // landing ends creative flight
    } else {
        m_velocity.y = (m_velocity.y - kGravity) * kVerticalDrag;
    }
    m_velocity.x *= friction;
    m_velocity.z *= friction;
}

} // namespace mc
