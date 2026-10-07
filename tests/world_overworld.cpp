// The M8 overworld generator.
#include "world/Blocks.h"
#include "world/OverworldGenerator.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace mc::world;

namespace {

uint64_t chunkHash(const Chunk& c) {
    uint64_t h = 1469598103934665603ull;
    for (int y = kMinY; y <= kMaxY; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                h ^= c.get(x, y, z);
                h *= 1099511628211ull;
            }
    return h;
}

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

} // namespace

TEST_CASE("overworld is deterministic per seed and position (pinned hash)") {
    const OverworldGenerator a(42), b(42), other(7);
    Chunk c1({3, -5}), c2({3, -5}), c3({3, -5});
    a.generate(c1);
    b.generate(c2);
    other.generate(c3);
    CHECK(chunkHash(c1) == chunkHash(c2));
    CHECK(chunkHash(c1) != chunkHash(c3));
    CHECK(c1.biomes()->cells == c2.biomes()->cells);
    // Changing this value means generated worlds changed: ask the user first.
    CHECK(chunkHash(c1) == 9661439017696757985ull);
}

TEST_CASE("overworld: bedrock floor, sea at 63, ores at their depths, lava only deep") {
    const OverworldGenerator gen(42);
    const auto& r = blockRegistry();
    int water = 0, waterAbove = 0, coal = 0, iron = 0, diamondsHigh = 0, diamonds = 0, lavaHigh = 0;
    for (int cz = -2; cz <= 2; ++cz)
        for (int cx = -2; cx <= 2; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    CHECK(r.blockOf(c.get(x, kMinY, z)) == blocks::Bedrock);
                    for (int y = kMinY; y <= kMaxY; ++y) {
                        const BlockId b = r.blockOf(c.get(x, y, z));
                        if (b == blocks::Water) {
                            ++water;
                            waterAbove += y >= OverworldGenerator::kSeaLevel;
                        }
                        coal += b == blocks::CoalOre || b == blocks::DeepslateCoalOre;
                        iron += b == blocks::IronOre || b == blocks::DeepslateIronOre;
                        if (b == blocks::DiamondOre || b == blocks::DeepslateDiamondOre) {
                            ++diamonds;
                            diamondsHigh += y > 16;
                        }
                        lavaHigh += b == blocks::Lava && y > OverworldGenerator::kLavaLevel;
                    }
                }
        }
    CHECK(waterAbove == 0); // no water above sea level (no springs/lakes yet)
    CHECK(coal > 100);      // 25 chunks
    CHECK(iron > 100);
    CHECK(diamonds > 0);
    CHECK(diamondsHigh == 0); // wiki: diamonds below y 16
    CHECK(lavaHigh == 0);
    (void)water;
}

TEST_CASE("overworld: the spawn point is dry land") {
    for (uint64_t seed : {1ull, 42ull, 12345ull}) {
        const OverworldGenerator gen(seed);
        const glm::dvec3 spawn = gen.findSpawn();
        CHECK(spawn.y >= OverworldGenerator::kSeaLevel);
        const auto biome = gen.biomeAt(gen.column(int(std::floor(spawn.x)), int(std::floor(spawn.z))));
        CHECK(biome != Biome::Ocean);
        CHECK(biome != Biome::DeepOcean);
    }
}

TEST_CASE("overworld: surfaceY matches the generated ground (trees stand on it)") {
    const OverworldGenerator gen(42);
    Chunk c({0, 0});
    gen.generate(c);
    const auto& r = blockRegistry();
    int matches = 0, checked = 0;
    for (int z = 0; z < 16; z += 3)
        for (int x = 0; x < 16; x += 3) {
            const int y = gen.surfaceY(x, z);
            if (y < OverworldGenerator::kSeaLevel) continue;
            ++checked;
            // The ground block (whatever surface rule made it) is solid; above it there
            // is no stone/dirt (caves never reach the top layers).
            const BlockStateId ground = c.get(x, y, z);
            const BlockStateId above = c.get(x, y + 1, z);
            matches += r.collides(ground) && (above == 0 || !r.opaqueCube(above) ||
                                              r.blockOf(above) == blocks::OakLog ||
                                              r.blockOf(above) == blocks::BirchLog ||
                                              r.blockOf(above) == blocks::SpruceLog ||
                                              r.blockOf(above) == blocks::AcaciaLog);
        }
    CHECK(matches == checked);
}

TEST_CASE("overworld: biomes cover oceans, land and mountains across a region") {
    const OverworldGenerator gen(42);
    bool seen[static_cast<int>(Biome::Count)] = {};
    for (int z = -4096; z <= 4096; z += 64)
        for (int x = -4096; x <= 4096; x += 64)
            seen[static_cast<int>(gen.biomeAt(gen.column(x, z)))] = true;
    int kinds = 0;
    for (bool s : seen)
        kinds += s;
    CHECK(seen[static_cast<int>(Biome::Plains)]);
    CHECK(seen[static_cast<int>(Biome::Forest)]);
    CHECK((seen[static_cast<int>(Biome::Ocean)] || seen[static_cast<int>(Biome::DeepOcean)]));
    CHECK(kinds >= 15);
}

TEST_CASE("debug: find a cave" * doctest::skip()) {
    const OverworldGenerator gen(42);
    int found = 0;
    for (int cz = 0; cz < 4 && found < 3; ++cz)
        for (int cx = 0; cx < 4 && found < 3; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int y = 30; y > -50 && found < 3; y -= 3)
                for (int z = 2; z < 14 && found < 3; z += 4)
                    for (int x = 2; x < 14 && found < 3; x += 4) {
                        int air = 0;
                        for (int dy = 0; dy < 4; ++dy)
                            for (int d = -2; d <= 2; ++d)
                                air += c.get(x + d, y + dy, z) == 0 && c.get(x, y + dy, z + d) == 0;
                        if (air == 20) {
                            std::printf("cave at %d %d %d\n", cx * 16 + x, y, cz * 16 + z);
                            ++found;
                        }
                    }
        }
}
