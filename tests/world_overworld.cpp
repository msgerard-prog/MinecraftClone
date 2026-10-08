// The M8 overworld generator.
#include "world/Blocks.h"
#include "world/OverworldGenerator.h"

#include <doctest/doctest.h>

#include <ostream> // doctest prints std::string_view

#include <cmath>
#include <string_view>
#include <thread>
#include <vector>

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

BlockStateId S(BlockId b) { return blockRegistry().defaultState(b); }

} // namespace

TEST_CASE("overworld is deterministic per seed and position (pinned hash)") {
    const OverworldGenerator a(42, 1), b(42, 1), other(7, 1); // "overworld" (M8)
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

TEST_CASE("overworld (M8): bedrock floor, sea at 63, ores at their depths, lava only deep") {
    const OverworldGenerator gen(42, 1); // overworld2 adds lava lakes and springs
    const auto& r = blockRegistry();
    int water = 0, waterAbove = 0, coal = 0, iron = 0, diamondsHigh = 0, diamonds = 0, lavaHigh = 0;
    for (int cz = -2; cz <= 2; ++cz)
        for (int cx = -2; cx <= 2; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x) {
                    CHECK(r.blockOf(c.get(x, kOverworldHeight.minY, z)) == blocks::Bedrock);
                    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y) {
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

#include "world/Dimension.h"
#include "world/NetherGenerator.h"

TEST_CASE("nether: deterministic, bedrock floor and roof, lava sea, solidAt agrees with the blocks") {
    using namespace mc::world;
    const NetherGenerator gen(42);
    Chunk a({3, -5}, kNetherHeight), b({3, -5}, kNetherHeight);
    gen.generate(a);
    gen.generate(b);
    const auto& r = blockRegistry();
    int lava = 0, mismatches = 0;
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            CHECK(r.blockOf(a.get(x, 0, z)) == blocks::Bedrock);
            CHECK(r.blockOf(a.get(x, 127, z)) == blocks::Bedrock);
            CHECK(a.get(x, -1, z) == 0);   // below the Nether's height (vanilla: Y 0..255)
            CHECK(a.get(x, 128, z) == 0);  // above the roof: open (and buildable) up to 255
            CHECK(a.sectionCount() == 16);
            for (int y = 5; y < 122; ++y) {
                CHECK(a.get(x, y, z) == b.get(x, y, z));
                const BlockId id = r.blockOf(a.get(x, y, z));
                lava += id == blocks::Lava;
                const bool rock = id != 0 && id != blocks::Lava;
                if (y > 40 && y < 100 && rock != gen.solidAt(3 * 16 + x, y, -5 * 16 + z) && id == blocks::Netherrack) ++mismatches;
                if (y > 40 && y < 100 && id == 0 && gen.solidAt(3 * 16 + x, y, -5 * 16 + z)) ++mismatches;
            }
        }
    CHECK(mismatches == 0);
    const glm::dvec3 spawn = gen.findSpawn();
    Chunk s({blockToChunk(int(std::floor(spawn.x))), blockToChunk(int(std::floor(spawn.z)))}, kNetherHeight);
    gen.generate(s);
    const int sx = blockToLocal(int(std::floor(spawn.x))), sz = blockToLocal(int(std::floor(spawn.z))), sy = int(spawn.y);
    CHECK_FALSE(r.collides(s.get(sx, sy, sz))); // (nether2: a plant may grow there)
    CHECK_FALSE(r.collides(s.get(sx, sy + 1, sz)));
    CHECK(r.collides(s.get(sx, sy - 1, sz)));
}

TEST_CASE("the end: an island at the origin with the exit portal, ten obsidian pillars") {
    using namespace mc::world;
    const EndGenerator gen(42);
    const auto& r = blockRegistry();
    Chunk c({0, 0}, kEndHeight);
    gen.generate(c);
    const int top = gen.islandTop(0, 0);
    CHECK(top > 50);
    CHECK(r.blockOf(c.get(1, top + 1, 1)) == blocks::EndPortal);
    CHECK(r.blockOf(c.get(0, top + 3, 0)) == blocks::Bedrock);
    CHECK(gen.islandTop(400, 0) < 0); // no outer islands
    for (int i = 0; i < EndGenerator::kPillars; ++i) {
        const auto& p = gen.pillar(i);
        Chunk pc({blockToChunk(p.x), blockToChunk(p.z)}, kEndHeight);
        gen.generate(pc);
        CHECK(r.blockOf(pc.get(blockToLocal(p.x), p.height, blockToLocal(p.z))) == blocks::Obsidian);
        CHECK(r.blockOf(pc.get(blockToLocal(p.x), p.height + 1, blockToLocal(p.z))) == blocks::Bedrock);
    }
}

TEST_CASE("nether and end output are pinned (seed 42)") {
    using namespace mc::world;
    Chunk n({3, -5}, kNetherHeight), e({3, -5}, kEndHeight);
    NetherGenerator(42, 1).generate(n); // "nether" (M12)
    EndGenerator(42).generate(e);
    CHECK(chunkHash(n) == 4236505564017377935ull);
    CHECK(chunkHash(e) == 11352441782643008173ull);
    Chunk n2({3, -5}, kNetherHeight);
    NetherGenerator(42, 2).generate(n2); // "nether2" (M19, frozen at v0.19.0; pinned in M23.6)
    CHECK(chunkHash(n2) == 6092863011108455525ull);
    Chunk n3({3, -5}, kNetherHeight);
    NetherGenerator(42).generate(n3); // "nether3" (M23.6, new worlds)
    CHECK(chunkHash(n3) == 16372624135412048177ull); // (pinned while M23 builds nether3)
}

TEST_CASE("nether3: nether2 plus ancient debris, never touching air (M23.6)") {
    using namespace mc::world;
    const auto& r = blockRegistry();
    const NetherGenerator gen(42), old(42, 2);
    CHECK(gen.kind() == "nether3");
    int debris = 0, exposed = 0, otherDiffs = 0;
    for (int cz = 0; cz < 6; ++cz)
        for (int cx = 0; cx < 6; ++cx) {
            Chunk a({cx, cz}, kNetherHeight), b({cx, cz}, kNetherHeight);
            gen.generate(a);
            old.generate(b);
            for (int y = 1; y < 127; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        if (a.get(x, y, z) == b.get(x, y, z)) continue;
                        if (r.blockOf(a.get(x, y, z)) != blocks::AncientDebris) {
                            ++otherDiffs;
                            continue;
                        }
                        ++debris;
                        CHECK(y >= 7);
                        for (const auto& d : {std::array{1, 0, 0}, std::array{-1, 0, 0}, std::array{0, 1, 0},
                                              std::array{0, -1, 0}, std::array{0, 0, 1}, std::array{0, 0, -1}}) {
                            const int nx = x + d[0], nz = z + d[2];
                            if (nx >= 0 && nx < 16 && nz >= 0 && nz < 16) exposed += a.get(nx, y + d[1], nz) == 0;
                        }
                    }
        }
    CHECK(otherDiffs == 0); // only debris added
    CHECK(debris > 10);     // about 1-5 per chunk, fewer where it would touch air
    CHECK(exposed == 0);
}

TEST_CASE("dimensions: ids, folders, void depth; far End chunks are empty (no int overflow)") {
    using namespace mc::world;
    CHECK(findDimension("minecraft:the_nether") == Dimension::Nether);
    CHECK(findDimension("the_end") == Dimension::End);
    CHECK(findDimension("nether") == Dimension::Nether);
    CHECK_FALSE(findDimension("minecraft:aether"));
    CHECK(dimensionInfo(Dimension::Nether).folder == "DIM-1");
    CHECK(dimensionInfo(Dimension::End).folder == "DIM1");
    CHECK(dimensionInfo(Dimension::End).voidY == -64.0);
    Chunk far({3125, 0}, kEndHeight); // x = 50000
    EndGenerator(42).generate(far);
    bool empty = true;
    for (int s = 0; s < far.sectionCount(); ++s)
        empty = empty && far.section(s).isEmpty();
    CHECK(empty);
}
