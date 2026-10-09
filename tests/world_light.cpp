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
    return l[kOverworldHeight.sectionIndex(y)]->sky.get(Section::index(x, blockToLocal(y), z));
}
uint8_t blk(const ChunkLight& l, int x, int y, int z) {
    return l[kOverworldHeight.sectionIndex(y)]->block.get(Section::index(x, blockToLocal(y), z));
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
    for (int s = 0; s < kMaxSections; ++s)
        CHECK(*a[s] == *b[s]);
    // Sections fully above everything are stored as a uniform value (no array).
    CHECK(a[kMaxSections - 1]->sky.isUniform());
    CHECK(a[kMaxSections - 1]->sky.uniformValue() == 15);
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
    lm.update(loaded, none, edits, edits, lit, relit, ready);
    for (int i = 0; i < 2000 && lm.pending() > 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lm.update(none, none, edits, edits, lit, relit, ready);
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
    const auto held = c.shareSection(kOverworldHeight.sectionIndex(70));
    c.set(1, 70, 1, S(blocks::Stone));
    CHECK(held->get(1, blockToLocal(70), 1) == 0);
    CHECK(c.get(1, 70, 1) == S(blocks::Stone));
    CHECK(c.shareSection(kOverworldHeight.sectionIndex(70)).get() != held.get());
}

namespace {

// Runs a LightManager until idle; collects its outputs.
struct LightRun {
    std::vector<ChunkPos> lit;
    std::vector<SectionPos> relit;
    std::vector<BlockPos> ready;
    void step(LightManager& lm, const std::vector<ChunkPos>& loaded,
              const std::vector<ChunkPos>& unloaded, const std::vector<BlockPos>& edits,
              const std::vector<BlockPos>& settling = {}) {
        std::vector<ChunkPos> l;
        std::vector<SectionPos> r;
        std::vector<BlockPos> e;
        lm.update(loaded, unloaded, edits, settling, l, r, e);
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

TEST_CASE("LightManager: settling edits (flowing fluids) come back at once and are relit later") {
    World w = floorWorld(64);
    LightManager lm(w, 1);
    LightRun run;
    std::vector<ChunkPos> all;
    w.forEachChunk([&](const Chunk& c) { all.push_back(c.pos()); });
    run.step(lm, all, {}, {});
    run.finish(lm);
    REQUIRE(w.chunk({0, 0})->lit());
    const BlockPos hole{8, 64, 8};
    w.setBlock(hole, 0);
    run.ready.clear();
    run.step(lm, {}, {}, {}, {hole});
    REQUIRE(run.ready.size() == 1); // re-meshed now (stale light for a moment)
    CHECK(lm.pending() == 0);       // background work doesn't count as pending
    for (int f = 0; f < 25; ++f)    // its settle slot comes (every 20 frames at most)
        run.step(lm, {}, {}, {});
    run.finish(lm);
    CHECK(w.chunk({0, 0})->skyLight(8, 64, 8) == 15); // then relit
    // A player edit after a settling one still jumps the queue: it waits for its relight.
    const BlockPos hole2{9, 64, 8};
    w.setBlock({10, 64, 8}, 0);
    w.setBlock(hole2, 0);
    run.ready.clear();
    run.step(lm, {}, {}, {hole2}, {{10, 64, 8}});
    CHECK(run.ready.size() == 1); // only the settling one
    run.finish(lm);
    for (int f = 0; f < 25; ++f)
        run.step(lm, {}, {}, {});
    run.finish(lm);
    CHECK(run.ready.size() == 2);
    CHECK(w.chunk({0, 0})->skyLight(9, 64, 8) == 15);
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
    REQUIRE(captureSection(w, {0, kOverworldHeight.maxY() >> 4, 0}, refs)); // top section
    buildPadded(refs, blocks.data(), skyL.data(), blkL.data());
    CHECK(skyL[paddedIndex(5, 16, 5)] == 15); // above the world
    REQUIRE(captureSection(w, {0, kOverworldHeight.minY >> 4, 0}, refs)); // bottom section
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
    const auto& s = *light[kOverworldHeight.sectionIndex(70)];
    CHECK(s.sky.get(Section::index(2, blockToLocal(70), 2)) == 0);
    CHECK(s.block.get(Section::index(9, blockToLocal(70), 8)) == 14);
    CHECK(light[kMaxSections - 1]->sky.get(0) == 0); // even at the top
}

TEST_CASE("a Nether-height world: blocks only in Y 0..255; light and mesh capture use its sections") {
    using namespace mc::world;
    World w;
    w.setHeight(kNetherHeight);
    w.setHasSkyLight(false);
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx)
            w.createChunk({cx, cz});
    CHECK(w.chunk({0, 0})->sectionCount() == 16);
    w.setBlock({1, -1, 1}, blockRegistry().defaultState(blocks::Stone));
    CHECK(w.getBlock({1, -1, 1}) == 0);
    w.setBlock({8, 255, 8}, blockRegistry().defaultState(blocks::Glowstone));
    CHECK(w.isInHeight(255));
    CHECK_FALSE(w.isInHeight(256));
    ChunkNeighbourhood n;
    REQUIRE(ChunkNeighbourhood::capture(w, {0, 0}, n));
    const ChunkLight light = computeChunkLight(n);
    CHECK(light[15]->block.get(Section::index(9, 15, 8)) == 14); // next to the glowstone at 255
    CHECK(light[16] == nullptr);                                 // no 17th section
    SectionRefs refs;
    REQUIRE(captureSection(w, {0, 15, 0}, refs)); // the top section
    CHECK(refs.blocks[22] == nullptr);             // above it: outside the world
    CHECK(refs.openSky == 0);
    REQUIRE(captureSection(w, {0, 0, 0}, refs));
    CHECK(refs.blocks[4] == nullptr); // below Y 0
}

// M31.1: incremental light.
#include "world/Random.h"

TEST_CASE("M31.1: incremental light after edits matches a full recompute, cell for cell") {
    World w;
    const auto& r = blockRegistry();
    const BlockStateId stone = r.defaultState(blocks::Stone);
    // 5x5 chunks: a stone floor at y 60-63, a roofed room in the middle, a pillar of glass.
    for (int cz = -2; cz <= 2; ++cz)
        for (int cx = -2; cx <= 2; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    for (int y = 60; y <= 63; ++y) c.set(x, y, z, stone);
        }
    for (int x = 2; x <= 13; ++x)
        for (int z = 2; z <= 13; ++z) w.setBlock({x, 70, z}, stone); // a roof over (2..13, 2..13)
    auto lightAll = [&](int radius) {
        for (int cz = -radius; cz <= radius; ++cz)
            for (int cx = -radius; cx <= radius; ++cx) {
                ChunkNeighbourhood n;
                REQUIRE(ChunkNeighbourhood::capture(w, {cx, cz}, n));
                w.chunk({cx, cz})->setLight(computeChunkLight(n));
            }
    };
    lightAll(1); // the 3x3 around the centre (their neighbourhoods are all loaded)
    Xoroshiro rng(7);
    const BlockStateId kinds[] = {stone, 0, r.defaultState(blocks::Glowstone), r.defaultState(blocks::Torch),
                                  r.defaultState(blocks::Glass), r.defaultState(blocks::Water)};
    for (int step = 0; step < 60; ++step) {
        const BlockPos p{int(rng.nextInt(16)), 61 + int(rng.nextInt(12)), int(rng.nextInt(16))};
        w.setBlock(p, kinds[rng.nextInt(6)]);
        IncrementalLightInput in;
        REQUIRE(ChunkNeighbourhood::capture(w, {0, 0}, in.blocks));
        for (int i = 0; i < 9; ++i) {
            const Chunk* c = w.chunk({i % 3 - 1, i / 3 - 1});
            for (int s = 0; s < c->sectionCount(); ++s) in.light[size_t(i)][size_t(s)] = c->light(s);
        }
        in.edits.push_back(p);
        IncrementalLightOutput out;
        updateLightIncremental(in, out);
        for (int i = 0; i < 9; ++i) w.chunk({i % 3 - 1, i / 3 - 1})->setLight(out.light[size_t(i)]);
        // The full recompute of the same 3x3 must agree everywhere.
        for (int i = 0; i < 9; ++i) {
            const ChunkPos cp{i % 3 - 1, i / 3 - 1};
            ChunkNeighbourhood n;
            REQUIRE(ChunkNeighbourhood::capture(w, cp, n));
            const ChunkLight full = computeChunkLight(n);
            const Chunk* c = w.chunk(cp);
            int bad = 0;
            for (int s = 0; s < c->sectionCount() && bad == 0; ++s)
                for (int k = 0; k < 4096 && bad == 0; ++k)
                    if (full[size_t(s)]->sky.get(k) != c->light(s)->sky.get(k) ||
                        full[size_t(s)]->block.get(k) != c->light(s)->block.get(k)) {
                        ++bad;
                        MESSAGE("step " << step << " edit " << p.x << "," << p.y << "," << p.z << " chunk " << cp.x << ","
                                        << cp.z << " section " << s << " cell " << k << " full sky/block "
                                        << int(full[size_t(s)]->sky.get(k)) << "/" << int(full[size_t(s)]->block.get(k))
                                        << " incremental " << int(c->light(s)->sky.get(k)) << "/"
                                        << int(c->light(s)->block.get(k)));
                    }
            CHECK(bad == 0);
        }
    }
}

TEST_CASE("M31.1: LightManager corrects an edit incrementally once its 3x3 chunks are lit") {
    World w;
    for (int cz = -2; cz <= 2; ++cz)
        for (int cx = -2; cx <= 2; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int y = 60; y <= 64; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, y, z, S(blocks::Stone));
        }
    LightManager lm(w, 1);
    LightRun run;
    std::vector<ChunkPos> all;
    w.forEachChunk([&](const Chunk& c) { all.push_back(c.pos()); });
    run.step(lm, all, {}, {});
    run.finish(lm);
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) REQUIRE(w.chunk({cx, cz})->lit());
    // A glowstone in a pit lights its neighbours (14) and across the chunk border (x = -1).
    w.setBlock({0, 64, 5}, S(blocks::Glowstone));
    run.step(lm, {}, {}, {{0, 64, 5}});
    run.finish(lm);
    REQUIRE(run.ready.size() == 1);
    CHECK(w.chunk({0, 0})->blockLight(0, 65, 5) == 14);
    CHECK(w.chunk({-1, 0})->blockLight(15, 65, 5) == 13);
    // Taking it away puts out its light again.
    run.ready.clear();
    w.setBlock({0, 64, 5}, S(blocks::Stone));
    run.step(lm, {}, {}, {{0, 64, 5}});
    run.finish(lm);
    REQUIRE(run.ready.size() == 1);
    CHECK(w.chunk({0, 0})->blockLight(0, 65, 5) == 0);
    CHECK(w.chunk({-1, 0})->blockLight(15, 65, 5) == 0);
    // Matching a full recompute of the centre.
    const ChunkLight full = lightOf(w);
    for (int y = 60; y < 80; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                CHECK(sky(full, x, y, z) == w.chunk({0, 0})->skyLight(x, y, z));
                CHECK(blk(full, x, y, z) == w.chunk({0, 0})->blockLight(x, y, z));
            }
}

TEST_CASE("M31 review: incremental light - several edits a job, the world's bottom and top, no sky light") {
    const auto& r = blockRegistry();
    const BlockStateId stone = r.defaultState(blocks::Stone);
    const BlockStateId kinds[] = {stone, 0, r.defaultState(blocks::Glowstone), r.defaultState(blocks::Torch),
                                  r.defaultState(blocks::Glass), r.defaultState(blocks::Water)};
    for (const bool sky : {true, false}) {
        World w;
        w.setHasSkyLight(sky);
        if (!sky) w.setHeight(kNetherHeight);
        const int lo = w.height().minY, hi = w.height().maxY();
        for (int cz = -2; cz <= 2; ++cz)
            for (int cx = -2; cx <= 2; ++cx) {
                Chunk& c = w.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        for (int y = lo; y <= lo + 12; ++y) c.set(x, y, z, stone); // solid bottom sections
            }
        for (int x = 2; x <= 13; ++x)
            for (int z = 2; z <= 13; ++z) w.setBlock({x, hi - 2, z}, stone); // a roof near the top
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                ChunkNeighbourhood n;
                REQUIRE(ChunkNeighbourhood::capture(w, {cx, cz}, n));
                w.chunk({cx, cz})->setLight(computeChunkLight(n));
            }
        Xoroshiro rng(sky ? 11 : 12);
        for (int step = 0; step < 40; ++step) {
            IncrementalLightInput in;
            const int edits = 1 + int(rng.nextInt(4));
            const bool top = rng.nextInt(2) == 0;
            const BlockPos base{int(rng.nextInt(14)), top ? hi - 4 + int(rng.nextInt(5)) : lo + int(rng.nextInt(16)),
                                int(rng.nextInt(14))};
            for (int e = 0; e < edits; ++e) { // (clustered: neighbours edited together)
                const BlockPos p{base.x + e % 2, std::clamp(base.y + e / 2, lo, hi), base.z + (e / 2) % 2};
                w.setBlock(p, kinds[rng.nextInt(6)]);
                in.edits.push_back(p);
            }
            REQUIRE(ChunkNeighbourhood::capture(w, {0, 0}, in.blocks));
            for (int i = 0; i < 9; ++i) {
                const Chunk* c = w.chunk({i % 3 - 1, i / 3 - 1});
                for (int s = 0; s < c->sectionCount(); ++s) in.light[size_t(i)][size_t(s)] = c->light(s);
            }
            IncrementalLightOutput out;
            updateLightIncremental(in, out);
            for (int i = 0; i < 9; ++i) w.chunk({i % 3 - 1, i / 3 - 1})->setLight(out.light[size_t(i)]);
            for (int i = 0; i < 9; ++i) {
                const ChunkPos cp{i % 3 - 1, i / 3 - 1};
                ChunkNeighbourhood n;
                REQUIRE(ChunkNeighbourhood::capture(w, cp, n));
                const ChunkLight full = computeChunkLight(n);
                const Chunk* c = w.chunk(cp);
                int bad = 0;
                for (int s = 0; s < c->sectionCount() && bad == 0; ++s)
                    for (int k = 0; k < 4096 && bad == 0; ++k)
                        if (full[size_t(s)]->sky.get(k) != c->light(s)->sky.get(k) ||
                            full[size_t(s)]->block.get(k) != c->light(s)->block.get(k)) {
                            ++bad;
                            MESSAGE("sky " << sky << " step " << step << " chunk " << cp.x << "," << cp.z << " section " << s
                                           << " cell " << k);
                        }
                CHECK(bad == 0);
            }
        }
    }
}

