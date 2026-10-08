// Oceans (M25.1): the "overworld4" generator's ocean biomes, floors, icebergs and
// flooded caves; waterlogged blocks.
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/OverworldGenerator.h"
#include "world/World.h"

#include <doctest/doctest.h>

#include <optional>
#include <ostream>
#include <string>

using namespace mc::world;

namespace {

const BlockRegistry& R() { return blockRegistry(); }
constexpr int kSeaLevel = OverworldGenerator::kSeaLevel;

// The first chunk (spiralling out from the origin, centres every 4 chunks) whose centre
// column has one of these biomes.
std::optional<ChunkPos> findChunk(const OverworldGenerator& gen, std::initializer_list<Biome> want, int reach = 160) {
    for (int ring = 0; ring <= reach; ring += 4)
        for (int cz = -ring; cz <= ring; cz += 4)
            for (int cx = -ring; cx <= ring; cx += 4) {
                if (std::max(std::abs(cx), std::abs(cz)) != ring) continue;
                const Biome b = gen.biomeAt(gen.column(cx * 16 + 8, cz * 16 + 8));
                for (Biome w : want)
                    if (b == w) return ChunkPos{cx, cz};
            }
    return std::nullopt;
}

int countBlock(const Chunk& c, BlockId b) {
    int n = 0;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) n += R().blockOf(c.get(x, y, z)) == b;
    return n;
}

} // namespace

TEST_CASE("waterlogged blocks: kelp and seagrass always, corals and pickles by their property; they hold water") {
    CHECK(R().waterlogged(R().defaultState(blocks::Kelp)));
    CHECK(R().waterlogged(R().defaultState(blocks::Seagrass)));
    const BlockStateId pickle = R().defaultState(blocks::SeaPickle);
    CHECK(R().waterlogged(pickle));
    CHECK_FALSE(R().waterlogged(R().set(pickle, properties::waterlogged, 1)));
    CHECK(R().lightEmission(R().set(pickle, properties::pickles, 3)) == 15); // 4 pickles in water
    CHECK(R().lightEmission(R().set(pickle, properties::waterlogged, 1)) == 0);
    CHECK(R().lightOpacity(R().defaultState(blocks::Kelp)) == 1); // (like water)
    CHECK(R().findBlock("dead_brain_coral_fan").has_value());
    CHECK(leftAfterBreaking(R().defaultState(blocks::Kelp)) == R().defaultState(blocks::Water));
    CHECK(leftAfterBreaking(R().defaultState(blocks::Stone)) == 0);
    CHECK(BlockUpdates::fluidAmount(R().defaultState(blocks::Seagrass)) == 8); // a source for its neighbours
}

TEST_CASE("overworld4: deep lukewarm/cold/frozen oceans; seagrass and kelp on ocean floors, always under water") {
    const OverworldGenerator gen(42);
    CHECK(gen.kind() == "overworld4");
    const auto deep = findChunk(gen, {Biome::DeepColdOcean, Biome::DeepLukewarmOcean, Biome::DeepFrozenOcean});
    REQUIRE(deep.has_value());
    const auto ocean = findChunk(gen, {Biome::Ocean, Biome::ColdOcean, Biome::LukewarmOcean, Biome::DeepOcean,
                                       Biome::DeepColdOcean, Biome::DeepLukewarmOcean});
    REQUIRE(ocean.has_value());
    Chunk c(*ocean);
    gen.generate(c);
    MESSAGE("kelp ocean chunk " << ocean->x << ", " << ocean->z);
    CHECK(countBlock(c, blocks::Seagrass) + countBlock(c, blocks::TallSeagrass) > 10);
    int kelpTips = 0;
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x)
            for (int y = 0; y < kSeaLevel; ++y) {
                const BlockStateId s = c.get(x, y, z);
                const BlockId b = R().blockOf(s);
                if (b != blocks::Kelp && b != blocks::Seagrass && b != blocks::KelpPlant) continue;
                const BlockId below = R().blockOf(c.get(x, y - 1, z));
                CHECK((R().collides(c.get(x, y - 1, z)) || below == blocks::KelpPlant)); // standing on something
                if (b == blocks::Kelp) {
                    ++kelpTips;
                    CHECK(R().blockOf(c.get(x, y + 1, z)) == blocks::Water); // the tip stays under water
                }
            }
    CHECK(kelpTips > 0);
}

