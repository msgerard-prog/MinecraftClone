#include "rendering/Camera.h"

#include "world/Rotation.h"

#include <glm/gtc/matrix_transform.hpp>

namespace mc::gfx {

glm::mat4 Camera::viewProjection(float aspect) const {
    const glm::vec3 eye(position);
    const glm::vec3 forward = world::lookVector(yaw, pitch);
    // Looking straight up/down makes "up" parallel to forward; use the yaw direction.
    const glm::vec3 up = std::abs(pitch) > 89.9f
                             ? glm::vec3(world::forwardFlat(yaw)) * (pitch > 0 ? 1.0f : -1.0f)
                             : glm::vec3(0, 1, 0);
    const glm::mat4 view = glm::lookAt(eye, eye + forward, up);
    const glm::mat4 proj = glm::perspective(glm::radians(fovDegrees), aspect, kNear, kFar);
    return proj * view;
}

} // namespace mc::gfx
