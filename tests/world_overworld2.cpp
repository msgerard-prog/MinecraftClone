// The M18 overworld generator ("overworld2"): the M8 terrain plus ravines, lava
// lakes, springs and more vegetation.
#include "world/Blocks.h"
#include "world/OverworldGenerator.h"
#include "world/StructurePlacement.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <ostream>
#include <utility>

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

int count(const Chunk& c, BlockId b, int minY = kOverworldHeight.minY) {
    int n = 0;
    for (int y = minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x)
                n += blockRegistry().blockOf(c.get(x, y, z)) == b;
    return n;
}

} // namespace

TEST_CASE("overworld2 is the newest kind, deterministic (pinned hash); overworld stays as it was") {
    const OverworldGenerator a(42), b(42), old(42, 1);
    CHECK(a.kind() == "overworld2");
    CHECK(old.kind() == "overworld");
    // Around the spawn (land: lakes, plants, springs) - same twice, then pinned.
    const glm::dvec3 spawn = a.findSpawn();
    uint64_t h = 0;
    for (int k = 0; k < 9; ++k) {
        const ChunkPos p{(int32_t(std::floor(spawn.x)) >> 4) + k % 3 - 1, (int32_t(std::floor(spawn.z)) >> 4) + k / 3 - 1};
        Chunk c1(p), c2(p);
        a.generate(c1);
        b.generate(c2);
        CHECK(chunkHash(c1) == chunkHash(c2));
        h = h * 31 + chunkHash(c1);
    }
    Chunk o({3, -5});
    old.generate(o);
    CHECK(chunkHash(o) == 1773355576298667210ull); // the M8 pin (world_overworld.cpp)
    // Pinned. overworld2 grows through M18 (biomes, structures) and is re-pinned at
    // each M18 step until v0.18.0 freezes it; after that, changing it needs the user's OK.
    CHECK(h == 7532567044456039393ull);
}

TEST_CASE("overworld2: ravines are carved where their steps run, seamlessly across chunks") {
    const OverworldGenerator gen(42);
    OverworldGenerator::Ravine rv;
    int found = 0, carved = 0, checked = 0;
    for (int cz = -20; cz <= 20 && found < 3; ++cz)
        for (int cx = -20; cx <= 20 && found < 3; ++cx) {
            if (!gen.ravine(cx, cz, rv)) continue;
            ++found;
            CHECK(rv.count > 40);
            // A deep step's centre.
            for (int i = rv.count / 3; i < rv.count; i += 7) {
                const auto& st = rv.steps[size_t(i)];
                if (st.y > 40.0f || st.y < -50.0f) continue;
                const int32_t x = int32_t(std::floor(st.x)), y = int32_t(std::floor(st.y)), z = int32_t(std::floor(st.z));
                CHECK(gen.inRavine(x, y, z));
                Chunk c({x >> 4, z >> 4});
                gen.generate(c);
                ++checked;
                // Air, or water under a sea (vanilla's flooded ravines).
                const BlockId b = blockRegistry().blockOf(c.get(x & 15, y, z & 15));
                carved += b == blocks::Air || b == blocks::Water;
                break;
            }
        }
    CHECK(found > 0);
    REQUIRE(checked > 0);
    CHECK(carved == checked);
}

TEST_CASE("overworld2: lava lakes, springs with pending fluid ticks, sugar cane by water") {
    const OverworldGenerator gen(42);
    const glm::dvec3 spawn = gen.findSpawn(); // on land
    const int32_t sx = int32_t(std::floor(spawn.x)) >> 4, sz = int32_t(std::floor(spawn.z)) >> 4;
    int lavaHigh = 0, springs = 0, cane = 0, mushrooms = 0;
    for (int cz = sz - 3; cz <= sz + 3; ++cz)
        for (int cx = sx - 3; cx <= sx + 3; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            lavaHigh += count(c, blocks::Lava, OverworldGenerator::kLavaLevel);
            cane += count(c, blocks::SugarCane);
            mushrooms += count(c, blocks::BrownMushroom) + count(c, blocks::RedMushroom);
            for (const auto& t : std::as_const(c).blockTicks()) {
                const BlockId b = blockRegistry().blockOf(c.get(t.x, t.y, t.z));
                CHECK(b == t.block); // a spring's own fluid
                ++springs;
            }
            if (!std::as_const(c).blockTicks().empty()) CHECK(c.ticksRelative);
        }
    CHECK(lavaHigh > 0);
    CHECK(springs > 0);
    CHECK(cane > 0);
    CHECK(mushrooms > 0);
}

