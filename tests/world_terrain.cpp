#include "world/Blocks.h"
#include "world/Noise.h"
#include "world/Random.h"
#include "world/TerrainGenerator.h"

#include <doctest/doctest.h>

#include <set>

using namespace mc::world;

namespace {

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

TEST_CASE("Xoroshiro: same seed same sequence, ranges respected") {
    Xoroshiro a(42), b(42), c(43);
    for (int i = 0; i < 100; ++i)
        CHECK(a.nextLong() == b.nextLong());
    CHECK(a.nextLong() != c.nextLong());
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.nextInt(10) < 10u);
        const double d = a.nextDouble();
        CHECK((d >= 0.0 && d < 1.0));
    }
}

TEST_CASE("improved noise: deterministic, bounded, varies smoothly") {
    OctaveNoise n(1234, -4, 4);
    OctaveNoise same(1234, -4, 4);
    double prev = n.noise2d(0, 0);
    double maxStep = 0;
    for (int x = 1; x < 200; ++x) {
        const double v = n.noise2d(x, 7);
        CHECK(v == same.noise2d(x, 7));
        CHECK((v > -1.2 && v < 1.2));
        maxStep = std::max(maxStep, std::abs(v - n.noise2d(x - 1, 7)));
        prev = v;
    }
    (void)prev;
    CHECK(maxStep < 0.3); // neighbouring blocks never jump wildly
}

TEST_CASE("terrain: bedrock floor, water up to sea level, surface rules") {
    const TerrainGenerator gen(42);
    const auto& r = blockRegistry();
    int ocean = 0, land = 0;
    for (int cz = -3; cz <= 3; ++cz) {
        for (int cx = -3; cx <= 3; ++cx) {
            Chunk c({cx * 9, cz * 9});
            gen.generate(c);
            for (int z = 0; z < 16; z += 5) {
                for (int x = 0; x < 16; x += 5) {
                    CHECK(r.blockOf(c.get(x, kOverworldHeight.minY, z)) == blocks::Bedrock);
                    const int h = gen.surfaceHeight(c.pos().x * 16 + x, c.pos().z * 16 + z);
                    CHECK(c.get(x, h, z) != 0);
                    CHECK(c.get(x, h + 1, z) == (h + 1 < TerrainGenerator::kSeaLevel
                                                     ? r.defaultState(blocks::Water)
                                                     : BlockStateId{0}));
                    if (h >= TerrainGenerator::kSeaLevel + 2) {
                        ++land;
                        CHECK(r.blockOf(c.get(x, h, z)) == blocks::GrassBlock);
                        CHECK(r.blockOf(c.get(x, h - 1, z)) == blocks::Dirt);
                    } else if (h < TerrainGenerator::kSeaLevel - 1) {
                        ++ocean;
                    }
                    CHECK(r.blockOf(c.get(x, -20, z)) == blocks::Deepslate);
                }
            }
        }
    }
    CHECK(land > 0); // this seed and area has both land and sea
    CHECK(ocean > 0);
}

TEST_CASE("terrain is deterministic per seed and position (pinned hash)") {
    const TerrainGenerator a(42), b(42), other(7);
    Chunk c1({3, -5}), c2({3, -5}), c3({3, -5});
    a.generate(c1);
    b.generate(c2);
    other.generate(c3);
    CHECK(chunkHash(c1) == chunkHash(c2));
    CHECK(chunkHash(c1) != chunkHash(c3));
    // Changing this value means generated worlds changed: ask the user first.
    CHECK(chunkHash(c1) == 274924119714909372ull);
}

TEST_CASE("terrain bands: bedrock thins out, deepslate fades in over y 0..7, 3 dirt") {
    const TerrainGenerator gen(5);
    const auto& r = blockRegistry();
    int bedrock[5] = {}, deepslate[9] = {};
    int columns = 0;
    for (int cz = 0; cz < 4; ++cz) {
        for (int cx = 0; cx < 4; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    ++columns;
                    for (int i = 0; i < 5; ++i)
                        bedrock[i] += r.blockOf(c.get(x, kOverworldHeight.minY + i, z)) == blocks::Bedrock;
                    for (int i = 0; i < 9; ++i)
                        deepslate[i] += r.blockOf(c.get(x, i, z)) == blocks::Deepslate;
                    const int h = gen.surfaceHeight(cx * 16 + x, cz * 16 + z);
                    if (h >= TerrainGenerator::kSeaLevel + 2) {
                        for (int d = 1; d <= 3; ++d)
                            CHECK(r.blockOf(c.get(x, h - d, z)) == blocks::Dirt);
                        CHECK(r.blockOf(c.get(x, h - 4, z)) != blocks::Dirt);
                    }
                }
            }
        }
    }
    CHECK(bedrock[0] == columns); // y -64 is solid
    for (int i = 1; i < 5; ++i) {
        const double expected = (5 - i) / 5.0;
        CHECK(bedrock[i] / double(columns) == doctest::Approx(expected).epsilon(0.08));
    }
    CHECK(deepslate[8] == 0); // never at y 8
    for (int i = 0; i < 8; ++i) {
        const double expected = (8 - i) / 8.0;
        CHECK(deepslate[i] / double(columns) == doctest::Approx(expected).epsilon(0.1));
    }
}
