// Light engine against the wiki's rules (Light page).
#include "world/Blocks.h"
#include "world/LightEngine.h"
#include "world/LightManager.h"

#include <doctest/doctest.h>

#include <algorithm>
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
    std::vector<BlockPos> ready;
    for (int z = -1; z <= 1; ++z)
        for (int x = -1; x <= 1; ++x) {
            w.createChunk({x, z});
            loaded.push_back({x, z});
        }
    std::vector<ChunkPos> allLit;
    lm.update(loaded, none, edits, lit, relit, ready);
    for (int i = 0; i < 2000 && lm.pending() > 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lm.update(none, none, edits, lit, relit, ready);
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

TEST_CASE("below a light-filtering block only the reduced level spreads (no free fall)") {
    // Regression: sky light below 15 used to fall straight down without loss.
    // A wide water layer on glass over a tall air room: the water has 14, and from
    // there it drops 1 per block, glass included (far from any open edge).
    World w = floorWorld(64);
    for (int x = -16; x < 32; ++x)
        for (int z = -16; z < 32; ++z) {
            w.setBlock({x, 80, z}, S(blocks::Water));
            w.setBlock({x, 79, z}, S(blocks::Glass));
        }
    const auto l = lightOf(w);
    CHECK(sky(l, 8, 80, 8) == 14); // in the water
    CHECK(sky(l, 8, 79, 8) == 13); // 14 isn't full sky light: it loses 1 per block
    CHECK(sky(l, 8, 78, 8) == 12);
    CHECK(sky(l, 8, 75, 8) == 9);
}

TEST_CASE("glass passes full sky light straight down") {
    World w = floorWorld(64);
    w.setBlock({5, 70, 5}, S(blocks::Glass));
    const auto l = lightOf(w);
    CHECK(sky(l, 5, 70, 5) == 15);
    CHECK(sky(l, 5, 69, 5) == 15);
}

TEST_CASE("block light reaches into the empty section above the highest blocks") {
    // Regression: light above the top non-empty section was forced to block 0.
    World w = floorWorld(64); // top section: y 64..79
    w.setBlock({8, 79, 8}, S(blocks::Glowstone));
    const auto l = lightOf(w);
    CHECK(blk(l, 8, 80, 8) == 14);
    CHECK(blk(l, 8, 85, 8) == 9);
}

TEST_CASE("copy-on-write: editing a shared section leaves the worker's copy intact") {
    World w;
    Chunk& c = w.createChunk({0, 0});
    const auto held = c.shareSection(sectionIndex(70));
    c.set(1, 70, 1, S(blocks::Stone));
    CHECK(held->get(1, blockToLocal(70), 1) == 0);
    CHECK(c.get(1, 70, 1) == S(blocks::Stone));
    CHECK(c.shareSection(sectionIndex(70)).get() != held.get());
}

namespace {

// Runs a LightManager until idle; collects its outputs.
struct LightRun {
    std::vector<ChunkPos> lit;
    std::vector<SectionPos> relit;
    std::vector<BlockPos> ready;
    void step(LightManager& lm, const std::vector<ChunkPos>& loaded,
              const std::vector<ChunkPos>& unloaded, const std::vector<BlockPos>& edits) {
        std::vector<ChunkPos> l;
        std::vector<SectionPos> r;
        std::vector<BlockPos> e;
        lm.update(loaded, unloaded, edits, l, r, e);
        lit.insert(lit.end(), l.begin(), l.end());
        relit.insert(relit.end(), r.begin(), r.end());
        ready.insert(ready.end(), e.begin(), e.end());
    }
    void finish(LightManager& lm) {
        for (int i = 0; i < 5000 && lm.pending() > 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            step(lm, {}, {}, {});
        }
        step(lm, {}, {}, {});
    }
};

} // namespace

TEST_CASE("LightManager: an edit is handed back only once its chunk is relit") {
    World w = floorWorld(64);
    // floorWorld has 3x3 chunks: only (0,0) has a full neighbourhood.
    LightManager lm(w, 1);
    LightRun run;
    std::vector<ChunkPos> all;
    w.forEachChunk([&](const Chunk& c) { all.push_back(c.pos()); });
    run.step(lm, all, {}, {});
    run.finish(lm);
    REQUIRE(w.chunk({0, 0})->lit());

    // Break a floor block: the hole's light changes, so the section is reported
    // relit, and the edit comes back with current light (never earlier).
    const BlockPos hole{8, 64, 8};
    w.setBlock(hole, 0);
    run.step(lm, {}, {}, {hole});
    CHECK(run.ready.empty()); // waits for the relight
    run.finish(lm);
    REQUIRE(run.ready.size() == 1);
    CHECK(run.ready[0] == hole);
    CHECK(w.chunk({0, 0})->skyLight(8, 64, 8) == 15);
    const SectionPos s{0, 64 >> 4, 0};
    CHECK(std::find(run.relit.begin(), run.relit.end(), s) != run.relit.end());

    // An edit in an unlit chunk (no full neighbourhood) is handed back at once.
    run.ready.clear();
    w.setBlock({20, 64, 4}, 0);
    run.step(lm, {}, {}, {{20, 64, 4}});
    CHECK(run.ready.size() == 1);
}

TEST_CASE("LightManager: a chunk unloaded and reloaded mid-job gets fresh light") {
    // Regression: versions restarted after an unload, so an old in-flight result
    // could be installed over the reloaded chunk.
    World w = floorWorld(64);
    LightManager lm(w, 1);
    LightRun run;
    std::vector<ChunkPos> all;
    w.forEachChunk([&](const Chunk& c) { all.push_back(c.pos()); });
    run.step(lm, all, {}, {}); // job for (0,0) submitted
    w.removeChunk({0, 0});
    run.step(lm, {}, {{0, 0}}, {});
    Chunk& again = w.createChunk({0, 0});
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x)
            again.set(x, 100, z, S(blocks::Stone)); // a roof the old job never saw
    run.step(lm, {{0, 0}}, {}, {});
    run.finish(lm);
    REQUIRE(again.lit());
    CHECK(again.skyLight(8, 99, 8) < 15); // lit with the roof
}

