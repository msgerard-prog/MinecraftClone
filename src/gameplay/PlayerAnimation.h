#pragma once

#include "world/Items.h"
#include "world/World.h"

#include <glm/glm.hpp>

namespace mc {

// How the player's body moves (M30.1): the state vanilla's player model and first-person
// hand read - limb swing, body yaw, the arm's attack swing, the item's equip dip and the
// walking bob. Advanced once a game tick; the getters interpolate with the frame's alpha.
class PlayerAnimation {
public:
    // Swing duration in ticks (vanilla: 6 for a hand without Haste/Mining Fatigue).
    static constexpr int kSwingTicks = 6;

    // `pos`/`prevPos`: the feet this tick and last; `headYaw`: vanilla degrees.
    void tick(const glm::dvec3& pos, const glm::dvec3& prevPos, float headYaw, bool onGround, world::ItemId held);
    // Starts the arm's swing (attacking, mining, using); a swing in progress restarts only
    // past its middle (vanilla: swinging resets once half done).
    void startSwing();

    float limbSwing(float alpha) const { return m_prevLimb + (m_limb - m_prevLimb) * alpha; }
    float limbAmount(float alpha) const { return m_prevLimbAmount + (m_limbAmount - m_prevLimbAmount) * alpha; }
    float bodyYaw(float alpha) const;
    // 0 at rest, rising to 1 over a swing.
    float swing(float alpha) const;
    // 1 = the item held up, 0 = lowered out of view (after switching items).
    float equip(float alpha) const { return m_prevEquip + (m_equip - m_prevEquip) * alpha; }
    // View bobbing (vanilla walkDist and bob): the walk phase and how hard it bobs (0..0.1).
    float walkPhase(float alpha) const { return m_prevWalk + (m_walk - m_prevWalk) * alpha; }
    float bobAmount(float alpha) const { return m_prevBob + (m_bob - m_prevBob) * alpha; }

private:
    float m_limb = 0.0f, m_prevLimb = 0.0f, m_limbAmount = 0.0f, m_prevLimbAmount = 0.0f;
    float m_body = 0.0f, m_prevBody = 0.0f;
    bool m_started = false;
    int m_swingTicks = -1; // -1: not swinging
    int m_prevSwingTicks = -1;
    float m_equip = 1.0f, m_prevEquip = 1.0f;
    world::ItemId m_held = 0;
    float m_walk = 0.0f, m_prevWalk = 0.0f, m_bob = 0.0f, m_prevBob = 0.0f;
};

// Third person (M30.1; vanilla Camera.getMaxZoom): how far the camera can back away from
// the eye along `back` (normalised), up to `maxDistance` (4), stopping 0.1 short of the
// first block any of 8 rays from the corners of a 0.2 cube around the eye hits.
double thirdPersonDistance(const world::World& world, const glm::dvec3& eye, const glm::dvec3& back,
                           double maxDistance = 4.0);

} // namespace mc
