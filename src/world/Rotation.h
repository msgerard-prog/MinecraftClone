#pragma once

#include <glm/glm.hpp>

#include <cmath>

// Vanilla rotation conventions (what F3 shows as "Facing ... (yaw / pitch)").
//   yaw   0 = south (+Z), 90 = west (-X), 180/-180 = north (-Z), -90 = east (+X)
//   pitch 0 = horizon, +90 = straight down, -90 = straight up
namespace mc::world {

inline constexpr float kMaxPitch = 90.0f;

// Unit vector the entity is looking along (vanilla Entity.calculateViewVector).
inline glm::vec3 lookVector(float yawDeg, float pitchDeg) {
    const float yaw = glm::radians(yawDeg);
    const float pitch = glm::radians(pitchDeg);
    return {-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
}

// Horizontal unit vectors used for WASD movement at a given yaw.
inline glm::vec3 forwardFlat(float yawDeg) {
    const float yaw = glm::radians(yawDeg);
    return {-std::sin(yaw), 0.0f, std::cos(yaw)};
}
// Player's right-hand side: south-facing player (yaw 0) has west (-X) on their right.
inline glm::vec3 rightFlat(float yawDeg) {
    const float yaw = glm::radians(yawDeg);
    return {-std::cos(yaw), 0.0f, -std::sin(yaw)};
}

} // namespace mc::world