TEST_CASE("padded light: above the world is open sky, below it dark; corners cross chunks") {
    World w = floorWorld(64);
    LightManager lm(w, 1);
    LightRun run;
    std::vector<ChunkPos> all;
    w.forEachChunk([&](const Chunk& c) { all.push_back(c.pos()); });
    run.step(lm, all, {}, {});
    run.finish(lm);
    std::vector<BlockStateId> blocks(kPaddedVolume);
    std::vector<uint8_t> skyL(kPaddedVolume), blkL(kPaddedVolume);
    SectionRefs refs;
    REQUIRE(captureSection(w, {0, kMaxY >> 4, 0}, refs)); // top section
    buildPadded(refs, blocks.data(), skyL.data(), blkL.data());
    CHECK(skyL[paddedIndex(5, 16, 5)] == 15); // above the world
    REQUIRE(captureSection(w, {0, kMinY >> 4, 0}, refs)); // bottom section
    buildPadded(refs, blocks.data(), skyL.data(), blkL.data());
    CHECK(skyL[paddedIndex(5, -1, 5)] == 0); // below the world
    // A failed capture (missing neighbour) keeps no references.
    CHECK_FALSE(captureSection(w, {1, 4, 1}, refs));
    for (const auto& b : refs.blocks)
        CHECK(b == nullptr);
    // Corner cell (-1, y, -1) of section (0, 4, 0) comes from chunk (-1, -1): stone at 64.
    REQUIRE(captureSection(w, {0, 4, 0}, refs));
    buildPadded(refs, blocks.data(), skyL.data(), blkL.data());
    CHECK(blocks[paddedIndex(-1, 0, -1)] == S(blocks::Stone));
    CHECK(blocks[paddedIndex(-1, 1, -1)] == 0);
}

TEST_CASE("without sky light (Nether, End) open air is dark; block light still spreads") {
    using namespace mc::world;
    World w;
    w.setHasSkyLight(false);
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx)
            w.createChunk({cx, cz});
    w.chunk({0, 0})->set(8, 70, 8, blockRegistry().defaultState(blocks::Glowstone));
    ChunkNeighbourhood n;
    REQUIRE(ChunkNeighbourhood::capture(w, {0, 0}, n));
    CHECK_FALSE(n.hasSkyLight);
    const ChunkLight light = computeChunkLight(n);
    const auto& s = *light[sectionIndex(70)];
    CHECK(s.sky.get(Section::index(2, blockToLocal(70), 2)) == 0);
    CHECK(s.block.get(Section::index(9, blockToLocal(70), 8)) == 14);
    CHECK(light[kSectionsPerChunk - 1]->sky.get(0) == 0); // even at the top
}
