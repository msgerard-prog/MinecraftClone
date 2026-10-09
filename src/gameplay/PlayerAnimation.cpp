#include "gameplay/PlayerAnimation.h"

#include "world/Raycast.h"

#include <algorithm>
#include <cmath>

namespace mc {

namespace {

float wrapDegrees(float a) {
    a = std::fmod(a + 180.0f, 360.0f);
    if (a < 0.0f) a += 360.0f;
    return a - 180.0f;
}

} // namespace

void PlayerAnimation::tick(const glm::dvec3& pos, const glm::dvec3& prevPos, float headYaw, bool onGround,
                           world::ItemId held) {
    if (!m_started) { // (the body starts facing where the head looks)
        m_started = true;
        m_body = m_prevBody = headYaw;
    }
    m_prevLimb = m_limb;
    m_prevLimbAmount = m_limbAmount;
    m_prevBody = m_body;
    m_prevSwingTicks = m_swingTicks;
    m_prevEquip = m_equip;
    m_prevWalk = m_walk;
    m_prevBob = m_bob;
    const double dx = pos.x - prevPos.x, dz = pos.z - prevPos.z;
    const float dist = float(std::sqrt(dx * dx + dz * dz));
    // Limbs (vanilla LivingEntity.calculateEntityAnimation): the swing amount eases toward
    // the distance moved x 4 (at most 1) and the swing phase advances by the amount.
    m_limbAmount += (std::min(dist * 4.0f, 1.0f) - m_limbAmount) * 0.4f;
    m_limb += m_limbAmount;
    // The body (vanilla's body rotation): while moving it turns toward the walking
    // direction; it never lags the head by more than 50 degrees.
    if (dist > 0.05f) {
        const float moveYaw = float(std::atan2(-dx, dz) * 180.0 / 3.14159265358979);
        float d = wrapDegrees(moveYaw - m_body);
        if (std::abs(d) > 95.0f) d = wrapDegrees(d + 180.0f); // (walking backwards: the body stays forward)
        m_body += d * 0.3f;
    }
    const float lag = wrapDegrees(headYaw - m_body);
    if (lag > 50.0f) m_body += lag - 50.0f;
    if (lag < -50.0f) m_body += lag + 50.0f;
    // Keep the previous value continuous with the new one (no 360 jumps when interpolating).
    m_prevBody = m_body - wrapDegrees(m_body - m_prevBody);
    if (m_swingTicks >= 0 && ++m_swingTicks >= kSwingTicks) m_swingTicks = -1;
    // Equipping: a new item drops the hand, which rises again over a few ticks (vanilla:
    // equipProgress steps by 0.4 a tick).
    if (held != m_held) {
        m_held = held;
        m_equip = m_prevEquip = 0.0f;
    } else {
        m_equip = std::min(1.0f, m_equip + 0.4f);
    }
    // View bobbing (vanilla Player.aiStep: walkDist += dist x 0.6; bob eases toward the
    // speed, at most 0.1, only on the ground).
    m_walk += dist * 0.6f;
    const float target = onGround ? std::min(0.1f, dist) : 0.0f;
    m_bob += (target - m_bob) * 0.4f;
}

void PlayerAnimation::startSwing() {
    if (m_swingTicks < 0 || m_swingTicks >= kSwingTicks / 2) m_swingTicks = 0;
}

float PlayerAnimation::bodyYaw(float alpha) const { return m_prevBody + (m_body - m_prevBody) * alpha; }

float PlayerAnimation::swing(float alpha) const {
    if (m_swingTicks < 0) return 0.0f;
    const float from = m_prevSwingTicks < 0 || m_prevSwingTicks > m_swingTicks ? 0.0f : float(m_prevSwingTicks);
    return std::clamp((from + (float(m_swingTicks) - from) * alpha) / float(kSwingTicks), 0.0f, 1.0f);
}

double thirdPersonDistance(const world::World& world, const glm::dvec3& eye, const glm::dvec3& back,
                           double maxDistance) {
    double best = maxDistance;
    for (int i = 0; i < 8; ++i) {
        const glm::dvec3 off((i & 1 ? 0.1 : -0.1), (i & 2 ? 0.1 : -0.1), (i & 4 ? 0.1 : -0.1));
        if (const auto hit = world::raycastBlocks(world, eye + off, back, maxDistance))
            best = std::min(best, hit->distance - 0.1);
    }
    return std::max(0.0, best);
}

} // namespace mc
