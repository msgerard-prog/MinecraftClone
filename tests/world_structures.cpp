// Structure placement (wiki: Structure set).
#include "world/StructurePlacement.h"

#include <doctest/doctest.h>

#include <cstdlib>

using namespace mc::world;

TEST_CASE("random spread: one candidate per region, at least `separation` chunks apart") {
    const uint64_t seed = 42;
    for (const RandomSpread& s : {kVillages, kDesertPyramids, kSwampHuts}) {
        for (int rz = -3; rz <= 3; ++rz)
            for (int rx = -3; rx <= 3; ++rx) {
                const ChunkPos c = spreadCandidate(seed, s, {rx * s.spacing + 1, rz * s.spacing + 1});
                CHECK(floorDivChunks(c.x, s.spacing) == rx);
                CHECK(floorDivChunks(c.z, s.spacing) == rz);
                CHECK(c.x - rx * s.spacing < s.spacing - s.separation);
                CHECK(isSpreadCandidate(seed, s, c));
                // The neighbour region's candidate is at least `separation` away on x.
                const ChunkPos n = spreadCandidate(seed, s, {(rx + 1) * s.spacing, rz * s.spacing});
                CHECK(n.x - c.x >= s.separation);
            }
        int count = 0;
        for (int z = 0; z < s.spacing; ++z)
            for (int x = 0; x < s.spacing; ++x)
                count += isSpreadCandidate(seed, s, {x, z});
        CHECK(count == 1);
    }
    // Different salts: different spots.
    CHECK_FALSE(spreadCandidate(seed, kDesertPyramids, {0, 0}) == spreadCandidate(seed, kIgloos, {0, 0}));
}

TEST_CASE("mineshaft candidates: about 0.4% of chunks") {
    int n = 0;
    for (int z = 0; z < 200; ++z)
        for (int x = 0; x < 200; ++x)
            n += isMineshaftCandidate(42, {x, z});
    CHECK(n > 100);
    CHECK(n < 220); // 40000 x 0.004 = 160
}
