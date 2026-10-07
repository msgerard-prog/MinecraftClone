// Camera::viewProjection is pure glm (no GL), so it is unit-tested.
#include "rendering/Camera.h"

#include <doctest/doctest.h>

#include <cmath>

namespace {

glm::vec4 project(const mc::gfx::Camera& cam, const glm::vec3& world) {
    return cam.viewProjection(16.0f / 9.0f) * glm::vec4(world, 1.0f);
}

bool allFinite(const glm::mat4& m) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r])) return false;
    return true;
}

} // namespace

TEST_CASE("camera faces along the vanilla look vector") {
    mc::gfx::Camera cam; // at origin, yaw 0 = south (+Z)
    const glm::vec4 clip = project(cam, {0, 0, 5});
    CHECK(clip.w > 0); // in front of the camera
    CHECK(clip.x / clip.w == doctest::Approx(0.0));
    CHECK(clip.y / clip.w == doctest::Approx(0.0));
    // Facing south, east (+X) is on the left of the screen.
    const glm::vec4 east = project(cam, {1, 0, 5});
    CHECK(east.x / east.w < 0);
}

TEST_CASE("camera is well-defined looking straight down or up") {
    mc::gfx::Camera cam;
    cam.pitch = 90.0f;
    CHECK(allFinite(cam.viewProjection(16.0f / 9.0f)));
    // Looking down with yaw 0 (south): south (+Z) is towards the top of the screen.
    const glm::vec4 south = project(cam, {0, -5, 1});
    CHECK(south.w > 0);
    CHECK(south.y / south.w > 0);

    cam.pitch = -90.0f;
    CHECK(allFinite(cam.viewProjection(16.0f / 9.0f)));
    const glm::vec4 above = project(cam, {0, 5, 0});
    CHECK(above.w > 0);
}