TEST_CASE("overworld4: warm oceans grow coral reefs and sea pickles; frozen oceans carry icebergs") {
    const OverworldGenerator gen(42);
    const auto warm = findChunk(gen, {Biome::WarmOcean}, 400);
    REQUIRE(warm.has_value());
    Chunk w(*warm);
    gen.generate(w);
    MESSAGE("warm ocean chunk " << warm->x << ", " << warm->z);
    int coral = 0;
    for (const char* kind : kCoralKinds) {
        coral += countBlock(w, *R().findBlock(std::string(kind) + "_coral_block"));
        coral += countBlock(w, *R().findBlock(std::string(kind) + "_coral_fan"));
    }
    CHECK(coral > 5);
    const auto frozen = findChunk(gen, {Biome::FrozenOcean, Biome::DeepFrozenOcean}, 400);
    REQUIRE(frozen.has_value());
    MESSAGE("frozen ocean chunk " << frozen->x << ", " << frozen->z);
    int iceAbove = 0;
    for (int dz = -3; dz <= 3; ++dz) // (icebergs start in 1 of 8 frozen chunks: look around)
        for (int dx = -3; dx <= 3; ++dx) {
            Chunk f({frozen->x + dx, frozen->z + dz});
            gen.generate(f);
            for (int z = 0; z < 16; ++z)
                for (int x = 0; x < 16; ++x)
                    for (int y = kSeaLevel; y < kSeaLevel + 14; ++y) {
                        const BlockId b = R().blockOf(f.get(x, y, z));
                        iceAbove += b == blocks::PackedIce || b == blocks::SnowBlock;
                    }
        }
    CHECK(iceAbove > 0);
}

TEST_CASE("overworld4: flooded caves - no water inside a chunk hangs over or beside cave air below the sea") {
    const OverworldGenerator gen(42);
    const auto ocean = findChunk(gen, {Biome::Ocean, Biome::DeepOcean, Biome::ColdOcean});
    REQUIRE(ocean.has_value());
    int caveWater = 0;
    for (int dz = -2; dz <= 2; ++dz)
        for (int dx = -2; dx <= 2; ++dx) {
            Chunk c({ocean->x + dx, ocean->z + dz});
            gen.generate(c);
            const BlockStateId water = R().defaultState(blocks::Water);
            for (int y = OverworldGenerator::kLavaLevel; y < kSeaLevel; ++y)
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        if (c.get(x, y, z) != water) continue;
                        if (y < 40) ++caveWater;
                        // (inside the chunk: its edges meet neighbours with their own barrier)
                        auto air = [&](int ax, int ay, int az) {
                            return ax >= 0 && ax < 16 && az >= 0 && az < 16 && c.get(ax, ay, az) == 0;
                        };
                        const bool leak = air(x, y - 1, z) || air(x - 1, y, z) || air(x + 1, y, z) ||
                                          air(x, y, z - 1) || air(x, y, z + 1);
                        CHECK_FALSE(leak);
                        if (leak) return;
                    }
        }
    CHECK(caveWater > 0); // (deep water: flooded caves under the sea floor)
}

TEST_CASE("overworld4 is deterministic; older generators keep their output (pinned)") {
    const OverworldGenerator a(42), b(42);
    Chunk c1({5, -3}), c2({5, -3});
    a.generate(c1);
    b.generate(c2);
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); y += 3)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) REQUIRE(c1.get(x, y, z) == c2.get(x, y, z));
    // A kelp-ocean chunk, pinned (re-pinned while M25 builds overworld4, frozen at v0.25.0).
    Chunk o({0, 0});
    a.generate(o);
    uint64_t h = 1469598103934665603ull;
    for (int y = kOverworldHeight.minY; y <= kOverworldHeight.maxY(); ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) {
                for (const char ch : R().toString(o.get(x, y, z))) { // (names: state ids move as blocks are added)
                    h ^= uint8_t(ch);
                    h *= 1099511628211ull;
                }
            }
    CHECK(h == 3424206281726334893ull);
}

