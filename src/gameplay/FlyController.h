#pragma once

#include <glm/glm.hpp>

namespace mc {

// Movement intent for one tick, already decoded from keys (no GLFW here).
struct MoveInput {
    float forward = 0.0f; // +1 W, -1 S
    float strafe = 0.0f;  // +1 D (right), -1 A
    float up = 0.0f;      // +1 jump, -1 sneak
    bool sprint = false;
};

// Creative-style free flight used until the real player physics in M4.
// Position advances only in tick() (20 TPS); rotation changes per frame in turn(),
// as in vanilla, where mouse look is applied every rendered frame.
class FlyController {
public:
    // Simplified constant speeds, blocks per second. M4 replaces these with
    // vanilla's acceleration + drag model.
    static constexpr double kFlySpeed = 10.92;      // wiki: Flying#Speed
    static constexpr double kSprintFlySpeed = 21.6; // wiki: Flying#Speed
    static constexpr double kVerticalSpeed = 7.49;  // wiki: Transportation

    void setPosition(const glm::dvec3& pos);
    void setRotation(float yawDeg, float pitchDeg);

    void tick(const MoveInput& input);
    // Benchmarks only: scales horizontal speed (e.g. 4 = streaming stress test).
    void setSpeedMultiplier(double k) { m_speedMultiplier = k; }

    // Mouse movement in screen pixels; sensitivity 0..1 (vanilla default 0.5).
    void turn(double dxPixels, double dyPixels, double sensitivity = 0.5);

    // Position between the previous and current tick for rendering (alpha 0..1).
    glm::dvec3 renderPosition(double alpha) const;

    const glm::dvec3& position() const { return m_pos; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }

private:
    glm::dvec3 m_prevPos{0.0};
    glm::dvec3 m_pos{0.0};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    double m_speedMultiplier = 1.0;
};

// Mouse look: degrees of rotation per pixel of mouse movement for a sensitivity
// 0..1: f = s*0.6 + 0.2; degrees = f^3 * 8 * 0.15 (0.15 deg/px at the default 0.5).
// Source: community documentation of vanilla mouse handling, not on the wiki.
// Still to confirm by an in-game measurement (see ROADMAP › Waiting on the user).
double degreesPerPixel(double sensitivity);

} // namespace mc
