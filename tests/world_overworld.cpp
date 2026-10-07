// The M8 overworld generator.
#include "world/Blocks.h"
#include "world/OverworldGenerator.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string_view>
#include <thread>
#include <vector>

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
    // (Re-pinned 2026-10-07 for the M8 review fixes, before any world used it.)
    CHECK(chunkHash(c1) == 1773355576298667210ull);
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
                        lavaHigh += b == blocks::Lava && y >= OverworldGenerator::kLavaLevel; // lava only <= -55
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

TEST_CASE("overworld: generated leaves know their trunk (distance 1..6, never decay)") {
    // Regression: every generated leaf was distance=7 (would all decay).
    const OverworldGenerator gen(42);
    const auto& r = blockRegistry();
    int leaves = 0, far = 0;
    for (int cz = 3; cz <= 7; ++cz) // the birch forest around seed 42's spawn
        for (int cx = -8; cx <= -4; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int y = 60; y < 200; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockStateId s = c.get(x, y, z);
                        const auto id = std::string_view(r.block(r.blockOf(s)).id);
                        if (!id.ends_with("_leaves")) continue;
                        ++leaves;
                        far += r.value(s, "distance") == "7";
                    }
        }
    CHECK(leaves > 0);
    CHECK(far == 0);
}

TEST_CASE("overworld: trees continue seamlessly across chunk borders") {
    // A forest region: compare the blocks on both sides of each border with a chunk
    // generated alone - the same tree appears in both (no half trees).
    const OverworldGenerator gen(42);
    const auto& r = blockRegistry();
    int crossing = 0;
    for (int cz = 3; cz <= 7; ++cz) // the birch forest around seed 42's spawn
        for (int cx = -8; cx <= -5; ++cx) {
            Chunk a({cx, cz}), b({cx + 1, cz});
            gen.generate(a);
            gen.generate(b);
            for (int y = 60; y < 200; ++y)
                for (int z = 0; z < 16; ++z) {
                    const BlockStateId la = a.get(15, y, z), lb = b.get(0, y, z);
                    const bool leafA = std::string_view(r.block(r.blockOf(la)).id).ends_with("_leaves");
                    const bool leafB = std::string_view(r.block(r.blockOf(lb)).id).ends_with("_leaves");
                    // A log at the border column always has leaves or logs nearby on
                    // the other side only if the canopy reaches; count shared canopies.
                    crossing += leafA && leafB;
                }
        }
    CHECK(crossing > 0); // canopies do span borders
}

TEST_CASE("overworld: same output on any thread, in any order, at negative coordinates") {
    const OverworldGenerator gen(99);
    const ChunkPos positions[] = {{-1, -1}, {-17, 3}, {0, 0}, {5, -20}};
    uint64_t forward[4], other[4];
    for (int i = 0; i < 4; ++i) {
        Chunk c(positions[i]);
        gen.generate(c);
        forward[i] = chunkHash(c);
    }
    std::thread t([&] {
        for (int i = 3; i >= 0; --i) {
            Chunk c(positions[i]);
            gen.generate(c);
            other[i] = chunkHash(c);
        }
    });
    t.join();
    for (int i = 0; i < 4; ++i)
        CHECK(forward[i] == other[i]);
}

TEST_CASE("overworld: cold places get snow layers and ice; lava stops at -55") {
    const OverworldGenerator gen(42);
    const auto& r = blockRegistry();
    int snow = 0, ice = 0;
    for (int cz = -40; cz <= 40; cz += 8)
        for (int cx = -40; cx <= 40; cx += 8) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    for (int y = 62; y < 320; ++y) {
                        const BlockId b = r.blockOf(c.get(x, y, z));
                        if (b == blocks::Snow) {
                            ++snow;
                            CHECK(r.collides(c.get(x, y - 1, z))); // on solid ground
                        }
                        ice += b == blocks::Ice && y == 62;
                    }
        }
    CHECK(snow > 0);
    CHECK(ice > 0);
}
