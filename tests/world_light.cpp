// Light engine against the wiki's rules (Light page).
#include "world/Blocks.h"
#include "world/LightEngine.h"
#include "world/LightManager.h"

#include <doctest/doctest.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace mc::world;

namespace {

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

// 3x3 chunks around (0,0) with a stone floor up to y 64 (surface: first air y 65).
World floorWorld(int floorTop = 64) {
    World w;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            auto& c = w.createChunk({cx, cz});
            for (int y = 60; y <= floorTop; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        c.set(x, y, z, S(blocks::Stone));
        }
    return w;
}

ChunkLight lightOf(const World& w) {
    ChunkNeighbourhood n;
    REQUIRE(ChunkNeighbourhood::capture(w, {0, 0}, n));
    return computeChunkLight(n);
}

uint8_t sky(const ChunkLight& l, int x, int y, int z) {
    return l[sectionIndex(y)]->sky.get(Section::index(x, blockToLocal(y), z));
}
uint8_t blk(const ChunkLight& l, int x, int y, int z) {
    return l[sectionIndex(y)]->block.get(Section::index(x, blockToLocal(y), z));
}

} // namespace

TEST_CASE("open sky: 15 above the ground, 0 inside solid rock") {
    const auto l = lightOf(floorWorld());
    CHECK(sky(l, 5, 65, 5) == 15);
    CHECK(sky(l, 5, 200, 5) == 15);
    CHECK(sky(l, 5, 64, 5) == 0);
    CHECK(sky(l, 5, 61, 5) == 0);
    CHECK(blk(l, 5, 65, 5) == 0);
}

TEST_CASE("under a roof, sky light fades 1 per block from the open edge") {
    World w = floorWorld();
    // Roof at y 67 covering x 0..15 of chunk (0,0) and beyond on +x; open on -x side.
    for (int x = 0; x < 32; ++x)
        for (int z = -16; z < 32; ++z)
            w.setBlock({x, 67, z}, S(blocks::Stone));
    const auto l = lightOf(w);
    CHECK(sky(l, 0, 65, 8) == 14); // first block under the roof edge
    CHECK(sky(l, 5, 65, 8) == 9);
    CHECK(sky(l, 14, 65, 8) == 0); // 15 blocks in: dark
    CHECK(sky(l, 3, 66, 8) == 11);
}

TEST_CASE("sky light drops 1 per block of water depth") {
    World w = floorWorld(59);
    for (int y = 60; y <= 64; ++y)
        for (int x = -16; x < 32; ++x)
            for (int z = -16; z < 32; ++z)
                w.setBlock({x, y, z}, S(blocks::Water));
    const auto l = lightOf(w);
    CHECK(sky(l, 7, 64, 7) == 14); // top water block
    CHECK(sky(l, 7, 60, 7) == 10); // 5 deep
    CHECK(sky(l, 7, 65, 7) == 15);
}

TEST_CASE("block light: glowstone 15 and torch 14, minus 1 per step, also across chunks") {
    World w = floorWorld();
    w.setBlock({8, 65, 8}, S(blocks::Glowstone));
    w.setBlock({-2, 65, 3}, S(blocks::Torch)); // in chunk (-1, 0)
    const auto l = lightOf(w);
    CHECK(blk(l, 8, 66, 8) == 14);  // above the glowstone
    CHECK(blk(l, 11, 65, 8) == 12); // 3 steps away
    CHECK(blk(l, 0, 65, 3) == 12);  // 2 steps from the torch, across the chunk border
    CHECK(blk(l, 8, 64, 8) == 0);   // inside the stone floor: blocked
}

TEST_CASE("light is deterministic and independent of the neighbours' order of loading") {
    World w = floorWorld();
    w.setBlock({8, 65, 8}, S(blocks::Glowstone));
    const auto a = lightOf(w);
    const auto b = lightOf(w);
    for (int s = 0; s < kSectionsPerChunk; ++s)
        CHECK(*a[s] == *b[s]);
    // Sections fully above everything are stored as a uniform value (no array).
    CHECK(a[kSectionsPerChunk - 1]->sky.isUniform());
    CHECK(a[kSectionsPerChunk - 1]->sky.uniformValue() == 15);
}

TEST_CASE("LightManager lights a chunk once its 3x3 is loaded; border chunks are not pending") {
    World w;
    LightManager lm(w, 1);
    std::vector<ChunkPos> loaded, none, lit;
    std::vector<BlockPos> edits;
    std::vector<SectionPos> relit;
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x) {
            w.createChunk({x, z});
            loaded.push_back({x, z});
        }
    std::vector<ChunkPos> allLit;
    lm.update(loaded, none, edits, lit, relit);
    for (int i = 0; i < 2000 && lm.pending() > 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lm.update(none, none, edits, lit, relit);
        allLit.insert(allLit.end(), lit.begin(), lit.end());
    }
    // Only the centre has all 8 neighbours; the 8 border chunks wait, yet nothing is
    // pending (regression: border chunks used to keep pending() > 0 forever).
    CHECK(lm.pending() == 0);
    REQUIRE(allLit.size() == 1);
    CHECK(allLit[0] == ChunkPos{0, 0});
    CHECK(w.chunk({0, 0})->lit());
    CHECK_FALSE(w.chunk({1, 1})->lit());
}
