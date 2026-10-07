#pragma once

#include <glm/glm.hpp>

namespace mc::gfx {

// Perspective camera using vanilla's rotation conventions (world/Rotation.h).
struct Camera {
    // FOV 70 (vertical, degrees) is vanilla's default (wiki: Options). The near plane
    // 0.05 blocks is not documented on the wiki; it is our choice (close enough to
    // put your face against a block without clipping).
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
