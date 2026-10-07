#include "rendering/Fog.h"

#include <doctest/doctest.h>

TEST_CASE("terrain fog: ends at the render distance, starts at 92% (wiki: Fog)") {
    const auto f12 = mc::gfx::terrainFog(12);
    CHECK(f12.end == doctest::Approx(192.0));
    CHECK(f12.start == doctest::Approx(176.64));
    const auto f32 = mc::gfx::terrainFog(32);
    CHECK(f32.end == doctest::Approx(512.0));
    CHECK(f32.start / f32.end == doctest::Approx(0.92));
}
