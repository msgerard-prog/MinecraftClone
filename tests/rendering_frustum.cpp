#include "rendering/Camera.h"
#include "rendering/Frustum.h"

#include <doctest/doctest.h>

TEST_CASE("frustum keeps boxes in front and drops boxes behind") {
    mc::gfx::Camera cam; // at origin facing south (+Z)
    const auto f = mc::gfx::Frustum::fromMatrix(cam.viewProjectionAtOrigin(16.0f / 9.0f));
    CHECK(f.intersectsBox({-1, -1, 10}, {1, 1, 12}));         // ahead
    CHECK_FALSE(f.intersectsBox({-1, -1, -12}, {1, 1, -10})); // behind
    CHECK_FALSE(f.intersectsBox({100, -1, 5}, {102, 1, 7}));  // far to the side
    CHECK(f.intersectsBox({-16, -16, -8}, {16, 16, 8}));      // contains the camera
}
