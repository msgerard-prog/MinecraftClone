// Mob pathfinding (wiki: Mob AI › Pathfinding).
#include "gameplay/Pathfinder.h"
#include "world/Blocks.h"

#include <doctest/doctest.h>

#include <array>

using namespace mc;
using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

World floorWorld() {
    World w;
    for (int cz = -2; cz <= 2; ++cz)
        for (int cx = -2; cx <= 2; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    c.set(x, 63, z, S(blocks::Stone));
        }
    return w;
}

struct Result {
    std::array<glm::ivec3, 64> cells{};
    int n = 0;
};
Result path(Pathfinder& pf, const World& w, glm::ivec3 a, glm::ivec3 b, int height = 2) {
    Result r;
    r.n = pf.find(w, a, b, height, 1024, r.cells.data(), 64);
    return r;
}

} // namespace

TEST_CASE("a straight walk on flat ground") {
    World w = floorWorld();
    Pathfinder pf;
    const Result r = path(pf, w, {0, 64, 0}, {5, 64, 0});
    REQUIRE(r.n == 5);
    CHECK(r.cells[4] == glm::ivec3(5, 64, 0));
    for (int i = 0; i < r.n; ++i)
        CHECK(r.cells[i].y == 64);
}

TEST_CASE("around a wall through its gap, and up a 1-block step") {
    World w = floorWorld();
    for (int z = -10; z <= 10; ++z)
        if (z != 6)
            for (int y = 64; y <= 66; ++y)
                w.setBlock({3, y, z}, S(blocks::Stone));
    Pathfinder pf;
    Result r = path(pf, w, {0, 64, 0}, {6, 64, 0});
    REQUIRE(r.n > 0);
    CHECK(r.cells[r.n - 1] == glm::ivec3(6, 64, 0));
    bool throughGap = false;
    for (int i = 0; i < r.n; ++i)
        throughGap = throughGap || (r.cells[i].x == 3 && r.cells[i].z == 6);
    CHECK(throughGap);
    World s = floorWorld();
    s.setBlock({2, 64, 0}, S(blocks::Stone)); // a step up onto a platform
    for (int x = 3; x <= 6; ++x)
        s.setBlock({x, 64, 0}, S(blocks::Stone));
    r = path(pf, s, {0, 64, 0}, {5, 65, 0});
    REQUIRE(r.n > 0);
    CHECK(r.cells[r.n - 1] == glm::ivec3(5, 65, 0));
}

TEST_CASE("mobs drop down at most 3 blocks and keep out of lava") {
    World w = floorWorld();
    for (int x = 0; x <= 2; ++x) // a ledge 4 high at x 0..2 (top at 67), ground beyond
        for (int y = 64; y <= 67; ++y)
            w.setBlock({x, y, 0}, S(blocks::Stone));
    Pathfinder pf;
    Result r = path(pf, w, {1, 68, 0}, {5, 64, 0});
    for (int i = 0; i < r.n; ++i)
        CHECK(r.cells[i].y >= 68); // can't drop 4: it stays on the ledge (partial path)
    World l = floorWorld();
    for (int z = -1; z <= 1; ++z)
        l.setBlock({2, 63, z}, S(blocks::Lava));
    r = path(pf, l, {0, 64, 0}, {4, 64, 0});
    REQUIRE(r.n > 0);
    for (int i = 0; i < r.n; ++i)
        CHECK_FALSE((r.cells[i].x == 2 && std::abs(r.cells[i].z) <= 1)); // walks around the pool
    CHECK(r.cells[r.n - 1] == glm::ivec3(4, 64, 0));
}

TEST_CASE("an unreachable goal gives a partial path to the nearest cell") {
    World w = floorWorld();
    for (int z = -30; z <= 30; ++z) // a high wall all the way
        for (int y = 64; y <= 67; ++y)
            w.setBlock({3, y, z}, S(blocks::Stone));
    Pathfinder pf;
    const Result r = path(pf, w, {0, 64, 0}, {6, 64, 0});
    REQUIRE(r.n > 0);
    CHECK(r.cells[r.n - 1].x == 2); // right up against the wall
}
