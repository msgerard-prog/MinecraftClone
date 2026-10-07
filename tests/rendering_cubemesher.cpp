// CubeMesher is GL-free, so its geometry rules are unit-tested here.
#include "rendering/CubeMesher.h"

#include <doctest/doctest.h>

using mc::gfx::BlockVertex;
using mc::world::Direction;

namespace {

glm::vec3 pos(const BlockVertex& v) { return {v.x, v.y, v.z}; }

} // namespace

TEST_CASE("vanilla directional face shading") {
    CHECK(mc::gfx::faceShade(Direction::Up) == 1.0f);
    CHECK(mc::gfx::faceShade(Direction::Down) == 0.5f);
    CHECK(mc::gfx::faceShade(Direction::North) == 0.8f);
    CHECK(mc::gfx::faceShade(Direction::South) == 0.8f);
    CHECK(mc::gfx::faceShade(Direction::East) == 0.6f);
    CHECK(mc::gfx::faceShade(Direction::West) == 0.6f);
}

TEST_CASE("every cube triangle is counter-clockwise seen from outside") {
    std::vector<BlockVertex> verts;
    mc::gfx::appendCube(verts, {3, -2, 5}, mc::gfx::CubeFaces{});
    REQUIRE(verts.size() == 36);
    const glm::vec3 center(3.5f, -1.5f, 5.5f);
    for (size_t t = 0; t < verts.size(); t += 3) {
        const glm::vec3 a = pos(verts[t]), b = pos(verts[t + 1]), c = pos(verts[t + 2]);
        const glm::vec3 n = glm::cross(b - a, c - a); // CCW => points towards the viewer
        const glm::vec3 outward = (a + b + c) / 3.0f - center;
        CHECK(glm::dot(n, outward) > 0.0f);
    }
}

TEST_CASE("faceMask emits only the selected faces") {
    std::vector<BlockVertex> verts;
    const uint8_t upOnly = 1u << static_cast<int>(Direction::Up);
    mc::gfx::appendCube(verts, {0, 0, 0}, mc::gfx::CubeFaces{}, upOnly);
    REQUIRE(verts.size() == 6);
    for (const auto& v : verts)
        CHECK(v.y == 1.0f);
}

TEST_CASE("face colour is tint x shade") {
    mc::gfx::CubeFaces faces;
    faces.tint[static_cast<int>(Direction::Down)] = 0xFF000000u | (200u << 16) | (100u << 8) | 50u;
    std::vector<BlockVertex> verts;
    mc::gfx::appendCube(verts, {0, 0, 0}, faces, 1u << static_cast<int>(Direction::Down));
    const uint32_t c = verts[0].color; // bytes in memory: R, G, B, A
    CHECK((c & 0xFF) == 25);           // 50 * 0.5
    CHECK(((c >> 8) & 0xFF) == 50);    // 100 * 0.5
    CHECK(((c >> 16) & 0xFF) == 100);  // 200 * 0.5
    CHECK((c >> 24) == 0xFF);
}

TEST_CASE("texture orientation: top of sprite = +Y on sides, north on the up face") {
    // Distinct UVs per face so we can tell which corner got which coordinate.
    mc::gfx::CubeFaces faces;
    for (int f = 0; f < mc::world::kDirectionCount; ++f) {
        faces.uv[f] = {f * 10.0f, f * 10.0f + 1.0f, f * 10.0f + 5.0f, f * 10.0f + 6.0f};
    }
    std::vector<BlockVertex> verts;
    mc::gfx::appendCube(verts, {0, 0, 0}, faces);
    for (int f = 0; f < mc::world::kDirectionCount; ++f) {
        const auto dir = static_cast<Direction>(f);
        const mc::gfx::UvRect& uv = faces.uv[f];
        // Seen from outside: "up" on the texture, and "right" on the texture.
        glm::vec3 texUp(0, 1, 0);
        if (dir == Direction::Up) texUp = {0, 0, -1};  // north
        if (dir == Direction::Down) texUp = {0, 0, 1}; // south
        const glm::vec3 n(mc::world::normal(dir));
        const glm::vec3 texRight = glm::cross(-n, texUp); // viewer looks along -n
        for (int i = 0; i < 6; ++i) {
            const BlockVertex& v = verts[f * 6 + i];
            const glm::vec3 p = pos(v) - glm::vec3(0.5f);
            INFO("face ", f, " vertex ", i);
            CHECK(v.v == (glm::dot(p, texUp) > 0 ? uv.v0 : uv.v1));    // v0 = top row
            CHECK(v.u == (glm::dot(p, texRight) > 0 ? uv.u1 : uv.u0)); // u0 = left column
        }
    }
}
