#include "world/Blocks.h"
#include "world/Raycast.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

World oneChunk() {
    World w;
    w.createChunk({0, 0});
    w.createChunk({-1, 0});
    return w;
}

BlockStateId stone() { return blockRegistry().defaultState(blocks::Stone); }

} // namespace

TEST_CASE("raycast hits the first block and reports the entered face") {
    World w = oneChunk();
    w.setBlock({5, 10, 2}, stone());
    // From the west, looking east (+x): enters through the west face.
    auto hit = raycastBlocks(w, {1.5, 10.5, 2.5}, {1, 0, 0}, 10);
    REQUIRE(hit.has_value());
    CHECK(hit->block == BlockPos{5, 10, 2});
    CHECK(hit->face == Direction::West);
    CHECK(hit->distance == doctest::Approx(3.5));
    CHECK(neighbour(hit->block, hit->face) == BlockPos{4, 10, 2});
    // From above, looking down: the top face.
    hit = raycastBlocks(w, {5.5, 14.2, 2.5}, {0, -1, 0}, 10);
    REQUIRE(hit.has_value());
    CHECK(hit->face == Direction::Up);
    CHECK(neighbour(hit->block, hit->face) == BlockPos{5, 11, 2});
}

TEST_CASE("raycast respects reach, skips air and water, works at negative coords") {
    World w = oneChunk();
    // Block at x = -4: its east face (x = -3) is 4.6 away from x = 1.6 - beyond
    // survival reach (4.5), within creative reach (5.0).
    w.setBlock({-4, 10, 2}, stone());
    w.setBlock({-1, 10, 2}, blockRegistry().defaultState(blocks::Water));
    CHECK_FALSE(raycastBlocks(w, {1.6, 10.5, 2.5}, {-1, 0, 0}, kSurvivalReach).has_value());
    const auto hit = raycastBlocks(w, {1.6, 10.5, 2.5}, {-1, 0, 0}, kCreativeReach);
    REQUIRE(hit.has_value()); // through the water
    CHECK(hit->block == BlockPos{-4, 10, 2});
    CHECK(hit->distance == doctest::Approx(4.6));
    CHECK(hit->face == Direction::East);
}

TEST_CASE("raycast along a diagonal finds a block a straight walk would skip") {
    World w = oneChunk();
    w.setBlock({3, 12, 3}, stone());
    const auto hit = raycastBlocks(w, {0.5, 10.5, 0.5}, {1, 0.7, 1}, 10);
    REQUIRE(hit.has_value());
    CHECK(hit->block == BlockPos{3, 12, 3});
}

TEST_CASE("raycast edge cases: inside a block, on a boundary, axis-aligned at negatives") {
    World w = oneChunk();
    w.setBlock({2, 10, 2}, stone());
    w.setBlock({4, 10, 2}, stone());
    // Origin inside a solid block: that block is skipped, the next one is hit.
    auto hit = raycastBlocks(w, {2.5, 10.5, 2.5}, {1, 0, 0}, 10);
    REQUIRE(hit.has_value());
    CHECK(hit->block == BlockPos{4, 10, 2});
    // Origin exactly on an integer boundary, moving in the negative direction.
    hit = raycastBlocks(w, {4.0, 10.5, 2.5}, {-1, 0, 0}, 10);
    REQUIRE(hit.has_value());
    CHECK(hit->block == BlockPos{2, 10, 2});
    CHECK(hit->face == Direction::East);
    // Two zero components, negative coordinates.
    w.setBlock({-7, 10, 3}, stone());
    hit = raycastBlocks(w, {-6.5, 14.5, 3.5}, {0, -1, 0}, 10); // x -6.5 is inside block -7
    REQUIRE(hit.has_value());
    CHECK(hit->block == BlockPos{-7, 10, 3});
    CHECK(hit->face == Direction::Up);
}
