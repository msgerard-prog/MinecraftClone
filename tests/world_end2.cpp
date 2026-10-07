// The M20 End generator ("end2"): outer islands, End biomes, chorus plants, cages.
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/NetherGenerator.h"

#include <doctest/doctest.h>

#include <array>
#include <ostream>

using namespace mc::world;

namespace {

uint64_t hashOf(const Chunk& c) {
    uint64_t h = 1469598103934665603ull;
    for (int y = 0; y < 256; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                h = (h ^ c.get(x, y, z)) * 1099511628211ull;
    return h;
}

} // namespace

TEST_CASE("end2: void up to ~1000 blocks, then outer islands with the four End biomes") {
    const EndGenerator gen(42, 2), old(42);
    CHECK(gen.kind() == "end2");
    CHECK(old.kind() == "end");
    std::array<int, size_t(Biome::Count)> seen{};
    int islandColumns = 0;
    for (int z = -4000; z <= 4000; z += 40)
        for (int x = -4000; x <= 4000; x += 40) {
            const double d2 = double(x) * x + double(z) * z;
            if (d2 < 900.0 * 900.0) {
                CHECK(gen.biomeAt(x, z) == Biome::TheEnd);
                CHECK(gen.outerValue(x, z) <= 0.0); // no outer islands near the middle
            } else if (d2 > 1100.0 * 1100.0) {
                ++seen[size_t(gen.biomeAt(x, z))];
                islandColumns += gen.outerValue(x, z) > 0.0;
            }
            CHECK(old.biomeAt(x, z) == Biome::TheEnd);
        }
    for (Biome b : {Biome::EndHighlands, Biome::EndMidlands, Biome::EndBarrens, Biome::SmallEndIslands})
        CHECK_MESSAGE(seen[size_t(b)] > 0, biomeInfo(b).id);
    // Islands cover a fair share of the outer End, but most of it is void.
    const int outer = seen[size_t(Biome::EndHighlands)] + seen[size_t(Biome::EndMidlands)] +
                      seen[size_t(Biome::EndBarrens)] + seen[size_t(Biome::SmallEndIslands)];
    CHECK(islandColumns > outer / 20);
    CHECK(islandColumns < outer / 2);
}

TEST_CASE("end2: highland chunks grow chorus plants on end stone, connected and ending in flowers") {
    const EndGenerator gen(42, 2);
    const auto& r = blockRegistry();
    int plants = 0, flowers = 0, chunks = 0;
    for (int cz = 70; cz < 160 && plants < 40; ++cz)
        for (int cx = 0; cx < 4 && plants < 40; ++cx) {
            if (gen.biomeAt(cx * 16 + 8, cz * 16 + 8) != Biome::EndHighlands) continue;
            ++chunks;
            Chunk c({cx, cz}, kEndHeight);
            gen.generate(c);
            CHECK(c.biomes()->at(8, 60, 8, kEndHeight) == Biome::EndHighlands);
            for (int y = 1; y < 255; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockStateId s = c.get(x, y, z);
                        if (r.blockOf(s) == blocks::ChorusFlower) {
                            ++flowers;
                            CHECK(r.get(s, properties::age5) == 5);
                        }
                        if (r.blockOf(s) != blocks::ChorusPlant) continue;
                        ++plants;
                        // Every plant connects to something, and a plant over a plant
                        // or end stone says so.
                        const BlockId below = r.blockOf(c.get(x, y - 1, z));
                        if (below == blocks::ChorusPlant || below == blocks::EndStone)
                            CHECK(r.get(s, properties::faceDown) == 0);
                    }
        }
    REQUIRE(chunks > 0);
    CHECK(plants >= 10);
    CHECK(flowers > 0);
}

TEST_CASE("end2: the two shortest pillars carry iron bar cages; end2 keeps the main island") {
    const EndGenerator gen(42, 2), old(42);
    const auto& r = blockRegistry();
    int caged = 0;
    for (int i = 0; i < EndGenerator::kPillars; ++i) {
        const auto& p = gen.pillar(i);
        Chunk c({blockToChunk(p.x + 2), blockToChunk(p.z)}, kEndHeight);
        gen.generate(c);
        const BlockStateId s = c.get(blockToLocal(p.x + 2), p.height + 2, blockToLocal(p.z));
        if (r.blockOf(s) == blocks::IronBars) {
            ++caged;
            CHECK(p.height <= 79);
            CHECK(r.get(s, properties::fireNorth) == 0); // joins the wall beside it
            CHECK(r.get(s, properties::fireEast) == 1);  // (outside: nothing)
        }
    }
    CHECK(caged == 2);
    for (int cz = -3; cz <= 3; ++cz)
        for (int cx = -3; cx <= 3; ++cx) { // the main island is unchanged away from the cages
            Chunk a({cx, cz}, kEndHeight), b({cx, cz}, kEndHeight);
            gen.generate(a);
            old.generate(b);
            int diff = 0;
            for (int y = 0; y < 256; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        diff += a.get(x, y, z) != b.get(x, y, z) &&
                                r.blockOf(a.get(x, y, z)) != blocks::IronBars &&
                                r.blockOf(b.get(x, y, z)) != blocks::EndPortal; // (shut until the dragon dies)
            CHECK(diff == 0);
        }
}

TEST_CASE("end2 output is deterministic and pinned (seed 42)") {
    const EndGenerator gen(42, 2);
    Chunk a({0, 90}, kEndHeight), b({0, 90}, kEndHeight);
    gen.generate(a);
    gen.generate(b);
    CHECK(hashOf(a) == hashOf(b));
    CHECK(hashOf(a) == 18334859166678057859ull); // (re-pinned while M20 builds end2, frozen at v0.20.0)
}

TEST_CASE("chorus plants need end stone or a standing plant; iron bars join neighbours") {
    World w;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx)
            w.createChunk({cx, cz});
    BlockUpdates updates(w);
    const auto& r = blockRegistry();
    const BlockStateId endStone = r.defaultState(blocks::EndStone), plant = r.defaultState(blocks::ChorusPlant);
    w.setBlock({4, 64, 4}, endStone);
    CHECK(BlockUpdates::placement(w, plant, {4, 65, 4}, Direction::Up, 0, 0));
    CHECK_FALSE(BlockUpdates::placement(w, plant, {6, 65, 4}, Direction::Up, 0, 0)); // nothing below
    w.updateBlock({4, 65, 4}, *BlockUpdates::placement(w, plant, {4, 65, 4}, Direction::Up, 0, 0));
    w.updateBlock({4, 66, 4}, *BlockUpdates::placement(w, plant, {4, 66, 4}, Direction::Up, 0, 0));
    w.updateBlock({5, 66, 4}, *BlockUpdates::placement(w, plant, {5, 66, 4}, Direction::Up, 0, 0)); // a branch
    CHECK(r.get(w.getBlock({4, 65, 4}), properties::faceDown) == 0);
    CHECK(r.get(w.getBlock({4, 66, 4}), properties::fireEast) == 0);
    w.updateBlock({4, 64, 4}, 0); // the end stone goes: the whole plant breaks
    CHECK(r.blockOf(w.getBlock({4, 65, 4})) == blocks::Air);
    CHECK(r.blockOf(w.getBlock({4, 66, 4})) == blocks::Air);
    CHECK(r.blockOf(w.getBlock({5, 66, 4})) == blocks::Air);
    CHECK(updates.drops().size() == 3);

    const BlockStateId bars = r.defaultState(blocks::IronBars);
    w.setBlock({10, 64, 9}, r.defaultState(blocks::Stone));
    w.updateBlock({10, 64, 10}, *BlockUpdates::placement(w, bars, {10, 64, 10}, Direction::Up, 0, 0));
    w.updateBlock({11, 64, 10}, *BlockUpdates::placement(w, bars, {11, 64, 10}, Direction::Up, 0, 0));
    const BlockStateId a = w.getBlock({10, 64, 10});
    CHECK(r.get(a, properties::fireNorth) == 0); // stone
    CHECK(r.get(a, properties::fireEast) == 0);  // the other bars (updated when they came)
    CHECK(r.get(a, properties::fireWest) == 1);
}

#include "gameplay/BlockInteraction.h"

TEST_CASE("chorus fruit: a teleport lands within 8 blocks on solid ground, never in fluids") {
    World w;
    for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
            Chunk& c = w.createChunk({cx, cz});
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    c.set(x, 63, z, blockRegistry().defaultState(cx == 1 ? blocks::Water : blocks::EndStone));
        }
    Xoroshiro rng(3);
    int landed = 0;
    for (int i = 0; i < 100; ++i) {
        const auto to = mc::chorusTeleport(w, {0.5, 64.0, 0.5}, rng);
        if (!to) continue;
        ++landed;
        CHECK(std::abs(to->x - 0.5) <= 8.5);
        CHECK(std::abs(to->z - 0.5) <= 8.5);
        CHECK(to->y == doctest::Approx(64.0));
        CHECK(to->x < 16.0); // (water from x 16: never there)
    }
    CHECK(landed > 80);
}
