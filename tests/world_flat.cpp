#include "world/Blocks.h"
#include "world/FlatGenerator.h"

#include <doctest/doctest.h>

using namespace mc::world;

namespace {

// FNV-1a over every block state of a chunk: pins worldgen output (hard rule 3).
uint64_t chunkHash(const Chunk& c) {
    uint64_t h = 1469598103934665603ull;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                h ^= c.get(x, y, z);
                h *= 1099511628211ull;
            }
    return h;
}

} // namespace

TEST_CASE("classic flat preset: bedrock, 2 dirt, grass from Y -64") {
    const auto gen = FlatGenerator::fromPreset(FlatGenerator::kClassicFlat);
    REQUIRE(gen.has_value());
    CHECK(gen->surfaceY() == -60);
    Chunk c({3, -7});
    gen->generate(c);
    const auto& r = blockRegistry();
    CHECK(r.blockOf(c.get(0, -64, 0)) == blocks::Bedrock);
    CHECK(r.blockOf(c.get(15, -63, 15)) == blocks::Dirt);
    CHECK(r.blockOf(c.get(7, -62, 3)) == blocks::Dirt);
    CHECK(r.blockOf(c.get(7, -61, 3)) == blocks::GrassBlock);
    CHECK(c.get(7, -60, 3) == 0);
    CHECK(c.section(0).nonAirCount() == 4 * 256);
    CHECK(c.section(1).isEmpty());
}

TEST_CASE("flat generation is deterministic (pinned hash)") {
    const auto gen = FlatGenerator::fromPreset(FlatGenerator::kClassicFlat);
    Chunk a({0, 0});
    Chunk b({-12, 40});
    gen->generate(a);
    gen->generate(b);
    CHECK(chunkHash(a) == chunkHash(b)); // flat: same for every chunk
    // Changing this value means generated worlds changed: ask the user first.
    CHECK(chunkHash(a) == 5571082899214163331ull);
}

TEST_CASE("preset parsing rejects bad input") {
    CHECK_FALSE(FlatGenerator::fromPreset("minecraft:nope").has_value());
    CHECK_FALSE(FlatGenerator::fromPreset("0*minecraft:stone").has_value());
    CHECK_FALSE(FlatGenerator::fromPreset("x*minecraft:stone").has_value());
    CHECK_FALSE(FlatGenerator::fromPreset("2000000000*minecraft:stone").has_value());
    CHECK_FALSE(FlatGenerator::fromPreset("384*stone,stone").has_value()); // 385 > 384
    CHECK(FlatGenerator::fromPreset("384*stone").has_value());
    CHECK(FlatGenerator::fromPreset("3*stone,oak_log[axis=x]")->layers().size() == 4);
}
