// The M19 Nether generator ("nether2"): five biomes and their features.
#include "world/Blocks.h"
#include "world/NetherGenerator.h"

#include <doctest/doctest.h>

#include <array>
#include <ostream>

using namespace mc::world;

TEST_CASE("nether2: five biomes with their ground and plants; the M12 nether stays wastes only") {
    const NetherGenerator gen(42), old(42, 1);
    CHECK(gen.kind() == "nether2");
    CHECK(old.kind() == "nether");
    std::array<bool, size_t(Biome::Count)> seen{};
    for (int z = -2000; z <= 2000; z += 32)
        for (int x = -2000; x <= 2000; x += 32) {
            seen[size_t(gen.biomeAt(x, z))] = true;
            CHECK(old.biomeAt(x, z) == Biome::NetherWastes);
        }
    for (Biome b : {Biome::NetherWastes, Biome::CrimsonForest, Biome::WarpedForest, Biome::SoulSandValley,
                    Biome::BasaltDeltas})
        CHECK_MESSAGE(seen[size_t(b)], biomeInfo(b).id);
    // A crimson forest chunk: nylium and its plants, huge fungi (stems and wart).
    int32_t cx = 0, cz = 0;
    bool found = false;
    for (int z = -1000; z <= 1000 && !found; z += 16)
        for (int x = -1000; x <= 1000 && !found; x += 16)
            if (gen.biomeAt(x + 8, z + 8) == Biome::CrimsonForest && gen.biomeAt(x - 8, z - 8) == Biome::CrimsonForest &&
                gen.biomeAt(x + 24, z + 24) == Biome::CrimsonForest) {
                cx = x >> 4, cz = z >> 4;
                found = true;
            }
    REQUIRE(found);
    int nylium = 0, stems = 0, wart = 0, roots = 0;
    for (int dz = 0; dz < 2; ++dz)
        for (int dx = 0; dx < 2; ++dx) {
            Chunk c({cx + dx, cz + dz}, kNetherHeight);
            gen.generate(c);
            for (int y = 1; y < 127; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = blockRegistry().blockOf(c.get(x, y, z));
                        nylium += b == blocks::CrimsonNylium;
                        stems += b == blocks::CrimsonStem;
                        wart += b == blocks::NetherWartBlock;
                        roots += b == blocks::CrimsonRoots;
                    }
            CHECK(c.biomes()->at(8, 64, 8, kNetherHeight) == gen.biomeAt((cx + dx) * 16 + 8, (cz + dz) * 16 + 8));
        }
    CHECK(nylium > 100);
    CHECK(stems > 4);
    CHECK(wart > 20);
    CHECK(roots > 10);
    Chunk a({cx, cz}, kNetherHeight), b({cx, cz}, kNetherHeight);
    gen.generate(a);
    gen.generate(b);
    uint64_t h = 1469598103934665603ull;
    for (int y = 0; y < 128; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                CHECK(a.get(x, y, z) == b.get(x, y, z));
                h = (h ^ a.get(x, y, z)) * 1099511628211ull;
            }
    CHECK(h == 18246991405672207176ull); // pinned (re-pinned through M19 until v0.19.0)
}