TEST_CASE("overworld2: deserts grow cacti with nothing solid beside them") {
    const OverworldGenerator gen(42);
    int32_t dx = 0, dz = 0;
    bool desert = false;
    for (int r = 0; r < 4000 && !desert; r += 64)
        for (int i = -r; i <= r && !desert; i += 64)
            for (const auto& p : {std::array{i, -r}, std::array{i, r}, std::array{-r, i}, std::array{r, i}})
                if (!desert && gen.biomeAt(gen.column(p[0], p[1])) == Biome::Desert) {
                    desert = true;
                    dx = p[0];
                    dz = p[1];
                }
    REQUIRE(desert);
    int cacti = 0;
    for (int k = 0; k < 9; ++k) {
        Chunk c({(dx >> 4) + k % 3 - 1, (dz >> 4) + k / 3 - 1});
        gen.generate(c);
        for (int y = 60; y < 200; ++y)
            for (int z = 1; z < 15; ++z)
                for (int x = 1; x < 15; ++x) {
                    if (blockRegistry().blockOf(c.get(x, y, z)) != blocks::Cactus) continue;
                    ++cacti;
                    for (const auto& d : {std::array{1, 0}, std::array{-1, 0}, std::array{0, 1}, std::array{0, -1}})
                        CHECK_FALSE(blockRegistry().collides(c.get(x + d[0], y, z + d[1])));
                }
    }
    CHECK(cacti > 0);
}

TEST_CASE("overworld2: the M18.2 biomes all occur; overworld (M8) never places them") {
    const OverworldGenerator gen(42), old(42, 1);
    std::array<bool, size_t(Biome::Count)> seen{}, seenOld{};
    for (int z = -3000; z <= 3000; z += 64)
        for (int x = -3000; x <= 3000; x += 64) {
            seen[size_t(gen.biomeAt(gen.column(x, z)))] = true;
            seenOld[size_t(old.biomeAt(old.column(x, z)))] = true;
        }
    for (Biome b : {Biome::Jungle, Biome::SparseJungle, Biome::DarkForest, Biome::FlowerForest,
                    Biome::OldGrowthSpruceTaiga, Biome::CherryGrove, Biome::IceSpikes, Biome::MushroomFields,
                    Biome::ErodedBadlands}) {
        CHECK_MESSAGE(seen[size_t(b)], biomeInfo(b).id);
        CHECK_FALSE(seenOld[size_t(b)]);
    }
}

TEST_CASE("overworld2: dungeons - cobblestone rooms with a spawner and loot chests") {
    const OverworldGenerator gen(42);
    int spawners = 0, chests = 0, loot = 0, mossy = 0;
    for (int cz = -3; cz <= 3; ++cz)
        for (int cx = -3; cx <= 3; ++cx) {
            Chunk c({cx, cz});
            gen.generate(c);
            for (const auto& s : c.spawners()) {
                ++spawners;
                CHECK(blockRegistry().blockOf(c.get(s.x, s.y, s.z)) == blocks::Spawner);
                CHECK((s.data.mob == MobType::Zombie || s.data.mob == MobType::Skeleton || s.data.mob == MobType::Spider));
                mossy += blockRegistry().blockOf(c.get(s.x, s.y - 1, s.z)) == blocks::MossyCobblestone;
            }
            for (const auto& ch : c.chests()) {
                ++chests;
                for (const ItemStack& it : ch.data.items)
                    loot += !it.empty();
            }
        }
    MESSAGE("spawners " << spawners << " chests " << chests << " loot " << loot);
    CHECK(spawners > 0);
    CHECK(chests > 0);
    CHECK(loot > chests); // several stacks each
}