TEST_CASE("M31 review: an edit comes back within a few updates while a long streaming queue runs") {
    World w;
    for (int cz = -6; cz <= 6; ++cz)
        for (int cx = -6; cx <= 6; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int y = 60; y <= 64; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) c.set(x, y, z, S(blocks::Stone));
        }
    LightManager lm(w, 1);
    LightRun run;
    // Light the middle 5x5 first, then stream the rest in while editing the middle.
    std::vector<ChunkPos> middle, rest;
    w.forEachChunk([&](const Chunk& c) {
        (std::abs(c.pos().x) <= 2 && std::abs(c.pos().z) <= 2 ? middle : rest).push_back(c.pos());
    });
    run.step(lm, middle, {}, {});
    run.finish(lm);
    REQUIRE(w.chunk({0, 0})->lit());
    REQUIRE(w.chunk({1, 1})->lit());
    run.ready.clear();
    run.step(lm, rest, {}, {}); // (a long streaming queue)
    w.setBlock({8, 64, 8}, 0);
    run.step(lm, {}, {}, {{8, 64, 8}});
    for (int i = 0; i < 50 && run.ready.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        run.step(lm, {}, {}, {});
    }
    CHECK(run.ready.size() == 1);
    CHECK(w.chunk({0, 0})->skyLight(8, 64, 8) == 15);
}
