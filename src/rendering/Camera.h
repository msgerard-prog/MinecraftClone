#pragma once

#include <glm/glm.hpp>

namespace mc::gfx {

// Perspective camera using vanilla's rotation conventions (world/Rotation.h).
struct Camera {
    // Vanilla defaults: FOV 70 (vertical, degrees), near plane 0.05 blocks.
    static constexpr float kDefaultFov = 70.0f;
    static constexpr float kNear = 0.05f;
    static constexpr float kFar = 1024.0f;

    glm::dvec3 position{0.0};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fovDegrees = kDefaultFov;

    glm::mat4 viewProjection(float aspect) const;
};

} // namespace mc::gfx