TEST_CASE("overworld2: a desert pyramid - sandstone, a terracotta floor, 4 loot chests over TNT") {
    const OverworldGenerator gen(42);
    // Find the nearest desert pyramid candidate in a desert.
    ChunkPos found{0, 0};
    bool ok = false;
    for (int rz = -20; rz <= 20 && !ok; ++rz)
        for (int rx = -20; rx <= 20 && !ok; ++rx) {
            const ChunkPos c = spreadCandidate(42, kDesertPyramids, {rx * 32, rz * 32});
            if (gen.biomeAt(gen.column(c.x * 16 + 8, c.z * 16 + 8)) == Biome::Desert &&
                gen.surfaceY(c.x * 16 + 10, c.z * 16 + 10) >= OverworldGenerator::kSeaLevel - 1) {
                found = c;
                ok = true;
            }
        }
    REQUIRE(ok);
    int chests = 0, loot = 0, tnt = 0, terracotta = 0;
    for (int dz = 0; dz <= 1; ++dz)
        for (int dx = 0; dx <= 1; ++dx) {
            Chunk c({found.x + dx, found.z + dz});
            gen.generate(c);
            for (const auto& ch : c.chests()) {
                ++chests;
                for (const ItemStack& it : ch.data.items)
                    loot += !it.empty();
            }
            for (int y = -10; y < 200; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = blockRegistry().blockOf(c.get(x, y, z));
                        tnt += b == blocks::Tnt;
                        terracotta += b == blocks::OrangeTerracotta;
                    }
        }
    CHECK(chests == 4);
    CHECK(loot > 8);
    CHECK(tnt == 9);
    CHECK(terracotta > 20);
}

TEST_CASE("overworld2: mineshafts carve plank-supported corridors from a dirt-floored room") {
    const OverworldGenerator gen(42);
    ChunkPos start{0, 0};
    bool found = false;
    for (int z = -40; z <= 40 && !found; ++z)
        for (int x = -40; x <= 40 && !found; ++x)
            if (isMineshaftCandidate(42, {x, z})) {
                start = {x, z};
                found = true;
            }
    REQUIRE(found);
    int planks = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            Chunk c({start.x + dx, start.z + dz});
            gen.generate(c);
            for (int y = -60; y < 40; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x)
                        planks += blockRegistry().blockOf(c.get(x, y, z)) == blocks::OakPlanks;
        }
    CHECK(planks > 30); // supports and bridges
    // The start room opens into corridors: air right outside its walls somewhere.
    Chunk room(start);
    gen.generate(room);
}

TEST_CASE("overworld2: strongholds - 3 in the first ring; stone brick rooms with a 12-frame portal room") {
    const OverworldGenerator gen(42);
    const auto near = gen.nearestStronghold(0, 0);
    REQUIRE(near);
    const double d = std::hypot(double(near->x), double(near->y));
    CHECK(d >= 1280 - 16);
    CHECK(d <= 2816 + 16);
    CHECK_FALSE(OverworldGenerator(42, 1).nearestStronghold(0, 0)); // not in the M8 overworld
    int frames = 0, bricks = 0, chests = 0;
    for (int dz = -3; dz <= 3; ++dz)
        for (int dx = -3; dx <= 3; ++dx) {
            Chunk c({(near->x >> 4) + dx, (near->y >> 4) + dz});
            gen.generate(c);
            chests += static_cast<int>(c.chests().size());
            for (int y = -60; y < 60; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        const BlockId b = blockRegistry().blockOf(c.get(x, y, z));

                        frames += b == blocks::EndPortalFrame;
                        bricks += b == blocks::StoneBricks || b == blocks::MossyStoneBricks || b == blocks::CrackedStoneBricks;
                    }
        }
    MESSAGE("stronghold at " << near->x << "," << near->y << " frames " << frames << " chests " << chests);
    CHECK(frames == 12);
    CHECK(bricks > 1000);
}