namespace {

// A stone floor (top y 63) under water up to y 70 over 3x3 chunks.
struct Sea {
    World world;
    BlockUpdates updates{world};
    int64_t time = 0;
    explicit Sea(bool water = true) {
        for (int cz = -1; cz <= 1; ++cz)
            for (int cx = -1; cx <= 1; ++cx) {
                Chunk& c = world.createChunk({cx, cz});
                for (int z = 0; z < 16; ++z)
                    for (int x = 0; x < 16; ++x) {
                        c.set(x, 63, z, R().defaultState(blocks::Stone));
                        if (water)
                            for (int y = 64; y <= 70; ++y) c.set(x, y, z, R().defaultState(blocks::Water));
                    }
            }
    }
    void put(BlockPos p, BlockStateId s) {
        updates.setTime(time);
        world.updateBlock(p, s);
    }
    void tick(int n) {
        for (int i = 0; i < n; ++i) {
            ++time;
            updates.setTime(time);
            updates.tick();
        }
    }
    BlockId block(BlockPos p) const { return R().blockOf(world.getBlock(p)); }
};

} // namespace

TEST_CASE("kelp grows into the water above, its old tip becoming a stem; a stem without its tip is the tip again") {
    Sea s;
    s.updates.setRandomTicks({0, 0}, 1, 1000); // (often; not 4096: the random repeats its low bits every 4096)
    s.put({0, 64, 0}, R().defaultState(blocks::Kelp));
    s.tick(200);
    CHECK(s.block({0, 64, 0}) == blocks::KelpPlant);
    CHECK((s.block({0, 65, 0}) == blocks::KelpPlant || s.block({0, 65, 0}) == blocks::Kelp));
    int top = 64;
    while (s.block({0, top + 1, 0}) == blocks::Kelp || s.block({0, top + 1, 0}) == blocks::KelpPlant) ++top;
    CHECK(top <= 70); // never out of the water
    CHECK(s.block({0, top, 0}) == blocks::Kelp);
    // Breaking the middle: the part above pops, the stem below becomes the tip, water stays.
    s.updates.setRandomTicks({0, 0}, 1, 0);
    s.put({0, 65, 0}, leftAfterBreaking(s.world.getBlock({0, 65, 0})));
    s.tick(2);
    CHECK(s.block({0, 65, 0}) == blocks::Water);
    CHECK(s.block({0, 64, 0}) == blocks::Kelp);
    if (top > 65) CHECK(s.block({0, 66, 0}) == blocks::Water);
}

TEST_CASE("living coral dies out of water; in water it lives") {
    Sea dry(false);
    const BlockStateId block = R().defaultState(*R().findBlock("tube_coral_block"));
    dry.put({0, 64, 0}, block);
    dry.tick(120);
    CHECK(dry.block({0, 64, 0}) == *R().findBlock("dead_tube_coral_block"));
    Sea wet;
    wet.put({0, 64, 0}, block);
    wet.put({1, 64, 0}, R().defaultState(*R().findBlock("fire_coral_fan"))); // (waterlogged)
    wet.tick(120);
    CHECK(wet.block({0, 64, 0}) == *R().findBlock("tube_coral_block"));
    CHECK(wet.block({1, 64, 0}) == *R().findBlock("fire_coral_fan"));
    const BlockStateId dryFan = R().set(R().defaultState(*R().findBlock("fire_coral_fan")), properties::waterlogged, 1);
    dry.put({3, 64, 0}, dryFan);
    dry.tick(120);
    CHECK(dry.block({3, 64, 0}) == *R().findBlock("dead_fire_coral_fan"));
}

TEST_CASE("a waterlogged block's water flows out like a source; water beside it stays a source") {
    Sea s(false);
    s.put({0, 64, 0}, R().defaultState(blocks::Seagrass));
    s.put({5, 64, 0}, R().defaultState(blocks::Stone)); // (a neighbour change wakes it)
    s.put({1, 64, 0}, R().defaultState(blocks::Stone));
    s.put({1, 64, 0}, 0);
    s.tick(40);
    CHECK(s.block({1, 64, 0}) == blocks::Water);
    CHECK(R().get(s.world.getBlock({1, 64, 0}), properties::level) == 1); // flowing from it
    CHECK(s.block({0, 64, 0}) == blocks::Seagrass);
    // Seagrass without its floor pops and leaves its water.
    Sea t;
    t.put({2, 64, 2}, R().defaultState(blocks::Seagrass));
    t.put({2, 63, 2}, R().defaultState(blocks::Water));
    t.tick(2);
    CHECK(t.block({2, 64, 2}) == blocks::Water);
    REQUIRE_FALSE(t.updates.drops().empty());
}
