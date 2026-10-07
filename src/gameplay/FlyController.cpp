#include "gameplay/FlyController.h"

#include "core/GameClock.h"
#include "world/Rotation.h"

#include <algorithm>
#include <cmath>

namespace mc {

double degreesPerPixel(double sensitivity) {
    const double f = sensitivity * 0.6 + 0.2;
    return f * f * f * 8.0 * 0.15;
}

void FlyController::setPosition(const glm::dvec3& pos) {
    m_pos = pos;
    m_prevPos = pos;
}

void FlyController::setRotation(float yawDeg, float pitchDeg) {
    m_yaw = yawDeg;
    m_pitch = std::clamp(pitchDeg, -world::kMaxPitch, world::kMaxPitch);
}

void FlyController::tick(const MoveInput& input) {
    m_prevPos = m_pos;

    glm::dvec3 horizontal = glm::dvec3(world::forwardFlat(m_yaw)) * double(input.forward) +
                            glm::dvec3(world::rightFlat(m_yaw)) * double(input.strafe);
    const double len = glm::length(horizontal);
    if (len > 1.0) horizontal /= len; // diagonal movement isn't faster (as in vanilla)

    const double speed = input.sprint ? kSprintFlySpeed : kFlySpeed;
    m_pos += horizontal * (speed * kTickSeconds);
    m_pos.y += double(input.up) * kVerticalSpeed * kTickSeconds;
}

void FlyController::turn(double dxPixels, double dyPixels, double sensitivity) {
    const double scale = degreesPerPixel(sensitivity);
    float yaw = m_yaw + static_cast<float>(dxPixels * scale);
    // Keep yaw in (-180, 180] like F3 shows it.
    yaw = std::remainder(yaw, 360.0f);
    setRotation(yaw, m_pitch + static_cast<float>(dyPixels * scale));
}

glm::dvec3 FlyController::renderPosition(double alpha) const {
    return m_prevPos + (m_pos - m_prevPos) * alpha;
}

} // namespace mc
